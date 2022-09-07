// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/battery_level_provider.h"

#include "base/ranges/algorithm.h"

namespace base {

BatteryLevelProvider::BatteryState BatteryLevelProvider::MakeBatteryState(
    const std::vector<BatteryDetails>& battery_details) {
  BatteryState state;

  state.battery_count = static_cast<int>(battery_details.size());
  state.is_external_power_connected =
      battery_details.size() == 0 ||
      base::ranges::any_of(battery_details, [](const BatteryDetails& details) {
        return details.is_external_power_connected;
      });
  state.current_capacity =
      battery_details.size() == 1
          ? absl::make_optional(battery_details.front().current_capacity)
          : absl::nullopt;
  state.full_charged_capacity =
      battery_details.size() == 1
          ? absl::make_optional(battery_details.front().full_charged_capacity)
          : absl::nullopt;
  state.charge_unit =
      battery_details.size() == 1
          ? absl::make_optional(battery_details.front().charge_unit)
          : absl::nullopt;
  state.capture_time = base::TimeTicks::Now();

  return state;
}

}  // namespace base
