#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "timer.h"
#include "lcd.h"
#include "key.h"
#include "beep.h"
#include "string.h"
#include "malloc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
/************************************************
 ALIENTEK ̽����STM32F407������ FreeRTOSʵ��19-1
 FreeRTOS���������Ӻ���ʵ��-�⺯���汾
 ����֧�֣�www.openedv.com
 �Ա����̣�http://eboard.taobao.com 
 ��ע΢�Ź���ƽ̨΢�źţ�"����ԭ��"����ѻ�ȡSTM32���ϡ�
 ������������ӿƼ����޹�˾  
 ���ߣ�����ԭ�� @ALIENTEK
************************************************/

//�������ȼ�
#define START_TASK_PRIO		1
//�����ջ��С	
#define START_STK_SIZE 		256  
//������
TaskHandle_t StartTask_Handler;
//������
void start_task(void *pvParameters);

//�������ȼ�
#define TASK1_TASK_PRIO		2
//�����ջ��С	
#define TASK1_STK_SIZE 		256  
//������
TaskHandle_t Task1Task_Handler;
//������
void task1_task(void *pvParameters);

//�������ȼ�
#define DATAPROCESS_TASK_PRIO 3
//�����ջ��С	
#define DATAPROCESS_STK_SIZE  256 
//������
TaskHandle_t DataProcess_Handler;
//������
void DataProcess_task(void *pvParameters);

//��ֵ�ź������
SemaphoreHandle_t BinarySemaphore;	//��ֵ�ź������

//������������õ�����ֵ
#define LED1ON	1
#define LED1OFF	2
#define BEEPON	3
#define BEEPOFF	4
#define COMMANDERR	0XFF

//����͹���ģʽǰ��Ҫ���������
void BeforeEnterSleep(void)
{
	//�ر�ĳЩ�͹���ģʽ�²�ʹ�õ�����ʱ�ӣ�
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, DISABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, DISABLE);	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, DISABLE);	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, DISABLE);	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, DISABLE);	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, DISABLE);	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOH, DISABLE);	  
}

//�˳��͹���ģʽ�Ժ���Ҫ���������
void AfterExitSleep(void)
{
	//�˳��͹���ģʽ�Ժ����Щ���رյ�����ʱ��
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, ENABLE);	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOH, ENABLE);	              
}

//���������Ӻ���
void vApplicationIdleHook(void)
{
	__disable_irq();
	//__dsb(portSY_FULL_READ_WRITE );//??????????????? keil intrinsic no equivalent in gcc
	//__isb(portSY_FULL_READ_WRITE );//???????????????
	
	BeforeEnterSleep();		//����˯��ģʽ֮ǰ��Ҫ���������
	WFI_SET();				//����˯��ģʽ
	AfterExitSleep();		//�˳�˯��ģʽ֮����Ҫ���������
	
	//__dsb(portSY_FULL_READ_WRITE );
	//__isb(portSY_FULL_READ_WRITE );
	__enable_irq();
}

//���ַ����е�Сд��ĸת��Ϊ��д
//str:Ҫת�����ַ���
//len���ַ�������
void LowerToCap(u8 *str,u8 len)
{
	u8 i;
	for(i=0;i<len;i++)
	{
		if((96<str[i])&&(str[i]<123))	//Сд��ĸ
		str[i]=str[i]-32;				//ת��Ϊ��д
	}
}

//������������ַ�������ת��������ֵ
//str������
//����ֵ: 0XFF�������������ֵ������ֵ
u8 CommandProcess(u8 *str)
{
	u8 CommandValue=COMMANDERR;
	if(strcmp((char*)str,"LED1ON")==0) CommandValue=LED1ON;
	else if(strcmp((char*)str,"LED1OFF")==0) CommandValue=LED1OFF;
	else if(strcmp((char*)str,"BEEPON")==0) CommandValue=BEEPON;
	else if(strcmp((char*)str,"BEEPOFF")==0) CommandValue=BEEPOFF;
	return CommandValue;
}

