// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/power_monitor_test.h"

#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/power_monitor/power_observer.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"

namespace base {
namespace test {

class PowerMonitorTestSource : public PowerMonitorSource {
 public:
  PowerMonitorTestSource() = default;
  ~PowerMonitorTestSource() override = default;

  // Retrieve current states.
  PowerThermalObserver::DeviceThermalState GetCurrentThermalState()
      const override;
  PowerStateObserver::BatteryPowerStatus GetBatteryPowerStatus() const override;

  // Sends asynchronous notifications to registered observers.
  void Suspend();
  void Resume();
  void SetBatteryPowerStatus(
      PowerStateObserver::BatteryPowerStatus battery_power_status);

  // Sends asynchronous notifications to registered observers and ensures they
  // are executed (i.e. RunUntilIdle()).
  void GeneratePowerStateEvent(
      PowerStateObserver::BatteryPowerStatus battery_power_status);
  void GenerateSuspendEvent();
  void GenerateResumeEvent();
  void GenerateThermalThrottlingEvent(
      PowerThermalObserver::DeviceThermalState new_thermal_state);
  void GenerateSpeedLimitEvent(int speed_limit);

 protected:
  PowerStateObserver::BatteryPowerStatus test_power_status_ =
      PowerStateObserver::BatteryPowerStatus::kUnknown;
  PowerThermalObserver::DeviceThermalState current_thermal_state_ =
      PowerThermalObserver::DeviceThermalState::kUnknown;
  int current_speed_limit_ = PowerThermalObserver::kSpeedLimitMax;
};

PowerThermalObserver::DeviceThermalState
PowerMonitorTestSource::GetCurrentThermalState() const {
  return current_thermal_state_;
}

void PowerMonitorTestSource::Suspend() {
  ProcessPowerEvent(SUSPEND_EVENT);
}

void PowerMonitorTestSource::Resume() {
  ProcessPowerEvent(RESUME_EVENT);
}

void PowerMonitorTestSource::SetBatteryPowerStatus(
    PowerStateObserver::BatteryPowerStatus battery_power_status) {
  test_power_status_ = battery_power_status;
  ProcessPowerEvent(POWER_STATE_EVENT);
}

void PowerMonitorTestSource::GeneratePowerStateEvent(
    PowerStateObserver::BatteryPowerStatus battery_power_status) {
  SetBatteryPowerStatus(battery_power_status);
  RunLoop().RunUntilIdle();
}

void PowerMonitorTestSource::GenerateSuspendEvent() {
  Suspend();
  RunLoop().RunUntilIdle();
}

void PowerMonitorTestSource::GenerateResumeEvent() {
  Resume();
  RunLoop().RunUntilIdle();
}

PowerStateObserver::BatteryPowerStatus
PowerMonitorTestSource::GetBatteryPowerStatus() const {
  return test_power_status_;
}

void PowerMonitorTestSource::GenerateThermalThrottlingEvent(
    PowerThermalObserver::DeviceThermalState new_thermal_state) {
  ProcessThermalEvent(new_thermal_state);
  current_thermal_state_ = new_thermal_state;
  RunLoop().RunUntilIdle();
}

void PowerMonitorTestSource::GenerateSpeedLimitEvent(int speed_limit) {
  ProcessSpeedLimitEvent(speed_limit);
  current_speed_limit_ = speed_limit;
  RunLoop().RunUntilIdle();
}

ScopedPowerMonitorTestSource::ScopedPowerMonitorTestSource() {
  auto power_monitor_test_source = std::make_unique<PowerMonitorTestSource>();
  power_monitor_test_source_ = power_monitor_test_source.get();
  base::PowerMonitor::GetInstance()->Initialize(
      std::move(power_monitor_test_source));
}

ScopedPowerMonitorTestSource::~ScopedPowerMonitorTestSource() {
  if (is_suspended_) {
    // Generate a resume here because there are global, leaky disk cache
    // threads that lives throughout a long portion of some test targets.
    // This thread is created the first time the disk cache is created, which
    // instantiates a disk cache backend, which in turn creates a CacheThread as
    // a global LazyInstance<>::Lazy. This thread gets its own
    // ThreadControllerPowerMonitor, so if this test case doesn't simulate a
    // resume, the next test will hit a DCHECK when it tries to generate its
    // first suspend event. The "correct" solution to this problem would be to
    // refactor the offending disk caches to support passing a thread through to
    // the disk cache backend in tests, and avoid having a global/leaky instance
    // around but this is a significant undertaking.
    GenerateResumeEvent();
  }
  base::PowerMonitor::GetInstance()->ShutdownForTesting();
}

PowerThermalObserver::DeviceThermalState
ScopedPowerMonitorTestSource::GetCurrentThermalState() const {
  return power_monitor_test_source_->GetCurrentThermalState();
}

PowerStateObserver::BatteryPowerStatus
ScopedPowerMonitorTestSource::GetBatteryPowerStatus() const {
  return power_monitor_test_source_->GetBatteryPowerStatus();
}

void ScopedPowerMonitorTestSource::Suspend() {
  is_suspended_ = true;
  power_monitor_test_source_->Suspend();
}

void ScopedPowerMonitorTestSource::Resume() {
  power_monitor_test_source_->Resume();
  is_suspended_ = false;
}

void ScopedPowerMonitorTestSource::SetBatteryPowerStatus(
    PowerStateObserver::BatteryPowerStatus battery_power_status) {
  power_monitor_test_source_->SetBatteryPowerStatus(battery_power_status);
}

void ScopedPowerMonitorTestSource::GenerateSuspendEvent() {
  is_suspended_ = true;
  power_monitor_test_source_->GenerateSuspendEvent();
}

void ScopedPowerMonitorTestSource::GenerateResumeEvent() {
  power_monitor_test_source_->GenerateResumeEvent();
  is_suspended_ = false;
}

void ScopedPowerMonitorTestSource::GeneratePowerStateEvent(
    PowerStateObserver::BatteryPowerStatus battery_power_status) {
  power_monitor_test_source_->GeneratePowerStateEvent(battery_power_status);
}

void ScopedPowerMonitorTestSource::GenerateThermalThrottlingEvent(
    PowerThermalObserver::DeviceThermalState new_thermal_state) {
  power_monitor_test_source_->GenerateThermalThrottlingEvent(new_thermal_state);
}

void ScopedPowerMonitorTestSource::GenerateSpeedLimitEvent(int speed_limit) {
  power_monitor_test_source_->GenerateSpeedLimitEvent(speed_limit);
}

PowerMonitorTestObserver::PowerMonitorTestObserver() = default;
PowerMonitorTestObserver::~PowerMonitorTestObserver() = default;

void PowerMonitorTestObserver::OnBatteryPowerStatusChange(
    PowerStateObserver::BatteryPowerStatus battery_power_status) {
  last_power_status_ = battery_power_status;
  power_state_changes_++;
}

void PowerMonitorTestObserver::OnSuspend() {
  suspends_++;
}

void PowerMonitorTestObserver::OnResume() {
  resumes_++;
}

void PowerMonitorTestObserver::OnThermalStateChange(
    PowerThermalObserver::DeviceThermalState new_state) {
  thermal_state_changes_++;
  last_thermal_state_ = new_state;
}

void PowerMonitorTestObserver::OnSpeedLimitChange(int speed_limit) {
  speed_limit_changes_++;
  last_speed_limit_ = speed_limit;
}

}  // namespace test
}  // namespace base
