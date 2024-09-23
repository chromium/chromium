// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/battery_level_provider.h"

#include "base/power_monitor/power_monitor_buildflags.h"
#include "base/ranges/algorithm.h"

namespace base {

#if !BUILDFLAG(HAS_BATTERY_LEVEL_PROVIDER_IMPL)
std::unique_ptr<BatteryLevelProvider> BatteryLevelProvider::Create() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/40871810): ChromeOS doesn't define
  // `HAS_BATTERY_LEVEL_PROVIDER_IMPL` but still supplies its own
  // `BatteryLevelProvider`
  NOTREACHED();
#else
  return nullptr;
#endif
}
#endif

BatteryLevelProvider::BatteryState BatteryLevelProvider::MakeBatteryState(
    const std::vector<BatteryDetails>& battery_details) {
  BatteryState state;

  state.battery_count = static_cast<int>(battery_details.size());
  state.is_external_power_connected =
      battery_details.size() == 0 ||
      base::ranges::any_of(battery_details, [](const BatteryDetails& details) {
        return details.is_external_power_connected;
      });

  // Only populate the following fields if there is one battery detail.
  if (battery_details.size() == 1) {
    state.current_capacity = battery_details.front().current_capacity;
    state.full_charged_capacity = battery_details.front().full_charged_capacity;
    state.voltage_mv = battery_details.front().voltage_mv;
    state.charge_unit = battery_details.front().charge_unit;
#if BUILDFLAG(IS_WIN)
    state.battery_discharge_granularity =
        battery_details.front().battery_discharge_granularity;
#endif  // BUILDFLAG(IS_WIN)
  }
  state.capture_time = base::TimeTicks::Now();

  return state;
}

}  // namespace base
