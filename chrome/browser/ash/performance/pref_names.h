// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PERFORMANCE_PREF_NAMES_H_
#define CHROME_BROWSER_ASH_PERFORMANCE_PREF_NAMES_H_

namespace ash::prefs {

// TimeDelta pref to record the accumulated duration of simulated battery
// status for doze mode.
inline constexpr char kDozeModeSimulatedBatteryStatusDuration[] =
    "ash.performance.doze_mode.simulated_battery_status_duration";

// TimeDelta pref to record the accumulated duration of real power
// status for doze mode.
inline constexpr char kDozeModeRealPowerStatusDuration[] =
    "ash.performance.doze_mode.real_power_status_duration";

// TimeDelta pref to record the accumulated duration of real battery
// status for doze mode.
inline constexpr char kDozeModeRealBatteryStatusDuration[] =
    "ash.performance.doze_mode.real_battery_status_duration";

// Integer pref used by the metrics::DailyEvent owned by
// ash::DozeModePowerStatusScheduler.
inline constexpr char kDozeModePowerStatusSchedulerDailySample[] =
    "ash.performance.doze_mode.daily_sample";

}  // namespace ash::prefs

#endif  // CHROME_BROWSER_ASH_PERFORMANCE_PREF_NAMES_H_
