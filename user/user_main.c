/* main.c -- MQTT client example
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/
#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "mqtt.h"
#include "wifi.h"
#include "config.h"
#include "debug.h"
#include "gpio.h"
#include "user_interface.h"
#include "mem.h"

MQTT_Client mqttClient;
ETSTimer tmp_timer;

uint8 dht_t1=0, dht_t2=0;
uint8 dht_d1=0, dht_d2=0;
uint8 dht_chk=0, dht_num=0;

const char *data_led_on = {"breaker:open"};
const char *data_led_off = {"breaker:close"};

uint8 dht_readat()
{
    uint8 i=0, dat=0;


    for(i=0; i<8; i++)
    {
        dht_num = 2;
        while((GPIO_INPUT_GET(12) == 0) && (dht_num ++));
        os_delay_us(40);
        dat = dat << 1;
        if(GPIO_INPUT_GET(12) == 1)
        {
            dht_num = 2;
            dat = dat | 0x01;
            while((GPIO_INPUT_GET(12) == 1) && (dht_num ++));
        }
    }

    return dat;
}

void dht_getdat()
{
    uint8 i=0;
    
    gpio_output_set(0, BIT12, BIT12,0);
    os_delay_us(20000);//40000
    gpio_output_set(BIT12, 0, 0,BIT12);

    os_delay_us(40);
    gpio_output_set(BIT12, 0, 0,BIT12);

    if(GPIO_INPUT_GET(14) == 0)
    {
        dht_num = 2;
        while((GPIO_INPUT_GET(12) == 0) && (dht_num ++));
        dht_num = 2;
        while((GPIO_INPUT_GET(12) == 1) && (dht_num ++));
        dht_d1 = dht_readat();
        dht_d2 = dht_readat();
        dht_t1 = dht_readat();
        dht_t2 = dht_readat();
        dht_chk = dht_readat();
    }
    gpio_output_set(BIT12, 0, 0,BIT12);
    os_delay_us(30000);
}

void ICACHE_FLASH_ATTR tmp_send(void *arg)
{
	char str[4];
	uint8_t num;

	MQTT_Client* client = (MQTT_Client*)arg;

	os_printf("send tmp data\n");

	dht_getdat();
	os_printf("d1 :%d\n", dht_d1);
	os_printf("d2 :%d\n", dht_d2);
	os_printf("t1 :%d\n", dht_t1);
	os_printf("t2 :%d\n", dht_t2);
    num = dht_d1 + dht_d2 + dht_t1 + dht_t2;
    if(num == dht_chk)
    {
        os_sprintf(str, "%d", dht_t1);
        os_printf("the string is %s\n", str);
        MQTT_Publish(client, "OID/t3", str, strlen(str), 0, 1);
    }
}

void wifiConnectCb(uint8_t status)
{
	if(status == STATION_GOT_IP){
		MQTT_Connect(&mqttClient);
	} else {
		MQTT_Disconnect(&mqttClient);
	}
}
void mqttConnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Connected\r\n");
	MQTT_Subscribe(client, "OID/app", 0);
	// MQTT_Subscribe(client, "/mqtt/topic/1", 1);
	// MQTT_Subscribe(client, "/mqtt/topic/2", 2);

	// MQTT_Publish(client, "OID/hdw", "hello0", 6, 0, 0);
	// MQTT_Publish(client, "/mqtt/topic/1", "hello1", 6, 1, 0);
	// MQTT_Publish(client, "/mqtt/topic/2", "hello2", 6, 2, 0);

	os_timer_disarm(&tmp_timer);
	os_timer_setfn(&tmp_timer, (os_timer_func_t *)tmp_send, mqttClient);
	os_timer_arm(&tmp_timer, 1000, 1);

}

void mqttDisconnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Disconnected\r\n");
}

void mqttPublishedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Published\r\n");
}

void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	char *topicBuf = (char*)os_zalloc(topic_len+1),
			*dataBuf = (char*)os_zalloc(data_len+1);

	MQTT_Client* client = (MQTT_Client*)args;

	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

    if(strncmp(data, data_led_on, 12) == 0)
    {
        GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 1);
        GPIO_OUTPUT_SET(GPIO_ID_PIN(5), 1);
    }
    else if(strncmp(data, data_led_off, 13) == 0)
    {
        GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 0);
        GPIO_OUTPUT_SET(GPIO_ID_PIN(5), 0);
    }

	INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);
	os_free(topicBuf);
	os_free(dataBuf);
}


/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
 *******************************************************************************/
uint32 ICACHE_FLASH_ATTR
user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;
    
    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;
            
        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;
            
        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;
            
        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;
            
        default:
            rf_cal_sec = 0;
            break;
    }
    
    return rf_cal_sec;
}


void user_init(void)
{
	// uart_init(BIT_RATE_115200, BIT_RATE_115200);
	// os_delay_us(1000000);

	//CFG_Load();

	//MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);
	MQTT_InitConnection(&mqttClient, "101.200.207.137", 1883, 0);

	//MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive, 1);
	MQTT_InitClient(&mqttClient, "10090", "10066", "123456", 120, 1);

	MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);

	//WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);
	WIFI_Connect("xtyk", "xtyk88888", wifiConnectCb);

	//DHT11 -- gpio12
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
    gpio_output_set(BIT12, 0, 0,BIT12);//1

    //led -- gpio4,gpio5
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);
    GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 0);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
    GPIO_OUTPUT_SET(GPIO_ID_PIN(5), 0);

	INFO("\r\nSystem started ...\r\n");
}
