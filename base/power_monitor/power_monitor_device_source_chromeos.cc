// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/power_monitor_device_source.h"

#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/power_monitor/power_observer.h"

namespace base {

namespace {

// The most-recently-seen power source.
PowerStateObserver::BatteryPowerStatus g_battery_power_status =
    PowerStateObserver::BatteryPowerStatus::kUnknown;

}  // namespace

// static
void PowerMonitorDeviceSource::SetPowerSource(
    PowerStateObserver::BatteryPowerStatus battery_power_status) {
  if (battery_power_status != g_battery_power_status) {
    g_battery_power_status = battery_power_status;
    ProcessPowerEvent(POWER_STATE_EVENT);
  }
}

// static
void PowerMonitorDeviceSource::HandleSystemSuspending() {
  ProcessPowerEvent(SUSPEND_EVENT);
}

// static
void PowerMonitorDeviceSource::HandleSystemResumed() {
  ProcessPowerEvent(RESUME_EVENT);
}

PowerStateObserver::BatteryPowerStatus
PowerMonitorDeviceSource::GetBatteryPowerStatus() const {
  return g_battery_power_status;
}

// static
void PowerMonitorDeviceSource::ThermalEventReceived(
    PowerThermalObserver::DeviceThermalState state) {
  auto* power_monitor = base::PowerMonitor::GetInstance();
  if (!power_monitor->IsInitialized()) {
    power_monitor->Initialize(std::make_unique<PowerMonitorDeviceSource>());
  }
  power_monitor->SetCurrentThermalState(state);

  ProcessThermalEvent(state);
}

PowerThermalObserver::DeviceThermalState
PowerMonitorDeviceSource::GetCurrentThermalState() const {
  return current_thermal_state_;
}

void PowerMonitorDeviceSource::SetCurrentThermalState(
    PowerThermalObserver::DeviceThermalState state) {
  current_thermal_state_ = state;
}

}  // namespace base
