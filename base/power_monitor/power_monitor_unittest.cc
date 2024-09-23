// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/power_monitor.h"

#include <optional>

#include "base/test/power_monitor_test.h"
#include "base/test/task_environment.h"
#include "power_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace test {

class PowerMonitorTest : public testing::Test {
 public:
  PowerMonitorTest(const PowerMonitorTest&) = delete;
  PowerMonitorTest& operator=(const PowerMonitorTest&) = delete;

 protected:
  PowerMonitorTest() = default;

  void PowerMonitorInitialize() { power_monitor_source_.emplace(); }

  ScopedPowerMonitorTestSource& source() {
    return power_monitor_source_.value();
  }

 private:
  TaskEnvironment task_environment_;
  std::optional<ScopedPowerMonitorTestSource> power_monitor_source_;
};

// PowerMonitorSource is tightly coupled with the PowerMonitor, so this test
// covers both classes.
TEST_F(PowerMonitorTest, PowerNotifications) {
  const int kObservers = 5;

  PowerMonitorInitialize();

  PowerMonitorTestObserver observers[kObservers];
  auto* power_monitor = PowerMonitor::GetInstance();
  for (auto& index : observers) {
    power_monitor->AddPowerSuspendObserver(&index);
    power_monitor->AddPowerStateObserver(&index);
    power_monitor->AddPowerThermalObserver(&index);
  }

  // Sending resume when not suspended should have no effect.
  source().GenerateResumeEvent();
  EXPECT_EQ(observers[0].resumes(), 0);

  // Pretend we suspended.
  source().GenerateSuspendEvent();
  // Ensure all observers were notified of the event
  for (const auto& index : observers)
    EXPECT_EQ(index.suspends(), 1);

  // Send a second suspend notification.  This should be suppressed.
  source().GenerateSuspendEvent();
  EXPECT_EQ(observers[0].suspends(), 1);

  // Pretend we were awakened.
  source().GenerateResumeEvent();
  EXPECT_EQ(observers[0].resumes(), 1);

  // Send a duplicate resume notification.  This should be suppressed.
  source().GenerateResumeEvent();
  EXPECT_EQ(observers[0].resumes(), 1);

  // Pretend the device has gone on battery power
  source().GeneratePowerStateEvent(
      PowerStateObserver::BatteryPowerStatus::kBatteryPower);
  EXPECT_EQ(observers[0].power_state_changes(), 1);
  EXPECT_EQ(observers[0].last_power_status(),
            PowerStateObserver::BatteryPowerStatus::kBatteryPower);

  // Repeated indications the device is on battery power should be suppressed.
  source().GeneratePowerStateEvent(
      PowerStateObserver::BatteryPowerStatus::kBatteryPower);
  EXPECT_EQ(observers[0].power_state_changes(), 1);

  // Pretend the device has gone off battery power
  source().GeneratePowerStateEvent(
      PowerStateObserver::BatteryPowerStatus::kExternalPower);
  EXPECT_EQ(observers[0].power_state_changes(), 2);
  EXPECT_EQ(observers[0].last_power_status(),
            PowerStateObserver::BatteryPowerStatus::kExternalPower);

  // Repeated indications the device is off battery power should be suppressed.
  source().GeneratePowerStateEvent(
      PowerStateObserver::BatteryPowerStatus::kExternalPower);
  EXPECT_EQ(observers[0].power_state_changes(), 2);

  // Send speed limit change notifications.
  source().GenerateSpeedLimitEvent(666);
  EXPECT_EQ(observers[0].speed_limit_changes(), 1);
  EXPECT_EQ(observers[0].last_speed_limit(), 666);
  source().GenerateSpeedLimitEvent(777);
  EXPECT_EQ(observers[0].speed_limit_changes(), 2);
  EXPECT_EQ(observers[0].last_speed_limit(), 777);

  EXPECT_EQ(observers[0].thermal_state_changes(), 0);

  // Send a power thermal change notification.
  source().GenerateThermalThrottlingEvent(
      PowerThermalObserver::DeviceThermalState::kNominal);
  EXPECT_EQ(observers[0].thermal_state_changes(), 1);
  EXPECT_EQ(observers[0].last_thermal_state(),
            PowerThermalObserver::DeviceThermalState::kNominal);

  // Send a duplicate power thermal notification.  This should be suppressed.
  source().GenerateThermalThrottlingEvent(
      PowerThermalObserver::DeviceThermalState::kNominal);
  EXPECT_EQ(observers[0].thermal_state_changes(), 1);

  // Send a different power thermal change notification.
  source().GenerateThermalThrottlingEvent(
      PowerThermalObserver::DeviceThermalState::kFair);
  EXPECT_EQ(observers[0].thermal_state_changes(), 2);
  EXPECT_EQ(observers[0].last_thermal_state(),
            PowerThermalObserver::DeviceThermalState::kFair);

  for (auto& index : observers) {
    power_monitor->RemovePowerSuspendObserver(&index);
    power_monitor->RemovePowerStateObserver(&index);
    power_monitor->RemovePowerThermalObserver(&index);
  }
}

