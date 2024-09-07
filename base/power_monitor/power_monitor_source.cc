// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/power_monitor_source.h"

#include "base/notreached.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_observer.h"
#include "build/build_config.h"

namespace base {

PowerMonitorSource::PowerMonitorSource() = default;
PowerMonitorSource::~PowerMonitorSource() = default;

PowerThermalObserver::DeviceThermalState
PowerMonitorSource::GetCurrentThermalState() const {
  return PowerThermalObserver::DeviceThermalState::kUnknown;
}

int PowerMonitorSource::GetInitialSpeedLimit() const {
  return PowerThermalObserver::kSpeedLimitMax;
}

void PowerMonitorSource::SetCurrentThermalState(
    PowerThermalObserver::DeviceThermalState state) {}

#if BUILDFLAG(IS_ANDROID)
int PowerMonitorSource::GetRemainingBatteryCapacity() const {
  return 0;
}
#endif  // BUILDFLAG(IS_ANDROID)

// static
void PowerMonitorSource::ProcessPowerEvent(PowerEvent event_id) {
  auto* power_monitor = base::PowerMonitor::GetInstance();
  if (!power_monitor->IsInitialized()) {
    return;
  }

  switch (event_id) {
    case POWER_STATE_EVENT:
      power_monitor->NotifyPowerStateChange(
          power_monitor->Source()->GetBatteryPowerStatus());
      break;
      case RESUME_EVENT:
        power_monitor->NotifyResume();
        break;
      case SUSPEND_EVENT:
        power_monitor->NotifySuspend();
        break;
  }
}

// static
void PowerMonitorSource::ProcessThermalEvent(
    PowerThermalObserver::DeviceThermalState new_thermal_state) {
  if (auto* power_monitor = base::PowerMonitor::GetInstance();
      power_monitor->IsInitialized()) {
    power_monitor->NotifyThermalStateChange(new_thermal_state);
  }
}

// static
void PowerMonitorSource::ProcessSpeedLimitEvent(int speed_limit) {
  if (auto* power_monitor = base::PowerMonitor::GetInstance();
      power_monitor->IsInitialized()) {
    power_monitor->NotifySpeedLimitChange(speed_limit);
  }
}

// static
const char* PowerMonitorSource::DeviceThermalStateToString(
    PowerThermalObserver::DeviceThermalState state) {
  switch (state) {
    case PowerThermalObserver::DeviceThermalState::kUnknown:
      return "Unknown";
    case PowerThermalObserver::DeviceThermalState::kNominal:
      return "Nominal";
    case PowerThermalObserver::DeviceThermalState::kFair:
      return "Fair";
    case PowerThermalObserver::DeviceThermalState::kSerious:
      return "Serious";
    case PowerThermalObserver::DeviceThermalState::kCritical:
      return "Critical";
  }
  NOTREACHED();
}

}  // namespace base
