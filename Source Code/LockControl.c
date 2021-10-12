/**********************************************************************
Author      : Brandon Polite
Course      : CSC210
Assignment  : Final Project
Due         : April 21st, 2021

Purpose     : Use a keypad to unlock a door with a specified code.
beep each time a input is recorded, use led to show status of system
Allow user to enter a 10 digit override code to make a new code for the system
**********************************************************************/
//Imports
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <wiringPi.h>
#include <softPwm.h>
#include <string.h>

//Prototype functions
void moveOnePeriod(int dir,int ms);
void moveSteps(int dir, int ms, int steps);
void motorStop(void);
void unlock(void);
void lock(void);
void setLedColor(int r, int g, int b);
void soundBuzzer(int ms);
void beepBuzzer(int beeps, int gapMs);
void initKeypad(void);
int findLowRow(void);
char getKey(void);
void getCode(char* code, int length);
void append(char* str, char c);
void blinkRed(int times, int gapMs);
void rFile(char* code);
void wFile(char* code);

/*************************************
 MOTOR HARDWARE CONFIGURATION
 ***********************************/
const int motorPins[]={1,4,5,6};            //define pins connected to four phase ABCD of stepper motor 
const int CCWStep[]={0x01,0x02,0x04,0x08};  //define power supply order for coil for rotating anticlockwise 
const int CWStep[]={0x08,0x04,0x02,0x01};   //define power supply order for coil for rotating clockwise

/*************************************
 LED HARDWARE CONFIGURATION
 ************************************/
//Set LED pin outs
#define ledPinRed    27
#define ledPinGreen  28
#define ledPinBlue   29
#define buzzerPin    25

void setupLedPin(void)
{
	softPwmCreate(ledPinRed,  0, 100);	//Create SoftPWM pin for red
	softPwmCreate(ledPinGreen,0, 100);  //Create SoftPWM pin for green
	softPwmCreate(ledPinBlue, 0, 100);  //Create SoftPWM pin for blue
}

/************************************
 KEYPAD HARDWARE CONFIGURATION
 ***********************************/
//Number of rows and columns on keypad
#define ROWS 4
#define COLS 4

//Pressed key start as null, define pins for each row/column
char pressedKey = '\0';
int rowPins[ROWS] = {7,0,2,3};
int colPins[COLS] = {12,21,22,23};

//define matrix values for keypad
char keys[ROWS][COLS] = {
   {'1', '2', '3', 'A'},
   {'4', '5', '6', 'B'},
   {'7', '8', '9', 'C'},
   {'*', '0', '#', 'D'}
};

