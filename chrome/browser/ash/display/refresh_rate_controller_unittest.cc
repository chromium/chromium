// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/display/refresh_rate_controller.h"

#include <memory>
#include <vector>

#include "ash/shell.h"
#include "ash/system/power/power_status.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/game_mode/game_mode_controller.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/test/action_logger.h"
#include "ui/display/manager/test/action_logger_util.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/manager/test/test_native_display_delegate.h"
#include "ui/display/manager/util/display_manager_test_util.h"
#include "ui/display/test/display_test_util.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"

namespace ash {
namespace {

using display::DisplayMode;
using display::DisplaySnapshot;
using display::FakeDisplaySnapshot;
using display::NativeDisplayDelegate;
using display::ScopedSetInternalDisplayIds;
using display::test::ActionLogger;
using display::test::TestNativeDisplayDelegate;
using game_mode::GameModeController;
using power_manager::PowerSupplyProperties;

using ModeState = DisplayPerformanceModeController::ModeState;
using DisplayStateList = display::DisplayConfigurator::DisplayStateList;
using GameMode = ResourcedClient::GameMode;

class MockNativeDisplayDelegate : public TestNativeDisplayDelegate {
 public:
  explicit MockNativeDisplayDelegate(ActionLogger* logger)
      : TestNativeDisplayDelegate(logger) {}
  MOCK_METHOD(void,
              GetSeamlessRefreshRates,
              (int64_t, display::GetSeamlessRefreshRatesCallback),
              (const override));
};

std::unique_ptr<DisplayMode> MakeDisplayMode(int width,
                                             int height,
                                             bool is_interlaced,
                                             float refresh_rate) {
  return display::CreateDisplayModePtrForTest({width, height}, is_interlaced,
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

std::unique_ptr<DisplaySnapshot> BuildVrrPanelSnapshot(
    int64_t id,
    display::DisplayConnectionType type) {
  return FakeDisplaySnapshot::Builder()
      .SetId(id)
      .SetType(type)
      .SetNativeMode(MakeDisplayMode(1920, 1200, false, 120.f))
      .SetCurrentMode(MakeDisplayMode(1920, 1200, false, 120.f))
      .SetVariableRefreshRateState(display::kVrrDisabled)
      .SetVsyncRateMin(48)
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

class RefreshRateControllerTest : public AshTestBase {
 public:
  RefreshRateControllerTest() {
    scoped_features_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kSeamlessRefreshRateSwitching,
                              ::features::kVariableRefreshRateAvailable,
                              ::features::kEnableVariableRefreshRate},
        /*disabled_features=*/{});
  }
  RefreshRateControllerTest(const RefreshRateControllerTest&) = delete;
  RefreshRateControllerTest& operator=(const RefreshRateControllerTest&) =
      delete;
  ~RefreshRateControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    logger_ = std::make_unique<ActionLogger>();
    native_display_delegate_ =
        new testing::NiceMock<MockNativeDisplayDelegate>(logger_.get());
    display_manager()->configurator()->SetDelegateForTesting(
        std::unique_ptr<NativeDisplayDelegate>(native_display_delegate_));
    game_mode_controller_ = std::make_unique<GameModeController>();
    performance_controller_ =
        Shell::Get()->display_performance_mode_controller();
    controller_ = std::make_unique<RefreshRateController>(
        Shell::Get()->display_configurator(), PowerStatus::Get(),
        game_mode_controller_.get(), performance_controller_.get());
  }

  void TearDown() override {
    controller_.reset();
    game_mode_controller_.reset();
    performance_controller_ = nullptr;
    AshTestBase::TearDown();
  }

 protected:
  void SetUpDisplays(std::vector<std::unique_ptr<DisplaySnapshot>> snapshots) {
    display::DisplayConfigurator::TestApi test_api(
        display_manager()->configurator());
    native_display_delegate_->SetOutputs(std::move(snapshots));
    display_manager()->configurator()->OnConfigurationChanged();
    display_manager()->configurator()->ForceInitialConfigure();
    ASSERT_TRUE(test_api.TriggerConfigureTimeout());
  }

  const DisplaySnapshot* GetDisplaySnapshot(int64_t display_id) {
    for (const DisplaySnapshot* snapshot :
         display_manager()->configurator()->cached_displays()) {
      if (snapshot->display_id() == display_id) {
        return snapshot;
      }
    }
    return nullptr;
  }

  std::unique_ptr<ActionLogger> logger_;
  std::unique_ptr<RefreshRateController> controller_;
  std::unique_ptr<GameModeController> game_mode_controller_;
  // Not owned.
  raw_ptr<DisplayPerformanceModeController> performance_controller_;
  // Owned by DisplayConfigurator.
  raw_ptr<MockNativeDisplayDelegate, DanglingUntriaged>
      native_display_delegate_;
  base::test::ScopedFeatureList scoped_features_;
};

TEST_F(RefreshRateControllerTest, ThrottleStateSetAtConstruction) {
  constexpr int64_t kDisplayId = 12345;
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      kDisplayId, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  ScopedSetInternalDisplayIds set_internal(kDisplayId);
  SetUpDisplays(std::move(snapshots));

  // Expect the initial state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  // Create a new RefreshRateController, and force throttle it.
  const bool force_throttle = true;
  std::unique_ptr<RefreshRateController> controller =
      std::make_unique<RefreshRateController>(
          Shell::Get()->display_configurator(), PowerStatus::Get(),
          game_mode_controller_.get(), performance_controller_, force_throttle);

  // Expect the state to be 60 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 60.f);
  }
}

TEST_F(RefreshRateControllerTest, ShouldNotThrottleOnAC) {
  constexpr int64_t kDisplayId = 12345;
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      kDisplayId, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  ScopedSetInternalDisplayIds set_internal(kDisplayId);
  SetUpDisplays(std::move(snapshots));

  // Expect the initial state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  // Set power state to indicate the device is on AC.
  PowerStatus::Get()->SetProtoForTesting(
      BuildFakePowerSupplyProperties(PowerSupplyProperties::AC, 100.f));
  controller_->OnPowerStatusChanged();

  // Expect the new state to be unchanged.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }
}

TEST_F(RefreshRateControllerTest, ShouldThrottleWithBatterySaverMode) {
  const int64_t display_id = GetPrimaryDisplay().id();
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      display_id, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  ScopedSetInternalDisplayIds set_internal(display_id);
  SetUpDisplays(std::move(snapshots));
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(GetPrimaryDisplay().work_area()));

