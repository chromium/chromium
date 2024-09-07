// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "base/power_monitor/power_monitor_device_source.h"

namespace base {

PowerStateObserver::BatteryPowerStatus
PowerMonitorDeviceSource::GetBatteryPowerStatus() const {
  return PowerStateObserver::BatteryPowerStatus::kExternalPower;
}

}  // namespace base
