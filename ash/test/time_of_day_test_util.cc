// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/time_of_day_test_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// `ASSERT_TRUE()` requires a function signature that returns `void`.
void ToTimeTodayInternal(const TimeOfDay& time_of_day,
                         base::Time* time_today_out) {
  const std::optional<base::Time> time_today = time_of_day.ToTimeToday();
  ASSERT_TRUE(time_today);
  *time_today_out = *time_today;
}

}  // namespace

base::Time ToTimeToday(const TimeOfDay& time_of_day) {
  base::Time time_today;
  ToTimeTodayInternal(time_of_day, &time_today);
  return time_today;
}

}  // namespace ash
