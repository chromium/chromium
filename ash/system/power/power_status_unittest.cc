// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_status.h"

#include <memory>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

using power_manager::PowerSupplyProperties;

namespace ash {
namespace {

class TestObserver : public PowerStatus::Observer {
 public:
  TestObserver() : power_changed_count_(0) {}
  ~TestObserver() override = default;

  int power_changed_count() const { return power_changed_count_; }

  // PowerStatus::Observer overrides:
  void OnPowerStatusChanged() override { ++power_changed_count_; }

 private:
  int power_changed_count_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class PowerStatusTest : public AshTestBase {
 public:
  PowerStatusTest() = default;
  ~PowerStatusTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    power_status_ = PowerStatus::Get();
    test_observer_.reset(new TestObserver);
    power_status_->AddObserver(test_observer_.get());
  }

  void TearDown() override {
    power_status_->RemoveObserver(test_observer_.get());
    test_observer_.reset();
    AshTestBase::TearDown();
  }

 protected:
  PowerStatus* power_status_ = nullptr;  // Not owned.
  std::unique_ptr<TestObserver> test_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerStatusTest);
};

TEST_F(PowerStatusTest, InitializeAndUpdate) {
  // Test that the initial power supply state should be acquired after
  // PowerStatus is instantiated. This depends on
  // PowerManagerClientStubImpl, which responds to power status update
  // requests, pretends there is a battery present, and generates some valid
  // power supply status data.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, test_observer_->power_changed_count());

  // Test RequestUpdate, test_obsever_ should be notified for power suuply
  // status change.
  power_status_->RequestStatusUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, test_observer_->power_changed_count());
}

TEST_F(PowerStatusTest, GetBatteryImageInfo) {
  PowerSupplyProperties prop;
  prop.set_external_power(PowerSupplyProperties::AC);
  prop.set_battery_state(PowerSupplyProperties::CHARGING);
  prop.set_battery_percent(98.0);
  power_status_->SetProtoForTesting(prop);
  const PowerStatus::BatteryImageInfo info_charging_98 =
      power_status_->GetBatteryImageInfo();

  // 99% should use the same icon as 98%.
  prop.set_battery_percent(99.0);
  power_status_->SetProtoForTesting(prop);
  EXPECT_TRUE(info_charging_98.ApproximatelyEqual(
      power_status_->GetBatteryImageInfo()));

  // A different icon should be used when the battery is full.
  prop.set_battery_state(PowerSupplyProperties::FULL);
  prop.set_battery_percent(100.0);
  power_status_->SetProtoForTesting(prop);
  EXPECT_FALSE(info_charging_98.ApproximatelyEqual(
      power_status_->GetBatteryImageInfo()));

  // A much-lower battery level should use a different icon.
  prop.set_battery_state(PowerSupplyProperties::CHARGING);
  prop.set_battery_percent(20.0);
  power_status_->SetProtoForTesting(prop);
  EXPECT_FALSE(info_charging_98.ApproximatelyEqual(
      power_status_->GetBatteryImageInfo()));

  // Ditto for 98%, but on USB instead of AC.
  prop.set_external_power(PowerSupplyProperties::USB);
  prop.set_battery_percent(98.0);
  power_status_->SetProtoForTesting(prop);
  EXPECT_FALSE(info_charging_98.ApproximatelyEqual(
      power_status_->GetBatteryImageInfo()));
}

