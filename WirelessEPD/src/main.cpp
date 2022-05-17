#include <Arduino.h>
#include <GxEPD2_BW.h> // including both doesn't use more code or ram
#include <GxEPD2_3C.h> // including both doesn't use more code or ram
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

/* InfluxDB Connection Parameters */
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define INFLUXDB_URL ""
// InfluxDB v2 server or cloud API authentication token (Use: InfluxDB UI -> Data -> Tokens -> <select token>)
#define INFLUXDB_TOKEN ""
// InfluxDB v2 organization id (Use: InfluxDB UI -> User -> About -> Common Ids )
#define INFLUXDB_ORG ""
// InfluxDB v2 bucket name (Use: InfluxDB UI ->  Data -> Buckets)
#define INFLUXDB_BUCKET ""
#define TZ_INFO ""
#include <esp_task_wdt.h>
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
unsigned long previousMillis = 0;
uint32_t co2_hist[25] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
uint32_t co2_max = 0;
uint32_t co2_min = 0;
GxEPD2_3C<GxEPD2_154_Z90c, GxEPD2_154_Z90c::HEIGHT> display(GxEPD2_154_Z90c(/*CS=D8*/ SS, /*DC=D3*/ 11, /*RST=D4*/ 12, /*BUSY=D2*/ 13));
bool boot = true;
void setup()
{

    pinMode(33, OUTPUT);
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.println(WiFi.status());
        delay(2500);
    }

    client.setHTTPOptions(HTTPOptions().httpReadTimeout(200));
    client.setHTTPOptions(HTTPOptions().connectionReuse(true));
    // Check server connection
    if (client.validateConnection())
    {
        Serial.print("Connected to InfluxDB: ");
        Serial.println(client.getServerUrl());
    }
    else
    {
        Serial.print("InfluxDB connection failed: ");
        Serial.println(client.getLastErrorMessage());
    }
    Serial.println("[INIT] IFDB CONNECTION OK");
}
void loop()
{
    esp_task_wdt_init(15, true);

    display.init();
    display.setRotation(1);
    for (uint8_t i = 0; i < 25; i++)
    {
        co2_hist[i] = 500;
        co2_max = 500;
    }

    while (1)
    {
        unsigned long currentMillis = millis();
        if ((currentMillis - previousMillis >= 300000) || boot)
        {
            display.fillScreen(GxEPD_BLACK);
            previousMillis = currentMillis;
            String query = "from(bucket: \"MESH\") |> range(start: -1h) |> filter(fn: (r) => r._measurement == \"CO2\") |> filter(fn: (r) => r.UID == \"22016\") |> filter(fn: (r) => r._field == \"CO2\") |> last()";
            FluxQueryResult result = client.query(query);
            uint32_t co2 = 1;
            while (result.next())
            {
                co2 = (uint32_t)result.getValueByName("_value").getDouble();
            }
            display.setFont(&FreeSansBold24pt7b);
            display.setTextColor(GxEPD_WHITE);
            display.setCursor(1, 38);
            display.print(co2);
            int16_t ulx, uly;
            uint16_t w, h;
            display.getTextBounds((String)co2, 1, 40, &ulx, &uly, &w, &h);
            display.setTextColor(GxEPD_WHITE);
            display.setFont(&FreeSansBold12pt7b);

            display.setTextSize(1);
            display.setCursor(ulx + w + 3, 40);
            display.print("PPM");
            display.setCursor(ulx + w + 3, 20);
            display.setTextColor(GxEPD_RED);
            display.print("CO2");
            display.setTextColor(GxEPD_WHITE);
            digitalWrite(33, !digitalRead(33));
            co2_max = 1;
            co2_min = 9999;
            for (uint8_t i = 1; i < 25; i++)
            {

                memmove(&co2_hist[i - 1], &co2_hist[i], sizeof(uint32_t));
                Serial.print(co2_hist[i - 1]);
                Serial.print(" ");
                if (co2_hist[i] > co2_max)
                    co2_max = co2_hist[i];
                if ((co2_hist[i] < co2_min) && co2_hist[i] != 0)
                    co2_min = co2_hist[i];
            }
            if ((uint32_t)co2 > co2_max)
                co2_max = (uint32_t)co2;
            if (((uint32_t)co2 < co2_min) && (uint32_t)co2 != 0)
                co2_min = (uint32_t)co2;
            co2_hist[24] = (uint32_t)co2;
            display.getTextBounds("CO2", ulx + w + 5, 20, &ulx, &uly, &w, &h);
            display.setCursor(ulx + w + 2, 18);
            display.setFont(&FreeSansBold9pt7b);
            display.print(co2_max);
            display.setCursor(ulx + w + 2, 38);
            display.setFont(&FreeSansBold9pt7b);
            display.print(co2_min);
            for (uint8_t i = 0; i < 24; i++)
            {
                float hy = (50.00 / ((float)co2_max - (float)co2_min)) * ((float)co2_hist[i] - (float)co2_min);
                float ly = 0;
                float l2y = 0;
                float l3y = 0;
                float l4y = 0;
                float diff = 0;
                if (i != 24)
                {
                    diff = (float)co2_hist[i + 1] - (float)co2_hist[i];
                    ly = (50.00 / ((float)co2_max - (float)co2_min)) * (((float)co2_hist[i] - (float)co2_min) + (diff * 0.15));
                    l2y = (50.00 / ((float)co2_max - (float)co2_min)) * (((float)co2_hist[i] - (float)co2_min) + (diff * 0.50));
                    l3y = (50.00 / ((float)co2_max - (float)co2_min)) * (((float)co2_hist[i] - (float)co2_min) + (diff * 0.85));
                    l4y = (50.00 / ((float)co2_max - (float)co2_min)) * (((float)co2_hist[i] - (float)co2_min) + (diff));

                    Serial.println(" ");
                    Serial.print(ly);
                    Serial.print(" ");
                    Serial.print(l2y);
                    Serial.print(" ");
                    Serial.print(l3y);
                    Serial.print(" ");
                    Serial.print(l4y);
                    Serial.print(" ");
                    Serial.print(diff);

                    display.drawLine(0 + i * 8, 100 - hy, 2 + i * 8, 100 - ly, GxEPD_WHITE);
                    display.drawLine(2 + i * 8, 100 - ly, 4 + i * 8, 100 - l2y, GxEPD_WHITE);
                    display.drawLine(4 + i * 8, 100 - l2y, 6 + i * 8, 100 - l3y, GxEPD_WHITE);
                    display.drawLine(6 + i * 8, 100 - l3y, 8 + i * 8, 100 - l4y, GxEPD_WHITE);
                    display.drawLine(0 + i * 8, 100 - hy - 1, 2 + i * 8, 100 - ly - 1, GxEPD_WHITE);
                    display.drawLine(2 + i * 8, 100 - ly - 1, 4 + i * 8, 100 - l2y - 1, GxEPD_WHITE);
                    display.drawLine(4 + i * 8, 100 - l2y - 1, 6 + i * 8, 100 - l3y - 1, GxEPD_WHITE);
                    display.drawLine(6 + i * 8, 100 - l3y - 1, 8 + i * 8, 100 - l4y - 1, GxEPD_WHITE);
                    display.drawLine(0 + i * 8, 100 - hy - 2, 2 + i * 8, 100 - ly - 2, GxEPD_WHITE);
                    display.drawLine(2 + i * 8, 100 - ly - 2, 4 + i * 8, 100 - l2y - 2, GxEPD_WHITE);
                    display.drawLine(4 + i * 8, 100 - l2y - 2, 6 + i * 8, 100 - l3y - 2, GxEPD_WHITE);
                    display.drawLine(6 + i * 8, 100 - l3y - 2, 8 + i * 8, 100 - l4y - 2, GxEPD_WHITE);

                    display.drawFastVLine(2 + i * 8, 105 - ly, ly, GxEPD_WHITE);
                    display.drawFastVLine(4 + i * 8, 105 - l2y, l2y, GxEPD_WHITE);
                    display.drawFastVLine(6 + i * 8, 105 - l3y, l3y, GxEPD_WHITE);
                    display.drawFastVLine(8 + i * 8, 105 - l4y, l4y, GxEPD_WHITE);
                    display.fillRect(0, 102, 200, 5, GxEPD_BLACK);
                    digitalWrite(33, !digitalRead(33));
                }
            }
            if (boot)
            {
                display.setTextColor(GxEPD_LIGHTGREY);
                display.setFont(&FreeSansBold9pt7b);
                display.setTextSize(1);
                display.setCursor(20, 70);
                display.print("Graph loading...");
                boot = false;
            }
            display.fillCircle(0 + 25 * 8 - 5, 100 - (50.00 / ((float)co2_max - (float)co2_min)) * ((float)co2_hist[24] - (float)co2_min), 3, GxEPD_WHITE);
            String query1 = "from(bucket: \"MESH\") |> range(start: -15m) |> filter(fn: (r) => r._measurement == \"Environment\") |> filter(fn: (r) => r.UID == \"4102\") |> filter(fn: (r) => r._field == \"Temperature\") |> last()";
            FluxQueryResult result1 = client.query(query1);
            double temp = 0;
            while (result1.next())
            {
                temp = (double)result1.getValueByName("_value").getDouble();
            }
            display.setFont(&FreeSansBold18pt7b);
            display.setTextColor(GxEPD_WHITE);
            display.setCursor(2, 130);
            display.print(temp);
            display.getTextBounds((String)temp, 2, 130, &ulx, &uly, &w, &h);
            display.setTextColor(GxEPD_WHITE);
            display.setFont(&FreeSansBold9pt7b);
            display.setTextSize(1);
            display.setCursor(ulx + w + 5, 132);
            display.print("deg C");
            display.setCursor(ulx + w + 5, 118);
            display.setTextColor(GxEPD_RED);
            display.print("TEMP");
            String query2 = "from(bucket: \"MESH\") |> range(start: -15m) |> filter(fn: (r) => r._measurement == \"Environment\") |> filter(fn: (r) => r.UID == \"4102\") |> filter(fn: (r) => r._field == \"Humidity\") |> last()";
            FluxQueryResult result2 = client.query(query2);
            double humi = 0;
            digitalWrite(33, !digitalRead(33));
            while (result2.next())
            {
                humi = (double)result2.getValueByName("_value").getDouble();
            }
            display.setFont(&FreeSansBold18pt7b);
            display.setTextColor(GxEPD_WHITE);
            display.setCursor(2, 160);
            display.print(humi);
            display.getTextBounds((String)humi, 2, 160, &ulx, &uly, &w, &h);
            display.setTextColor(GxEPD_WHITE);
            display.setFont(&FreeSansBold9pt7b);
            display.setTextSize(1);
            display.setCursor(ulx + w + 5, 162);
            display.print("%");
            display.setCursor(ulx + w + 5, 148);
            display.setTextColor(GxEPD_RED);
            display.print("HUMIDITY");
            String query3 = "from(bucket: \"MESH\") |> range(start: -15m) |> filter(fn: (r) => r._measurement == \"Environment\") |> filter(fn: (r) => r.UID == \"4102\") |> filter(fn: (r) => r._field == \"Pressure\") |> last()";
            FluxQueryResult result3 = client.query(query3);
            uint16_t pres = 0;
            digitalWrite(33, !digitalRead(33));
            while (result3.next())
            {
                pres = (uint16_t)result3.getValueByName("_value").getDouble();
            }
            display.setFont(&FreeSansBold18pt7b);
            display.setTextColor(GxEPD_WHITE);
            display.setCursor(2, 191);
            display.print((uint16_t)pres);
            display.getTextBounds((String)pres, 2, 160, &ulx, &uly, &w, &h);
            display.setTextColor(GxEPD_WHITE);
            display.setFont(&FreeSansBold9pt7b);
            display.setTextSize(1);
            display.setCursor(ulx + w + 5, 192);
            display.print("hPa");
            display.setCursor(ulx + w + 5, 178);
            display.setTextColor(GxEPD_RED);
            display.print("PRESSURE");

            digitalWrite(33, !digitalRead(33));
            display.display();
            digitalWrite(33, !digitalRead(33));
        }
        // yield();
        String keepalive = "from(bucket: \"MESH\") |> range(start: -0m) |> filter(fn: (r) => r._measurement == \"NULL\") |> filter(fn: (r) => r.UID == \"NULL\") |> filter(fn: (r) => r._field == \"NULL\") |> last()";
        FluxQueryResult l = client.query(keepalive);
        delay(5000);
        digitalWrite(33, !digitalRead(33));
    }
}
