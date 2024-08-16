// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/refresh_rate_controller.h"

#include <memory>
#include <optional>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/power/power_status.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/game_mode/game_mode_controller.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/display/manager/display_change_observer.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/test/action_logger.h"
#include "ui/display/manager/test/action_logger_util.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/manager/test/test_native_display_delegate.h"
#include "ui/display/manager/util/display_manager_test_util.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace {

using display::DisplayMode;
using display::DisplaySnapshot;
using display::FakeDisplaySnapshot;
using display::NativeDisplayDelegate;
using display::test::ActionLogger;
using display::test::TestNativeDisplayDelegate;
using game_mode::GameModeController;
using power_manager::PowerSupplyProperties;

using ModeState = DisplayPerformanceModeController::ModeState;
using DisplayStateList = display::DisplayConfigurator::DisplayStateList;
using GameMode = ResourcedClient::GameMode;

constexpr int kDefaultVsyncRateMin = 48;

class MockNativeDisplayDelegate : public TestNativeDisplayDelegate {
 public:
  explicit MockNativeDisplayDelegate(ActionLogger* logger)
      : TestNativeDisplayDelegate(logger) {
    ON_CALL(*this, GetSeamlessRefreshRates)
        .WillByDefault(
            [this](int64_t display_id,
                   display::GetSeamlessRefreshRatesCallback callback) {
              return TestNativeDisplayDelegate::GetSeamlessRefreshRates(
                  display_id, std::move(callback));
            });

    ON_CALL(*this, Configure)
        .WillByDefault(
            [this](const std::vector<display::DisplayConfigurationParams>&
                       config_requests,
                   display::ConfigureCallback callback,
                   display::ModesetFlags modeset_flags) {
              return TestNativeDisplayDelegate::Configure(
                  config_requests, std::move(callback), modeset_flags);
            });
  }

  MOCK_METHOD(void,
              GetSeamlessRefreshRates,
              (int64_t, display::GetSeamlessRefreshRatesCallback),
              (const override));

  MOCK_METHOD(void,
              Configure,
              (const std::vector<display::DisplayConfigurationParams>&,
               display::ConfigureCallback,
               display::ModesetFlags),
              (override));
};

