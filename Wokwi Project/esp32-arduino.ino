#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <ESP32Servo.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define BUZZER 5
#define LED_1 14
#define LED_2 4
#define LED_3 2
#define PB_CANCEL 25
#define PB_OK 32
#define PB_UP 35
#define PB_DOWN 27
#define DHTPIN 12
#define LDR_LEFT 34
#define LDR_RIGHT 33
#define SERVO 19

#define NTP_SERVER    "pool.ntp.org" 
#define UTC_OFFSET_DST 0

float UTC_OFFSET = 5.5;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

Servo myservo;

int hours = 0;
int minutes = 0;
int seconds = 0;

bool alarm_enabled = true;
int n_alarms = 3;
int alarm_hours[] = {0, 0, 0};
int alarm_minutes[] = {0, 0, 0};
bool alarm_triggered[] = {false, false, false};

int n_notes = 8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};

int current_mode = 0;
int max_mode = 5;
String modes[] = {"1 - Set Time", "2 - Set Alarm 1", "3 - Set Alarm 2", "4 - Set Alarm 3", "5 - Disable Alarms"};

char lightAr[6];
float intensityLDRLeft;
float intensityLDRRight;
String intensity;

int pos = 0;

int minA;
float controllingFactor;
int theta;
float cf;
float I;
float dir;

void setup() {
  
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  pinMode(LED_3, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);
  pinMode(LDR_LEFT, INPUT);
  pinMode(LDR_RIGHT, INPUT);

  dhtSensor.setup(DHTPIN, DHTesp::DHT22);

  myservo.attach(SERVO, 500, 2400);

  Serial.begin(9600);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); //Don't proceed, loop forever
  }
  display.display();
  delay(2000); //Pause for 2 seconds

  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    display.clearDisplay();
    print_line("Connecting to WIFI", 0, 0, 1);
  }

  display.clearDisplay();
  print_line("Connected to WIFI", 0, 0, 1);
  delay(500);

  setupMqtt();

  configTime(UTC_OFFSET * 3600, UTC_OFFSET_DST, NTP_SERVER);

  //Clear the buffer
  display.clearDisplay();
  print_line("Welcome to Medibox!", 2, 0, 1); //(String, cursor_column, cursor_row, text_size)
  delay(500);
  display.clearDisplay();

  digitalWrite(LED_1,LOW);
  digitalWrite(LED_2,LOW);
  digitalWrite(LED_3,LOW);

}

void loop() {
  if(!mqttClient.connected()){
    connectToBroker();
  }

  mqttClient.loop();

  theta = minA*dir+(180-minA)*I*cf ; 

// Serial.print("minA ");
// Serial.println(minA);
// Serial.print("D ");
// Serial.println(dir);
// Serial.print("I ");
// Serial.println(I);
// Serial.print("cf ");
// Serial.println(cf);

  if (theta > 180){
    theta = 180;
  }
  if (dir!=0 && cf !=0 && minA !=0){
    myservo.write(theta);
    //Serial.println(theta);
    delay(15);
  }
    
  updateIntensity();

  mqttClient.publish("LIGHT-INTENSITY-YR",lightAr);

  update_time_with_check_alarms();
  if (digitalRead(PB_OK) == LOW) {
    delay(50);
    display.clearDisplay();
    print_line("MENU", 40, 20, 2);
    delay(750);
    display.clearDisplay();
    to_the_menu();
  }
  check_temp();
}

void updateIntensity(){
  intensityLDRLeft = analogRead(LDR_LEFT)/4095.0;
  intensityLDRRight =  analogRead(LDR_RIGHT)/4095.0;

  if (intensityLDRLeft > intensityLDRRight){
    intensity = "L" + String(intensityLDRLeft,5);
    intensity.toCharArray(lightAr,6);
    I = intensityLDRLeft;
    dir = 1.5;
  }
  else {
    intensity = "R" + String(intensityLDRRight,5);
    intensity.toCharArray(lightAr,6);
    I = intensityLDRRight;
    dir = 0.5;
  } 

}

