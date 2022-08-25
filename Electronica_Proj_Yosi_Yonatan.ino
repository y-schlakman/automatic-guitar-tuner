#include "LiquidCrystal_I2C.h" // lcd i2c library
#include "FastLED.h" // led strip library
#include <FFTLIB.h> // FFT algorithm library

#define MIC_PIN A0
#define DATA_SIZE 512
#define SAMPLING_RATE 1000.0 // sampling rate is 1000HZ
#define MICROS_BETWEEN_SAMPLES 1000  // micro seconds between samples = 1,000,000 / sampling rate
#define NUM_LEDS 15
#define LED_PIN 7
#define HZ_ALLOWED_RANGE 10 // the hz "cushen" that will be displayed on led strip (+-10 hz from the target freq, past that it'll be the most right or left LED with red color
#define MOTOR_IN_3 4 // motor h bridge control 1
#define MOTOR_IN_4 5 // motor h bridge control 2
#define ENB_PIN 6 // motor speed pin
#define MOTOR_SPEED 200 // the speed of the motor from 0 to 255 (0v to 5v)
#define HIGH_E 329.63 // high e note's hz value
#define LOW_E 82.407 // low e note's hz value
#define ACCURACY 1.3// the allowed error for it to be considered tuned
#define HZ_THRESH 30 // the limit where we say that the sound is too out of tune for us - and most probably is noise

//Set the pins on the I2C chip used for LCD connections
//ADDR,EN,R/W,RS,D4,D5,D6,D7
LiquidCrystal_I2C lcd(0x3F,2,1,0,4,5,6,7); // 0x27 is the default I2C bus address of the backpack-see article

//led array
CRGB leds[NUM_LEDS];

unsigned long last_micro_seconds = 0L;
int data[DATA_SIZE]; //fft wave data array
FFTLIB fft; //fft library object
float closest; //the closest frequency to the one we are trying to get to
float goodNote = HIGH_E;//the string we are tuning
int successCount = 0;//ammount of fft runs that resulted in peak of tuned string

void setup()
{
  Serial.begin(9600);//start serial comunication
  
  startUpLcd();//write startup message to lcd screen
  startUpLeds();//light up green LEDs
  endStartUps();//end startup sequence
  
  analogWrite(ENB_PIN, MOTOR_SPEED);//set motor speed
  
  for(int i = 3; i > 0; --i)//display countdown to tuning
  {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("tuning in:");
    lcd.setCursor(0,1);
    lcd.print(i);
    delay(1000);
  }
}

void loop()
{
  doFFT();//run the fft algorithm

  //find closest peak frequency to the wanted notes frequency
  closest = -1000;
  for(int i = 0; i < 5; ++i)
  {
    if(abs(closest-goodNote) > abs(fft.f_peaks[i]-goodNote))
    {
      closest = fft.f_peaks[i];
    }
  }

  //show the closest freq on lcd
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("closest is:");
  lcd.setCursor(0,1);
  lcd.print(closest);

  //light LEDs based off of the distance from the good notes frequency
  showDistFromFreq(closest, goodNote);
  
  if(abs(closest - goodNote) < HZ_THRESH)//dont tune not kashur sounds :)
  {
    if(abs(closest - goodNote) > ACCURACY)//if not tuned enough
    {
      successCount = 0;//reset the success count

      //we need to tighten - counterClockwise
      if(closest < goodNote)
      {
        digitalWrite(MOTOR_IN_3, HIGH);//counter-clockwise
        digitalWrite(MOTOR_IN_4, LOW);
      }
      
      //we need to loosen - clockwise
      else
      {
        digitalWrite(MOTOR_IN_3, LOW);//counter-clockwise
        digitalWrite(MOTOR_IN_4, HIGH);
      }

      //make the motor apply limited spin so as not to overshoot the note - or chas vechalila rip a string
      delay(300);
      
      //turn off motors
      digitalWrite(MOTOR_IN_3, LOW);//counter-clockwise
      digitalWrite(MOTOR_IN_4, LOW);
    }

    //if tuned enough
    else
    {
      successCount++;

      //check if we are tuned for enough time to be sure it is true and not just fluctuating
      if(successCount == 2)
      {
        //print tuned
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("TUNED!!!");
        lcd.setCursor(0,1);
        lcd.print("reset for next");

        //end program by infinite loop
        while(true){}
      }
    }
  }
}

