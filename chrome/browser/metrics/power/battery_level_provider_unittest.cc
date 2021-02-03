// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/battery_level_provider.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeBatteryLevelProvider : public BatteryLevelProvider {
 public:
  using BatteryInterface = BatteryLevelProvider::BatteryInterface;

  explicit FakeBatteryLevelProvider(std::vector<BatteryInterface> details)
      : details_(std::move(details)) {}

 private:
  std::vector<BatteryInterface> GetBatteryInterfaceList() override {
    return details_;
  }

  std::vector<BatteryInterface> details_;
};

}  // namespace

using BatteryInterface = FakeBatteryLevelProvider::BatteryInterface;

TEST(BatteryLevelProviderTest, NoInterface) {
  FakeBatteryLevelProvider provider({});
  auto state = provider.GetBatteryState();
  EXPECT_EQ(0U, state.interface_count);
  EXPECT_EQ(0U, state.battery_count);
  EXPECT_FALSE(state.charge_level.has_value());
  EXPECT_FALSE(state.on_battery);
}

TEST(BatteryLevelProviderTest, NoBattery) {
  FakeBatteryLevelProvider provider({BatteryInterface(false)});
  auto state = provider.GetBatteryState();
  EXPECT_EQ(1U, state.interface_count);
  EXPECT_EQ(0U, state.battery_count);
  EXPECT_FALSE(state.charge_level.has_value());
  EXPECT_FALSE(state.on_battery);
}

TEST(BatteryLevelProviderTest, PluggedBattery) {
  FakeBatteryLevelProvider provider({BatteryInterface({true, 42, 100})});
  auto state = provider.GetBatteryState();
  EXPECT_EQ(1U, state.interface_count);
  EXPECT_EQ(1U, state.battery_count);
  ASSERT_TRUE(state.charge_level.has_value());
  EXPECT_EQ(0.42, *state.charge_level);
  EXPECT_FALSE(state.on_battery);
}

TEST(BatteryLevelProviderTest, DischargingBattery) {
  FakeBatteryLevelProvider provider({BatteryInterface({false, 42, 100})});
  auto state = provider.GetBatteryState();
  EXPECT_EQ(1U, state.interface_count);
  EXPECT_EQ(1U, state.battery_count);
  ASSERT_TRUE(state.charge_level.has_value());
  EXPECT_EQ(0.42, *state.charge_level);
  EXPECT_TRUE(state.on_battery);
}

TEST(BatteryLevelProviderTest, InvalidBattery) {
  FakeBatteryLevelProvider provider({BatteryInterface(true)});
  auto state = provider.GetBatteryState();
  EXPECT_EQ(1U, state.interface_count);
  EXPECT_EQ(1U, state.battery_count);
  EXPECT_FALSE(state.charge_level.has_value());
  EXPECT_FALSE(state.on_battery);
}

TEST(BatteryLevelProviderTest, MultipleInterfaces) {
  FakeBatteryLevelProvider provider(
      {BatteryInterface(false), BatteryInterface({false, 42, 100})});
  auto state = provider.GetBatteryState();
  EXPECT_EQ(2U, state.interface_count);
  EXPECT_EQ(1U, state.battery_count);
  ASSERT_TRUE(state.charge_level.has_value());
  EXPECT_EQ(0.42, *state.charge_level);
  EXPECT_TRUE(state.on_battery);
}

TEST(BatteryLevelProviderTest, MultipleBatteries) {
  FakeBatteryLevelProvider provider(
      {BatteryInterface({true, 10, 100}), BatteryInterface({false, 30, 100})});
  auto state = provider.GetBatteryState();
  EXPECT_EQ(2U, state.interface_count);
  EXPECT_EQ(2U, state.battery_count);
  ASSERT_TRUE(state.charge_level.has_value());
  EXPECT_EQ(0.20, *state.charge_level);
  EXPECT_FALSE(state.on_battery);
}

TEST(BatteryLevelProviderTest, MultipleBatteriesDischarging) {
  FakeBatteryLevelProvider provider(
      {BatteryInterface({false, 10, 100}), BatteryInterface({false, 30, 100})});
  auto state = provider.GetBatteryState();
  EXPECT_EQ(2U, state.interface_count);
  EXPECT_EQ(2U, state.battery_count);
  ASSERT_TRUE(state.charge_level.has_value());
  EXPECT_EQ(0.20, *state.charge_level);
  EXPECT_TRUE(state.on_battery);
}

TEST(BatteryLevelProviderTest, MultipleBatteriesInvalid) {
  FakeBatteryLevelProvider provider(
      {BatteryInterface(true), BatteryInterface({false, 10, 100})});
  auto state = provider.GetBatteryState();
  EXPECT_EQ(2U, state.interface_count);
  EXPECT_EQ(2U, state.battery_count);
  EXPECT_FALSE(state.charge_level.has_value());
  EXPECT_FALSE(state.on_battery);
}