std::unique_ptr<DisplayMode> MakeDisplayMode(
    int width,
    int height,
    bool is_interlaced,
    float refresh_rate,
    const std::optional<float>& vsync_rate_min = std::nullopt) {
  return std::make_unique<DisplayMode>(gfx::Size{width, height}, is_interlaced,
                                       refresh_rate, vsync_rate_min);
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
    display::DisplayConnectionType type,
    int vsync_rate_min = kDefaultVsyncRateMin) {
  return FakeDisplaySnapshot::Builder()
      .SetId(id)
      .SetType(type)
      .SetNativeMode(MakeDisplayMode(1920, 1200, false, 120.f, vsync_rate_min))
      .SetCurrentMode(MakeDisplayMode(1920, 1200, false, 120.f, vsync_rate_min))
      .SetVariableRefreshRateState(
          display::VariableRefreshRateState::kVrrDisabled)
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

const ui::Compositor* GetCompositorForDisplayId(int64_t display_id) {
  aura::Window* root = Shell::GetRootWindowForDisplayId(display_id);
  CHECK(root);

  return root->GetHost()->compositor();
}

DisplayStateList SnapshotsToDisplayStateList(
    const std::vector<std::unique_ptr<DisplaySnapshot>>& snapshots) {
  // Create a DisplayStateList pointing to the snapshot.
  DisplayStateList state_list;
  state_list.reserve(snapshots.size());
  for (auto& snapshot : snapshots) {
    state_list.push_back(snapshot.get());
  }
  return state_list;
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
    game_mode_controller_->set_game_mode_changed_callback(
        base::BindRepeating([](aura::Window* window, GameMode game_mode) {
          ash::Shell::Get()->refresh_rate_controller()->SetGameMode(
              window, game_mode == GameMode::BOREALIS);
        }));

    performance_controller_ =
        Shell::Get()->display_performance_mode_controller();
    controller_ = Shell::Get()->refresh_rate_controller();
    display_change_observer_ =
        std::make_unique<display::DisplayChangeObserver>(display_manager());
    display_manager()->configurator()->AddObserver(
        display_change_observer_.get());
  }

  void TearDown() override {
    display_manager()->configurator()->RemoveObserver(
        display_change_observer_.get());
    display_change_observer_.reset();
    game_mode_controller_.reset();
    controller_ = nullptr;
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
  std::unique_ptr<GameModeController> game_mode_controller_;
  std::unique_ptr<display::DisplayChangeObserver> display_change_observer_;
  // Not owned.
  raw_ptr<RefreshRateController> controller_;
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
  SetUpDisplays(std::move(snapshots));

  // Expect the initial state to be 120 Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(kDisplayId);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  // Create a new RefreshRateController, and force throttle it.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kForceRefreshRateThrottle);

  std::unique_ptr<RefreshRateController> controller =
      std::make_unique<RefreshRateController>(
          Shell::Get()->display_configurator(), PowerStatus::Get(),
          performance_controller_);

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
  const int64_t internal_id = GetPrimaryDisplay().id();
  const int64_t external_id = display::GetASynthesizedDisplayId();
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      internal_id, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      external_id, display::DISPLAY_CONNECTION_TYPE_HDMI));
  SetUpDisplays(std::move(snapshots));
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(GetSecondaryDisplay().work_area()));
  ASSERT_EQ(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window.get()).id(),
      external_id);

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
  const display::Display primary = GetPrimaryDisplay();
  const int64_t secondary_id = display::GetASynthesizedDisplayId();
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      primary.id(), display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      secondary_id, display::DISPLAY_CONNECTION_TYPE_HDMI));
  SetUpDisplays(std::move(snapshots));
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(GetSecondaryDisplay().work_area()));
  ASSERT_EQ(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window.get()).id(),
      secondary_id);

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
  const display::Display internal = GetPrimaryDisplay();
  const int64_t external_id = display::GetASynthesizedDisplayId();
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      internal.id(), display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      external_id, display::DISPLAY_CONNECTION_TYPE_HDMI));
  SetUpDisplays(std::move(snapshots));
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(GetSecondaryDisplay().work_area()));
  ASSERT_EQ(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window.get()).id(),
      external_id);

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
  const int64_t external_id = display::GetASynthesizedDisplayId();
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildVrrPanelSnapshot(
      internal_id, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  snapshots.push_back(BuildVrrPanelSnapshot(
      external_id, display::DISPLAY_CONNECTION_TYPE_HDMI));
  SetUpDisplays(std::move(snapshots));
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(GetPrimaryDisplay().work_area()));
  ASSERT_EQ(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window.get()).id(),
      internal_id);

  // Expect VRR to be initially disabled.
  {
    const DisplaySnapshot* internal_snapshot = GetDisplaySnapshot(internal_id);
    ASSERT_NE(internal_snapshot, nullptr);
    ASSERT_TRUE(internal_snapshot->IsVrrCapable());
    EXPECT_FALSE(internal_snapshot->IsVrrEnabled());
    EXPECT_EQ(base::Hertz(kDefaultVsyncRateMin),
              GetCompositorForDisplayId(internal_id)
                  ->max_vsync_interval_for_testing());
    EXPECT_EQ(display::VariableRefreshRateState::kVrrDisabled,
              GetCompositorForDisplayId(internal_id)->vrr_state_for_testing());

    const DisplaySnapshot* external_snapshot = GetDisplaySnapshot(external_id);
    ASSERT_NE(external_snapshot, nullptr);
    ASSERT_TRUE(external_snapshot->IsVrrCapable());
    EXPECT_FALSE(external_snapshot->IsVrrEnabled());
    EXPECT_EQ(base::Hertz(kDefaultVsyncRateMin),
              GetCompositorForDisplayId(internal_id)
                  ->max_vsync_interval_for_testing());
    EXPECT_EQ(display::VariableRefreshRateState::kVrrDisabled,
              GetCompositorForDisplayId(external_id)->vrr_state_for_testing());
  }

  // Set the game mode to indicate the user is gaming.
  game_mode_controller_->NotifySetGameMode(GameMode::BOREALIS,
                                           ash::WindowState::Get(window.get()));

  // Expect the new state to have VRR enabled on the Borealis display only.
  {
    const DisplaySnapshot* internal_snapshot = GetDisplaySnapshot(internal_id);
    ASSERT_NE(internal_snapshot, nullptr);
    EXPECT_TRUE(internal_snapshot->IsVrrEnabled());
    EXPECT_EQ(base::Hertz(kDefaultVsyncRateMin),
              GetCompositorForDisplayId(internal_id)
                  ->max_vsync_interval_for_testing());
    EXPECT_EQ(display::VariableRefreshRateState::kVrrEnabled,
              GetCompositorForDisplayId(internal_id)->vrr_state_for_testing());

    const DisplaySnapshot* external_snapshot = GetDisplaySnapshot(external_id);
    ASSERT_NE(external_snapshot, nullptr);
    EXPECT_FALSE(external_snapshot->IsVrrEnabled());
    EXPECT_EQ(base::Hertz(kDefaultVsyncRateMin),
              GetCompositorForDisplayId(internal_id)
                  ->max_vsync_interval_for_testing());
    EXPECT_EQ(display::VariableRefreshRateState::kVrrDisabled,
              GetCompositorForDisplayId(external_id)->vrr_state_for_testing());
  }

  // Reset the game mode.
  game_mode_controller_->NotifySetGameMode(GameMode::OFF,
                                           ash::WindowState::Get(window.get()));

  // Expect the new state to have VRR disabled.
  {
    const DisplaySnapshot* internal_snapshot = GetDisplaySnapshot(internal_id);
    ASSERT_NE(internal_snapshot, nullptr);
    EXPECT_FALSE(internal_snapshot->IsVrrEnabled());
    EXPECT_EQ(base::Hertz(kDefaultVsyncRateMin),
              GetCompositorForDisplayId(internal_id)
                  ->max_vsync_interval_for_testing());
    EXPECT_EQ(display::VariableRefreshRateState::kVrrDisabled,
              GetCompositorForDisplayId(internal_id)->vrr_state_for_testing());

    const DisplaySnapshot* external_snapshot = GetDisplaySnapshot(external_id);
    ASSERT_NE(external_snapshot, nullptr);
    EXPECT_FALSE(external_snapshot->IsVrrEnabled());
    EXPECT_EQ(base::Hertz(kDefaultVsyncRateMin),
              GetCompositorForDisplayId(internal_id)
                  ->max_vsync_interval_for_testing());
    EXPECT_EQ(display::VariableRefreshRateState::kVrrDisabled,
              GetCompositorForDisplayId(external_id)->vrr_state_for_testing());
  }

  game_mode_controller_->NotifySetGameMode(GameMode::OFF,
                                           ash::WindowState::Get(window.get()));
}

