// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_SCHEDULE_SERVICE_UTILS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_SCHEDULE_SERVICE_UTILS_H_

#include <utility>

#include "base/time/clock.h"
#include "base/time/time.h"

namespace notifications {

using TimePair = std::pair<base::Time, base::Time>;
using TimeDeltaPair = std::pair<base::TimeDelta, base::TimeDelta>;

// Given suggested |morning| and |evening| windows together with current time,
// calculate actual deliver time window from |out_start_time| to |out_end_time|.
// Both windows duration should be within 12 hours, and start of |evening|
// should be later than end of |morning|.
// Return false if the inputs are invalid.
// [begining_of_today,morning_window_end] => deliver on today morning.
// (morning_window_end, evening_window_end] => deliver on today evening.
// (evening_window_end, end_of_today] => deliver on tomorrow morning.
bool NextTimeWindow(base::Clock* clock,
                    const TimeDeltaPair& morning,
                    const TimeDeltaPair& evening,
                    TimePair* out);

// Retrieves the time stamp of a certain hour at a certain day from today.
// |hour| must be in the range of [0, 23].
// |today| is a timestamp to define today, usually caller can directly pass in
// the current system time.
// |day_delta| is the different between the output date and today.
// Returns false if the conversion is failed.
bool ToLocalHour(int hour,
                 const base::Time& today,
                 int day_delta,
                 base::Time* out);

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_SCHEDULE_SERVICE_UTILS_H_