  // Expect the initial state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(display_id);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  // Set power state to indicate the device is on AC, and
  // Battery Saver Mode is enabled.
  PowerStatus::Get()->SetProtoForTesting(
      BuildFakePowerSupplyProperties(PowerSupplyProperties::AC, 100.f));
  PowerStatus::Get()->SetBatterySaverStateForTesting(true);
  performance_controller_->OnPowerStatusChanged();

  // Expect the new state to be 60Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(display_id);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 60.f);
  }

  // Set the game mode to indicate the user is gaming.
  game_mode_controller_->NotifySetGameMode(GameMode::BOREALIS,
                                           ash::WindowState::Get(window.get()));

  // Expect the new state to still be 60Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(display_id);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 60.f);
  }

  game_mode_controller_->NotifySetGameMode(GameMode::OFF,
                                           ash::WindowState::Get(window.get()));
}

TEST_F(RefreshRateControllerTest, ShouldThrottleOnBattery) {
  constexpr int64_t kDisplayId = 12345;
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      kDisplayId, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  ScopedSetInternalDisplayIds set_internal(kDisplayId);
  SetUpDisplays(std::move(snapshots));

  // Expect the initial state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  // Set power state to indicate the device is on battery.
  PowerStatus::Get()->SetProtoForTesting(BuildFakePowerSupplyProperties(
      PowerSupplyProperties::DISCONNECTED, 80.0f));
  controller_->OnPowerStatusChanged();

  // Expect the new state to be 60 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 60.f);
  }
}

TEST_F(RefreshRateControllerTest,
       ShouldNotThrottleForBorealisOnInternalDisplay) {
  const int64_t display_id = GetPrimaryDisplay().id();
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      display_id, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  ScopedSetInternalDisplayIds set_internal(display_id);
  SetUpDisplays(std::move(snapshots));
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(GetPrimaryDisplay().work_area()));

  // Expect the initial state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(display_id);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  // Set power state to indicate the device is on battery.
  PowerStatus::Get()->SetProtoForTesting(BuildFakePowerSupplyProperties(
      PowerSupplyProperties::DISCONNECTED, 80.0f));
  controller_->OnPowerStatusChanged();

  // Expect the new state to be 60 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(display_id);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 60.f);
  }

  // Set the game mode to indicate the user is gaming.
  game_mode_controller_->NotifySetGameMode(GameMode::BOREALIS,
                                           ash::WindowState::Get(window.get()));

  // Expect the new state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(display_id);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  game_mode_controller_->NotifySetGameMode(GameMode::OFF,
                                           ash::WindowState::Get(window.get()));
}