// Tests that the |icon_badge| member of BatteryImageInfo is set correctly
// with various power supply property values.
TEST_F(PowerStatusTest, BatteryImageInfoIconBadge) {
  PowerSupplyProperties prop;

  // A charging battery connected to AC power should have a bolt badge.
  prop.set_external_power(PowerSupplyProperties::AC);
  prop.set_battery_state(PowerSupplyProperties::CHARGING);
  prop.set_battery_percent(98.0);
  power_status_->SetProtoForTesting(prop);
  const gfx::VectorIcon* bolt_icon =
      power_status_->GetBatteryImageInfo().icon_badge;
  EXPECT_TRUE(bolt_icon);

  // A discharging battery connected to AC should also have a bolt badge.
  prop.set_battery_state(PowerSupplyProperties::DISCHARGING);
  power_status_->SetProtoForTesting(prop);
  EXPECT_EQ(bolt_icon, power_status_->GetBatteryImageInfo().icon_badge);

  // A charging battery connected to USB power should have an
  // unreliable badge.
  prop.set_external_power(PowerSupplyProperties::USB);
  prop.set_battery_state(PowerSupplyProperties::CHARGING);
  power_status_->SetProtoForTesting(prop);
  const gfx::VectorIcon* unreliable_icon =
      power_status_->GetBatteryImageInfo().icon_badge;
  EXPECT_NE(unreliable_icon, bolt_icon);
  EXPECT_TRUE(unreliable_icon);

  // A discharging battery connected to USB power should also have an
  // unreliable badge.
  prop.set_battery_state(PowerSupplyProperties::DISCHARGING);
  power_status_->SetProtoForTesting(prop);
  EXPECT_EQ(unreliable_icon, power_status_->GetBatteryImageInfo().icon_badge);

  // Show the right icon when no battery is present.
  prop.set_external_power(PowerSupplyProperties::DISCONNECTED);
  prop.set_battery_state(PowerSupplyProperties::NOT_PRESENT);
  power_status_->SetProtoForTesting(prop);
  const gfx::VectorIcon* x_icon =
      power_status_->GetBatteryImageInfo().icon_badge;
  EXPECT_TRUE(x_icon);
  EXPECT_NE(bolt_icon, x_icon);
  EXPECT_NE(unreliable_icon, x_icon);

  // Do not show a badge when the battery is discharging.
  prop.set_battery_state(PowerSupplyProperties::DISCHARGING);
  power_status_->SetProtoForTesting(prop);
  EXPECT_FALSE(power_status_->GetBatteryImageInfo().icon_badge);

  // Show the right icon for a discharging battery when it falls below
  // a charge level of PowerStatus::kCriticalBatteryChargePercentage.
  prop.set_battery_percent(PowerStatus::kCriticalBatteryChargePercentage);
  power_status_->SetProtoForTesting(prop);
  EXPECT_FALSE(power_status_->GetBatteryImageInfo().icon_badge);
  prop.set_battery_percent(PowerStatus::kCriticalBatteryChargePercentage - 1);
  power_status_->SetProtoForTesting(prop);
  const gfx::VectorIcon* alert_icon =
      power_status_->GetBatteryImageInfo().icon_badge;
  EXPECT_TRUE(alert_icon);
  EXPECT_NE(bolt_icon, alert_icon);
  EXPECT_NE(unreliable_icon, alert_icon);
  EXPECT_NE(x_icon, alert_icon);
}

// Tests that the battery image changes appropriately with various power supply
// property values.
TEST_F(PowerStatusTest, BatteryImageInfoChargeLevel) {
  PowerSupplyProperties prop;

  // No charge level is drawn when the battery is not present.
  prop.set_external_power(PowerSupplyProperties::DISCONNECTED);
  prop.set_battery_state(PowerSupplyProperties::NOT_PRESENT);
  power_status_->SetProtoForTesting(prop);
  EXPECT_EQ(0, power_status_->GetBatteryImageInfo().charge_percent);

  PowerStatus* power_status = power_status_;
  auto get_battery_image = [&power_status]() {
    return gfx::Image(
        power_status->GetBatteryImage(power_status->GetBatteryImageInfo(), 16,
                                      SK_ColorYELLOW, SK_ColorGREEN));
  };

  prop.set_external_power(PowerSupplyProperties::AC);
  prop.set_battery_state(PowerSupplyProperties::CHARGING);
  prop.set_battery_percent(0.0);
  EXPECT_EQ(0, power_status_->GetBatteryImageInfo().charge_percent);
  gfx::Image empty_image = get_battery_image();

  // 16% and 17% look different (assuming a height of 16, i.e. kTrayIconSize).
  prop.set_battery_percent(16.0);
  power_status_->SetProtoForTesting(prop);
  EXPECT_EQ(16, power_status_->GetBatteryImageInfo().charge_percent);
  gfx::Image image_16 = get_battery_image();
  EXPECT_FALSE(gfx::test::AreImagesEqual(empty_image, image_16));
  prop.set_battery_percent(17.0);
  power_status_->SetProtoForTesting(prop);
  EXPECT_EQ(17, power_status_->GetBatteryImageInfo().charge_percent);
  gfx::Image image_17 = get_battery_image();
  EXPECT_FALSE(gfx::test::AreImagesEqual(image_16, image_17));

  // 99% and 100% look different.
  prop.set_battery_percent(99.0);
  power_status_->SetProtoForTesting(prop);
  EXPECT_EQ(99, power_status_->GetBatteryImageInfo().charge_percent);
  gfx::Image image_99 = get_battery_image();
  prop.set_battery_percent(100.0);
  power_status_->SetProtoForTesting(prop);
  EXPECT_EQ(100, power_status_->GetBatteryImageInfo().charge_percent);
  gfx::Image image_100 = get_battery_image();
  EXPECT_FALSE(gfx::test::AreImagesEqual(image_99, image_100));
}

