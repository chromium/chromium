// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/power_monitor_test.h"

#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"

namespace base {
namespace test {

class PowerMonitorTestSource : public PowerMonitorSource {
 public:
  PowerMonitorTestSource() = default;
  ~PowerMonitorTestSource() override = default;

  // Retrieve current states.
  PowerThermalObserver::DeviceThermalState GetCurrentThermalState() override;
  bool IsOnBatteryPower() override;

  // Sends asynchronous notifications to registered observers.
  void Suspend();
  void Resume();
  void SetOnBatteryPower(bool on_battery_power);

  // Sends asynchronous notifications to registered observers and ensures they
  // are executed (i.e. RunUntilIdle()).
  void GeneratePowerStateEvent(bool on_battery_power);
  void GenerateSuspendEvent();
  void GenerateResumeEvent();
  void GenerateThermalThrottlingEvent(
      PowerThermalObserver::DeviceThermalState new_thermal_state);

 protected:
  bool test_on_battery_power_ = false;
  PowerThermalObserver::DeviceThermalState current_thermal_state_ =
      PowerThermalObserver::DeviceThermalState::kUnknown;
};

PowerThermalObserver::DeviceThermalState
PowerMonitorTestSource::GetCurrentThermalState() {
  return current_thermal_state_;
}

void PowerMonitorTestSource::Suspend() {
  ProcessPowerEvent(SUSPEND_EVENT);
}

void PowerMonitorTestSource::Resume() {
  ProcessPowerEvent(RESUME_EVENT);
}

void PowerMonitorTestSource::SetOnBatteryPower(bool on_battery_power) {
  test_on_battery_power_ = on_battery_power;
  ProcessPowerEvent(POWER_STATE_EVENT);
}

void PowerMonitorTestSource::GeneratePowerStateEvent(bool on_battery_power) {
  SetOnBatteryPower(on_battery_power);
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

bool PowerMonitorTestSource::IsOnBatteryPower() {
  return test_on_battery_power_;
}

void PowerMonitorTestSource::GenerateThermalThrottlingEvent(
    PowerThermalObserver::DeviceThermalState new_thermal_state) {
  ProcessThermalEvent(new_thermal_state);
  current_thermal_state_ = new_thermal_state;
  RunLoop().RunUntilIdle();
}

ScopedPowerMonitorTestSource::ScopedPowerMonitorTestSource() {
  auto power_monitor_test_source = std::make_unique<PowerMonitorTestSource>();
  power_monitor_test_source_ = power_monitor_test_source.get();
  base::PowerMonitor::Initialize(std::move(power_monitor_test_source));
}

ScopedPowerMonitorTestSource::~ScopedPowerMonitorTestSource() {
  base::PowerMonitor::ShutdownForTesting();
}

PowerThermalObserver::DeviceThermalState
ScopedPowerMonitorTestSource::GetCurrentThermalState() {
  return power_monitor_test_source_->GetCurrentThermalState();
}

bool ScopedPowerMonitorTestSource::IsOnBatteryPower() {
  return power_monitor_test_source_->IsOnBatteryPower();
}

void ScopedPowerMonitorTestSource::Suspend() {
  power_monitor_test_source_->Suspend();
}

void ScopedPowerMonitorTestSource::Resume() {
  power_monitor_test_source_->Resume();
}

void ScopedPowerMonitorTestSource::SetOnBatteryPower(bool on_battery_power) {
  power_monitor_test_source_->SetOnBatteryPower(on_battery_power);
}

void ScopedPowerMonitorTestSource::GenerateSuspendEvent() {
  power_monitor_test_source_->GenerateSuspendEvent();
}

void ScopedPowerMonitorTestSource::GenerateResumeEvent() {
  power_monitor_test_source_->GenerateResumeEvent();
}

void ScopedPowerMonitorTestSource::GeneratePowerStateEvent(
    bool on_battery_power) {
  power_monitor_test_source_->GeneratePowerStateEvent(on_battery_power);
}

void ScopedPowerMonitorTestSource::GenerateThermalThrottlingEvent(
    PowerThermalObserver::DeviceThermalState new_thermal_state) {
  power_monitor_test_source_->GenerateThermalThrottlingEvent(new_thermal_state);
}

PowerMonitorTestObserver::PowerMonitorTestObserver() = default;
PowerMonitorTestObserver::~PowerMonitorTestObserver() = default;

void PowerMonitorTestObserver::OnPowerStateChange(bool on_battery_power) {
  last_power_state_ = on_battery_power;
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

}  // namespace test
}  // namespace base
