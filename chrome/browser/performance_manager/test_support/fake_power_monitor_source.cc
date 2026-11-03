// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/test_support/fake_power_monitor_source.h"

namespace performance_manager {

base::PowerStateObserver::BatteryPowerStatus
FakePowerMonitorSource::GetBatteryPowerStatus() const {
  return battery_power_status_;
}

bool FakePowerMonitorSource::IsPowerSaverModeEnabled() const {
  return power_saver_mode_enabled_;
}

void FakePowerMonitorSource::SetBatteryPowerStatus(
    base::PowerStateObserver::BatteryPowerStatus battery_power_status) {
  battery_power_status_ = battery_power_status;
  ProcessPowerEvent(POWER_STATE_EVENT);
}

void FakePowerMonitorSource::SetPowerSaverModeEnabled(bool enabled) {
  power_saver_mode_enabled_ = enabled;
  ProcessPowerSaverModeEvent(enabled);
}

}  // namespace performance_manager
