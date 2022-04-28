//Justin Harris
//RTOS -- Assignment 6 -- HDC 1080
//2-16-22
//This program reads inputs from the HDC1080 Temperature
//Humidity sensor, prints them out to the console, and displays
//them on a 7 segment LED.

//FreeRTOS headers
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

//C Headers
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>


//Pico Headers
#include "pico/stdlib.h"
#include "tusb.h"
#include "pico/binary_info.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/adc.h"
#include "hardware/uart.h"

// I2C reserves some addresses for special purposes. We exclude these from the scan.
// These are any addresses of the form 000 0xxx or 111 1xxx
#define HDC1080TEMPREG 0x00
#define HDC1080HUMREG 0x01
#define HDC1080CONFIGREG 0x02
#define HDC1080ADDRESS 0x40
#define HDC1080SN1 0xFB
#define HDC1080SN2 0xFC
#define HDC1080SN3 0xFD
#define HDC1080DEVICEIDREG 0xFE
#define HDC1080DEVICEID 0xFF
#define I2C_PORT i2c1

//define 7-segment led pins
#define SevenSegCC1 11  //right number
#define SevenSegCC2 10  //left number

#define SevenSegA 26    //Top bar
#define SevenSegB 27    //Top right
#define SevenSegC 29    //bottom right
#define SevenSegD 18    //bottom bar
#define SevenSegE 25    //bottom left
#define SevenSegF 7     //Top Left
#define SevenSegG 28    //Middle
#define SevenSegDP 24   //decimal points

//Function prototypes
int readConfigReg();
int readMFID();
int readSN1();
int readSN2();
int readSN3();
int readTemperature();
int readHumidity();

//Task Prototypes
void readHDC1080Task();
void segLEDLeft();
void segLEDRight();

//Define Queue variable to hold humidity and temp values
QueueHandle_t tempHumqueue;

//Define semaphore for 7segLEDs
SemaphoreHandle_t ledSem;

int main() {
    // Enable UART so we can print status output
  stdio_init_all();
  while (!tud_cdc_connected()) { sleep_ms(100);  }    
    
    // This example will use I2C1 on the default SDA and SCL pins
    i2c_init(I2C_PORT, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);

    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));

    //initialize Semaphores
    vSemaphoreCreateBinary(ledSem);

    //initialize Queues
    tempHumqueue = xQueueCreate(2, sizeof(int));
    
    //initialize task to read from HDC1080
    xTaskCreate(readHDC1080Task, "readHDC1080Task", 256, NULL, 1, NULL);

    //initialize tasks to display on 7 seg leds
    xTaskCreate(segLEDLeft, "segLEDLeft", 128, NULL, 1, NULL);
    xTaskCreate(segLEDRight, "segLEDRight", 128, NULL, 1, NULL);

    //start scheduler
    vTaskStartScheduler();
  
  while(1){};
}

//This is the main task that reads the data from the HDC1080
//and sends the data to a queue to display on 7-segment LEDs
void readHDC1080Task() {

    //Initialize variables
    int configStat;
    int mfID;
    int serialNum1;
    int serialNum2;
    int serialNum3;
    int temperatureInC;
    int temperatureInF;
    int humidity;
    int clearQueue;

    //Get Device ID values and print out on intial execution
    configStat = readConfigReg();
    mfID = readMFID();
    serialNum1 = readSN1();
    serialNum2 = readSN2();
    serialNum3 = readSN3();


    printf("Configuration Register = 0x%X\n", configStat);
    printf("Manufacturer ID = 0x%X\n", mfID);
    printf("Serial Number = %X-%X-%X\n", serialNum1, serialNum2, serialNum3);

    while(true){
        
        //Get current Temperature in C
        temperatureInC = readTemperature();

        //Convert Temperature in C to F
        temperatureInF = (temperatureInC * 1.8) + 32;

        //Get current Humidity
        humidity = readHumidity();

        printf("Temperature in C: %d\n", temperatureInC);
        printf("Temperature in F: %d\n", temperatureInF);
        printf("Humidity %d\n", humidity);

        //Send humidity data to queue and delay for 5 seconds
        xQueueSend(tempHumqueue, &humidity, 0);
        vTaskDelay(5000/portTICK_PERIOD_MS);
        
        //Clear Queue
        xQueueReceive(tempHumqueue, &clearQueue, 0);

        //send temperature data in F and delay for 5 seconds
        xQueueSend(tempHumqueue, &temperatureInF, 0);
        vTaskDelay(5000/portTICK_PERIOD_MS);

        //clear Queue
        xQueueReceive(tempHumqueue, &clearQueue, 0);

    }
}


