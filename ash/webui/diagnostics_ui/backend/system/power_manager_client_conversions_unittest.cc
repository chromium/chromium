// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/system/power_manager_client_conversions.h"

#include "ash/webui/diagnostics_ui/mojom/system_data_provider.mojom.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {
namespace {

// One of |time_to_full| or |time_to_empty| must be 0. The other can be either
// -1 to signify that the time is being calculated or a positive number to
// represent the number of seconds until empty or full.
power_manager::PowerSupplyProperties ConstructPowerSupplyProperties(
    bool is_calculating_battery_time,
    int64_t time_to_full,
    int64_t time_to_empty) {
  power_manager::PowerSupplyProperties props;
  props.set_is_calculating_battery_time(is_calculating_battery_time);
  props.set_battery_time_to_full_sec(time_to_full);
  props.set_battery_time_to_empty_sec(time_to_empty);
  return props;
}

}  // namespace

class PowerManagerClientConversionsTest : public testing::Test {
 public:
  PowerManagerClientConversionsTest() = default;
  ~PowerManagerClientConversionsTest() override = default;
};

TEST_F(PowerManagerClientConversionsTest, BatteryState) {
  EXPECT_EQ(mojom::BatteryState::kCharging,
            ConvertBatteryStateFromProto(
                power_manager::PowerSupplyProperties_BatteryState_CHARGING));
  EXPECT_EQ(mojom::BatteryState::kDischarging,
            ConvertBatteryStateFromProto(
                power_manager::PowerSupplyProperties_BatteryState_DISCHARGING));
  EXPECT_EQ(mojom::BatteryState::kFull,
            ConvertBatteryStateFromProto(
                power_manager::PowerSupplyProperties_BatteryState_FULL));
}

TEST_F(PowerManagerClientConversionsTest, PowerSource) {
  EXPECT_EQ(mojom::ExternalPowerSource::kAc,
            ConvertPowerSourceFromProto(
                power_manager::PowerSupplyProperties_ExternalPower_AC));
  EXPECT_EQ(mojom::ExternalPowerSource::kUsb,
            ConvertPowerSourceFromProto(
                power_manager::PowerSupplyProperties_ExternalPower_USB));
  EXPECT_EQ(
      mojom::ExternalPowerSource::kDisconnected,
      ConvertPowerSourceFromProto(
          power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED));
}

TEST_F(PowerManagerClientConversionsTest, PowerTime) {
  // Full BatteryState returns an empty string
  auto props = ConstructPowerSupplyProperties(false, 125, 0);
  EXPECT_EQ(std::u16string(),
            ConstructPowerTime(mojom::BatteryState::kFull, props));

  // If the battery is charging but is_calculating_battery_time is true, expect
  // an empty string.
  props = ConstructPowerSupplyProperties(true, 125, 0);
  EXPECT_EQ(std::u16string(),
            ConstructPowerTime(mojom::BatteryState::kCharging, props));

  // If the battery is discharging but is_calculating_battery_time is true,
  // expect an empty string.
  props = ConstructPowerSupplyProperties(true, 0, 125);
  EXPECT_EQ(std::u16string(),
            ConstructPowerTime(mojom::BatteryState::kDischarging, props));

  // If the battery is charging but time_to_full is -1, expect an empty string.
  props = ConstructPowerSupplyProperties(false, -1, 0);
  EXPECT_EQ(std::u16string(),
            ConstructPowerTime(mojom::BatteryState::kCharging, props));

  // If the battery is discharging but time_to_empty is -1, expect an empty
  // string.
  props = ConstructPowerSupplyProperties(false, 0, -1);
  EXPECT_EQ(std::u16string(),
            ConstructPowerTime(mojom::BatteryState::kDischarging, props));

  // Battery charging with 11220 seconds (3h 7m) remaining.
  props = ConstructPowerSupplyProperties(false, 11220, 0);
  EXPECT_EQ(u"3 hours and 7 minutes",
            ConstructPowerTime(mojom::BatteryState::kCharging, props));

  // Battery discharging with 10380 seconds (2h 53m) remaining.
  props = ConstructPowerSupplyProperties(false, 0, 10380);
  EXPECT_EQ(u"2 hours and 53 minutes",
            ConstructPowerTime(mojom::BatteryState::kDischarging, props));
}

}  // namespace diagnostics
}  // namespace ash
