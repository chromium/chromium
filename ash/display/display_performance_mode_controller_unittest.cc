// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_performance_mode_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace {
power_manager::PowerSupplyProperties BuildFakePowerSupplyProperties(
    power_manager::PowerSupplyProperties::ExternalPower charger_state,
    double battery_percent) {
  power_manager::PowerSupplyProperties fake_power;
  fake_power.set_external_power(charger_state);
  fake_power.set_battery_percent(battery_percent);
  return fake_power;
}
}  // namespace

class DisplayPerformanceModeControllerTest : public AshTestBase {
 public:
  DisplayPerformanceModeControllerTest() = default;
  DisplayPerformanceModeControllerTest(
      const DisplayPerformanceModeControllerTest&) = delete;
  DisplayPerformanceModeControllerTest& operator=(
      const DisplayPerformanceModeControllerTest&) = delete;
  ~DisplayPerformanceModeControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    Shell::Get()->display_performance_mode_controller()->AddObserver(
        &observer_);
  }

  void TearDown() override {
    Shell::Get()->display_performance_mode_controller()->RemoveObserver(
        &observer_);

    AshTestBase::TearDown();
  }

 protected:
  using ModeState = DisplayPerformanceModeController::ModeState;

  class MockObserver : public DisplayPerformanceModeController::Observer {
   public:
    MOCK_METHOD(void,
                OnDisplayPerformanceModeChanged,
                (ModeState new_state),
                (override));
  };

  MockObserver observer_;
};

TEST_F(DisplayPerformanceModeControllerTest, TestPowerSaverOnLowBattery) {
  EXPECT_CALL(observer_,
              OnDisplayPerformanceModeChanged(ModeState::kPowerSaver));

  PowerStatus::Get()->SetProtoForTesting(BuildFakePowerSupplyProperties(
      power_manager::PowerSupplyProperties::DISCONNECTED, 10.f));
  PowerStatus::Get()->SetBatterySaverStateForTesting(true);
  Shell::Get()->display_performance_mode_controller()->OnPowerStatusChanged();
}

TEST_F(DisplayPerformanceModeControllerTest, TestPowerSaverOnAc) {
  EXPECT_CALL(observer_,
              OnDisplayPerformanceModeChanged(ModeState::kPowerSaver));

  PowerStatus::Get()->SetProtoForTesting(BuildFakePowerSupplyProperties(
      power_manager::PowerSupplyProperties::AC, 100.f));
  PowerStatus::Get()->SetBatterySaverStateForTesting(true);
  Shell::Get()->display_performance_mode_controller()->OnPowerStatusChanged();
}

TEST_F(DisplayPerformanceModeControllerTest, TestIntelligentOnAc) {
  // Should not be called as the default is already intelligent.
  EXPECT_CALL(observer_, OnDisplayPerformanceModeChanged).Times(0);

  PowerStatus::Get()->SetProtoForTesting(BuildFakePowerSupplyProperties(
      power_manager::PowerSupplyProperties::AC, 15.f));
  Shell::Get()->display_performance_mode_controller()->OnPowerStatusChanged();
}

TEST_F(DisplayPerformanceModeControllerTest,
       TestResetToIntelligentAfterPowerSaver) {
  EXPECT_CALL(observer_,
              OnDisplayPerformanceModeChanged(ModeState::kPowerSaver));

  PowerStatus::Get()->SetProtoForTesting(BuildFakePowerSupplyProperties(
      power_manager::PowerSupplyProperties::DISCONNECTED, 15.f));
  PowerStatus::Get()->SetBatterySaverStateForTesting(true);
  Shell::Get()->display_performance_mode_controller()->OnPowerStatusChanged();

  EXPECT_CALL(observer_,
              OnDisplayPerformanceModeChanged(ModeState::kIntelligent));
  PowerStatus::Get()->SetBatterySaverStateForTesting(false);
  Shell::Get()->display_performance_mode_controller()->OnPowerStatusChanged();
}

TEST_F(DisplayPerformanceModeControllerTest, TestModeStateOnAddObserver) {
  // Check that adding the controller reports back the current mode.
  Shell::Get()->display_performance_mode_controller()->RemoveObserver(
      &observer_);
  ModeState current_state =
      Shell::Get()->display_performance_mode_controller()->AddObserver(
          &observer_);
  EXPECT_EQ(current_state, ModeState::kDefault);
}

TEST_F(DisplayPerformanceModeControllerTest, TestModeStateReadBack) {
  EXPECT_EQ(Shell::Get()
                ->display_performance_mode_controller()
                ->GetCurrentStateForTesting(),
            ModeState::kDefault);

  EXPECT_CALL(observer_,
              OnDisplayPerformanceModeChanged(ModeState::kPowerSaver));
  PowerStatus::Get()->SetBatterySaverStateForTesting(true);
  Shell::Get()->display_performance_mode_controller()->OnPowerStatusChanged();

  EXPECT_EQ(Shell::Get()
                ->display_performance_mode_controller()
                ->GetCurrentStateForTesting(),
            ModeState::kPowerSaver);
}

}  // namespace ash
