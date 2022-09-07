// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_POWER_MONITOR_BATTERY_LEVEL_PROVIDER_H_
#define BASE_POWER_MONITOR_BATTERY_LEVEL_PROVIDER_H_

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/power_monitor/power_monitor_buildflags.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !BUILDFLAG(HAS_BATTERY_LEVEL_PROVIDER_IMPL)
#error battery_level_provider.h can only be included on platforms with a \
       working implementation.
#endif

namespace base {

// BatteryLevelProvider provides an interface for querying battery state.
// A platform specific implementation is obtained with
// BatteryLevelProvider::Create().
class BASE_EXPORT BatteryLevelProvider {
 public:
  // The three possible units of data returned by OS battery query functions,
  // kMWh and kMAh are self-explanatory and the desired state of things, while
  // kRelative occurs when Windows returns imprecise battery counters.
  enum class BatteryLevelUnit {
    kMWh,
    kMAh,
    kRelative,
  };

  // Represents an aggregated state of all the batteries on the system at a
  // certain point in time.
  struct BatteryState {
    // Number of batteries on the system.
    int battery_count = 0;

    // Whether the system is connected to an external source of power. Defaults
    // to `true` if `battery_count` is 0.
    bool is_external_power_connected = false;

    // Current battery capacity. nullopt if `battery_count` != 1.
    absl::optional<uint64_t> current_capacity;

    // Fully charged battery capacity. nullopt if `battery_count` != 1.
    absl::optional<uint64_t> full_charged_capacity;

    // The unit of the battery's charge. (MAh on Mac and MWh or Relative on
    // Windows). nullopt if `battery_count` != 1.
    absl::optional<BatteryLevelUnit> charge_unit;

    // The time at which the battery state capture took place.
    base::TimeTicks capture_time;
  };

  // Creates a platform specific BatteryLevelProvider able to retrieve battery
  // state.
  static std::unique_ptr<BatteryLevelProvider> Create();

  virtual ~BatteryLevelProvider() = default;

  BatteryLevelProvider(const BatteryLevelProvider& other) = delete;
  BatteryLevelProvider& operator=(const BatteryLevelProvider& other) = delete;

  // Queries the current battery state and forwards it to `callback` when ready
  // (forwards nullopt on retrieval error). `callback` will not be invoked if
  // the BatteryLevelProvider is destroyed.
  virtual void GetBatteryState(
      base::OnceCallback<void(const absl::optional<BatteryState>&)>
          callback) = 0;

 protected:
  BatteryLevelProvider() = default;

  struct BatteryDetails {
    // Whether the battery is connected to an external power source.
    bool is_external_power_connected;

    // The current battery capacity.
    uint64_t current_capacity;

    // The battery's fully charged capacity.
    uint64_t full_charged_capacity;

    // The battery's unit of charge.
    BatteryLevelUnit charge_unit;
  };

  // Constructs a `BatteryState` from a list of `BatteryDetails`. The list can
  // be empty if there are no batteries on the system.
  static BatteryState MakeBatteryState(
      const std::vector<BatteryDetails>& battery_details);
};

}  // namespace base

#endif  // BASE_POWER_MONITOR_BATTERY_LEVEL_PROVIDER_H_