TEST_F(RefreshRateControllerTest,
       ThrottlingUnaffectedForBorealisOnExternalDisplay) {
  const int64_t external_id = GetPrimaryDisplay().id();
  const int64_t internal_id = external_id + 1;
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      internal_id, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      external_id, display::DISPLAY_CONNECTION_TYPE_HDMI));
  SetUpDisplays(std::move(snapshots));
  ScopedSetInternalDisplayIds set_internal(internal_id);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(GetPrimaryDisplay().work_area()));

  // Expect the initial state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(internal_id);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  // Set power state to indicate the device is on battery.
  PowerStatus::Get()->SetProtoForTesting(BuildFakePowerSupplyProperties(
      PowerSupplyProperties::DISCONNECTED, 80.0f));
  controller_->OnPowerStatusChanged();

  // Expect the new state to be 60 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(internal_id);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 60.f);
  }

  // Set the game mode to indicate the user is gaming on the external display.
  game_mode_controller_->NotifySetGameMode(GameMode::BOREALIS,
                                           ash::WindowState::Get(window.get()));

  // Expect the state to be unaffected.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(internal_id);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 60.f);
  }

  game_mode_controller_->NotifySetGameMode(GameMode::OFF,
                                           ash::WindowState::Get(window.get()));
}

TEST_F(RefreshRateControllerTest, ThrottlingUpdatesWhenBorealisWindowMoves) {
  UpdateDisplay("300x200,300x200");
  const display::Display primary = GetPrimaryDisplay();
  const display::Display secondary = GetSecondaryDisplay();
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      primary.id(), display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      secondary.id(), display::DISPLAY_CONNECTION_TYPE_HDMI));
  SetUpDisplays(std::move(snapshots));
  ScopedSetInternalDisplayIds set_internal(primary.id());
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(secondary.work_area()));
  ASSERT_EQ(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window.get()).id(),
      secondary.id());

  // Set power state to indicate the device is on battery.
  PowerStatus::Get()->SetProtoForTesting(BuildFakePowerSupplyProperties(
      PowerSupplyProperties::DISCONNECTED, 80.0f));
  controller_->OnPowerStatusChanged();

  // Set the game mode to indicate the user is gaming on the external display.
  game_mode_controller_->NotifySetGameMode(GameMode::BOREALIS,
                                           ash::WindowState::Get(window.get()));

  // Expect the state to be 60Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(primary.id());
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 60.f);
  }

  // Move the borealis window to the internal display.
  window->SetBoundsInScreen(primary.work_area(), primary);
  ASSERT_EQ(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window.get()).id(),
      primary.id());

  // Expect the new state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(primary.id());
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  game_mode_controller_->NotifySetGameMode(GameMode::OFF,
                                           ash::WindowState::Get(window.get()));
}

TEST_F(RefreshRateControllerTest, ThrottlingUpdatesWhenDisplaysChange) {
  UpdateDisplay("300x200,300x200");
  const display::Display internal = GetPrimaryDisplay();
  const display::Display external = GetSecondaryDisplay();
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      internal.id(), display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      external.id(), display::DISPLAY_CONNECTION_TYPE_HDMI));
  SetUpDisplays(std::move(snapshots));
  ScopedSetInternalDisplayIds set_internal(internal.id());
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(external.work_area()));
  ASSERT_EQ(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window.get()).id(),
      external.id());

  // Set power state to indicate the device is on battery.
  PowerStatus::Get()->SetProtoForTesting(BuildFakePowerSupplyProperties(
      PowerSupplyProperties::DISCONNECTED, 80.0f));
  controller_->OnPowerStatusChanged();

  // Set the game mode to indicate the user is gaming on the external display.
  game_mode_controller_->NotifySetGameMode(GameMode::BOREALIS,
                                           ash::WindowState::Get(window.get()));

  // Expect the state to be 60Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(internal.id());
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 60.f);
  }

  // Swap displays causing borealis window to move to the internal display.
  SwapPrimaryDisplay();
  ASSERT_EQ(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window.get()).id(),
      internal.id());

  // Expect the new state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(internal.id());
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  game_mode_controller_->NotifySetGameMode(GameMode::OFF,
                                           ash::WindowState::Get(window.get()));
}

TEST_F(RefreshRateControllerTest, ShouldNotThrottleExternalDisplay) {
  constexpr int64_t kDisplayId = 12345;
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      kDisplayId, display::DISPLAY_CONNECTION_TYPE_HDMI));
  ScopedSetInternalDisplayIds set_internal(kDisplayId);
  SetUpDisplays(std::move(snapshots));

  // Expect the initial state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  // Set power state to indicate the device is on battery and battery is low.
  PowerStatus::Get()->SetProtoForTesting(BuildFakePowerSupplyProperties(
      PowerSupplyProperties::DISCONNECTED, 10.f));
  controller_->OnPowerStatusChanged();

  // Expect the state to be unchanged.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }
}

