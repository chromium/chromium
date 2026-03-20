// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/hi_res_timer_manager.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/power_monitor/power_monitor.h"
#include "base/time/time.h"

namespace base {

namespace {

bool HighResolutionTimerAllowed() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableHighResTimer);
}

}  // namespace

HighResolutionTimerManager::HighResolutionTimerManager()
    : hi_res_clock_available_(false) {
  // Register for PowerMonitor callbacks only if high-resolution
  // timers are allowed. If high-resolution timers are disabled
  // we won't receive power state change callbacks and
  // hi_res_clock_available_ will remain at its initial value.
  if (HighResolutionTimerAllowed()) {
    auto* power_monitor = base::PowerMonitor::GetInstance();
    DCHECK(power_monitor->IsInitialized());
    const bool on_battery =
        power_monitor->AddPowerStateObserverAndReturnBatteryPowerStatus(this) ==
        PowerStateObserver::BatteryPowerStatus::kBatteryPower;
    UseHiResClock(!on_battery);
  }
}

HighResolutionTimerManager::~HighResolutionTimerManager() {
  if (HighResolutionTimerAllowed()) {
    auto* power_monitor = base::PowerMonitor::GetInstance();
    power_monitor->RemovePowerStateObserver(this);
    UseHiResClock(false);
  }
}

void HighResolutionTimerManager::OnBatteryPowerStatusChange(
    base::PowerStateObserver::BatteryPowerStatus battery_power_status) {
  DCHECK(HighResolutionTimerAllowed());
  UseHiResClock(battery_power_status !=
                PowerStateObserver::BatteryPowerStatus::kBatteryPower);
}

void HighResolutionTimerManager::UseHiResClock(bool use) {
  DCHECK(HighResolutionTimerAllowed());
  if (use == hi_res_clock_available_) {
    return;
  }
  hi_res_clock_available_ = use;
  Time::EnableHighResolutionTimer(use);
}

}  // namespace base