TEST_F(RefreshRateControllerTest, ShouldDisableVrrWithBatterySaverMode) {
  const int64_t display_id = GetPrimaryDisplay().id();
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildVrrPanelSnapshot(
      display_id, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
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
    EXPECT_EQ(base::Hertz(kDefaultVsyncRateMin),
              GetCompositorForDisplayId(display_id)
                  ->max_vsync_interval_for_testing());
    EXPECT_EQ(display::VariableRefreshRateState::kVrrEnabled,
              GetCompositorForDisplayId(display_id)->vrr_state_for_testing());
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
    EXPECT_EQ(base::Hertz(kDefaultVsyncRateMin),
              GetCompositorForDisplayId(display_id)
                  ->max_vsync_interval_for_testing());
    EXPECT_EQ(display::VariableRefreshRateState::kVrrDisabled,
              GetCompositorForDisplayId(display_id)->vrr_state_for_testing());
  }

  game_mode_controller_->NotifySetGameMode(GameMode::OFF,
                                           ash::WindowState::Get(window.get()));
}

TEST_F(RefreshRateControllerTest, VrrUpdatesWhenBorealisWindowMoves) {
  constexpr int kVsyncRateMinInternal = 48;
  constexpr int kVsyncRateMinExternal = 40;

  const display::Display internal = GetPrimaryDisplay();
  const int64_t external_id = display::GetASynthesizedDisplayId();
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildVrrPanelSnapshot(
      internal.id(), display::DISPLAY_CONNECTION_TYPE_INTERNAL,
      kVsyncRateMinInternal));
  snapshots.push_back(
      BuildVrrPanelSnapshot(external_id, display::DISPLAY_CONNECTION_TYPE_HDMI,
                            kVsyncRateMinExternal));
  SetUpDisplays(std::move(snapshots));
  const display::Display external = GetSecondaryDisplay();
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(internal.work_area()));
  ASSERT_EQ(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window.get()).id(),
      internal.id());

  // Expect VRR to be initially disabled.
  {
    const DisplaySnapshot* internal_snapshot =
        GetDisplaySnapshot(internal.id());
    ASSERT_NE(internal_snapshot, nullptr);
    ASSERT_TRUE(internal_snapshot->IsVrrCapable());
    EXPECT_FALSE(internal_snapshot->IsVrrEnabled());
    EXPECT_EQ(base::Hertz(kVsyncRateMinInternal),
              GetCompositorForDisplayId(internal.id())
                  ->max_vsync_interval_for_testing());
    EXPECT_EQ(
        display::VariableRefreshRateState::kVrrDisabled,
        GetCompositorForDisplayId(internal.id())->vrr_state_for_testing());

    const DisplaySnapshot* external_snapshot =
        GetDisplaySnapshot(external.id());
    ASSERT_NE(external_snapshot, nullptr);
    ASSERT_TRUE(external_snapshot->IsVrrCapable());
    EXPECT_FALSE(external_snapshot->IsVrrEnabled());
    EXPECT_EQ(base::Hertz(kVsyncRateMinExternal),
              GetCompositorForDisplayId(external.id())
                  ->max_vsync_interval_for_testing());
    EXPECT_EQ(
        display::VariableRefreshRateState::kVrrDisabled,
        GetCompositorForDisplayId(external.id())->vrr_state_for_testing());
  }

  // Set the game mode to indicate the user is gaming on the internal display.
  game_mode_controller_->NotifySetGameMode(GameMode::BOREALIS,
                                           ash::WindowState::Get(window.get()));

  // Expect the new state to have VRR enabled on the internal display only.
  {
    const DisplaySnapshot* internal_snapshot =
        GetDisplaySnapshot(internal.id());
    ASSERT_NE(internal_snapshot, nullptr);
    EXPECT_TRUE(internal_snapshot->IsVrrEnabled());
    EXPECT_EQ(base::Hertz(kVsyncRateMinInternal),
              GetCompositorForDisplayId(internal.id())
                  ->max_vsync_interval_for_testing());
    EXPECT_EQ(
        display::VariableRefreshRateState::kVrrEnabled,
        GetCompositorForDisplayId(internal.id())->vrr_state_for_testing());

    const DisplaySnapshot* external_snapshot =
        GetDisplaySnapshot(external.id());
    ASSERT_NE(external_snapshot, nullptr);
    EXPECT_FALSE(external_snapshot->IsVrrEnabled());
    EXPECT_EQ(base::Hertz(kVsyncRateMinExternal),
              GetCompositorForDisplayId(external.id())
                  ->max_vsync_interval_for_testing());
    EXPECT_EQ(
        display::VariableRefreshRateState::kVrrDisabled,
        GetCompositorForDisplayId(external.id())->vrr_state_for_testing());
  }

  // Move borealis window to the external display.
  window->SetBoundsInScreen(external.work_area(), external);
  ASSERT_EQ(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window.get()).id(),
      external.id());

  // Expect the new state to have VRR enabled on the external display only.
  {
    const DisplaySnapshot* internal_snapshot =
        GetDisplaySnapshot(internal.id());
    ASSERT_NE(internal_snapshot, nullptr);
    EXPECT_FALSE(internal_snapshot->IsVrrEnabled());
    EXPECT_EQ(base::Hertz(kVsyncRateMinInternal),
              GetCompositorForDisplayId(internal.id())
                  ->max_vsync_interval_for_testing());
    EXPECT_EQ(
        display::VariableRefreshRateState::kVrrDisabled,
        GetCompositorForDisplayId(internal.id())->vrr_state_for_testing());

    const DisplaySnapshot* external_snapshot =
        GetDisplaySnapshot(external.id());
    ASSERT_NE(external_snapshot, nullptr);
    EXPECT_TRUE(external_snapshot->IsVrrEnabled());
    EXPECT_EQ(base::Hertz(kVsyncRateMinExternal),
              GetCompositorForDisplayId(external.id())
                  ->max_vsync_interval_for_testing());
    EXPECT_EQ(
        display::VariableRefreshRateState::kVrrEnabled,
        GetCompositorForDisplayId(external.id())->vrr_state_for_testing());
  }

  game_mode_controller_->NotifySetGameMode(GameMode::OFF,
                                           ash::WindowState::Get(window.get()));
}

