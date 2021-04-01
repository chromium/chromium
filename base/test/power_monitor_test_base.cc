// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/power_monitor_test_base.h"

#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"

namespace base {

PowerMonitorTestSource::PowerMonitorTestSource() {
  DCHECK(CurrentThread::Get())
      << "PowerMonitorTestSource requires a MessageLoop.";
}

PowerMonitorTestSource::~PowerMonitorTestSource() = default;

PowerThermalObserver::DeviceThermalState
PowerMonitorTestSource::GetCurrentThermalState() {
  return current_thermal_state_;
}

void PowerMonitorTestSource::GeneratePowerStateEvent(bool on_battery_power) {
  test_on_battery_power_ = on_battery_power;
  ProcessPowerEvent(POWER_STATE_EVENT);
  RunLoop().RunUntilIdle();
}

void PowerMonitorTestSource::GenerateSuspendEvent() {
  ProcessPowerEvent(SUSPEND_EVENT);
  RunLoop().RunUntilIdle();
}

void PowerMonitorTestSource::GenerateResumeEvent() {
  ProcessPowerEvent(RESUME_EVENT);
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

}  // namespace base