void setupMqtt(){
  mqttClient.setServer("test.mosquitto.org",1883);
  mqttClient.setCallback(receiveCallback);
}

void receiveCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char payloadCharAr[length];
  for(int i=0;i<length;i++){
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }

  Serial.println();

  if (strcmp(topic,"MIN-ANGLE-YR")==0){
    minA  = atoi(payloadCharAr);
  } 
  else if (strcmp(topic, "CONTROLLING-FACTOR-YR")==0){
    cf = atof(payloadCharAr);
  }
}

void connectToBroker(){
  while(!mqttClient.connected()){
    Serial.print("Attempting MQTT connection..");
    if (mqttClient.connect("ESP-12345645454")){
      Serial.println("conncted S");
      mqttClient.subscribe("MIN-ANGLE-YR");
      mqttClient.subscribe("CONTROLLING-FACTOR-YR");
    }
    else{
      Serial.print("failed ");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

void print_line(String text, int column, int row, int text_size) {
  display.setTextSize(text_size);		//Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);	//Draw white text
  display.setCursor(column, row);		//Start at (row,column)
  display.println(text);

  display.display();
}

void print_time_now(void) {
  display.clearDisplay();
  print_line(String(hours), 30, 1, 2);
  print_line(":", 50, 0, 2);
  print_line(String(minutes), 60, 1, 2);
  print_line(":", 80, 0, 2);
  print_line(String(seconds), 90, 1, 2);
}


void update_time() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char timeHour[3];
  strftime(timeHour, 3, "%H", &timeinfo);
  hours = atoi(timeHour);

  char timeMinute[3];
  strftime(timeMinute, 3, "%M", &timeinfo);
  minutes = atoi(timeMinute);

  char timeSecond[3];
  strftime(timeSecond, 3, "%S", &timeinfo);
  seconds = atoi(timeSecond);
}

void ring_alarm() {
  display.clearDisplay();
  print_line("Medicine  Time!", 0, 0, 2);
  digitalWrite(LED_1, HIGH);

  bool break_happened = false;
  while (digitalRead(PB_CANCEL) == HIGH && break_happened == false) {

    for (int i = 0; i < n_notes; i++) {
      if (digitalRead(PB_CANCEL) == LOW) {
        delay(200);
        break_happened = true;
        break;
      }
      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }


  digitalWrite(LED_1, LOW);
  display.clearDisplay();

}


int wait_for_button_press() {
  while (true) {
    if (digitalRead(PB_UP) == LOW) {
      delay(200);
      return PB_UP;
    }
    else if (digitalRead(PB_DOWN) == LOW) {
      delay(200);
      return PB_DOWN;
    }
    else if (digitalRead(PB_OK) == LOW) {
      delay(200);
      return PB_OK;
    }
    else if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);
      return PB_CANCEL;
    }
    update_time();
  }
}

void to_the_menu() {
  while (digitalRead(PB_CANCEL) == HIGH) {
    display.clearDisplay();
    print_line(modes[current_mode], 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      current_mode += 1;
      current_mode = current_mode % max_mode;
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      current_mode -= 1;
      if (current_mode < 0) {
        current_mode = max_mode - 1;
      }

    }
    else if (pressed == PB_OK) {
      delay(200);
      run_mode(current_mode);
    }
  }
}


void update_time_with_check_alarms() {
  update_time();
  print_time_now();

  if (alarm_enabled == true) {

    for (int i = 0; i < n_alarms; i++) {
      if (alarm_triggered[i] == false && alarm_hours[i] == hours && alarm_minutes[i] == minutes) {
        ring_alarm();
        alarm_triggered[i] = true;
      }
    }
  }
}


void run_mode(int mode) {
  if (mode == 0) {
    set_time();
  }
  else if (mode == 1 || mode == 2 || mode == 3) {
    set_alarm(mode - 1);
  }
  else if (mode == 4) {
    alarm_enabled = false;
    display.clearDisplay();
    print_line("All Alarms Disabled", 0, 0, 1);
    delay(1000);
  }

}


void set_time() {
  display.clearDisplay();
  int temp_hour = 0;
  while (true) {
    display.clearDisplay();
    print_line("Enter Offset hour(UTC) : " + String(temp_hour), 0, 0, 2);
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_hour += 1;
      temp_hour = temp_hour % 12;
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_hour -= 1;
      if (temp_hour < -12) {
        temp_hour = 12;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
    display.clearDisplay();
    print_line(String(temp_hour), 0, 50, 1);
  }

  int temp_minute = 0;
  display.clearDisplay();
  while (true) {
    display.clearDisplay();
    print_line("Enter Offset minute(UTC) : " + String(temp_minute), 0, 0, 1);
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_minute += 5;
      temp_minute = temp_minute % 60;
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_minute -= 5;
      if (temp_minute < 0) {
        temp_minute = 55;
      }

    }
    else if (pressed == PB_OK) {
      delay(200);
      UTC_OFFSET = temp_hour + temp_minute / 60;
      configTime(UTC_OFFSET * 3600, UTC_OFFSET_DST, NTP_SERVER);
      delay(200);
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
    display.clearDisplay();
    print_line(String(temp_hour), 0, 50, 1);
    print_line(":", 30, 50, 2);
    print_line(String(temp_minute), 40, 50, 1);
  }
  display.clearDisplay();
  print_line("UTC OFFSET is set!", 0, 0, 2);
  delay(2000);

}


void set_alarm(int alarm) {
  display.clearDisplay();
  int temp_hour = alarm_hours[alarm];

  while (true) {
    display.clearDisplay();
    print_line("Enter alarm hour: " + String(temp_hour), 0, 0, 1);
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_hour += 1;
      temp_hour = temp_hour % 24;
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_hour -= 1;
      if (temp_hour <= 0) {
        temp_hour = 23;
      }

    }
    else if (pressed == PB_OK) {
      delay(200);
      alarm_hours[alarm] = temp_hour;
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
    display.clearDisplay();
    print_line(String(temp_hour), 0, 50, 2);

  }

  int temp_minute = alarm_minutes[alarm];
  display.clearDisplay();

  while (true) {
    display.clearDisplay();
    print_line("Enter alarm minute :" + String(temp_minute), 0, 0, 1);
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_minute += 1;
      temp_minute = temp_minute % 60;
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_minute -= 1;
      if (temp_minute <= 0) {
        temp_minute = 59;
      }

    }
    else if (pressed == PB_OK) {
      delay(200);
      alarm_minutes[alarm] = temp_minute;
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
    display.clearDisplay();
    print_line(String(temp_hour), 0, 50, 2);
    print_line(":", 30, 50, 2);
    print_line(String(temp_minute), 40, 50, 2);
  }
  display.clearDisplay();
  print_line("Alarm is set:", 0, 0, 2);
  delay(200);
  print_line(String(alarm_hours[alarm]), 0, 50, 2);
  print_line(String(":"), 30, 50, 2);
  print_line(String(alarm_minutes[alarm]), 40, 50, 2);
  delay(2000);
}


void check_temp() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  display.clearDisplay();
  if (data.temperature > 32) {
    display.clearDisplay();
    print_line("TEMPERATURE IS HIGH", 0, 40, 1);
    digitalWrite(LED_2, HIGH);
  }
  else if (data.temperature < 26) {
    display.clearDisplay();
    print_line("TEMPERATURE IS LOW", 0, 40, 1);
    digitalWrite(LED_2, HIGH);
  }
  else {
    digitalWrite(LED_2, LOW);
  }

  if (data.humidity > 80) {
    //display.clearDisplay();
    print_line("HUMIDITY IS HIGH", 0, 50, 1);
    digitalWrite(LED_3, HIGH);
  }
  else if (data.humidity < 60) {
    //display.clearDisplay();
    print_line("HUMIDITY IS LOW", 0, 50, 1);
    digitalWrite(LED_3, HIGH);
  }
  else {
    digitalWrite(LED_3, LOW);
  }
}