//Function to read a return the Configuration Register Status
//Value will be printed once at the beginning of the
//readHDC1080Task  task
int readConfigReg(){

    uint8_t cfReg[2];    
    uint8_t cfRegVal = HDC1080CONFIGREG;

    int ret;

      //write blocking for Configuration Register
      ret = i2c_write_blocking(I2C_PORT, HDC1080ADDRESS, &cfRegVal, 1, false);
      vTaskDelay(100/portTICK_PERIOD_MS);

      //read blocking. Read Configuration Register
      ret = i2c_read_blocking(I2C_PORT, HDC1080ADDRESS, cfReg, 2, false);
      int fullcfReg = cfReg[0]<<8|cfReg[1];

      return fullcfReg;

}

//Function to read and return the Manufacturing ID
//Value will be printed once at the beginning of the
//readHDC1080Task  task
int readMFID(){

    	uint8_t manufactID[2];
      uint8_t mfVal = HDC1080DEVICEIDREG;
      int ret;

      //write blocking for MF ID
      ret = i2c_write_blocking(I2C_PORT, HDC1080ADDRESS, &mfVal, 1, false);
      vTaskDelay(100/portTICK_PERIOD_MS);

      //read blocking. Return Full Manufacturing ID
      ret = i2c_read_blocking(I2C_PORT, HDC1080ADDRESS, manufactID, 2, false);
      int fullMfID = manufactID[0]<<8|manufactID[1];

      return fullMfID;

}

//Function to read and return the 1st block of the Serial Number
//Value will be printed once at the beginning of the
//readHDC1080Task  task
int readSN1(){

      uint8_t sn1[2];
      uint8_t sn1RegVal = HDC1080SN1;
      int ret;

      //write blocking for sn1 register
      ret = i2c_write_blocking(I2C_PORT, HDC1080ADDRESS, &sn1RegVal, 1, false);
      vTaskDelay(100/portTICK_PERIOD_MS);

      //read blocking for sn1
      ret = i2c_read_blocking(I2C_PORT, HDC1080ADDRESS, sn1, 2, false);
      int fullSN1 = sn1[0]<<8|sn1[1];
      
      return fullSN1;
}

//Function to read and return the 2nd block of the Serial Number
//Value will be printed once at the beginning of the
//readHDC1080Task  task
int readSN2(){

      uint8_t sn2[2];
      uint8_t sn2RegVal = HDC1080SN2;
      int ret;

      //write blocking for sn1 register
      ret = i2c_write_blocking(I2C_PORT, HDC1080ADDRESS, &sn2RegVal, 1, false);
      vTaskDelay(100/portTICK_PERIOD_MS);

      //read blocking for sn1
      ret = i2c_read_blocking(I2C_PORT, HDC1080ADDRESS, sn2, 2, false);
      int fullSN2 = sn2[0]<<8|sn2[1];
      
      return fullSN2;
}

//Function to read and return the 3rd block of the Serial Number
//Value will be printed once at the beginning of the
//readHDC1080Task  task
int readSN3(){

      uint8_t sn3[2];
      uint8_t sn3RegVal = HDC1080SN3;
      int ret;

      //write blocking for sn1 register
      ret = i2c_write_blocking(I2C_PORT, HDC1080ADDRESS, &sn3RegVal, 1, false);
      vTaskDelay(100/portTICK_PERIOD_MS);

      //read blocking for sn1
      ret = i2c_read_blocking(I2C_PORT, HDC1080ADDRESS, sn3, 2, false);
      int fullSN3 = sn3[0]<<8|sn3[1];
      
      return fullSN3;
}