int main(void)
{ 
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);//����ϵͳ�ж����ȼ�����4
	delay_init(168);					//��ʼ����ʱ����
	uart_init(115200);     				//��ʼ������
	LED_Init();		        			//��ʼ��LED�˿�
	KEY_Init();							//��ʼ������
	BEEP_Init();						//��ʼ��������
	my_mem_init(SRAMIN);            	//��ʼ���ڲ��ڴ��
    
	vTraceEnable(TRC_START);//start tracealyzer

	//������ʼ����
    xTaskCreate((TaskFunction_t )start_task,            //������
                (const char*    )"start_task",          //��������
                (uint16_t       )START_STK_SIZE,        //�����ջ��С
                (void*          )NULL,                  //���ݸ��������Ĳ���
                (UBaseType_t    )START_TASK_PRIO,       //�������ȼ�
                (TaskHandle_t*  )&StartTask_Handler);   //������              
    vTaskStartScheduler();          //�����������
}

//��ʼ����������
void start_task(void *pvParameters)
{
    taskENTER_CRITICAL();           //�����ٽ���
	
	//������ֵ�ź���
	BinarySemaphore=xSemaphoreCreateBinary();	
    //����TASK1����
    xTaskCreate((TaskFunction_t )task1_task,             
                (const char*    )"task1_task",           
                (uint16_t       )TASK1_STK_SIZE,        
                (void*          )NULL,                  
                (UBaseType_t    )TASK1_TASK_PRIO,        
                (TaskHandle_t*  )&Task1Task_Handler);   
    //����TASK2����
    xTaskCreate((TaskFunction_t )DataProcess_task,     
                (const char*    )"keyprocess_task",   
                (uint16_t       )DATAPROCESS_STK_SIZE,
                (void*          )NULL,
                (UBaseType_t    )DATAPROCESS_TASK_PRIO,
                (TaskHandle_t*  )&DataProcess_Handler); 
    vTaskDelete(StartTask_Handler); //ɾ����ʼ����
    taskEXIT_CRITICAL();            //�˳��ٽ���
}

//task1������
void task1_task(void *pvParameters)
{
	while(1)
	{
		LED0=!LED0;
        vTaskDelay(500);             	//��ʱ500ms��Ҳ����500��ʱ�ӽ���	
	}
}

//DataProcess_task����
void DataProcess_task(void *pvParameters)
{
	u8 len=0;
	u8 CommandValue=COMMANDERR;
	BaseType_t err=pdFALSE;
	
	u8 *CommandStr;
	while(1)
	{
		err=xSemaphoreTake(BinarySemaphore,portMAX_DELAY);	//��ȡ�ź���
		if(err==pdTRUE)										//��ȡ�ź����ɹ�
		{
			len=USART_RX_STA&0x3fff;						//�õ��˴ν��յ������ݳ���
			CommandStr=mymalloc(SRAMIN,len+1);				//�����ڴ�
			sprintf((char*)CommandStr,"%s",USART_RX_BUF);
			CommandStr[len]='\0';							//�����ַ�����β����
			LowerToCap(CommandStr,len);						//���ַ���ת��Ϊ��д		
			CommandValue=CommandProcess(CommandStr);		//�������
			if(CommandValue!=COMMANDERR)
			{
				printf("����Ϊ:%s\r\n",CommandStr);
				switch(CommandValue)						//��������
				{
					case LED1ON: 
						LED1=0;
						break;
					case LED1OFF:
						LED1=1;
						break;
					case BEEPON:
						BEEP=1;
						break;
					case BEEPOFF:
						BEEP=0;
						break;
				}
			}
			else
			{
				printf("��Ч���������������!!\r\n");
			}
			USART_RX_STA=0;
			memset(USART_RX_BUF,0,USART_REC_LEN);			//���ڽ��ջ���������
			myfree(SRAMIN,CommandStr);						//�ͷ��ڴ�
		}
	}
}

