// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/battery_level_provider.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeBatteryLevelProvider : public BatteryLevelProvider {
 public:
  using BatteryInterface = BatteryLevelProvider::BatteryInterface;

  FakeBatteryLevelProvider() = default;

  static BatteryState MakeBatteryState(
      const std::vector<BatteryInterface>& battery_interfaces) {
    return BatteryLevelProvider::MakeBatteryState(battery_interfaces);
  }
};

}  // namespace

using BatteryInterface = FakeBatteryLevelProvider::BatteryInterface;

TEST(BatteryLevelProviderTest, NoInterface) {
  auto state = FakeBatteryLevelProvider::MakeBatteryState({});
  EXPECT_EQ(0U, state.interface_count);
  EXPECT_EQ(0U, state.battery_count);
  EXPECT_FALSE(state.charge_level.has_value());
  EXPECT_FALSE(state.on_battery);
}

TEST(BatteryLevelProviderTest, NoBattery) {
  auto state =
      FakeBatteryLevelProvider::MakeBatteryState({BatteryInterface(false)});
  EXPECT_EQ(1U, state.interface_count);
  EXPECT_EQ(0U, state.battery_count);
  EXPECT_FALSE(state.charge_level.has_value());
  EXPECT_FALSE(state.on_battery);
}

TEST(BatteryLevelProviderTest, PluggedBattery) {
  auto state = FakeBatteryLevelProvider::MakeBatteryState(
      {BatteryInterface({true, 42, 100})});
  EXPECT_EQ(1U, state.interface_count);
  EXPECT_EQ(1U, state.battery_count);
  ASSERT_TRUE(state.charge_level.has_value());
  EXPECT_EQ(0.42, *state.charge_level);
  EXPECT_FALSE(state.on_battery);
}

TEST(BatteryLevelProviderTest, DischargingBattery) {
  auto state = FakeBatteryLevelProvider::MakeBatteryState(
      {BatteryInterface({false, 42, 100})});
  EXPECT_EQ(1U, state.interface_count);
  EXPECT_EQ(1U, state.battery_count);
  ASSERT_TRUE(state.charge_level.has_value());
  EXPECT_EQ(0.42, *state.charge_level);
  EXPECT_TRUE(state.on_battery);
}

TEST(BatteryLevelProviderTest, InvalidBattery) {
  auto state =
      FakeBatteryLevelProvider::MakeBatteryState({BatteryInterface(true)});
  EXPECT_EQ(1U, state.interface_count);
  EXPECT_EQ(1U, state.battery_count);
  EXPECT_FALSE(state.charge_level.has_value());
  EXPECT_FALSE(state.on_battery);
}

TEST(BatteryLevelProviderTest, MultipleInterfaces) {
  auto state = FakeBatteryLevelProvider::MakeBatteryState(
      {BatteryInterface(false), BatteryInterface({false, 42, 100})});
  EXPECT_EQ(2U, state.interface_count);
  EXPECT_EQ(1U, state.battery_count);
  ASSERT_TRUE(state.charge_level.has_value());
  EXPECT_EQ(0.42, *state.charge_level);
  EXPECT_TRUE(state.on_battery);
}

TEST(BatteryLevelProviderTest, MultipleBatteries) {
  auto state = FakeBatteryLevelProvider::MakeBatteryState(
      {BatteryInterface({true, 10, 100}), BatteryInterface({false, 30, 100})});
  EXPECT_EQ(2U, state.interface_count);
  EXPECT_EQ(2U, state.battery_count);
  ASSERT_TRUE(state.charge_level.has_value());
  EXPECT_EQ(0.20, *state.charge_level);
  EXPECT_FALSE(state.on_battery);
}

TEST(BatteryLevelProviderTest, MultipleBatteriesDischarging) {
  auto state = FakeBatteryLevelProvider::MakeBatteryState(
      {BatteryInterface({false, 10, 100}), BatteryInterface({false, 30, 100})});
  EXPECT_EQ(2U, state.interface_count);
  EXPECT_EQ(2U, state.battery_count);
  ASSERT_TRUE(state.charge_level.has_value());
  EXPECT_EQ(0.20, *state.charge_level);
  EXPECT_TRUE(state.on_battery);
}

TEST(BatteryLevelProviderTest, MultipleBatteriesInvalid) {
  auto state = FakeBatteryLevelProvider::MakeBatteryState(
      {BatteryInterface(true), BatteryInterface({false, 10, 100})});
  EXPECT_EQ(2U, state.interface_count);
  EXPECT_EQ(2U, state.battery_count);
  EXPECT_FALSE(state.charge_level.has_value());
  EXPECT_FALSE(state.on_battery);
}