void startUpLcd()
{
  //Set up LCD module
  lcd.begin (16,2); // 16 x 2 LCD module
  lcd.setBacklightPin(3,POSITIVE); // BL, BL_POL
  lcd.setBacklight(HIGH);
  lcd.setCursor(0,0);

  //display start-up text
  lcd.print("Yosi & Yonatan's");
  lcd.setCursor(2,1);
  lcd.print("Epic Project");
}

void startUpLeds()
{
  //neopixel - type of led
  //led_pin - the pwm of the leds
  //leds array of colors that each element represents a led in the strip
  //num_leds - number of leds in the strip
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);

  //cool green lights
  for(int i=0;i<NUM_LEDS;++i)
  {
    leds[i] = CHSV(80, 255, (255/NUM_LEDS)*(i+1));
    FastLED.show();
    delay(50);
  }
}

void endStartUps()
{
  //keep the lcd and leds in startup for 600 ms
  delay(600);
  
  //turn off all leds
  for(int i=0;i<NUM_LEDS;++i)
  {
    leds[i] = CRGB::Black;
  }
  FastLED.show();

  //clear lcd screan
  lcd.clear();
}

void showDistFromFreq(float currentFreq, float targetFreq)
{
  //turn off all leds
  for(int i=0;i<NUM_LEDS;++i)
  {
    leds[i] = CRGB::Black;
  }

  //find the led in the strip to light based off of the distance of currentfreq from targetfreq
  int ledToLight = map(currentFreq, targetFreq - HZ_ALLOWED_RANGE, targetFreq + HZ_ALLOWED_RANGE, 0, NUM_LEDS - 1);
  
  //make sure that we don't escape the boundries of the led array
  ledToLight = constrain(ledToLight, 0, NUM_LEDS - 1);
  
  int ledColor;

  //if currentfreq is smaller than targetfreq then map from target to target plus allowed range, to 0 to 80
  if(currentFreq < targetFreq)
  {
    ledColor = map(currentFreq, targetFreq - HZ_ALLOWED_RANGE, targetFreq, 0, 80);
    ledColor = constrain(ledColor, 0, 80);
  }

  //if currentfreq is bigger than targetfreq then map from target to target minus allowed range, to 80 to 0
  else
  {
    ledColor = map(currentFreq, targetFreq, targetFreq + HZ_ALLOWED_RANGE, 80, 0);
    ledColor = constrain(ledColor, 0, 80);
  }

  //light the LED in the index that we found in the color that we found
  leds[ledToLight] = CHSV(ledColor, 255, 100);
  FastLED.show();
}

void doFFT()
{
  //re-initialize the micro second variable
  last_micro_seconds = micros();

  //sample the data with spaces of MICROS_BETWEEN_SAMPLES
  for(int i = 0; i < DATA_SIZE; ++i)
  {
    while(micros() - last_micro_seconds < MICROS_BETWEEN_SAMPLES){}

    //add the current iterations time to last_micro_seconds so that next iteration start as the value of micros() 
    last_micro_seconds += MICROS_BETWEEN_SAMPLES;

    //take sample
    data[i] = analogRead(MIC_PIN);
  }
  
  //run the fft on the samples - after that fft.f_peaks array will have top 5 frequency peaks
  fft.FFT(data,DATA_SIZE, SAMPLING_RATE);

  //print peak info to serial
  Serial.println(fft.f_peaks[0]);
  Serial.println(fft.f_peaks[1]);
  Serial.println(fft.f_peaks[2]);
  Serial.println(fft.f_peaks[3]);
  Serial.println(fft.f_peaks[4]);
  Serial.println();
}