// Tests that positive time-to-full and time-to-empty estimates are honored.
TEST_F(PowerStatusTest, PositiveBatteryTimeEstimates) {
  constexpr auto kTime = base::TimeDelta::FromSeconds(120);

  PowerSupplyProperties prop;
  prop.set_external_power(PowerSupplyProperties::AC);
  prop.set_battery_state(PowerSupplyProperties::CHARGING);
  prop.set_battery_time_to_full_sec(kTime.InSeconds());
  power_status_->SetProtoForTesting(prop);
  base::Optional<base::TimeDelta> time = power_status_->GetBatteryTimeToFull();
  ASSERT_TRUE(time);
  EXPECT_EQ(kTime, *time);

  prop.Clear();
  prop.set_external_power(PowerSupplyProperties::DISCONNECTED);
  prop.set_battery_state(PowerSupplyProperties::DISCHARGING);
  prop.set_battery_time_to_empty_sec(kTime.InSeconds());
  power_status_->SetProtoForTesting(prop);
  time = power_status_->GetBatteryTimeToEmpty();
  ASSERT_TRUE(time);
  EXPECT_EQ(kTime, *time);
}

// Tests that missing time-to-full and time-to-empty estimates (which powerd
// sends when no battery is present) and negative ones (which powerd sends when
// the battery current is close to zero) are disregarded:
// https://crbug.com/930358
TEST_F(PowerStatusTest, MissingBatteryTimeEstimates) {
  // No battery.
  PowerSupplyProperties prop;
  prop.set_external_power(PowerSupplyProperties::AC);
  prop.set_battery_state(PowerSupplyProperties::NOT_PRESENT);
  power_status_->SetProtoForTesting(prop);
  base::Optional<base::TimeDelta> time = power_status_->GetBatteryTimeToFull();
  EXPECT_FALSE(time) << *time << " returned despite missing battery";
  time = power_status_->GetBatteryTimeToEmpty();
  EXPECT_FALSE(time) << *time << " returned despite missing battery";

  // Battery is charging, but negative estimate provided.
  prop.set_battery_state(PowerSupplyProperties::CHARGING);
  prop.set_battery_time_to_full_sec(-1);
  power_status_->SetProtoForTesting(prop);
  time = power_status_->GetBatteryTimeToFull();
  EXPECT_FALSE(time) << *time << " returned despite negative estimate";

  // Battery is discharging, but negative estimate provided.
  prop.Clear();
  prop.set_external_power(PowerSupplyProperties::DISCONNECTED);
  prop.set_battery_state(PowerSupplyProperties::DISCHARGING);
  prop.set_battery_time_to_empty_sec(-1);
  power_status_->SetProtoForTesting(prop);
  time = power_status_->GetBatteryTimeToEmpty();
  EXPECT_FALSE(time) << *time << " returned despite negative estimate";
}

}  // namespace ash
