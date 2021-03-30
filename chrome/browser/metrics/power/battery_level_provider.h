// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_BATTERY_LEVEL_PROVIDER_H_
#define CHROME_BROWSER_METRICS_POWER_BATTERY_LEVEL_PROVIDER_H_

#include "base/callback.h"
#include "base/optional.h"
#include "base/time/time.h"

// BatteryLevelProvider provides an interface for querying battery state.
// A platform specific implementation is obtained with
// BatteryLevelProvider::Create().
class BatteryLevelProvider {
 public:
  // Represents an aggregated state of all the batteries on the system at a
  // certain point in time.
  struct BatteryState {
    BatteryState(size_t interface_count,
                 size_t battery_count,
                 base::Optional<double> charge_level,
                 bool on_battery,
                 base::TimeTicks capture_time);
    BatteryState(const BatteryState&);

    // Number of device interfaces that accept a battery on the system.
    size_t interface_count = 0;

    // Number of batteries detected on the system.
    size_t battery_count = 0;

    // A fraction of the maximal battery capacity of the system, in the range
    // [0.00, 1.00], or nullopt if no battery is present or querying charge
    // level failed. This may be nullopt even if |on_battery == true|, which
    // indicates a failure to grab the battery level.
    base::Optional<double> charge_level = 0;

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

  // Queries the current battery state and returns it to |callback| when ready.
  virtual void GetBatteryState(
      base::OnceCallback<void(const BatteryState&)> callback) = 0;

 protected:
  BatteryLevelProvider() = default;

  struct BatteryDetails {
    // True if the battery is connected and drawing power from external power
    // source.
    bool is_connected;

    // The current battery capacity.
    uint64_t current_capacity;

    // The battery's fully charged capacity.
    uint64_t full_charged_capacity;
  };

  struct BatteryInterface {
    // Indicates whether a battery is present on the interface, without
    // additional battery details.
    explicit BatteryInterface(bool battery_present_in);
    // Provides the details of a battery that was detected on the interface.
    explicit BatteryInterface(const BatteryDetails& details_in);
    BatteryInterface(const BatteryInterface&);

    // True if a battery is detected.
    const bool battery_present;

    // Detailed power state of the battery. This may be nullopt even if
    // |battery_present == true| when the details couldn't be queried.
    const base::Optional<BatteryDetails> details;
  };

  static BatteryState MakeBatteryState(
      const std::vector<BatteryInterface>& battery_interfaces);
};

#endif  // CHROME_BROWSER_METRICS_POWER_BATTERY_LEVEL_PROVIDER_H_
