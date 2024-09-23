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

// Pairs together a `ScheduleCheckpoint` and the time at which it's
// hit.
struct Slot {
  ScheduleCheckpoint checkpoint;
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

// Working with null and infinite `base::Time` instances are invalid and cause
// undue complexity to account for. They should never be provided by the caller.
bool IsValidTimestamp(const base::Time t) {
  return !t.is_null() && !t.is_inf();
}

// The returned vector has one `Slot` per `ScheduleCheckpoint` and is
// sorted by `Slot::time`. The time at which `Slot` <i> ends is by definition
// `Slot` <i + 1>'s `time`. Also note that:
// * The schedule is cyclic. The next `Slot` after the last one is the first
//   `Slot`.
// * The schedule is guaranteed to be centered around "now":
//   * `schedule[0].time` <= `now` < `schedule[0].time + kOneDay`
//   * `schedule[0].time` <= `schedule[i].time` < `schedule[0].time + kOneDay`
//     for all indices <i> in the returned `schedule`.
std::vector<Slot> BuildSchedule(const base::Time now,
                                base::Time start_time,
                                base::Time end_time,
                                const ScheduleType schedule_type) {
  DCHECK(!now.is_null());
  // The `schedule` could theoretically start with any checkpoint because it's
  // cyclic. `end_time` has been picked arbitrarily since it's easiest in the
  // case of a `kSunsetToSunrise` to set the rest of the checkpoints relative to
  // sunrise (`end_time` for that `ScheduleType`).
  //
  // `end_time` must first be shifted by a whole number of days such that
  // `end_time` <= `now` < `end_time + kOneDay`.
  //
  // Example with `schedule_type` == `kSunsetToSunrise`:
  // Start (sunset): 6:00 PM, End (sunrise): 6:00 AM, Now: 3:00 AM
  //
  //                                    3:00    6:00             18:00
  // <---------------------------------- + ----- + --------------- + ----->
  //                                     |       |                 |
  //                                    now   end_time        start_time
  const base::TimeDelta amount_to_advance_end_time =
      (now - end_time).FloorToMultiple(kOneDay);
  end_time += amount_to_advance_end_time;
  //    6:00                            3:00                    18:00
  // <-- + ----------------------------- + ---------------------- + ----->
  //     |                               |                        |
  //  end_time                           now                  start_time
  // (previous day)

  // Shift `start_time` such that
  // `end_time` <= `start_time` < `end_time + kOneDay`.
  start_time = ShiftWithinOneDayFrom(end_time, start_time);
  //    6:00               18:00        3:00    6:00
  // <-- + ----------------- + --------- + ----- + ---------------------->
  //     |                   |           |       |
  //  end_time          start_time      now   end_time
  // (previous day)                          (current day)

  std::vector<Slot> schedule;
  switch (schedule_type) {
    case ScheduleType::kCustom:
      schedule.push_back({ScheduleCheckpoint::kDisabled, end_time});
      schedule.push_back({ScheduleCheckpoint::kEnabled, start_time});
      break;
    case ScheduleType::kSunsetToSunrise: {
      const base::TimeDelta daylight_duration = start_time - end_time;
      DCHECK_GE(daylight_duration, base::TimeDelta());
      schedule.push_back({ScheduleCheckpoint::kSunrise, end_time});
      schedule.push_back(
          {ScheduleCheckpoint::kMorning, end_time + daylight_duration / 3});
      schedule.push_back({ScheduleCheckpoint::kLateAfternoon,
                          end_time + daylight_duration * 5 / 6});
      schedule.push_back({ScheduleCheckpoint::kSunset, start_time});
      break;
    }
    case ScheduleType::kNone:
      NOTREACHED() << "kNone ScheduleType does not support any automatic "
                      "feature changes";
  }
  //    6:00 10:00   16:00 18:00         3:00    6:00
  // <-- + --- + ----- + --- + ---------- + ----- + ---------------------->
  //     |     |       |     |            |       |
  //  end_time morning late sunset       now   end_time
  // (previous day)  afternoon               (current day)
  DVLOG(1) << "Schedule: " << ToString(schedule);
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

Position GetCurrentPosition(const base::Time now,
                            const base::Time start_time,
                            const base::Time end_time,
                            const ScheduleType schedule_type) {
  CHECK(IsValidTimestamp(now));
  CHECK(IsValidTimestamp(start_time));
  CHECK(IsValidTimestamp(end_time));
  const std::vector<Slot> schedule =
      BuildSchedule(now, start_time, end_time, schedule_type);
  DCHECK(!schedule.empty());
  DCHECK_GE(now, schedule.front().time);
  DCHECK_LT(now - schedule.front().time, kOneDay);

  for (size_t idx = 0; idx < schedule.size(); ++idx) {
    const Slot next_slot = GetNextSlot(idx, schedule);
    if (now >= schedule[idx].time && now < next_slot.time) {
      return {schedule[idx].checkpoint, next_slot.checkpoint,
              next_slot.time - now};
    }
  }
  NOTREACHED() << "Failed to find ScheduleCheckpoint for now=" << now
               << " schedule:\n"
               << ToString(schedule);
}

base::Time ShiftWithinOneDayFrom(const base::Time origin,
                                 const base::Time time_in) {
  CHECK(IsValidTimestamp(origin));
  CHECK(IsValidTimestamp(time_in));
  const base::TimeDelta amount_to_advance_time_in =
      (origin - time_in).CeilToMultiple(kOneDay);
  return time_in + amount_to_advance_time_in;
}

}  // namespace ash::schedule_utils
