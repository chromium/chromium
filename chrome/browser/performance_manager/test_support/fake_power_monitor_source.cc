// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/test_support/fake_power_monitor_source.h"

namespace performance_manager {

bool FakePowerMonitorSource::IsOnBatteryPower() {
  return on_battery_power_;
}

void FakePowerMonitorSource::SetOnBatteryPower(bool on_battery_power) {
  on_battery_power_ = on_battery_power;
  ProcessPowerEvent(POWER_STATE_EVENT);
}

}  // namespace performance_manager
