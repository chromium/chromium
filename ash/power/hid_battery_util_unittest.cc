// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/power/hid_battery_util.h"

#include <string>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using HidBatteryUtilTest = testing::Test;

TEST_F(HidBatteryUtilTest, IsHIDBattery) {
  EXPECT_FALSE(IsHIDBattery(std::string()));
  EXPECT_FALSE(IsHIDBattery("invalid-path"));
  EXPECT_FALSE(IsHIDBattery("/sys/class/power_supply/hid-"));
  EXPECT_FALSE(IsHIDBattery("-battery"));
  EXPECT_FALSE(IsHIDBattery("/sys/class/power_supply/hid--battery"));

  EXPECT_TRUE(
      IsHIDBattery("/sys/class/power_supply/hid-A0:b1:C2:d3:E4:f5-battery"));
}

TEST_F(HidBatteryUtilTest, ExtractHIDIdentifier) {
  EXPECT_EQ(std::string(), ExtractHIDBatteryIdentifier("invalid-path"));
  EXPECT_EQ("A0:b1:C2:d3:E4:f5",
            ExtractHIDBatteryIdentifier(
                "/sys/class/power_supply/hid-A0:b1:C2:d3:E4:f5-battery"));
}

TEST_F(HidBatteryUtilTest, ExtractBluetoothAddressFromHIDBatteryPath) {
  EXPECT_EQ(std::string(),
            ExtractBluetoothAddressFromHIDBatteryPath("invalid-path"));

  // 3 characters at the end of the address, "f55".
  EXPECT_EQ(std::string(),
            ExtractBluetoothAddressFromHIDBatteryPath(
                "/sys/class/power_supply/hid-A0:b1:C2:d3:E4:f55-battery"));

  // 3 characters at the start of the address, "A00".
  EXPECT_EQ(std::string(),
            ExtractBluetoothAddressFromHIDBatteryPath(
                "/sys/class/power_supply/hid-A00:b1:C2:d3:E4:f5-battery"));

  EXPECT_EQ("f5:e4:d3:c2:b1:a0",
            ExtractBluetoothAddressFromHIDBatteryPath(
                "/sys/class/power_supply/hid-A0:b1:C2:d3:E4:f5-battery"));
}

}  // namespace ash