TEST_F(RefreshRateControllerTest, ShouldThrottleOnUSBCharger) {
  constexpr int64_t kDisplayId = 12345;
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      kDisplayId, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  ScopedSetInternalDisplayIds set_internal(kDisplayId);
  SetUpDisplays(std::move(snapshots));

  // Expect the initial state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  // Set power state to indicate the device is on a low powered charger.
  PowerStatus::Get()->SetProtoForTesting(
      BuildFakePowerSupplyProperties(PowerSupplyProperties::USB, 10.f));
  controller_->OnPowerStatusChanged();

  // Expect the new state to be 60 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 60.f);
  }
}

TEST_F(RefreshRateControllerTest, ShouldEnableVrrForBorealis) {
  const int64_t internal_id = GetPrimaryDisplay().id();
  const int64_t external_id = internal_id + 1;
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildVrrPanelSnapshot(
      internal_id, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  snapshots.push_back(BuildVrrPanelSnapshot(
      external_id, display::DISPLAY_CONNECTION_TYPE_HDMI));
  ScopedSetInternalDisplayIds set_internal(internal_id);
  SetUpDisplays(std::move(snapshots));
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(GetPrimaryDisplay().work_area()));

  // Expect VRR to be initially disabled.
  {
    const DisplaySnapshot* internal_snapshot = GetDisplaySnapshot(internal_id);
    ASSERT_NE(internal_snapshot, nullptr);
    ASSERT_TRUE(internal_snapshot->IsVrrCapable());
    EXPECT_FALSE(internal_snapshot->IsVrrEnabled());

    const DisplaySnapshot* external_snapshot = GetDisplaySnapshot(external_id);
    ASSERT_NE(external_snapshot, nullptr);
    ASSERT_TRUE(external_snapshot->IsVrrCapable());
    EXPECT_FALSE(external_snapshot->IsVrrEnabled());
  }

  // Set the game mode to indicate the user is gaming.
  game_mode_controller_->NotifySetGameMode(GameMode::BOREALIS,
                                           ash::WindowState::Get(window.get()));

  // Expect the new state to have VRR enabled on the Borealis display only.
  {
    const DisplaySnapshot* internal_snapshot = GetDisplaySnapshot(internal_id);
    ASSERT_NE(internal_snapshot, nullptr);
    EXPECT_TRUE(internal_snapshot->IsVrrEnabled());

    const DisplaySnapshot* external_snapshot = GetDisplaySnapshot(external_id);
    ASSERT_NE(external_snapshot, nullptr);
    EXPECT_FALSE(external_snapshot->IsVrrEnabled());
  }

  // Reset the game mode.
  game_mode_controller_->NotifySetGameMode(GameMode::OFF,
                                           ash::WindowState::Get(window.get()));

  // Expect the new state to have VRR disabled.
  {
    const DisplaySnapshot* internal_snapshot = GetDisplaySnapshot(internal_id);
    ASSERT_NE(internal_snapshot, nullptr);
    EXPECT_FALSE(internal_snapshot->IsVrrEnabled());

    const DisplaySnapshot* external_snapshot = GetDisplaySnapshot(external_id);
    ASSERT_NE(external_snapshot, nullptr);
    EXPECT_FALSE(external_snapshot->IsVrrEnabled());
  }

  game_mode_controller_->NotifySetGameMode(GameMode::OFF,
                                           ash::WindowState::Get(window.get()));
}

TEST_F(RefreshRateControllerTest, ShouldDisableVrrWithBatterySaverMode) {
  const int64_t display_id = GetPrimaryDisplay().id();
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildVrrPanelSnapshot(
      display_id, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  ScopedSetInternalDisplayIds set_internal(display_id);
  SetUpDisplays(std::move(snapshots));
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(GetPrimaryDisplay().work_area()));

  // Set the game mode to indicate the user is gaming.
  game_mode_controller_->NotifySetGameMode(GameMode::BOREALIS,
                                           ash::WindowState::Get(window.get()));

  // Expect the initial state to have VRR enabled.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(display_id);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_TRUE(snapshot->IsVrrCapable());
    EXPECT_TRUE(snapshot->IsVrrEnabled());
  }

  // Set power state to indicate the device is on AC, and
  // Battery Saver Mode is enabled.
  PowerStatus::Get()->SetProtoForTesting(
      BuildFakePowerSupplyProperties(PowerSupplyProperties::AC, 100.f));
  PowerStatus::Get()->SetBatterySaverStateForTesting(true);
  performance_controller_->OnPowerStatusChanged();

  // Expect the new state to have VRR disabled.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(display_id);
    ASSERT_NE(snapshot, nullptr);
    EXPECT_FALSE(snapshot->IsVrrEnabled());
  }

  game_mode_controller_->NotifySetGameMode(GameMode::OFF,
                                           ash::WindowState::Get(window.get()));
}

