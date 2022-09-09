// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/schedule_service_utils.h"

#include "base/check_op.h"

namespace notifications {
namespace {

bool ValidateTimeWindow(const TimeDeltaPair& window) {
  return (window.second - window.first < base::Hours(12) &&
          window.second >= window.first);
}

}  // namespace

bool ToLocalHour(int hour,
                 const base::Time& today,
                 int day_delta,
                 base::Time* out) {
  DCHECK_GE(hour, 0);
  DCHECK_LE(hour, 23);
  DCHECK(out);

  // Gets the local time at |hour| in yesterday.
  base::Time another_day = today + base::Days(day_delta);
  base::Time::Exploded another_day_exploded;
  another_day.LocalExplode(&another_day_exploded);
  another_day_exploded.hour = hour;
  another_day_exploded.minute = 0;
  another_day_exploded.second = 0;
  another_day_exploded.millisecond = 0;

  // Converts local exploded time to time stamp.
  return base::Time::FromLocalExploded(another_day_exploded, out);
}

bool NextTimeWindow(base::Clock* clock,
                    const TimeDeltaPair& morning,
                    const TimeDeltaPair& evening,
                    TimePair* out) {
  auto now = clock->Now();
  base::Time beginning_of_today;
  // verify the inputs.
  if (!ToLocalHour(0, now, 0, &beginning_of_today) ||
      !ValidateTimeWindow(morning) || !ValidateTimeWindow(evening) ||
      morning.second > evening.first) {
    return false;
  }

  auto today_morning_window = std::pair<base::Time, base::Time>(
      beginning_of_today + morning.first, beginning_of_today + morning.second);
  if (now <= today_morning_window.second) {
    *out = std::move(today_morning_window);
    return true;
  }

  auto today_evening_window = std::pair<base::Time, base::Time>(
      beginning_of_today + evening.first, beginning_of_today + evening.second);
  if (now <= today_evening_window.second) {
    *out = std::move(today_evening_window);
    return true;
  }

  // tomorrow morning window.
  *out = std::pair<base::Time, base::Time>(
      beginning_of_today + base::Days(1) + morning.first,
      beginning_of_today + base::Days(1) + morning.second);
  return true;
}

}  // namespace notifications
