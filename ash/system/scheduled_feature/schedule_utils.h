// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SCHEDULED_FEATURE_SCHEDULE_UTILS_H_
#define ASH_SYSTEM_SCHEDULED_FEATURE_SCHEDULE_UTILS_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/schedule_enums.h"
#include "base/time/time.h"

namespace ash::schedule_utils {

struct ASH_EXPORT Position {
  // The most recent `ScheduleCheckpoint` that was hit.
  ScheduleCheckpoint current_checkpoint;
  // The next `ScheduleCheckpoint` that will be hit.
  ScheduleCheckpoint next_checkpoint;
  // Time from now until the `next_checkpoint`.
  base::TimeDelta time_until_next_checkpoint;
};

// Returns the current position in a schedule where the feature is enabled from
// the `start_time` until the `end_time` with the specified `schedule_type`. The
// date of the provided start/end times are irrelevant; their corresponding
// times of day are extracted and used internally.
ASH_EXPORT Position GetCurrentPosition(const base::Time now,
                                       const base::Time start_time,
                                       const base::Time end_time,
                                       const ScheduleType schedule_type);

// Shifts `time_in` by a whole number of days such that it's < 1 day from the
// `origin`:
// `origin` <= output < `origin` + 24 hours
ASH_EXPORT base::Time ShiftWithinOneDayFrom(const base::Time origin,
                                            const base::Time time_in);

}  // namespace ash::schedule_utils

#endif  // ASH_SYSTEM_SCHEDULED_FEATURE_SCHEDULE_UTILS_H_
