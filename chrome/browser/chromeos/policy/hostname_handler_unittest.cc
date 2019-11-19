// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/hostname_handler.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class HostnameHandlerTest : public testing::Test {
 public:
  HostnameHandlerTest() {}

  void formatAndAssert(const std::string& expected,
                       const std::string& name_template,
                       const std::string& asset_id,
                       const std::string& serial,
                       const std::string& mac,
                       const std::string& machine_name,
                       const std::string& location) {
    auto result = HostnameHandler::FormatHostname(
        name_template, asset_id, serial, mac, machine_name, location);
    ASSERT_EQ(expected, result);
  }
};

TEST_F(HostnameHandlerTest, Basic) {
  formatAndAssert("name", "name", "asset123", "SER1AL123", "0000deadbeef",
                  "chrome_machine", "loc123");
}

TEST_F(HostnameHandlerTest, SubstituteValidAssetId) {
  formatAndAssert("chromebook-asset123", "chromebook-${ASSET_ID}", "asset123",
                  "SER1AL123", "0000deadbeef", "chrome_machine", "loc123");
}

TEST_F(HostnameHandlerTest, SubstituteValidSerial) {
  formatAndAssert("chromebook-SER1AL123", "chromebook-${SERIAL_NUM}",
                  "asset123", "SER1AL123", "0000deadbeef", "chrome_machine",
                  "loc123");
}

TEST_F(HostnameHandlerTest, SubstituteValidMAC) {
  formatAndAssert("chromebook-0000deadbeef", "chromebook-${MAC_ADDR}",
                  "asset123", "SER1AL123", "0000deadbeef", "chrome_machine",
                  "loc123");
}

TEST_F(HostnameHandlerTest, SubstituteMachineName) {
  formatAndAssert("chromebook-chrome_machine", "chromebook-${MACHINE_NAME}",
                  "asset123", "SER1AL123", "0000deadbeef", "chrome_machine",
                  "loc123");
}

TEST_F(HostnameHandlerTest, SubstituteLocation) {
  formatAndAssert("chromebook-loc123", "chromebook-${LOCATION}", "asset123",
                  "SER1AL123", "0000deadbeef", "chrome_machine", "loc123");
}

TEST_F(HostnameHandlerTest, MixedSubstitution) {
  formatAndAssert(
      "chromebook-deadbeef-SER1AL123-asset123-chrome_machine-loc123",
      "chromebook-${MAC_ADDR}-${SERIAL_NUM}-${ASSET_ID}-${MACHINE_NAME}-"
      "${LOCATION}",
      "asset123", "SER1AL123", "deadbeef", "chrome_machine", "loc123");
}

TEST_F(HostnameHandlerTest, SubstituteInvalidSerial) {
  formatAndAssert("", "chromebook-${SERIAL_NUM}", "asset123", "Serial number",
                  "0000deadbeef", "chrome_machine", "loc123");
}

TEST_F(HostnameHandlerTest, IncorrectTemplateVariable) {
  formatAndAssert("", "chromebook-${SERIAL_NUMBER}", "asset123", "SERIAL123",
                  "0000deadbeef", "chrome_machine", "loc123");
}

TEST_F(HostnameHandlerTest, InvalidFirstCharacter) {
  formatAndAssert("", "-somename", "asset123", "Serial number", "0000deadbeef",
                  "chrome_machine", "loc123");
}

TEST_F(HostnameHandlerTest, HostnameTooLong) {
  formatAndAssert("", "${ASSET_ID}${ASSET_ID}${ASSET_ID}",
                  "1234567890123456789012345678901", "serial", "0000deadbeef",
                  "chrome_machine", "loc123");
}

TEST_F(HostnameHandlerTest, HostnameExactly63Chars) {
  formatAndAssert(
      "1234567890123456789012345678901-1234567890123456789012345678901",
      "${ASSET_ID}-${ASSET_ID}", "1234567890123456789012345678901", "serial",
      "0000deadbeef", "chrome_machine", "loc123");
}

}  // namespace policy
