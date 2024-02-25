// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/battery_level_provider.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class FakeBatteryLevelProvider : public BatteryLevelProvider {
 public:
  using BatteryDetails = BatteryLevelProvider::BatteryDetails;

  FakeBatteryLevelProvider() = default;

  static BatteryState MakeBatteryState(
      const std::vector<BatteryDetails>& battery_details) {
    return BatteryLevelProvider::MakeBatteryState(battery_details);
  }
};

}  // namespace

using BatteryDetails = FakeBatteryLevelProvider::BatteryDetails;

TEST(BatteryLevelProviderTest, NoBattery) {
  auto state = FakeBatteryLevelProvider::MakeBatteryState({});
  EXPECT_EQ(0, state.battery_count);
  EXPECT_TRUE(state.is_external_power_connected);
  EXPECT_FALSE(state.current_capacity.has_value());
  EXPECT_FALSE(state.full_charged_capacity.has_value());
  EXPECT_NE(base::TimeTicks(), state.capture_time);
}

TEST(BatteryLevelProviderTest, SingleBatteryWithExternalPower) {
  auto state = FakeBatteryLevelProvider::MakeBatteryState(
      {BatteryDetails({.is_external_power_connected = true,
                       .current_capacity = 42,
                       .full_charged_capacity = 100})});
  EXPECT_EQ(1, state.battery_count);
  EXPECT_TRUE(state.is_external_power_connected);
  EXPECT_EQ(42U, state.current_capacity);
  EXPECT_EQ(100U, state.full_charged_capacity);
  EXPECT_NE(base::TimeTicks(), state.capture_time);
}

TEST(BatteryLevelProviderTest, SingleBatteryDischarging) {
  auto state = FakeBatteryLevelProvider::MakeBatteryState(
      {BatteryDetails({.is_external_power_connected = false,
                       .current_capacity = 42,
                       .full_charged_capacity = 100})});
  EXPECT_EQ(1, state.battery_count);
  EXPECT_FALSE(state.is_external_power_connected);
  EXPECT_EQ(42U, state.current_capacity);
  EXPECT_EQ(100U, state.full_charged_capacity);
  EXPECT_NE(base::TimeTicks(), state.capture_time);
}

TEST(BatteryLevelProviderTest, MultipleBatteriesWithExternalPower) {
  auto state = FakeBatteryLevelProvider::MakeBatteryState(
      {BatteryDetails({.is_external_power_connected = false,
                       .current_capacity = 42,
                       .full_charged_capacity = 100}),
       BatteryDetails({.is_external_power_connected = true,
                       .current_capacity = 10,
                       .full_charged_capacity = 100})});
  EXPECT_EQ(2, state.battery_count);
  EXPECT_TRUE(state.is_external_power_connected);
  EXPECT_EQ(std::nullopt, state.current_capacity);
  EXPECT_EQ(std::nullopt, state.full_charged_capacity);
  EXPECT_NE(base::TimeTicks(), state.capture_time);
}

TEST(BatteryLevelProviderTest, MultipleBatteriesDischarging) {
  auto state = FakeBatteryLevelProvider::MakeBatteryState(
      {BatteryDetails({.is_external_power_connected = false,
                       .current_capacity = 42,
                       .full_charged_capacity = 100}),
       BatteryDetails({.is_external_power_connected = false,
                       .current_capacity = 10,
                       .full_charged_capacity = 100})});
  EXPECT_EQ(2, state.battery_count);
  EXPECT_FALSE(state.is_external_power_connected);
  EXPECT_EQ(std::nullopt, state.current_capacity);
  EXPECT_EQ(std::nullopt, state.full_charged_capacity);
  EXPECT_NE(base::TimeTicks(), state.capture_time);
}

TEST(BatteryLevelProviderTest, SingleBatteryMAh) {
  auto state = FakeBatteryLevelProvider::MakeBatteryState({BatteryDetails(
      {.is_external_power_connected = false,
       .current_capacity = 42,
       .full_charged_capacity = 100,
       .voltage_mv = 12,
       .charge_unit = BatteryLevelProvider::BatteryLevelUnit::kMAh})});
  EXPECT_EQ(1, state.battery_count);
  EXPECT_FALSE(state.is_external_power_connected);
  EXPECT_EQ(42U, state.current_capacity);
  EXPECT_EQ(100U, state.full_charged_capacity);
  EXPECT_TRUE(state.voltage_mv.has_value());
  EXPECT_EQ(12U, *state.voltage_mv);
  EXPECT_EQ(BatteryLevelProvider::BatteryLevelUnit::kMAh, state.charge_unit);
  EXPECT_NE(base::TimeTicks(), state.capture_time);
}

}  // namespace base
