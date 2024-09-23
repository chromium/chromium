// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_POWER_MONITOR_SOURCE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_POWER_MONITOR_SOURCE_H_

#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/power_monitor/power_observer.h"

namespace performance_manager {

class FakePowerMonitorSource : public base::PowerMonitorSource {
 public:
  base::PowerStateObserver::BatteryPowerStatus GetBatteryPowerStatus()
      const override;
  void SetBatteryPowerStatus(
      base::PowerStateObserver::BatteryPowerStatus battery_power_status);

 private:
  base::PowerStateObserver::BatteryPowerStatus battery_power_status_ =
      base::PowerStateObserver::BatteryPowerStatus::kUnknown;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_POWER_MONITOR_SOURCE_H_
