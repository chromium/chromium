// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/scheduled_feature/schedule_utils.h"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"

namespace ash::schedule_utils {

namespace {

constexpr base::TimeDelta kOneDay = base::Days(1);

// Pairs together a `SunsetToSunriseCheckpoint` and the time at which it's
// hit.
struct Slot {
  SunsetToSunriseCheckpoint checkpoint;
  base::Time time;
};

// For debugging purposes only.
std::string ToString(const std::vector<Slot>& schedule) {
  std::stringstream ss;
  ss << std::endl;
  for (const Slot& slot : schedule) {
    ss << static_cast<int>(slot.checkpoint) << ": " << slot.time << std::endl;
  }
  return ss.str();
}

// The returned vector has one `Slot` per `SunsetToSunriseCheckpoint` and is
// sorted by `Slot::time`. The time at which `Slot` <i> ends is by definition
// `Slot` <i + 1>'s `time`. Also note that:
// * The schedule is cyclic. The next `Slot` after the last one is the first
//   `Slot`.
// * The schedule is guaranteed to be centered around "now":
//   * `schedule[0].time` <= `now` < `schedule[0].time + kOneDay`
//   * `schedule[0].time` <= `schedule[i].time` < `schedule[0].time + kOneDay`
//     for all indices <i> in the returned `schedule`.
std::vector<Slot> BuildSchedule(base::Time sunrise_time,
                                base::Time sunset_time,
                                const base::Time now) {
  DCHECK(!now.is_null());
  // The `schedule` could theoretically start with any checkpoint because it's
  // cyclic. Sunrise has been picked arbitrarily since it's easiest to set the
  // rest of the checkpoints relative to it.
  //
  // Sunrise must first be shifted by a whole number of days such that
  // `sunrise_time` <= `now` < `sunrise_time + kOneDay`.
  const base::TimeDelta amount_to_advance_sunrise =
      (now - sunrise_time).FloorToMultiple(kOneDay);
  sunrise_time += amount_to_advance_sunrise;

  // Shift `sunset_time` such that
  // `sunrise_time` <= `sunset_time` < `sunrise_time + kOneDay`.
  sunset_time = ShiftWithinOneDayFrom(sunrise_time, sunset_time);

  const base::TimeDelta daylight_duration = sunset_time - sunrise_time;
  DCHECK_GE(daylight_duration, base::TimeDelta());
  std::vector<Slot> schedule;
  schedule.push_back({SunsetToSunriseCheckpoint::kSunrise, sunrise_time});
  schedule.push_back({SunsetToSunriseCheckpoint::kMorning,
                      sunrise_time + daylight_duration / 3});
  schedule.push_back({SunsetToSunriseCheckpoint::kLateAfternoon,
                      sunrise_time + daylight_duration * 5 / 6});
  schedule.push_back({SunsetToSunriseCheckpoint::kSunset, sunset_time});
  DVLOG(1) << "Sunset-to-sunrise schedule: " << ToString(schedule);
  return schedule;
}

// Accounts for the fact that `schedule` is cyclic: When `current_idx`
// refers to the last `Slot`, the next `Slot` is actually the first `Slot` with
// its timestamp advanced by one day.
Slot GetNextSlot(const size_t current_idx, const std::vector<Slot>& schedule) {
  DCHECK(!schedule.empty());
  DCHECK_LT(current_idx, schedule.size());
  for (size_t next_idx = current_idx + 1; next_idx < schedule.size();
       ++next_idx) {
    // Some extremely rare corner cases where the next `Slot`'s time could be
    // exactly equal to the current `Slot` instead of greater than it:
    // * Sunrise and sunset are exactly the same time in a geolocation where
    //   there is literally no night or no daylight.
    // * Sunrise and sunset are a couple microseconds apart, leaving
    //   `base::Time` without enough resolution to fit morning and afternoon
    //   between them at unique times.
    // Therefore, this iterates from the current `Slot` until the next `Slot` is
    // found with a greater time.
    if (schedule[next_idx].time > schedule[current_idx].time) {
      return schedule[next_idx];
    }
  }
  return {schedule.front().checkpoint, schedule.front().time + kOneDay};
}

}  // namespace

Position GetCurrentPosition(const base::Time sunrise_time,
                            const base::Time sunset_time,
                            const base::Time now) {
  const std::vector<Slot> schedule =
      BuildSchedule(sunrise_time, sunset_time, now);
  DCHECK(!schedule.empty());
  DCHECK_GE(now, schedule.front().time);
  DCHECK_LT(now - schedule.front().time, kOneDay);
  DVLOG(1) << "Sunset-to-sunrise schedule: " << ToString(schedule);

  for (size_t idx = 0; idx < schedule.size(); ++idx) {
    const Slot next_slot = GetNextSlot(idx, schedule);
    if (now >= schedule[idx].time && now < next_slot.time) {
      return {schedule[idx].checkpoint, next_slot.checkpoint,
              next_slot.time - now};
    }
  }
  NOTREACHED() << "Failed to find SunsetToSunriseCheckpoint for now=" << now
               << " schedule:\n"
               << ToString(schedule);
  return Position();
}

base::Time ShiftWithinOneDayFrom(const base::Time origin,
                                 const base::Time time_in) {
  const base::TimeDelta amount_to_advance_time_in =
      (origin - time_in).CeilToMultiple(kOneDay);
  return time_in + amount_to_advance_time_in;
}

}  // namespace ash::schedule_utils
