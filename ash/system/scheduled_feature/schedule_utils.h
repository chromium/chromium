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
  // The most recent `SunsetToSunriseCheckpoint` that was hit.
  SunsetToSunriseCheckpoint current_checkpoint;
  // The next `SunsetToSunriseCheckpoint` that will be hit.
  SunsetToSunriseCheckpoint next_checkpoint;
  // Time from now until the `next_checkpoint`.
  base::TimeDelta time_until_next_checkpoint;
};

// Returns the current position in the schedule using local `sunrise_time`
// and `sunset_time`. The date of the provided sunrise/sunset times are
// irrelevant; their corresponding times of day are extracted and used
// internally.
ASH_EXPORT Position GetCurrentPosition(const base::Time sunrise_time,
                                       const base::Time sunset_time,
                                       const base::Time now);

// Shifts `time_in` by a whole number of days such that it's < 1 day from the
// `origin`:
// `origin` <= output < `origin` + 24 hours
ASH_EXPORT base::Time ShiftWithinOneDayFrom(const base::Time origin,
                                            const base::Time time_in);

}  // namespace ash::schedule_utils

#endif  // ASH_SYSTEM_SCHEDULED_FEATURE_SCHEDULE_UTILS_H_