//BEGIN MAIN
int main(void)
{
    //Setup Wiring pi
    wiringPiSetup();
    for(int i=0;i<4;i++){
        pinMode(motorPins[i],OUTPUT);
    } 

    //setup led (Since lock is assumed unlocked, set color to green)
    setupLedPin();
    setLedColor(0,100,100);
    
    //set buzzer pin
    pinMode(buzzerPin, OUTPUT);

    //setup keypad
    initKeypad();

    char pin[5];
    rFile(pin);
    char pinOff[] = "0000";
    char initiateReset[] = "AACD";
    const char RESET[] = "1235813471";
    char userReset[11];
    char userInput[5];

    //Display Program Header
    printf("--------------LOCK CONTROL SYSTEM START--------------");

    //While loop to gather user input and run program until user quits
    while(1)
    {   
        //get user input
        printf("\nEnter pin to unlock: \n");  
        getCode(userInput, 4);

        //If code is correct,
        if(strcmp(userInput, pin) == 0)
        {
            soundBuzzer(200);       //sound buzzer for 200ms
            setLedColor(0,0,100);   //set led to yellow for wait
            unlock();               //run unlock sequence
        }
        else if (strcmp(userInput, initiateReset) == 0)
        {
            //if user types reset pin
            beepBuzzer(2, 100);     //beep buzzer 2 times
            setLedColor(100,100,0); //set led to blue to indicate reset pin

            //Get user to enter 10 digit override code
            printf("\nEnter Override code: \n");
            getCode(userReset,10);

            //If 10 digit override is correct,
            if(strcmp(userReset, RESET) == 0)
            {
                beepBuzzer(3, 100);     //Beep 3 times to indicate code reset
                setLedColor(50,50,50);  //set led color to white

                //get new user code, CODE CANNOT BE SAME AS RESET PIN
                int resetComplete = 0;
                //while loop to ensure code is not same as initiate reset or pin off
                while(resetComplete == 0)
                {
                    //get users new pin
                    printf("Enter new code\n");
                    getCode(pin, 4);

                    //if pin matches initiate reset or pinoff, beep and ask for different pin
                    if(strcmp(pin, initiateReset) == 0 || strcmp(pin, pinOff) == 0)
                    {
                        beepBuzzer(1,100);
                        setLedColor(50,50,50);  //set led color to white again
                    }
                    else
                    {
                        //if pin does not match protected codes,
                        beepBuzzer(2,100);      //beep 2 times
                        setLedColor(0,100,100); //keep led red
                        wFile(pin);             //write new pin to file
                        resetComplete = 1;      //end while loop
                    }
                }
            }
            else
            {
                //if user does not enter correct override code
                soundBuzzer(100);       //beep
                setLedColor(0,100,100); //set led color back to red
            }
        }
        else
        {
            //turn off system is pinOff is entered
            if(strcmp(userInput, pinOff) == 0)
            {
                setLedColor(100,100,100);
                delay(500);
                exit(0);
            }
            //If user does not enter pinOff, sound buzzer to show wrong pin was entered
            beepBuzzer(3,50);
        }
    }
    return 0;
}

/**************************************************************
 
 MOTOR DRIVING FUNCTIONS
 Information gathered from freenove documentation on Step Motor

 *************************************************************/

//Four phase stepping motor, four steps is a cycle.
//Function to move one cycle (Four Steps)
void moveOnePeriod(int dir,int ms)
{
    int i=0,j=0;
    //cycle according to power supply order
    for (j=0;j<4;j++)
    {   
        //assign to each pin, a total of 4 pins
        for (i=0;i<4;i++)
        {  
            //if dir == 1, rotate clockwise
            if(dir == 1)
            {
                digitalWrite(motorPins[i],(CCWStep[j] == (1<<i)) ? HIGH : LOW);
            }
            //Otherwise, rotate anticlockwise
            else
            {
                digitalWrite(motorPins[i],(CWStep[j] == (1<<i)) ? HIGH : LOW);
            }    
        }

        //the delay can not be less than 3ms, otherwise it will exceed speed limit of the motor
        if(ms<3)
        {
            ms = 3;
        }       
        delay(ms);
    }
}

//Function to rotate motor for more than one period
//Iteratively calls moveOnePeriod
void moveSteps(int dir, int ms, int steps)
{
    int i;
    for(i=0;i<steps;i++)
    {
        moveOnePeriod(dir,ms);
    }
}

//Function to stop motor rotation
void motorStop()
{   
    int i;
    for(i=0;i<4;i++)
    {
        digitalWrite(motorPins[i],LOW);
    }   
}

//UNLOCK FUNCTION, rotate 180 degrees anti clockwise
void unlock(void)
{
    moveSteps(1,3,256);     //rotating 180°  AntiClockwise
    delay(500);             //wait for rotation
    setLedColor(100,0,100); //Change led color to match lock status (GREEN)
    beepBuzzer(5, 800);
    lock();
}

//LOCK FUNCTION, rotate 180 degrees clockwise
void lock(void)
{
    moveSteps(0,3,256);     //rotating 180° Clockwise
    delay(500);             //wait for rotation
    setLedColor(0,100,100); //Change led color to match lock status (RED)
}

/**********************************
 FUNCTION FOR LED COLOR ASSIGNMENT
 *********************************/
void setLedColor(int r, int g, int b)
{
    softPwmWrite(ledPinRed, r);
    softPwmWrite(ledPinGreen, g);
    softPwmWrite(ledPinBlue, b);
}