//This function reads the current temperature from the HDC1080.
//This function is called once every 10 seconds
int readTemperature(){

  uint8_t temperatue[2];
  uint8_t tempRegVal = HDC1080TEMPREG;
  int ret;
  int fullTemperatureC;
  int fullTemperatureF;

    //write block for temperature
      ret = i2c_write_blocking(I2C_PORT, HDC1080ADDRESS, &tempRegVal, 1, false);
      vTaskDelay(100/portTICK_PERIOD_MS);

      //read block for temperature
      ret = i2c_read_blocking(I2C_PORT, HDC1080ADDRESS, temperatue, 2, false);
      int fullTemperature = temperatue[0]<<8|temperatue[1];

      float actualTempC = (fullTemperature / 65536.0) * 165 - 40;

      fullTemperatureC = round(actualTempC);

      return fullTemperatureC;

}

//This function reads the current humidity from the HDC1080
//This function is called once every 10 seconds
int readHumidity(){

      uint8_t humidty[2];
      uint8_t humRegVal = HDC1080HUMREG;
 
      int ret;
      int rndHumidity;

      //write block for humidity
      ret = i2c_write_blocking(I2C_PORT, HDC1080ADDRESS, &humRegVal, 1, false);
      vTaskDelay(100/portTICK_PERIOD_MS);

      //read block for humidity
      ret = i2c_read_blocking(I2C_PORT, HDC1080ADDRESS, humidty, 2, false);
      int fullHumidity = humidty[0]<<8|humidty[1];

      float actualHumidity = (fullHumidity / 65536.0)*100.0;

      rndHumidity = round(actualHumidity);      

      return rndHumidity;

}

