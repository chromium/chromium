// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/device_name_policy_handler_name_generator.h"

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DeviceNamePolicyHandlerNameGeneratorTest : public testing::Test {
 public:
  DeviceNamePolicyHandlerNameGeneratorTest() = default;

  void FormatAndAssert(std::string_view expected,
                       std::string_view name_template,
                       std::string_view asset_id,
                       std::string_view serial,
                       std::string_view mac,
                       std::string_view machine_name,
                       std::string_view location) {
    auto result = FormatHostname(name_template, asset_id, serial, mac,
                                 machine_name, location);
    ASSERT_EQ(expected, result);
  }
};

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, Basic) {
  FormatAndAssert("name", "name", "asset123", "SER1AL123", "0000deadbeef",
                  "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, SubstituteValidAssetId) {
  FormatAndAssert("chromebook-asset123", "chromebook-${ASSET_ID}", "asset123",
                  "SER1AL123", "0000deadbeef", "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, SubstituteValidSerial) {
  FormatAndAssert("chromebook-SER1AL123", "chromebook-${SERIAL_NUM}",
                  "asset123", "SER1AL123", "0000deadbeef", "chrome_machine",
                  "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, SubstituteValidMAC) {
  FormatAndAssert("chromebook-0000deadbeef", "chromebook-${MAC_ADDR}",
                  "asset123", "SER1AL123", "0000deadbeef", "chrome_machine",
                  "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, SubstituteMachineName) {
  FormatAndAssert("chromebook-chrome_machine", "chromebook-${MACHINE_NAME}",
                  "asset123", "SER1AL123", "0000deadbeef", "chrome_machine",
                  "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, SubstituteLocation) {
  FormatAndAssert("chromebook-loc123", "chromebook-${LOCATION}", "asset123",
                  "SER1AL123", "0000deadbeef", "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, MixedSubstitution) {
  FormatAndAssert(
      "chromebook-deadbeef-SER1AL123-asset123-chrome_machine-loc123",
      "chromebook-${MAC_ADDR}-${SERIAL_NUM}-${ASSET_ID}-${MACHINE_NAME}-"
      "${LOCATION}",
      "asset123", "SER1AL123", "deadbeef", "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, SubstituteInvalidSerial) {
  FormatAndAssert("", "chromebook-${SERIAL_NUM}", "asset123", "Serial number",
                  "0000deadbeef", "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, IncorrectTemplateVariable) {
  FormatAndAssert("", "chromebook-${SERIAL_NUMBER}", "asset123", "SERIAL123",
                  "0000deadbeef", "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, InvalidFirstCharacter) {
  FormatAndAssert("", "-somename", "asset123", "Serial number", "0000deadbeef",
                  "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, HostnameTooLong) {
  FormatAndAssert("", "${ASSET_ID}${ASSET_ID}${ASSET_ID}",
                  "1234567890123456789012345678901", "serial", "0000deadbeef",
                  "chrome_machine", "loc123");
}

TEST_F(DeviceNamePolicyHandlerNameGeneratorTest, HostnameExactly63Chars) {
  FormatAndAssert(
      "1234567890123456789012345678901-1234567890123456789012345678901",
      "${ASSET_ID}-${ASSET_ID}", "1234567890123456789012345678901", "serial",
      "0000deadbeef", "chrome_machine", "loc123");
}

}  // namespace policy