//function to blink in red
void blinkRed(int times, int gapMs)
{
    setLedColor(100,100,100); //led off
    delay(gapMs);
    setLedColor(0,100,100); //led red
}

/***********************************
 BUZZER FUNCTIONs 
 **********************************/
//Sound buzzer for specified # of milliseconds
void soundBuzzer(int ms)
{
    digitalWrite(buzzerPin, HIGH);
    delay(ms);
    digitalWrite(buzzerPin, LOW);
}

//Make buzzer beep over a specified interval
void beepBuzzer(int beeps, int gapMs)
{
    //for loop to beep i # of times
    for(int i = 0; i < beeps; i++)
    {
        soundBuzzer(200); //sound for 200ms
        delay(gapMs);   //delay specified gap
    }
}

/*********************************
 KEYPAD DRIVING FUNCTIONS
 ********************************/
//pin setup function
void initKeypad()
{
    //set column pins
    for (int c = 0; c < COLS; c++)
    {
        pinMode(colPins[c], OUTPUT);   
        digitalWrite(colPins[c], HIGH);
    }
    //set row pins
    for (int r = 0; r < ROWS; r++)
    {
        pinMode(rowPins[0], INPUT);   
        pullUpDnControl(rowPins[r], PUD_UP);
    }
}

//Function to identify when key is pressed
int findLowRow()
{
    for (int r = 0; r < ROWS; r++)
    {
        if (digitalRead(rowPins[r]) == LOW)
            return r;
    }

    return -1;
}

//Function to return a specific key that is pressed
char getKey()
{
    int rowIndex;
    //Traverse through each column
    for (int c = 0; c < COLS; c++)
    {
        //set to low
        digitalWrite(colPins[c], LOW);

        //find low row
        rowIndex = findLowRow();

        //If low row is found, key may have been pressed
        if (rowIndex > -1)
        {
            //check if key was pressed
            if (!pressedKey)
            {
                //Set key press to corresponding matrix entry and return
                pressedKey = keys[rowIndex][c];
                return pressedKey;
            }
        }
        //Reset column to high
        digitalWrite(colPins[c], HIGH);
    }
    //return null if nothing was pressed
    pressedKey = '\0';
    return pressedKey;
}

//Function to get a string of specified length from keypad
void getCode(char* code, int length)
{
    int codecount = 0;  //counter
    code[length] = '\0';    //set last char in string to '\0'

    //While loop to get keypad entries until codecount is reached
    while(1)
    {
        //Get keypad input
        char input = getKey();

        //If there was an input, write to string and notify
        if (input)
        {
            printf("%c\n", input); //show key pressed
            blinkRed(1,100);        //blink led to indicate pressed key
            beepBuzzer(1,100);      //beep buzzer to indicate pressed key
            code[codecount] = input;    //write input to string
            codecount++;                //increment codecount
        }

        //If codecount has been reached,return to main, stop input
        if (codecount == length)
        {
            return;
        }

        //delay to force gaps between keypresses
        delay(250);
    }
}

/*********************************
 FUNCTIONS TO READ AND WRITE TO FILE
 ********************************/
//Read
void rFile(char* code)
{
    //create file
    FILE *fptr;

    //Try to open file, if file isnt opened, display error and load default pin
    if((fptr = fopen("Key.txt", "r")) == NULL)
    {
        printf("\n\nError opening file\n\n");   //display error
        sprintf(code, "%d", 1234);              //load default code "1234"
        printf("\n\nDEFAULT KEY LOADED\n\n");   //display key load message
        setLedColor(100,0,0);                   //set led color to blue to show default code load.
        return;
    }

    //If file does open, read line from file with new line delimitter.
    fscanf(fptr, "%[^\n]", code);
    fclose(fptr);   //close file if opened
}

//Write
void wFile(char* code)
{
    //create file
    FILE *fptr;
    //Display error if file does not open
    if((fptr = fopen("Key.txt", "w")) == NULL)
    {
        printf("\n\nERROR WRITING\n\n");
    }
    
    fprintf(fptr, "%s\n", code);    //write code to file with new line delimmiter
    fclose(fptr);   //close file if opened
}