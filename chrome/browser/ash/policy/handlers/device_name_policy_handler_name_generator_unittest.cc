// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/device_name_policy_handler_name_generator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DeviceNamePolicyHandlerNameGeneratorTest : public testing::Test {
 public:
  DeviceNamePolicyHandlerNameGeneratorTest() {}

  void formatAndAssert(const std::string& expected,
                       const std::string& name_template,
                       const std::string& asset_id,
                       const std::string& serial,
                       const std::string& mac,
                       const std::string& machine_name,
                       const std::string& location) {
    auto result = FormatHostname(name_template, asset_id, serial, mac,
                                 machine_name, location);
    ASSERT_EQ(expected, result);
  }
};

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, Basic) {
  formatAndAssert("name", "name", "asset123", "SER1AL123", "0000deadbeef",
                  "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, SubstituteValidAssetId) {
  formatAndAssert("chromebook-asset123", "chromebook-${ASSET_ID}", "asset123",
                  "SER1AL123", "0000deadbeef", "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, SubstituteValidSerial) {
  formatAndAssert("chromebook-SER1AL123", "chromebook-${SERIAL_NUM}",
                  "asset123", "SER1AL123", "0000deadbeef", "chrome_machine",
                  "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, SubstituteValidMAC) {
  formatAndAssert("chromebook-0000deadbeef", "chromebook-${MAC_ADDR}",
                  "asset123", "SER1AL123", "0000deadbeef", "chrome_machine",
                  "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, SubstituteMachineName) {
  formatAndAssert("chromebook-chrome_machine", "chromebook-${MACHINE_NAME}",
                  "asset123", "SER1AL123", "0000deadbeef", "chrome_machine",
                  "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, SubstituteLocation) {
  formatAndAssert("chromebook-loc123", "chromebook-${LOCATION}", "asset123",
                  "SER1AL123", "0000deadbeef", "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, MixedSubstitution) {
  formatAndAssert(
      "chromebook-deadbeef-SER1AL123-asset123-chrome_machine-loc123",
      "chromebook-${MAC_ADDR}-${SERIAL_NUM}-${ASSET_ID}-${MACHINE_NAME}-"
      "${LOCATION}",
      "asset123", "SER1AL123", "deadbeef", "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, SubstituteInvalidSerial) {
  formatAndAssert("", "chromebook-${SERIAL_NUM}", "asset123", "Serial number",
                  "0000deadbeef", "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, IncorrectTemplateVariable) {
  formatAndAssert("", "chromebook-${SERIAL_NUMBER}", "asset123", "SERIAL123",
                  "0000deadbeef", "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, InvalidFirstCharacter) {
  formatAndAssert("", "-somename", "asset123", "Serial number", "0000deadbeef",
                  "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, HostnameTooLong) {
  formatAndAssert("", "${ASSET_ID}${ASSET_ID}${ASSET_ID}",
                  "1234567890123456789012345678901", "serial", "0000deadbeef",
                  "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, HostnameExactly63Chars) {
  formatAndAssert(
      "1234567890123456789012345678901-1234567890123456789012345678901",
      "${ASSET_ID}-${ASSET_ID}", "1234567890123456789012345678901", "serial",
      "0000deadbeef", "chrome_machine", "loc123");
}

}  // namespace policy