TEST_F(RefreshRateControllerTest,
       RequestSeamlessRefreshRatesOnInternalDisplayModeChanged) {
  constexpr int64_t kDisplayId = 12345;

  // Create a vector of DisplaySnapshot.
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      kDisplayId, display::DISPLAY_CONNECTION_TYPE_INTERNAL));

  EXPECT_CALL(*native_display_delegate_,
              GetSeamlessRefreshRates(kDisplayId, testing::_));
  controller_->OnDisplayConfigurationChanged(
      SnapshotsToDisplayStateList(snapshots));

  // When the internal display is turned off, it will have no mode set.
  snapshots[0]->set_current_mode(nullptr);
  EXPECT_CALL(*native_display_delegate_,
              GetSeamlessRefreshRates(testing::_, testing::_))
      .Times(0);
  controller_->OnDisplayConfigurationChanged(
      SnapshotsToDisplayStateList(snapshots));
}

TEST_F(RefreshRateControllerTest, RequestSeamlessRefreshRatesMultipleDisplays) {
  constexpr int64_t kInternalDisplayId = 12345;
  constexpr int64_t kExternalDisplayId = 67890;

  // Create a vector of DisplaySnapshot.
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      kInternalDisplayId, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      kExternalDisplayId, display::DISPLAY_CONNECTION_TYPE_DISPLAYPORT));

  EXPECT_CALL(*native_display_delegate_,
              GetSeamlessRefreshRates(kInternalDisplayId, testing::_));
  EXPECT_CALL(*native_display_delegate_,
              GetSeamlessRefreshRates(kExternalDisplayId, testing::_));
  controller_->OnDisplayConfigurationChanged(
      SnapshotsToDisplayStateList(snapshots));
}

