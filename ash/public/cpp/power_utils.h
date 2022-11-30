// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_POWER_UTILS_H_
#define ASH_PUBLIC_CPP_POWER_UTILS_H_

#include "ash/public/cpp/ash_public_export.h"

namespace base {
class TimeDelta;
}

namespace ash {

namespace power_utils {

// Returns true if |time| should be displayed in the UI. Less-than-a-minute or
// very large values aren't displayed.
bool ASH_PUBLIC_EXPORT ShouldDisplayBatteryTime(const base::TimeDelta& time);

// Returns the battery's remaining charge, rounded to an integer with a
// maximum value of 100.
int ASH_PUBLIC_EXPORT GetRoundedBatteryPercent(double battery_percent);

// Copies the hour and minute components of |time| to |hours| and |minutes|.
// The minute component is rounded rather than truncated: a |time| value
// corresponding to 92 seconds will produce a |minutes| value of 2, for
// example.
void ASH_PUBLIC_EXPORT SplitTimeIntoHoursAndMinutes(const base::TimeDelta& time,
                                                    int* hours,
                                                    int* minutes);

}  // namespace power_utils

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_POWER_UTILS_H_
