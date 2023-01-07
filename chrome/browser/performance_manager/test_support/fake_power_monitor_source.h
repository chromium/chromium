// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_POWER_MONITOR_SOURCE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_POWER_MONITOR_SOURCE_H_

#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"

namespace performance_manager {

class FakePowerMonitorSource : public base::PowerMonitorSource {
 public:
  bool IsOnBatteryPower() override;
  void SetOnBatteryPower(bool on_battery_power);

 private:
  bool on_battery_power_ = false;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_POWER_MONITOR_SOURCE_H_