//This function controls numbers displayed on the left number
//of the 7 segment LED. segLEDLeft and segLED right share
//the ledSem semaphore to alternate blinking sides
//quickly so there is no flashing.
void segLEDLeft()
{
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    // initialize digital pin LED_BUILTIN as an output.
    gpio_init(SevenSegA);   //top bar
    gpio_init(SevenSegB);   //top right
    gpio_init(SevenSegC);   //bottom right?
    gpio_init(SevenSegD);   //bottom bar
    gpio_init(SevenSegE);   //bottom left
    gpio_init(SevenSegF);   //Top Left
    gpio_init(SevenSegG);   //Middle
    gpio_init(SevenSegDP);  //decimal points

    gpio_init(SevenSegCC1);
    gpio_init(SevenSegCC2);

    gpio_set_dir(SevenSegA, GPIO_OUT);  //top bar
    gpio_set_dir(SevenSegB, GPIO_OUT);  //top right
    gpio_set_dir(SevenSegC, GPIO_OUT);  //bottom right?
    gpio_set_dir(SevenSegD, GPIO_OUT);  //bottom bar
    gpio_set_dir(SevenSegE, GPIO_OUT);  //bottom left
    gpio_set_dir(SevenSegF, GPIO_OUT);  //Top Left
    gpio_set_dir(SevenSegG, GPIO_OUT);  //Middle
    gpio_set_dir(SevenSegDP, GPIO_OUT); //Decimal points

    gpio_set_dir(SevenSegCC1, GPIO_OUT);
    gpio_set_dir(SevenSegCC2, GPIO_OUT);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    int leftNum;

    while(true){
        
        //initialize counter variable
        int i;

        //peek at current value in the queue and get the left
        //side by dividing by 10
        if(xQueuePeek(tempHumqueue, &leftNum, 0)){
            leftNum = leftNum / 10;
        }

        //switch case for which segments to light up on the left
        //side of the 7 segment led to display leftNum.
        //In each case, this
        //function will take and release the shared semaphore.
        switch(leftNum){

        //Case 0
        case 0 :
            for (i = 0; i < 15; i++){
                xSemaphoreTake(ledSem, 1);
                
                gpio_put(SevenSegCC1, 0);   //right multiplex
                gpio_put(SevenSegCC2, 1);   //left multiplex
                gpio_put(SevenSegA, 1);    //top bar   
                gpio_put(SevenSegB, 1);    //top right  
                gpio_put(SevenSegC, 1);    //bottom right
                gpio_put(SevenSegD, 1);    //bottom bar
                gpio_put(SevenSegE, 1);    //bottom left
                gpio_put(SevenSegF, 1);    //Top Left
                gpio_put(SevenSegG, 0);    //Middle              
                
                xSemaphoreGive(ledSem);
                
                vTaskDelay(1/portTICK_PERIOD_MS);
            }
            break;

        //case 1
        case 1 :
        for(i=0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);
            
            gpio_put(SevenSegCC1, 0);   //right multiplex
            gpio_put(SevenSegCC2, 1);   //left multiplex
            gpio_put(SevenSegA, 0);    //top bar   
            gpio_put(SevenSegB, 1);    //top right  
            gpio_put(SevenSegC, 1);    //bottom right
            gpio_put(SevenSegD, 0);    //bottom bar
            gpio_put(SevenSegE, 0);    //bottom left
            gpio_put(SevenSegF, 0);    //Top Left
            gpio_put(SevenSegG, 0);    //Middle             
            
            xSemaphoreGive(ledSem);
            
            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;

        //case 2
        case 2 :
        for(i=0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);
            
            gpio_put(SevenSegCC1, 0);   //right multiplex
            gpio_put(SevenSegCC2, 1);   //left multiplex
            gpio_put(SevenSegA, 1);    //top bar   
            gpio_put(SevenSegB, 1);    //top right  
            gpio_put(SevenSegC, 0);    //bottom right
            gpio_put(SevenSegD, 1);    //bottom bar
            gpio_put(SevenSegE, 1);    //bottom left
            gpio_put(SevenSegF, 0);    //Top Left
            gpio_put(SevenSegG, 1);    //Middle                            
            
            xSemaphoreGive(ledSem);
            
            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;

        //case 3
        case 3 :
        for(i=0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);
            
            gpio_put(SevenSegCC1, 0);   //right multiplex
            gpio_put(SevenSegCC2, 1);   //left multiplex
            gpio_put(SevenSegA, 1);    //top bar   
            gpio_put(SevenSegB, 1);    //top right  
            gpio_put(SevenSegC, 1);    //bottom right
            gpio_put(SevenSegD, 1);    //bottom bar
            gpio_put(SevenSegE, 0);    //bottom left
            gpio_put(SevenSegF, 0);    //Top Left
            gpio_put(SevenSegG, 1);    //Middle               
            
            xSemaphoreGive(ledSem);
            
            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;

        //case 4
        case 4 :
        for(i=0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);
            
            gpio_put(SevenSegCC1, 0);   //right multiplex
            gpio_put(SevenSegCC2, 1);   //left multiplex
            gpio_put(SevenSegA, 0);    //top bar   
            gpio_put(SevenSegB, 1);    //top right  
            gpio_put(SevenSegC, 1);    //bottom right
            gpio_put(SevenSegD, 0);    //bottom bar
            gpio_put(SevenSegE, 0);    //bottom left
            gpio_put(SevenSegF, 1);    //Top Left
            gpio_put(SevenSegG, 1);    //Middle              
            
            xSemaphoreGive(ledSem);
            
            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;

        //case 5
        case 5 :
        for(i=0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);
            
            gpio_put(SevenSegCC1, 0);   //right multiplex
            gpio_put(SevenSegCC2, 1);   //left multiplex
            gpio_put(SevenSegA, 1);    //top bar   
            gpio_put(SevenSegB, 0);    //top right  
            gpio_put(SevenSegC, 1);    //bottom right
            gpio_put(SevenSegD, 1);    //bottom bar
            gpio_put(SevenSegE, 0);    //bottom left
            gpio_put(SevenSegF, 1);    //Top Left
            gpio_put(SevenSegG, 1);    //Middle                 
            
            xSemaphoreGive(ledSem);
            
            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;

        //case 6
        case 6 :
        for(i=0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);

            gpio_put(SevenSegCC1, 0);   //right multiplex
            gpio_put(SevenSegCC2, 1);   //left multiplex
            gpio_put(SevenSegA, 1);    //top bar   
            gpio_put(SevenSegB, 0);    //top right  
            gpio_put(SevenSegC, 1);    //bottom right
            gpio_put(SevenSegD, 1);    //bottom bar
            gpio_put(SevenSegE, 1);    //bottom left
            gpio_put(SevenSegF, 1);    //Top Left
            gpio_put(SevenSegG, 1);    //Middle                
            
            xSemaphoreGive(ledSem);
            
            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;

        //case 7
        case 7 :
        for(i=0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);

            gpio_put(SevenSegCC1, 0);   //right multiplex
            gpio_put(SevenSegCC2, 1);   //left multiplex
            gpio_put(SevenSegA, 1);    //top bar   
            gpio_put(SevenSegB, 1);    //top right  
            gpio_put(SevenSegC, 1);    //bottom right
            gpio_put(SevenSegD, 0);    //bottom bar
            gpio_put(SevenSegE, 0);    //bottom left
            gpio_put(SevenSegF, 0);    //Top Left
            gpio_put(SevenSegG, 0);    //Middle               
            
            xSemaphoreGive(ledSem);

            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;

        //case 8
        case 8 :
        for(i=0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);

            gpio_put(SevenSegCC1, 0);   //right multiplex
            gpio_put(SevenSegCC2, 1);   //left multiplex
            gpio_put(SevenSegA, 1);    //top bar   
            gpio_put(SevenSegB, 1);    //top right  
            gpio_put(SevenSegC, 1);    //bottom right
            gpio_put(SevenSegD, 1);    //bottom bar
            gpio_put(SevenSegE, 1);    //bottom left
            gpio_put(SevenSegF, 1);    //Top Left
            gpio_put(SevenSegG, 1);    //Middle                
            
            xSemaphoreGive(ledSem);

            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;

        //case 9
        case 9 :
        for(i=0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);

            gpio_put(SevenSegCC1, 0);   //right multiplex
            gpio_put(SevenSegCC2, 1);   //left multiplex
            gpio_put(SevenSegA, 1);    //top bar   
            gpio_put(SevenSegB, 1);    //top right  
            gpio_put(SevenSegC, 1);    //bottom right
            gpio_put(SevenSegD, 1);    //bottom bar
            gpio_put(SevenSegE, 0);    //bottom left
            gpio_put(SevenSegF, 1);    //Top Left
            gpio_put(SevenSegG, 1);    //Middle                
            
            xSemaphoreGive(ledSem);

            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;
        }
    }
}

