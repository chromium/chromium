// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/fake_power_status.h"

#include "ash/test/ash_test_base.h"

namespace ash {

using FakePowerStatusTest = AshTestBase;

TEST_F(FakePowerStatusTest, UsbCharging) {
  // The default value of whether the USB Charger is connected should be false.
  FakePowerStatus fake_instance;
  EXPECT_FALSE(fake_instance.IsUsbChargerConnected());

  fake_instance.SetIsUsbChargerConnected(true);

  // After setting to true, IsUsbChargerConnected() should return true.
  EXPECT_TRUE(fake_instance.IsUsbChargerConnected());
}

TEST_F(FakePowerStatusTest, BatteryPresent) {
  // The default value of whether the battery is present should be true.
  FakePowerStatus fake_instance;
  EXPECT_TRUE(fake_instance.IsBatteryPresent());

  fake_instance.SetIsBatteryPresent(false);

  // After setting to false, IsBatteryPresent() should return false.
  EXPECT_FALSE(fake_instance.IsBatteryPresent());
}

TEST_F(FakePowerStatusTest, BatterySaverActive) {
  // The default value of whether the battery saver mode is active should be
  // false.
  FakePowerStatus fake_instance;
  EXPECT_FALSE(fake_instance.IsBatterySaverActive());

  fake_instance.SetIsBatterySaverActive(true);

  // After setting to true, IsBatterySaverActive() should return true.
  EXPECT_TRUE(fake_instance.IsBatterySaverActive());
}

TEST_F(FakePowerStatusTest, LinePowerConnected) {
  // The default value of whether the line power is connected should be false.
  FakePowerStatus fake_instance;
  EXPECT_FALSE(fake_instance.IsLinePowerConnected());

  fake_instance.SetIsLinePowerConnected(true);

  // After setting to true, IsLinePowerConnected() should return true.
  EXPECT_TRUE(fake_instance.IsLinePowerConnected());
}

TEST_F(FakePowerStatusTest, BatteryPercentage) {
  // The default battery percentage should be 50.0.
  FakePowerStatus fake_instance;
  EXPECT_EQ(fake_instance.GetBatteryPercent(), 50.0);

  fake_instance.SetBatteryPercent(75.0);

  // After setting to 75.0, GetBatteryPercent() should return 75.0.
  EXPECT_EQ(fake_instance.GetBatteryPercent(), 75.0);
}

TEST_F(FakePowerStatusTest, DefaultState) {
  // Set non-default states.
  FakePowerStatus fake_instance;
  fake_instance.SetIsBatteryPresent(false);
  fake_instance.SetIsBatterySaverActive(true);
  fake_instance.SetIsLinePowerConnected(true);
  fake_instance.SetIsUsbChargerConnected(true);

  // Reset to default state.
  fake_instance.SetDefaultState();

  // Check that all values are back to their default states.
  EXPECT_TRUE(fake_instance.IsBatteryPresent());
  EXPECT_FALSE(fake_instance.IsBatterySaverActive());
  EXPECT_FALSE(fake_instance.IsLinePowerConnected());
  EXPECT_FALSE(fake_instance.IsUsbChargerConnected());
}

}  // namespace ash
