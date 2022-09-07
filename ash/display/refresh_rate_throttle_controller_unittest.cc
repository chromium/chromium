// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/refresh_rate_throttle_controller.h"

#include <memory>
#include <vector>

#include "ash/shell.h"
#include "ash/system/power/power_status.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "ui/display/fake/fake_display_snapshot.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/test/action_logger.h"
#include "ui/display/manager/test/action_logger_util.h"
#include "ui/display/manager/test/test_native_display_delegate.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"

namespace ash {
namespace {

using display::DisplayMode;
using display::DisplaySnapshot;
using display::FakeDisplaySnapshot;
using display::NativeDisplayDelegate;
using display::test::ActionLogger;
using display::test::TestNativeDisplayDelegate;
using power_manager::PowerSupplyProperties;

std::unique_ptr<DisplayMode> MakeDisplayMode(int width,
                                             int height,
                                             bool is_interlaced,
                                             float refresh_rate) {
  return std::make_unique<DisplayMode>(gfx::Size(width, height), is_interlaced,
                                       refresh_rate);
}

std::unique_ptr<DisplaySnapshot> BuildDualRefreshPanelSnapshot(
    int64_t id,
    display::DisplayConnectionType type) {
  return FakeDisplaySnapshot::Builder()
      .SetId(id)
      .SetType(type)
      .SetNativeMode(MakeDisplayMode(1920, 1200, false, 120.f))
      .AddMode(MakeDisplayMode(1920, 1200, false, 60.f))
      .SetCurrentMode(MakeDisplayMode(1920, 1200, false, 120.f))
      .Build();
}

PowerSupplyProperties BuildFakePowerSupplyProperties(
    PowerSupplyProperties::ExternalPower charger_state,
    double battery_percent) {
  PowerSupplyProperties fake_power;
  fake_power.set_external_power(charger_state);
  fake_power.set_battery_percent(battery_percent);
  return fake_power;
}

class RefreshRateThrottleControllerTest : public AshTestBase {
 public:
  RefreshRateThrottleControllerTest() = default;
  RefreshRateThrottleControllerTest(const RefreshRateThrottleControllerTest&) =
      delete;
  RefreshRateThrottleControllerTest& operator=(
      const RefreshRateThrottleControllerTest&) = delete;
  ~RefreshRateThrottleControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    logger_ = std::make_unique<ActionLogger>();
    native_display_delegate_ = new TestNativeDisplayDelegate(logger_.get());
    display_manager()->configurator()->SetDelegateForTesting(
        std::unique_ptr<NativeDisplayDelegate>(native_display_delegate_));
    controller_ = std::make_unique<RefreshRateThrottleController>(
        Shell::Get()->display_configurator(), PowerStatus::Get());
  }

  void TearDown() override {
    controller_.reset();
    AshTestBase::TearDown();
  }

 protected:
  void SetUpDisplays(
      const std::vector<std::unique_ptr<DisplaySnapshot>>& snapshots) {
    std::vector<DisplaySnapshot*> outputs;
    for (const std::unique_ptr<DisplaySnapshot>& snapshot : snapshots)
      outputs.push_back(snapshot.get());

    display::DisplayConfigurator::TestApi test_api(
        display_manager()->configurator());
    native_display_delegate_->set_outputs(outputs);
    display_manager()->configurator()->OnConfigurationChanged();
    display_manager()->configurator()->ForceInitialConfigure();
    ASSERT_TRUE(test_api.TriggerConfigureTimeout());
  }

  const DisplaySnapshot* GetDisplaySnapshot(int64_t display_id) {
    for (const DisplaySnapshot* snapshot :
         display_manager()->configurator()->cached_displays()) {
      if (snapshot->display_id() == display_id)
        return snapshot;
    }
    return nullptr;
  }

  std::unique_ptr<ActionLogger> logger_;
  std::unique_ptr<RefreshRateThrottleController> controller_;
  // Owned by DisplayConfigurator.
  TestNativeDisplayDelegate* native_display_delegate_;
};

TEST_F(RefreshRateThrottleControllerTest, ShouldNotThrottleOnAC) {
  constexpr int64_t kDisplayId = 12345;
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      kDisplayId, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  SetUpDisplays(snapshots);

  // Expect the initial state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  // Set power state to indicate the device is on AC.
  PowerStatus::Get()->SetProtoForTesting(
      BuildFakePowerSupplyProperties(PowerSupplyProperties::AC, 100.0));
  controller_->OnPowerStatusChanged();

  // Expect the new state to be unchanged.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }
}

TEST_F(RefreshRateThrottleControllerTest, ShouldThrottleOnBattery) {
  constexpr int64_t kDisplayId = 12345;
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      kDisplayId, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  SetUpDisplays(snapshots);

  // Expect the initial state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  // Set power state to indicate the device is on battery.
  PowerStatus::Get()->SetProtoForTesting(BuildFakePowerSupplyProperties(
      PowerSupplyProperties::DISCONNECTED, 100.0));
  controller_->OnPowerStatusChanged();

  // Expect the new state to be 60 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 60.f);
  }
}

TEST_F(RefreshRateThrottleControllerTest, ShouldNotAffectExternalDisplay) {
  constexpr int64_t kDisplayId = 12345;
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      kDisplayId, display::DISPLAY_CONNECTION_TYPE_HDMI));
  SetUpDisplays(snapshots);

  // Expect the initial state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  // Set power state to indicate the device is on battery.
  PowerStatus::Get()->SetProtoForTesting(BuildFakePowerSupplyProperties(
      PowerSupplyProperties::DISCONNECTED, 100.0));
  controller_->OnPowerStatusChanged();

  // Expect the state to be unchanged.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }
}

TEST_F(RefreshRateThrottleControllerTest, ShouldThrottleOnUSBCharger) {
  constexpr int64_t kDisplayId = 12345;
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      kDisplayId, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  SetUpDisplays(snapshots);

  // Expect the initial state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  // Set power state to indicate the device is on a low powered charger.
  PowerStatus::Get()->SetProtoForTesting(
      BuildFakePowerSupplyProperties(PowerSupplyProperties::USB, 100.0));
  controller_->OnPowerStatusChanged();

  // Expect the new state to be 60 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 60.f);
  }
}

TEST_F(RefreshRateThrottleControllerTest, ShouldThrottleOnLowBattery) {
  constexpr int64_t kDisplayId = 12345;
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      kDisplayId, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  SetUpDisplays(snapshots);

  // Expect the initial state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  // Set power state to indicate the device is on a high-powered charger,
  // but the battery is critically low.
  PowerStatus::Get()->SetProtoForTesting(
      BuildFakePowerSupplyProperties(PowerSupplyProperties::AC, 4.0));
  controller_->OnPowerStatusChanged();

  // Expect the new state to be 60 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 60.f);
  }

  // Set the power state to indicate the device has charged above
  // critical level.
  PowerStatus::Get()->SetProtoForTesting(
      BuildFakePowerSupplyProperties(PowerSupplyProperties::AC, 6.0));
  controller_->OnPowerStatusChanged();

  // Expect the new state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }
}

}  // namespace
}  // namespace ash