TEST_F(RefreshRateControllerTest,
       RequestSeamlessRefreshRatesOnInternalDisplayModeChanged) {
  constexpr int64_t kDisplayId = 12345;
  ScopedSetInternalDisplayIds set_internal(kDisplayId);

  // Create a vector of DisplaySnapshot.
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      kDisplayId, display::DISPLAY_CONNECTION_TYPE_INTERNAL));

  // Create a DisplayStateList pointing to the snapshot.
  DisplayStateList state_list;
  for (auto& snapshot : snapshots) {
    state_list.push_back(snapshot.get());
  }

  EXPECT_CALL(*native_display_delegate_,
              GetSeamlessRefreshRates(kDisplayId, testing::_));
  controller_->OnDisplayModeChanged(state_list);

  // When the internal display is turned off, it will have no mode set.
  snapshots[0]->set_current_mode(nullptr);
  EXPECT_CALL(*native_display_delegate_,
              GetSeamlessRefreshRates(testing::_, testing::_))
      .Times(0);
  controller_->OnDisplayModeChanged(state_list);
}

TEST_F(RefreshRateControllerTest, RequestSeamlessRefreshRatesMultipleDisplays) {
  constexpr int64_t kInternalDisplayId = 12345;
  constexpr int64_t kExternalDisplayId = 67890;
  ScopedSetInternalDisplayIds set_internal(kInternalDisplayId);

  // Create a vector of DisplaySnapshot.
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      kInternalDisplayId, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      kExternalDisplayId, display::DISPLAY_CONNECTION_TYPE_DISPLAYPORT));

  // Create a DisplayStateList pointing to the snapshot.
  DisplayStateList state_list;
  for (auto& snapshot : snapshots) {
    state_list.push_back(snapshot.get());
  }

  EXPECT_CALL(*native_display_delegate_,
              GetSeamlessRefreshRates(kInternalDisplayId, testing::_));
  EXPECT_CALL(*native_display_delegate_,
              GetSeamlessRefreshRates(kExternalDisplayId, testing::_));
  controller_->OnDisplayModeChanged(state_list);
}

TEST_F(RefreshRateControllerTest, TestBorealisWithHighPerformance) {
  const int64_t internal_id = GetPrimaryDisplay().id();
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildVrrPanelSnapshot(
      internal_id, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  ScopedSetInternalDisplayIds set_internal(internal_id);
  SetUpDisplays(std::move(snapshots));
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(GetPrimaryDisplay().work_area()));

  game_mode_controller_->NotifySetGameMode(GameMode::OFF,
                                           WindowState::Get(window.get()));
  performance_controller_->SetHighPerformanceModeByUser(true);

  // Expect VRR to be disabled. The VrrEnabled feature is specifically only for
  // borealis gaming, and it hasn't been vetted for other applications.
  {
    const DisplaySnapshot* internal_snapshot = GetDisplaySnapshot(internal_id);
    ASSERT_TRUE(internal_snapshot);
    ASSERT_TRUE(internal_snapshot->IsVrrCapable());
    EXPECT_FALSE(internal_snapshot->IsVrrEnabled());
  }

  // Set the game mode to indicate the user is gaming.
  game_mode_controller_->NotifySetGameMode(GameMode::BOREALIS,
                                           WindowState::Get(window.get()));

  // Expect the new state to have VRR enabled on the Borealis display only.
  {
    const DisplaySnapshot* internal_snapshot = GetDisplaySnapshot(internal_id);
    ASSERT_TRUE(internal_snapshot);
    EXPECT_TRUE(internal_snapshot->IsVrrEnabled());
  }

  game_mode_controller_->NotifySetGameMode(GameMode::OFF,
                                           WindowState::Get(window.get()));
}

TEST_F(RefreshRateControllerTest, TestThrottlingWithHighPerformance) {
  constexpr int64_t display_id = 12345;
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      display_id, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  ScopedSetInternalDisplayIds set_internal(display_id);
  SetUpDisplays(std::move(snapshots));

  // Expect the initial state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(display_id);
    ASSERT_TRUE(snapshot);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  performance_controller_->SetHighPerformanceModeByUser(true);

  // Expect the new state to be unchanged.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(display_id);
    ASSERT_TRUE(snapshot);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }
}

}  // namespace
}  // namespace ash
