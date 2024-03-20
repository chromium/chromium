// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/brightness_controller_chromeos.h"

#include <memory>

#include "ash/shell.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/power/power_status.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/session_manager/session_manager_types.h"

namespace ash {

namespace {

power_manager::PowerSupplyProperties BuildFakePowerSupplyProperties(
    power_manager::PowerSupplyProperties::ExternalPower charger_state) {
  power_manager::PowerSupplyProperties fake_power;
  fake_power.set_external_power(charger_state);
  fake_power.set_battery_percent(50);
  return fake_power;
}

void SetBatteryPower() {
  DCHECK(PowerStatus::IsInitialized());
  PowerStatus::Get()->SetProtoForTesting(BuildFakePowerSupplyProperties(
      power_manager::PowerSupplyProperties::DISCONNECTED));
}

void SetChargerPower() {
  DCHECK(PowerStatus::IsInitialized());
  PowerStatus::Get()->SetProtoForTesting(
      BuildFakePowerSupplyProperties(power_manager::PowerSupplyProperties::AC));
}

}  // namespace

class BrightnessControllerChromeosTest : public AshTestBase {
 public:
  BrightnessControllerChromeosTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    AshTestBase::SetUp();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override { AshTestBase::TearDown(); }

  BrightnessControlDelegate* brightness_control_delegate() {
    return Shell::Get()->brightness_control_delegate();
  }

 protected:
  void AdvanceClock(base::TimeDelta time) {
    task_environment()->AdvanceClock(time);
  }

  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(BrightnessControllerChromeosTest,
       HistogramTest_DecreaseBrightnessOnLoginScreen) {
  // Metric count should start at 0.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      0);

  // Start on the login screen
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  // "Unplug" the device from charger
  SetBatteryPower();

  // Wait for a period of time, then send a brightness event
  int seconds_to_wait = 11;
  AdvanceClock(base::Seconds(seconds_to_wait));
  brightness_control_delegate()->HandleBrightnessDown();

  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      1);
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      base::Seconds(seconds_to_wait), 1);

  // Login
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Wait for a period of time, then send another brightness event
  AdvanceClock(base::Seconds(5));
  brightness_control_delegate()->HandleBrightnessDown();

  // The number of events should not have changed, since we already recorded a
  // metric on the login screen.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      1);
}

TEST_F(BrightnessControllerChromeosTest, HistogramTest_LoginSecondary) {
  // Metric count should start at 0.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      0);

  // Start on the "secondary" login screen (i.e. another user is already
  // logged-in, and another user is now logging in).
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_SECONDARY);

  // "Unplug" the device from charger
  SetBatteryPower();

  // Wait for a period of time, then send a brightness event
  int seconds_to_wait = 22;
  AdvanceClock(base::Seconds(seconds_to_wait));
  brightness_control_delegate()->HandleBrightnessDown();

  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      1);
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      base::Seconds(seconds_to_wait), 1);

  // Login
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Wait for a period of time, then send another brightness event
  AdvanceClock(base::Seconds(5));
  brightness_control_delegate()->HandleBrightnessDown();

  // The number of events should not have changed, since we already recorded a
  // metric on the login screen.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      1);
}

TEST_F(BrightnessControllerChromeosTest, HistogramTest_PowerSourceCharger) {
  // Metric count should start at 0.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "IncreaseBrightness.ChargerPower",
      0);

  // Start on the login screen
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  // "Plug in" the charger
  SetChargerPower();

  // Wait for a period of time, then send a brightness event
  int seconds_to_wait = 8;
  AdvanceClock(base::Seconds(seconds_to_wait));
  brightness_control_delegate()->HandleBrightnessUp();

  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "IncreaseBrightness.ChargerPower",
      1);
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "IncreaseBrightness.ChargerPower",
      base::Seconds(seconds_to_wait), 1);
}

TEST_F(BrightnessControllerChromeosTest,
       HistogramTest_BrightnessChangeAfterLogin) {
  // Metric count should start at 0.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.AfterLogin."
      "SetBrightness.ChargerPower",
      0);

  // Start on the login screen
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  // "Plug in" the charger
  SetChargerPower();

  // Wait for a period of time, but don't change brightness.
  AdvanceClock(base::Seconds(9));

  // Login
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Wait for a period of time, then send another brightness event
  int seconds_to_wait = 5;
  AdvanceClock(base::Seconds(seconds_to_wait));
  brightness_control_delegate()->SetBrightnessPercent(50, true);

  // Expect a record with the number of seconds since login, not since the
  // beginning of the login screen.
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.AfterLogin."
      "SetBrightness.ChargerPower",
      base::Seconds(seconds_to_wait), 1);
}

TEST_F(BrightnessControllerChromeosTest,
       HistogramTest_BrightnessChangeAfterLogin_LongDuration) {
  // Metric count should start at 0.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.AfterLogin."
      "IncreaseBrightness.BatteryPower",
      0);

  // Start on the login screen
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  // "Unplug" the charger
  SetBatteryPower();

  // Wait for a period of time, but don't change brightness.
  AdvanceClock(base::Seconds(20));

  // Login
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Wait for a period of time, then send another brightness event
  int minutes_to_wait = 35;
  AdvanceClock(base::Minutes(minutes_to_wait));
  brightness_control_delegate()->HandleBrightnessUp();

  // Expect a record with the number of minutes since login, not since the
  // beginning of the login screen.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.AfterLogin."
      "IncreaseBrightness.BatteryPower",
      1);
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.AfterLogin."
      "IncreaseBrightness.BatteryPower",
      base::Minutes(minutes_to_wait), 1);
}

TEST_F(BrightnessControllerChromeosTest,
       HistogramTest_BrightnessChangeAfterLogin_DurationGreaterThanOneHour) {
  // Start on the login screen
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  // "Unplug" the charger
  SetBatteryPower();

  // Wait for a period of time, but don't change brightness.
  AdvanceClock(base::Seconds(1));

  // Login
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Wait for > 1 hour, then send a brightness event.
  AdvanceClock(base::Hours(1) + base::Minutes(5));
  brightness_control_delegate()->HandleBrightnessUp();

  // Since the brightness event occurred >1 hour after login, don't record it.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.AfterLogin."
      "IncreaseBrightness.BatteryPower",
      0);
}

class BrightnessControllerChromeosTest_NonApplicableSessionStates
    : public BrightnessControllerChromeosTest,
      public testing::WithParamInterface<session_manager::SessionState> {};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    BrightnessControllerChromeosTest_NonApplicableSessionStates,
    testing::ValuesIn<session_manager::SessionState>({
        session_manager::SessionState::LOCKED,
        session_manager::SessionState::LOGGED_IN_NOT_ACTIVE,
        session_manager::SessionState::OOBE,
        session_manager::SessionState::RMA,
        session_manager::SessionState::UNKNOWN,
    }));

TEST_P(BrightnessControllerChromeosTest_NonApplicableSessionStates,
       HistogramTest_NonApplicableSessionStates) {
  GetSessionControllerClient()->SetSessionState(GetParam());

  // "Plug in" the charger
  SetChargerPower();

  // Wait for a period of time, then send a brightness event
  AdvanceClock(base::Seconds(8));
  brightness_control_delegate()->HandleBrightnessUp();

  // Expect no events, since the event did not happen on the login screen or
  // after login.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "IncreaseBrightness.ChargerPower",
      0);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.AfterLogin."
      "IncreaseBrightness.ChargerPower",
      0);
}

}  // namespace ash