TEST_F(RefreshRateControllerTest, SeamlessRefreshRatesChanged) {
  const int64_t display_id = GetPrimaryDisplay().id();

  // Calls to GetSeamlessRefreshRates only return a single refresh rate.
  ON_CALL(*native_display_delegate_,
          GetSeamlessRefreshRates(display_id, testing::_))
      .WillByDefault(base::test::RunOnceCallbackRepeatedly<1>(
          std::make_optional(std::vector<float>{120.f})));

  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildDualRefreshPanelSnapshot(
      display_id, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
  auto display_list = SnapshotsToDisplayStateList(snapshots);
  SetUpDisplays(std::move(snapshots));

  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(display_id);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  // Set PowerSaver mode, which will prefer a throttled refresh rate.
  controller_->OnDisplayPerformanceModeChanged(
      DisplayPerformanceModeController::ModeState::kPowerSaver);

  // Expect the state to be 120Hz, since there are no downclock modes.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(display_id);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 120.f);
  }

  // Calls to GetSeamlessRefreshRates return two refresh rates for the
  // current mode and downclock mode.
  ON_CALL(*native_display_delegate_,
          GetSeamlessRefreshRates(display_id, testing::_))
      .WillByDefault(base::test::RunOnceCallbackRepeatedly<1>(
          std::make_optional(std::vector<float>{120.f, 60.f})));

  // Notify the controller of a configuration change to request updated seamless
  // refresh rates and update the refresh rate override.
  controller_->OnDisplayConfigurationChanged(display_list);

  // Expect the new state to be 60Hz.
  {
    const DisplaySnapshot* snapshot = GetDisplaySnapshot(display_id);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_NE(snapshot->current_mode(), nullptr);
    EXPECT_EQ(snapshot->current_mode()->refresh_rate(), 60.f);
  }
}

