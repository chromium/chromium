// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/handlers/device_name_policy_handler.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DeviceNamePolicyHandlerTest : public testing::Test {
 public:
  DeviceNamePolicyHandlerTest() {}

  void formatAndAssert(const std::string& expected,
                       const std::string& name_template,
                       const std::string& asset_id,
                       const std::string& serial,
                       const std::string& mac,
                       const std::string& machine_name,
                       const std::string& location) {
    auto result = DeviceNamePolicyHandler::FormatHostname(
        name_template, asset_id, serial, mac, machine_name, location);
    ASSERT_EQ(expected, result);
  }
};

TEST_F(DeviceNamePolicyHandlerTest, Basic) {
  formatAndAssert("name", "name", "asset123", "SER1AL123", "0000deadbeef",
                  "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerTest, SubstituteValidAssetId) {
  formatAndAssert("chromebook-asset123", "chromebook-${ASSET_ID}", "asset123",
                  "SER1AL123", "0000deadbeef", "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerTest, SubstituteValidSerial) {
  formatAndAssert("chromebook-SER1AL123", "chromebook-${SERIAL_NUM}",
                  "asset123", "SER1AL123", "0000deadbeef", "chrome_machine",
                  "loc123");
}

TEST_F(DeviceNamePolicyHandlerTest, SubstituteValidMAC) {
  formatAndAssert("chromebook-0000deadbeef", "chromebook-${MAC_ADDR}",
                  "asset123", "SER1AL123", "0000deadbeef", "chrome_machine",
                  "loc123");
}

TEST_F(DeviceNamePolicyHandlerTest, SubstituteMachineName) {
  formatAndAssert("chromebook-chrome_machine", "chromebook-${MACHINE_NAME}",
                  "asset123", "SER1AL123", "0000deadbeef", "chrome_machine",
                  "loc123");
}

TEST_F(DeviceNamePolicyHandlerTest, SubstituteLocation) {
  formatAndAssert("chromebook-loc123", "chromebook-${LOCATION}", "asset123",
                  "SER1AL123", "0000deadbeef", "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerTest, MixedSubstitution) {
  formatAndAssert(
      "chromebook-deadbeef-SER1AL123-asset123-chrome_machine-loc123",
      "chromebook-${MAC_ADDR}-${SERIAL_NUM}-${ASSET_ID}-${MACHINE_NAME}-"
      "${LOCATION}",
      "asset123", "SER1AL123", "deadbeef", "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerTest, SubstituteInvalidSerial) {
  formatAndAssert("", "chromebook-${SERIAL_NUM}", "asset123", "Serial number",
                  "0000deadbeef", "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerTest, IncorrectTemplateVariable) {
  formatAndAssert("", "chromebook-${SERIAL_NUMBER}", "asset123", "SERIAL123",
                  "0000deadbeef", "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerTest, InvalidFirstCharacter) {
  formatAndAssert("", "-somename", "asset123", "Serial number", "0000deadbeef",
                  "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerTest, HostnameTooLong) {
  formatAndAssert("", "${ASSET_ID}${ASSET_ID}${ASSET_ID}",
                  "1234567890123456789012345678901", "serial", "0000deadbeef",
                  "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerTest, HostnameExactly63Chars) {
  formatAndAssert(
      "1234567890123456789012345678901-1234567890123456789012345678901",
      "${ASSET_ID}-${ASSET_ID}", "1234567890123456789012345678901", "serial",
      "0000deadbeef", "chrome_machine", "loc123");
}

}  // namespace policy
