/*
  WiFiEsp test: ClientTest
  http://www.kccistc.net/
  작성일 : 2022.12.19
  작성자 : IoT 임베디드 KSH
*/
#define DEBUG
//#define DEBUG_WIFI

#include <WiFiEsp.h>
#include <SoftwareSerial.h>
#include <MsTimer2.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

#define AP_SSID "iot0"
#define AP_PASS "iot00000"
#define SERVER_NAME "10.10.141.78"
#define SERVER_PORT 5000
#define LOGID "LYH_ARD"
#define PASSWD "PASSWD"

// 핀 define 부분
#define GAS_PIN A1
#define WATER_LEVEL_PIN A0 
#define WIFIRX 8  //6:RX-->ESP8266 TX
#define WIFITX 7  //7:TX -->ESP8266 RX
#define CMD_SIZE 50
#define ARR_CNT 5

bool timerIsrFlag = false;
char recvId[10] = "LYH_LIN";  // SQL 저장 클라이이언트 ID
char sendId[10] = "LYH_ARD";
char sendBuf[CMD_SIZE];
char lcdLine1[17] = "Smart Parking Lot";
char lcdLine2[17] = "WiFi Connecting!";

int gasValue = 0;
int waterLevel = 0;
unsigned int secCount;
char getSensorId[10];
int sensorTime;
bool updateTimeFlag = false;

typedef struct {
  int year;
  int month;
  int day;
  int hour;
  int min;
  int sec;
} DATETIME;

DATETIME dateTime = {0, 0, 0, 12, 0, 0};
SoftwareSerial wifiSerial(WIFIRX, WIFITX); // 8:RX-->ESP8266 TX, 7:TX -->ESP8266 RX
WiFiEspClient client;
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  lcd.init();
  lcd.backlight();
  lcdDisplay(0, 0, lcdLine1);
  lcdDisplay(0, 1, lcdLine2);


  // 센서 핀 설정 부분
  pinMode(GAS_PIN, INPUT);         // 가스 센서 핀을 입력으로 설정
  pinMode(WATER_LEVEL_PIN, INPUT); // 물수위 센서 핀을 입력으로 설정

  // wifi 통신부분
#ifdef DEBUG
  Serial.begin(115200); //DEBUG
#endif
  wifi_Setup();

  MsTimer2::set(1000, timerIsr); // 1000ms period
  MsTimer2::start();
}

void loop() {
  if (client.available()) {
    socketEvent();
  }

  if (timerIsrFlag) //1초에 한번씩 실행
  {
    timerIsrFlag = false;
    if (!(secCount % 2)) //5초에 한번씩 실행
    {
      gasValue = analogRead(GAS_PIN);
      waterLevel = analogRead(WATER_LEVEL_PIN);

#ifdef DEBUG
      // Serial.print("Gas Value: ");
      // Serial.print(gasValue);
      // Serial.print(" Water Level: ");
      // Serial.println(waterLevel);
      
    // 센서 값 LCD에 표시 부분
#endif
      sprintf(lcdLine2, "G:%d, W:%d", gasValue, waterLevel);
      lcdDisplay(0, 1, lcdLine2);

      if (!client.connected()) {
        lcdDisplay(0, 1, "Server Down");
        server_Connect();
      }
    }
    // 센서 값을 서버로 전송
    if (sensorTime != 0 && !(secCount % sensorTime))
    {
     // sprintf(sendBuf, "[LYH_SQL]SET@%d@%d@%d\r\n", getSensorId, cds, (int)temp, (int)humi);
      ///sprintf(sendBuf, "[%s]SENSOR@%d@%d@%d\r\n", getSensorId, cds, (int)temp, (int)humi);  
      sprintf(sendBuf, "[%s]DATA@%d@%d\n", getSensorId, gasValue, waterLevel);
      client.write(sendBuf, strlen(sendBuf));
      client.flush(); 
    }
    sprintf(lcdLine1, "%02d.%02d  %02d:%02d:%02d", dateTime.month, dateTime.day, dateTime.hour, dateTime.min, dateTime.sec );
    lcdDisplay(0, 0, lcdLine1);
    if (updateTimeFlag) {
      client.print("[GETTIME]\n");
      updateTimeFlag = false;
    }
  }
}

