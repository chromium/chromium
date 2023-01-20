// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_BATTERY_HEALTH_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_BATTERY_HEALTH_H_

#include <string>

namespace app_list {

class BatteryHealth {
 public:
  BatteryHealth() = default;

  ~BatteryHealth() = default;

  void SetCycleCount(int cycle_count) { cycle_count_ = cycle_count; }
  void SetBatteryWearPercentage(int battery_wear_percentage) {
    battery_wear_percentage_ = battery_wear_percentage;
  }
  void SetPowerTime(const std::u16string& power_time) {
    power_time_ = power_time;
  }
  void SetBatteryPercentage(int battery_percentage) {
    battery_percentage_ = battery_percentage;
  }

  int GetCycleCount() const { return cycle_count_; }
  int GetBatteryWearPercentage() const { return battery_wear_percentage_; }
  std::u16string GetPowerTime() const { return power_time_; }
  int GetBatteryPercentage() const { return battery_percentage_; }

 private:
  int cycle_count_ = 0;
  int battery_wear_percentage_ = 0;
  std::u16string power_time_ = u"";
  int battery_percentage_ = 0;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_BATTERY_HEALTH_H_