//This function controls numbers displayed on the right number
//of the 7 segment LED. segLEDLeft and segLED right share
//the ledSem semaphore to alternate blinking sides
//quickly so there is no flashing.
void segLEDRight()
{
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    // initialize digital pin LED_BUILTIN as an output.
    gpio_init(SevenSegA);   //top bar
    gpio_init(SevenSegB);   //top right
    gpio_init(SevenSegC);   //bottom right?
    gpio_init(SevenSegD);   //bottom bar
    gpio_init(SevenSegE);   //bottom left
    gpio_init(SevenSegF);   //Top Left
    gpio_init(SevenSegG);   //Middle
    gpio_init(SevenSegDP);  //decimal points

    gpio_init(SevenSegCC1);
    gpio_init(SevenSegCC2);

    gpio_set_dir(SevenSegA, GPIO_OUT);  //top bar
    gpio_set_dir(SevenSegB, GPIO_OUT);  //top right
    gpio_set_dir(SevenSegC, GPIO_OUT);  //bottom right?
    gpio_set_dir(SevenSegD, GPIO_OUT);  //bottom bar
    gpio_set_dir(SevenSegE, GPIO_OUT);  //bottom left
    gpio_set_dir(SevenSegF, GPIO_OUT);  //Top Left
    gpio_set_dir(SevenSegG, GPIO_OUT);  //Middle
    gpio_set_dir(SevenSegDP, GPIO_OUT); //Decimal points

    gpio_set_dir(SevenSegCC1, GPIO_OUT);
    gpio_set_dir(SevenSegCC2, GPIO_OUT);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    int rightNum;

    while(true){

        //initialize variables for counter
        int i;
        
        //peek at number in the queue and get the right digit by
        //perfoming a mod on the number in the queue
        if(xQueuePeek(tempHumqueue, &rightNum, 0)){
            rightNum = rightNum % 10;
        }
        
        //Switch case to determine which segments of the right digit
        //on the 7 segment led to light up based on rightNum. 
        //In each case, this
        //function will take and release the shared semaphore.
        switch(rightNum){
        //Case 0
        case 0 :
        for (i = 0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);
            
            gpio_put(SevenSegCC1, 1);   //right multiplex
            gpio_put(SevenSegCC2, 0);   //left multiplex
            gpio_put(SevenSegA, 1);    //top bar   
            gpio_put(SevenSegB, 1);    //top right  
            gpio_put(SevenSegC, 1);    //bottom right
            gpio_put(SevenSegD, 1);    //bottom bar
            gpio_put(SevenSegE, 1);    //bottom left
            gpio_put(SevenSegF, 1);    //Top Left
            gpio_put(SevenSegG, 0);    //Middle              
            
            xSemaphoreGive(ledSem);
            
            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;

        //case 1
        case 1 :
        for(i=0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);
            
            gpio_put(SevenSegCC1, 1);   //right multiplex
            gpio_put(SevenSegCC2, 0);   //left multiplex
            gpio_put(SevenSegA, 0);    //top bar   
            gpio_put(SevenSegB, 1);    //top right  
            gpio_put(SevenSegC, 1);    //bottom right
            gpio_put(SevenSegD, 0);    //bottom bar
            gpio_put(SevenSegE, 0);    //bottom left
            gpio_put(SevenSegF, 0);    //Top Left
            gpio_put(SevenSegG, 0);    //Middle              
            
            xSemaphoreGive(ledSem);
            
            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;

        //case 2
        case 2 :
        for(i=0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);
            
            gpio_put(SevenSegCC1, 1);   //right multiplex
            gpio_put(SevenSegCC2, 0);   //left multiplex
            gpio_put(SevenSegA, 1);    //top bar   
            gpio_put(SevenSegB, 1);    //top right  
            gpio_put(SevenSegC, 0);    //bottom right
            gpio_put(SevenSegD, 1);    //bottom bar
            gpio_put(SevenSegE, 1);    //bottom left
            gpio_put(SevenSegF, 0);    //Top Left
            gpio_put(SevenSegG, 1);    //Middle             
            
            xSemaphoreGive(ledSem);
           
            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;

        //case 3
        case 3 :
        for(i=0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);
            
            gpio_put(SevenSegCC1, 1);   //right multiplex
            gpio_put(SevenSegCC2, 0);   //left multiplex
            gpio_put(SevenSegA, 1);    //top bar   
            gpio_put(SevenSegB, 1);    //top right  
            gpio_put(SevenSegC, 1);    //bottom right
            gpio_put(SevenSegD, 1);    //bottom bar
            gpio_put(SevenSegE, 0);    //bottom left
            gpio_put(SevenSegF, 0);    //Top Left
            gpio_put(SevenSegG, 1);    //Middle  
                               
            xSemaphoreGive(ledSem);
            
            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;

        //case 4
        case 4 :
        for(i=0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);
            
            gpio_put(SevenSegCC1, 1);   //right multiplex
            gpio_put(SevenSegCC2, 0);   //left multiplex
            gpio_put(SevenSegA, 0);    //top bar   
            gpio_put(SevenSegB, 1);    //top right  
            gpio_put(SevenSegC, 1);    //bottom right
            gpio_put(SevenSegD, 0);    //bottom bar
            gpio_put(SevenSegE, 0);    //bottom left
            gpio_put(SevenSegF, 1);    //Top Left
            gpio_put(SevenSegG, 1);    //Middle  
                          
            
            xSemaphoreGive(ledSem);
            
            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;

        //case 5
        case 5 :
        for(i=0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);
            
            gpio_put(SevenSegCC1, 1);   //right multiplex
            gpio_put(SevenSegCC2, 0);   //left multiplex
            gpio_put(SevenSegA, 1);    //top bar   
            gpio_put(SevenSegB, 0);    //top right  
            gpio_put(SevenSegC, 1);    //bottom right
            gpio_put(SevenSegD, 1);    //bottom bar
            gpio_put(SevenSegE, 0);    //bottom left
            gpio_put(SevenSegF, 1);    //Top Left
            gpio_put(SevenSegG, 1);    //Middle                 
            
            xSemaphoreGive(ledSem);
            
            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;

        //case 6
        case 6 :
        for(i=0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);

            gpio_put(SevenSegCC1, 1);   //right multiplex
            gpio_put(SevenSegCC2, 0);   //left multiplex
            gpio_put(SevenSegA, 1);    //top bar   
            gpio_put(SevenSegB, 0);    //top right  
            gpio_put(SevenSegC, 1);    //bottom right
            gpio_put(SevenSegD, 1);    //bottom bar
            gpio_put(SevenSegE, 1);    //bottom left
            gpio_put(SevenSegF, 1);    //Top Left
            gpio_put(SevenSegG, 1);    //Middle                
            
            xSemaphoreGive(ledSem);
            
            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;

        //case 7
        case 7 :
        for(i=0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);

            gpio_put(SevenSegCC1, 1);   //right multiplex
            gpio_put(SevenSegCC2, 0);   //left multiplex
            gpio_put(SevenSegA, 1);    //top bar   
            gpio_put(SevenSegB, 1);    //top right  
            gpio_put(SevenSegC, 1);    //bottom right
            gpio_put(SevenSegD, 0);    //bottom bar
            gpio_put(SevenSegE, 0);    //bottom left
            gpio_put(SevenSegF, 0);    //Top Left
            gpio_put(SevenSegG, 0);    //Middle               
            
            xSemaphoreGive(ledSem);

            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;

        //case 8
        case 8 :
        for(i=0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);

            gpio_put(SevenSegCC1, 1);   //right multiplex
            gpio_put(SevenSegCC2, 0);   //left multiplex
            gpio_put(SevenSegA, 1);    //top bar   
            gpio_put(SevenSegB, 1);    //top right  
            gpio_put(SevenSegC, 1);    //bottom right
            gpio_put(SevenSegD, 1);    //bottom bar
            gpio_put(SevenSegE, 1);    //bottom left
            gpio_put(SevenSegF, 1);    //Top Left
            gpio_put(SevenSegG, 1);    //Middle                
            
            xSemaphoreGive(ledSem);

            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;

        //case 9
        case 9 :
        for(i=0; i < 15; i++){
            xSemaphoreTake(ledSem, 1);

            gpio_put(SevenSegCC1, 1);   //right multiplex
            gpio_put(SevenSegCC2, 0);   //left multiplex
            gpio_put(SevenSegA, 1);    //top bar   
            gpio_put(SevenSegB, 1);    //top right  
            gpio_put(SevenSegC, 1);    //bottom right
            gpio_put(SevenSegD, 1);    //bottom bar
            gpio_put(SevenSegE, 0);    //bottom left
            gpio_put(SevenSegF, 1);    //Top Left
            gpio_put(SevenSegG, 1);    //Middle                
            
            xSemaphoreGive(ledSem);

            vTaskDelay(1/portTICK_PERIOD_MS);
        }
        break;
        }
    }
}



