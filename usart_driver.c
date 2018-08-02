#include "usart_driver.h"
#include "serial_posix.h"
#include "serial.h"
#include "debug.h"
#include "frame_phase.h"

int16_t calculateCRC(unsigned char *_regs,unsigned char arraySize)
{
  unsigned int temp, temp2, flag;
  unsigned char i,j;

  temp = 0xFFFF;
  for (i = 0; i < arraySize; i++)
  {
    temp = temp ^ *(_regs+i);
    for (j = 1; j <= 8; j++)
    {
      flag = temp & 0x0001;
      temp >>= 1;
      if (flag)
        temp ^= 0xA001;
    }
  }
  temp2 = temp >> 8;
  temp = (temp << 8) | temp2;
  temp &= 0xFFFF;
  return temp;
}

static int isCrcVailed(unsigned char *_regs,unsigned char arraySize)
{
    int16_t crc_rec = *(_regs+arraySize-2)<<8 + *(_regs+arraySize-1);

    int16_t crc_calc = calculateCRC(_regs,arraySize-2);

    printf("crc_rec is %x ,calc is %x",crc_rec,crc_calc);

    if(crc_rec == crc_calc)
    {
        return 1;
    }
    else
    {
        return 0;
    }

}

void send(struct usart_driver *framer,uint8_t *data,int len)
{
    int pos=0;

    if(len<0 || len>FRAME_BUFF_LEN)
        return ;
    //
    int16_t crc16 = calculateCRC(data,len);
    uint8_t buf[len+sizeof(crc16)];
    printf("crc is %d \n",crc16);
    fflush(stdout);
    memcpy(buf,data,len);
    memcpy(buf+len,&crc16,sizeof(crc16));
    len+=sizeof(crc16);
    //put SOH in first byte
    framer->buf[pos++] = SSOH;

    for(int i=0; i<len; i++,pos++)
    {
        if ( *(buf+i) == SSOH || *(buf+i) == SEOT || *(buf+i) == SESC)
        {
            //put ESC
            framer->buf[pos++] = SESC;
        }
        // put data into buf
        framer->buf[pos] = *(buf+i);

    }
    //put CRC

    //put EOT
    framer->buf[pos++] = SEOT;

    //send to usart
    SerialWrite(framer->buf,pos);

}

void usartread(struct usart_driver *driver,uint8_t byte)
{
    zxy_framer *framer_driver =  &__framer;

    if(!driver->escflag && byte == SESC)
    {
        driver->escflag = 1;
        debug_print("ESC SET\n");
        return;
    }

    if(!driver->escflag && byte == SSOH)
    {
        driver->receiveLen=0;
        debug_print("SOH SET\n");
        return;
    }

    if(!driver->escflag && byte == SEOT)
    {
        driver->recSucess = 1;
        driver->escflag =0;
        //calc CRC
        if(isCrcVailed(driver->rec_buf,driver->receiveLen))
        {
            framer_driver->input(framer_driver,driver->rec_buf,driver->receiveLen-2);
        }
        else
        {
            printf("CRC not vaild");
        }
        //debug_print("recSucess %d\n",driver->receiveLen);

       // for(int i=0;i<driver->receiveLen;i++)
         //   debug_print("%c",*(driver->rec_buf+i));

        return;
    }

    if(driver->escflag)
    {
        *(driver->rec_buf+driver->receiveLen++) = byte;
        driver->escflag = 0;
        debug_print("rec  esc data\n");
        return;
    }

    *(driver->rec_buf+driver->receiveLen++) = byte;
    debug_print("rec  data\n");
    return;

}
    uint8_t buf[FRAME_BUFF_LEN];
    uint8_t rec_buf[FRAME_BUFF_LEN];

usart_driver __usart_driver=
{
    buf,
    rec_buf,
    0,
    0,
    0,
    send,
    usartread
};