void socketEvent()
{
  int i = 0;
  char * pToken;
  char * pArray[ARR_CNT] = {0};
  char recvBuf[CMD_SIZE] = {0};
  int len;

  sendBuf[0] = '\0';
  len = client.readBytesUntil('\n', recvBuf, CMD_SIZE);
  client.flush();
#ifdef DEBUG
  Serial.print("recv : ");
  Serial.print(recvBuf);
#endif
  pToken = strtok(recvBuf, "[@]");
  while (pToken != NULL)
  {
    pArray[i] =  pToken;
    if (++i >= ARR_CNT)
      break;
    pToken = strtok(NULL, "[@]");
  }
  if ((strlen(pArray[1]) + strlen(pArray[2])) < 16)
  {
    sprintf(lcdLine2, "%s %s", pArray[1], pArray[2]);
    lcdDisplay(0, 1, lcdLine2);
  }
  if (!strncmp(pArray[1], " New", 4)) // New Connected
  {
#ifdef DEBUG
    Serial.write('\n');
#endif
    strcpy(lcdLine2, "Server Connected");
    lcdDisplay(0, 1, lcdLine2);
    updateTimeFlag = true;
    return ;
  }
  else if (!strncmp(pArray[1], " Alr", 4)) //Already logged
  {
#ifdef DEBUG
    Serial.write('\n');
#endif
    client.stop();
    server_Connect();
    return ;
  }
  else if (!strncmp(pArray[1], "GETDATA", 9)) {
    if (pArray[2] != NULL) {
      sensorTime = atoi(pArray[2]);
      strcpy(getSensorId, pArray[0]);
      return;
    } else {
      sensorTime = 0;
      sprintf(sendBuf, "[%s]DATA@%d@%d\n", pArray[0], gasValue, waterLevel);
      //sprintf(sendBuf, "[%s]GAS:%d, WATER:%d\n", pArray[0], gasValue, waterLevel);
    }
  }
  else if(!strcmp(pArray[0],"GETTIME")) {  //GETTIME
    dateTime.year = (pArray[1][0]-0x30) * 10 + pArray[1][1]-0x30 ;
    dateTime.month =  (pArray[1][3]-0x30) * 10 + pArray[1][4]-0x30 ;
    dateTime.day =  (pArray[1][6]-0x30) * 10 + pArray[1][7]-0x30 ;
    dateTime.hour = (pArray[1][9]-0x30) * 10 + pArray[1][10]-0x30 ;
    dateTime.min =  (pArray[1][12]-0x30) * 10 + pArray[1][13]-0x30 ;
    dateTime.sec =  (pArray[1][15]-0x30) * 10 + pArray[1][16]-0x30 ;
#ifdef DEBUG
//    sprintf(sendBuf,"\nTime %02d.%02d.%02d %02d:%02d:%02d\n\r",dateTime.year,dateTime.month,dateTime.day,dateTime.hour,dateTime.min,dateTime.sec );
//    Serial.println(sendBuf);
#endif
    return;
  } 
  else
    return;

  client.write(sendBuf, strlen(sendBuf));
  client.flush();

#ifdef DEBUG
  Serial.print(", send : ");
  Serial.print(sendBuf);
#endif
}

void timerIsr() {
  timerIsrFlag = true;
  secCount++;
  clock_calc(&dateTime);
}

void clock_calc(DATETIME *dateTime) {
  dateTime->sec++;          // increment second
  if (dateTime->sec >= 60) {                              // if second = 60, second = 0
    dateTime->sec = 0;
    dateTime->min++; 
    if (dateTime->min >= 60) {                          // if minute = 60, minute = 0
      dateTime->min = 0;
      dateTime->hour++;                               // increment hour
      if (dateTime->hour == 24) {
        dateTime->hour = 0;
        updateTimeFlag = true;
      }
    }
  }
}

void wifi_Setup() {
  wifiSerial.begin(38400);
  wifi_Init();
  server_Connect();
}

void wifi_Init() {
  do {
    WiFi.init(&wifiSerial);
    if (WiFi.status() == WL_NO_SHIELD) {
#ifdef DEBUG_WIFI
      Serial.println("WiFi shield not present");
#endif
    } else
      break;
  } while (1);

#ifdef DEBUG_WIFI
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(AP_SSID);
#endif
  while (WiFi.begin(AP_SSID, AP_PASS) != WL_CONNECTED) {
#ifdef DEBUG_WIFI
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(AP_SSID);
#endif
  }
  sprintf(lcdLine1, "ID:%s", LOGID);
  lcdDisplay(0, 0, lcdLine1);
  sprintf(lcdLine2, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  lcdDisplay(0, 1, lcdLine2);

#ifdef DEBUG_WIFI
  Serial.println("You're connected to the network");
  printWifiStatus();
#endif
}

int server_Connect() {
#ifdef DEBUG_WIFI
  Serial.println("Starting connection to server...");
#endif

  if (client.connect(SERVER_NAME, SERVER_PORT)) {
#ifdef DEBUG_WIFI
    Serial.println("Connect to server");
#endif
    client.print("["LOGID":"PASSWD"]");
  } else {
#ifdef DEBUG_WIFI
    Serial.println("server connection failure");
#endif
  }
}

void printWifiStatus() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void lcdDisplay(int x, int y, char *str) {
  int len = 16 - strlen(str);
  lcd.setCursor(x, y);
  lcd.print(str);
  for (int i = len; i > 0; i--)
    lcd.print(" ");
}