TEST_F(PowerMonitorTest, ThermalThrottling) {
  PowerMonitorTestObserver observer;
  auto* power_monitor = PowerMonitor::GetInstance();
  power_monitor->AddPowerThermalObserver(&observer);

  PowerMonitorInitialize();

  constexpr PowerThermalObserver::DeviceThermalState kThermalStates[] = {
      PowerThermalObserver::DeviceThermalState::kUnknown,
      PowerThermalObserver::DeviceThermalState::kNominal,
      PowerThermalObserver::DeviceThermalState::kFair,
      PowerThermalObserver::DeviceThermalState::kSerious,
      PowerThermalObserver::DeviceThermalState::kCritical};

  for (const auto state : kThermalStates) {
    source().GenerateThermalThrottlingEvent(state);
    EXPECT_EQ(state, source().GetCurrentThermalState());
    EXPECT_EQ(observer.last_thermal_state(), state);
  }

  power_monitor->RemovePowerThermalObserver(&observer);
}

TEST_F(PowerMonitorTest, AddPowerSuspendObserverBeforeAndAfterInitialization) {
  PowerMonitorTestObserver observer1;
  PowerMonitorTestObserver observer2;
  auto* power_monitor = PowerMonitor::GetInstance();

  // An observer is added before the PowerMonitor initialization.
  power_monitor->AddPowerSuspendObserver(&observer1);

  PowerMonitorInitialize();

  // An observer is added after the PowerMonitor initialization.
  power_monitor->AddPowerSuspendObserver(&observer2);

  // Simulate suspend/resume notifications.
  source().GenerateSuspendEvent();
  EXPECT_EQ(observer1.suspends(), 1);
  EXPECT_EQ(observer2.suspends(), 1);
  EXPECT_EQ(observer1.resumes(), 0);
  EXPECT_EQ(observer2.resumes(), 0);

  source().GenerateResumeEvent();
  EXPECT_EQ(observer1.resumes(), 1);
  EXPECT_EQ(observer2.resumes(), 1);

  power_monitor->RemovePowerSuspendObserver(&observer1);
  power_monitor->RemovePowerSuspendObserver(&observer2);
}

TEST_F(PowerMonitorTest, AddPowerStateObserverBeforeAndAfterInitialization) {
  PowerMonitorTestObserver observer1;
  PowerMonitorTestObserver observer2;
  auto* power_monitor = PowerMonitor::GetInstance();

  // An observer is added before the PowerMonitor initialization.
  power_monitor->AddPowerStateObserver(&observer1);

  PowerMonitorInitialize();

  // An observer is added after the PowerMonitor initialization.
  power_monitor->AddPowerStateObserver(&observer2);

  // Simulate power state transitions (e.g. battery on/off).
  EXPECT_EQ(observer1.power_state_changes(), 0);
  EXPECT_EQ(observer2.power_state_changes(), 0);
  source().GeneratePowerStateEvent(
      PowerStateObserver::BatteryPowerStatus::kBatteryPower);
  EXPECT_EQ(observer1.power_state_changes(), 1);
  EXPECT_EQ(observer2.power_state_changes(), 1);
  source().GeneratePowerStateEvent(
      PowerStateObserver::BatteryPowerStatus::kExternalPower);
  EXPECT_EQ(observer1.power_state_changes(), 2);
  EXPECT_EQ(observer2.power_state_changes(), 2);

  power_monitor->RemovePowerStateObserver(&observer1);
  power_monitor->RemovePowerStateObserver(&observer2);
}

TEST_F(PowerMonitorTest, SuspendStateReturnedFromAddObserver) {
  PowerMonitorTestObserver observer1;
  PowerMonitorTestObserver observer2;
  auto* power_monitor = PowerMonitor::GetInstance();

  PowerMonitorInitialize();

  EXPECT_FALSE(power_monitor->AddPowerSuspendObserverAndReturnSuspendedState(
      &observer1));

  source().GenerateSuspendEvent();

  EXPECT_TRUE(power_monitor->AddPowerSuspendObserverAndReturnSuspendedState(
      &observer2));

  EXPECT_EQ(observer1.suspends(), 1);
  EXPECT_EQ(observer2.suspends(), 0);
  EXPECT_EQ(observer1.resumes(), 0);
  EXPECT_EQ(observer2.resumes(), 0);

  power_monitor->RemovePowerSuspendObserver(&observer1);
  power_monitor->RemovePowerSuspendObserver(&observer2);
}

TEST_F(PowerMonitorTest, PowerStateReturnedFromAddObserver) {
  PowerMonitorTestObserver observer1;
  PowerMonitorTestObserver observer2;
  auto* power_monitor = PowerMonitor::GetInstance();

  PowerMonitorInitialize();

  // An observer is added before the on-battery notification.
  EXPECT_NE(power_monitor->AddPowerStateObserverAndReturnBatteryPowerStatus(
                &observer1),
            PowerStateObserver::BatteryPowerStatus::kBatteryPower);

  source().GeneratePowerStateEvent(
      PowerStateObserver::BatteryPowerStatus::kBatteryPower);

  // An observer is added after the on-battery notification.
  EXPECT_EQ(power_monitor->AddPowerStateObserverAndReturnBatteryPowerStatus(
                &observer2),
            PowerStateObserver::BatteryPowerStatus::kBatteryPower);

  EXPECT_EQ(observer1.power_state_changes(), 1);
  EXPECT_EQ(observer2.power_state_changes(), 0);

  power_monitor->RemovePowerStateObserver(&observer1);
  power_monitor->RemovePowerStateObserver(&observer2);
}

}  // namespace test
}  // namespace base
