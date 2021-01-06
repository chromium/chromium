// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_BATTERY_LEVEL_PROVIDER_H_
#define CHROME_BROWSER_METRICS_POWER_BATTERY_LEVEL_PROVIDER_H_

#include "base/callback.h"
#include "base/optional.h"

// BatteryLevelProvider provides an interface for querrying battery state.
// A platform specific implementation is obtained with
// BatteryLevelProvider::Create().
class BatteryLevelProvider {
 public:
  // Represents the state of the battery at a certain point in time.
  struct BatteryState {
    // A fraction of the maximal battery capacity of the system, in the range
    // [0.00, 1.00].
    double charge_level = 0;

    // True if the system is running on battery power, false if the system is
    // drawing power from an external power source.
    bool on_battery = false;

    // The time at which the battery state capture took place.
    base::TimeTicks capture_time;
  };

  // Creates a platform specific BatteryLevelProvider able to retrieve battery
  // state.
  static std::unique_ptr<BatteryLevelProvider> Create();

  virtual ~BatteryLevelProvider() = default;

  BatteryLevelProvider(const BatteryLevelProvider& other) = delete;
  BatteryLevelProvider& operator=(const BatteryLevelProvider& other) = delete;

  // Returns the current battery state, or nullopt if no battery is present or
  // querying battery information failed.
  virtual base::Optional<BatteryState> GetBatteryState() = 0;

 protected:
  BatteryLevelProvider() = default;
};

#endif  // CHROME_BROWSER_METRICS_POWER_BATTERY_LEVEL_PROVIDER_H_
