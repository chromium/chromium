// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/battery_level_provider.h"

BatteryLevelProvider::BatteryState::BatteryState(
    size_t interface_count,
    size_t battery_count,
    base::Optional<double> charge_level,
    bool on_battery,
    base::TimeTicks capture_time)
    : interface_count(interface_count),
      battery_count(battery_count),
      charge_level(charge_level),
      on_battery(on_battery),
      capture_time(capture_time) {}

BatteryLevelProvider::BatteryState::BatteryState(const BatteryState&) = default;

BatteryLevelProvider::BatteryInterface::BatteryInterface(
    bool battery_present_in)
    : battery_present(battery_present_in) {}

BatteryLevelProvider::BatteryInterface::BatteryInterface(
    const BatteryDetails& details_in)
    : battery_present(true), details(details_in) {}

BatteryLevelProvider::BatteryInterface::BatteryInterface(
    const BatteryInterface&) = default;

BatteryLevelProvider::BatteryState BatteryLevelProvider::MakeBatteryState(
    const std::vector<BatteryInterface>& battery_interfaces) {
  const base::TimeTicks capture_time = base::TimeTicks::Now();

  uint64_t total_max_capacity = 0;
  uint64_t total_current_capacity = 0;
  bool on_battery = true;
  bool any_capacity_invalid = false;
  size_t battery_count = 0;

  for (auto& interface : battery_interfaces) {
    // The interface might have no battery.
    if (!interface.battery_present)
      continue;

    // Counts the number of interfaces that has |battery_present == true|.
    ++battery_count;

    // The state is considered on battery power only if all of the batteries
    // are explicitly marked as not connected.
    if (!interface.details.has_value() || interface.details->is_connected)
      on_battery = false;

    if (!interface.details.has_value()) {
      any_capacity_invalid = true;
      continue;
    }

    // Total capacity is averaged across all the batteries.
    total_current_capacity += interface.details->current_capacity;
    total_max_capacity += interface.details->full_charged_capacity;
  }

  base::Optional<double> charge_level;
  // Avoid invalid division.
  if (!any_capacity_invalid && total_max_capacity != 0) {
    charge_level = static_cast<double>(total_current_capacity) /
                   static_cast<double>(total_max_capacity);
  }
  // If no battery was detected, we consider the system to be drawing power
  // from an external power source, which is different from |on_battery|'s
  // default value.
  if (battery_count == 0)
    on_battery = false;

  return {battery_interfaces.size(), battery_count, charge_level, on_battery,
          capture_time};
}
