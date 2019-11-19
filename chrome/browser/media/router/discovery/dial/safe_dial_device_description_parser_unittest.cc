// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/media/router/discovery/dial/safe_dial_device_description_parser.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

namespace {

constexpr char kDeviceDescriptionWithService[] =
    "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\n"
    "<specVersion>\n"
    "<major>1</major>\n"
    "<minor>0</minor>\n"
    "</specVersion>\n"
    "<URLBase>http://172.31.71.84:8008</URLBase>\n"
    "<device>\n"
    "<deviceType>urn:dial-multiscreen-org:device:dial:1</deviceType>\n"
    "<friendlyName>eureka9019</friendlyName>\n"
    "<manufacturer>Google Inc.</manufacturer>\n"
    "<modelName>Eureka Dongle</modelName>\n"
    "<serialNumber>123456789000</serialNumber>\n"
    "<UDN>uuid:d90dda41-8fa0-61ac-0567-f949d3e34b0e</UDN>\n"
    "<serviceList>\n"
    "<service>\n"
    "<serviceType>urn:dial-multiscreen-org:service:dial:1</serviceType>\n"
    "<serviceId>urn:dial-multiscreen-org:serviceId:dial</serviceId>\n"
    "<controlURL>/ssdp/notfound</controlURL>\n"
    "<eventSubURL>/ssdp/notfound</eventSubURL>\n"
    "<SCPDURL>/ssdp/notfound</SCPDURL>\n"
    "<servicedata xmlns=\"uri://cloudview.google.com/...\">\n"
    "</servicedata>\n"
    "</service>\n"
    "</serviceList>\n"
    "</device>\n"
    "</root>\n";

constexpr char kDeviceDescriptionWithoutService[] =
    "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\n"
    "<specVersion>\n"
    "<major>1</major>\n"
    "<minor>0</minor>\n"
    "</specVersion>\n"
    "<URLBase>http://172.31.71.84:8008</URLBase>\n"
    "<device>\n"
    "<deviceType>urn:dial-multiscreen-org:device:dial:1</deviceType>\n"
    "<friendlyName>eureka9020</friendlyName>\n"
    "<manufacturer>Google Inc.</manufacturer>\n"
    "<modelName>Eureka Dongle</modelName>\n"
    "<serialNumber>123456789000</serialNumber>\n"
    "<UDN>uuid:d90dda41-8fa0-61ac-0567-f949d3e34b0f</UDN>\n"
    "</device>\n"
    "</root>\n";

std::string& Replace(std::string& input,
                     const std::string& from,
                     const std::string& to) {
  size_t pos = input.find(from);
  if (pos == std::string::npos)
    return input;

  return input.replace(pos, from.size(), to);
}

}  // namespace

class SafeDialDeviceDescriptionParserTest : public testing::Test {
 public:
  SafeDialDeviceDescriptionParserTest() = default;

  ParsedDialDeviceDescription Parse(
      const std::string& xml,
      const GURL& app_url,
      SafeDialDeviceDescriptionParser::ParsingError expected_error) {
    ParsedDialDeviceDescription device_description;
    SafeDialDeviceDescriptionParser::ParsingError error;
    base::RunLoop run_loop;
    SafeDialDeviceDescriptionParser parser;
    parser.Parse(
        xml, app_url,
        base::BindOnce(
            [](base::Closure quit_loop,
               ParsedDialDeviceDescription* out_device_description,
               SafeDialDeviceDescriptionParser::ParsingError* out_error,
               const ParsedDialDeviceDescription& device_description,
               SafeDialDeviceDescriptionParser::ParsingError error) {
              *out_device_description = device_description;
              *out_error = error;
              quit_loop.Run();
            },
            run_loop.QuitClosure(), &device_description, &error));
    run_loop.Run();
    EXPECT_EQ(static_cast<int>(expected_error), static_cast<int>(error));
    return device_description;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  DISALLOW_COPY_AND_ASSIGN(SafeDialDeviceDescriptionParserTest);
};

TEST_F(SafeDialDeviceDescriptionParserTest, TestInvalidXml) {
  ParsedDialDeviceDescription device_description = Parse(
      "", GURL(), SafeDialDeviceDescriptionParser::ParsingError::kInvalidXml);
  EXPECT_TRUE(device_description.unique_id.empty());
}

TEST_F(SafeDialDeviceDescriptionParserTest, TestParse) {
  std::string xml_text(kDeviceDescriptionWithService);

  GURL app_url("http://www.myapp.com");
  ParsedDialDeviceDescription device_description = Parse(
      xml_text, app_url, SafeDialDeviceDescriptionParser::ParsingError::kNone);
  EXPECT_EQ("urn:dial-multiscreen-org:device:dial:1",
            device_description.device_type);
  EXPECT_EQ("eureka9019", device_description.friendly_name);
  EXPECT_EQ("Eureka Dongle", device_description.model_name);
  EXPECT_EQ("uuid:d90dda41-8fa0-61ac-0567-f949d3e34b0e",
            device_description.unique_id);
  EXPECT_EQ(app_url, device_description.app_url);
}

TEST_F(SafeDialDeviceDescriptionParserTest, TestParseWithSpecialCharacter) {
  std::string old_name = "<friendlyName>eureka9019</friendlyName>";
  std::string new_name = "<friendlyName>Samsung LED40\'s</friendlyName>";

  std::string xml_text(kDeviceDescriptionWithService);
  xml_text = Replace(xml_text, old_name, new_name);

  ParsedDialDeviceDescription device_description = Parse(
      xml_text, GURL(), SafeDialDeviceDescriptionParser::ParsingError::kNone);
  EXPECT_EQ("urn:dial-multiscreen-org:device:dial:1",
            device_description.device_type);
  EXPECT_EQ("Samsung LED40\'s", device_description.friendly_name);
  EXPECT_EQ("Eureka Dongle", device_description.model_name);
  EXPECT_EQ("uuid:d90dda41-8fa0-61ac-0567-f949d3e34b0e",
            device_description.unique_id);
}

TEST_F(SafeDialDeviceDescriptionParserTest,
       TestParseWithoutFriendlyNameModelName) {
  std::string friendly_name = "<friendlyName>eureka9020</friendlyName>";
  std::string model_name = "<modelName>Eureka Dongle</modelName>";

  std::string xml_text(kDeviceDescriptionWithoutService);
  xml_text = Replace(xml_text, friendly_name, "");
  xml_text = Replace(xml_text, model_name, "");

  ParsedDialDeviceDescription device_description = Parse(
      xml_text, GURL(), SafeDialDeviceDescriptionParser::ParsingError::kNone);
  EXPECT_TRUE(device_description.friendly_name.empty());
  EXPECT_TRUE(device_description.model_name.empty());
}

TEST_F(SafeDialDeviceDescriptionParserTest, TestParseWithoutFriendlyName) {
  std::string friendly_name = "<friendlyName>eureka9020</friendlyName>";

  std::string xml_text(kDeviceDescriptionWithoutService);
  xml_text = Replace(xml_text, friendly_name, "");

  ParsedDialDeviceDescription device_description = Parse(
      xml_text, GURL(), SafeDialDeviceDescriptionParser::ParsingError::kNone);
  EXPECT_EQ("urn:dial-multiscreen-org:device:dial:1",
            device_description.device_type);
  EXPECT_EQ("Eureka Dongle [4b0f]", device_description.friendly_name);
  EXPECT_EQ("Eureka Dongle", device_description.model_name);
  EXPECT_EQ("uuid:d90dda41-8fa0-61ac-0567-f949d3e34b0f",
            device_description.unique_id);
}

}  // namespace media_router