TEST_F(RefreshRateControllerTest, TestBorealisWithHighPerformance) {
  const int64_t internal_id = GetPrimaryDisplay().id();
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildVrrPanelSnapshot(
      internal_id, display::DISPLAY_CONNECTION_TYPE_INTERNAL));
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

TEST_F(RefreshRateControllerTest, CompositorsGetVrrIntervalsOnSwap) {
  constexpr int kVsyncRateMinInternal = 48;
  constexpr int kVsyncRateMinExternal = 40;

  scoped_features_.Reset();
  scoped_features_.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableVariableRefreshRateAlwaysOn},
      /*disabled_features=*/{});

  const int64_t internal_id = GetPrimaryDisplay().id();
  const int64_t external_id = display::GetASynthesizedDisplayId();
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(BuildVrrPanelSnapshot(
      internal_id, display::DISPLAY_CONNECTION_TYPE_INTERNAL,
      kVsyncRateMinInternal));
  snapshots.push_back(
      BuildVrrPanelSnapshot(external_id, display::DISPLAY_CONNECTION_TYPE_HDMI,
                            kVsyncRateMinExternal));
  SetUpDisplays(std::move(snapshots));

  // Verify VRR is enabled on both displays.
  {
    const DisplaySnapshot* internal_snapshot = GetDisplaySnapshot(internal_id);
    ASSERT_NE(internal_snapshot, nullptr);
    ASSERT_TRUE(internal_snapshot->IsVrrCapable());
    ASSERT_TRUE(internal_snapshot->IsVrrEnabled());

    const DisplaySnapshot* external_snapshot = GetDisplaySnapshot(external_id);
    ASSERT_NE(external_snapshot, nullptr);
    ASSERT_TRUE(external_snapshot->IsVrrCapable());
    ASSERT_TRUE(external_snapshot->IsVrrEnabled());
  }

  const ui::Compositor* primary = GetCompositorForDisplayId(internal_id);
  ASSERT_NE(primary, nullptr);
  const ui::Compositor* secondary = GetCompositorForDisplayId(external_id);
  ASSERT_NE(secondary, nullptr);

  // Expect VRR intervals to be set on each display's compositor.
  {
    EXPECT_EQ(primary, GetCompositorForDisplayId(internal_id));
    EXPECT_EQ(base::Hertz(kVsyncRateMinInternal),
              primary->max_vsync_interval_for_testing());
    EXPECT_EQ(display::VariableRefreshRateState::kVrrEnabled,
              primary->vrr_state_for_testing());

    EXPECT_EQ(secondary, GetCompositorForDisplayId(external_id));
    EXPECT_EQ(base::Hertz(kVsyncRateMinExternal),
              secondary->max_vsync_interval_for_testing());
    EXPECT_EQ(display::VariableRefreshRateState::kVrrEnabled,
              secondary->vrr_state_for_testing());
  }

  SwapPrimaryDisplay();

  // Expect compositors to have swapped displays, and VRR intervals to be
  // updated accordingly.
  {
    EXPECT_EQ(primary, GetCompositorForDisplayId(external_id));
    EXPECT_EQ(base::Hertz(kVsyncRateMinExternal),
              primary->max_vsync_interval_for_testing());
    EXPECT_EQ(display::VariableRefreshRateState::kVrrEnabled,
              primary->vrr_state_for_testing());

    EXPECT_EQ(secondary, GetCompositorForDisplayId(internal_id));
    EXPECT_EQ(base::Hertz(kVsyncRateMinInternal),
              secondary->max_vsync_interval_for_testing());
    EXPECT_EQ(display::VariableRefreshRateState::kVrrEnabled,
              secondary->vrr_state_for_testing());
  }
}

}  // namespace
}  // namespace ash
