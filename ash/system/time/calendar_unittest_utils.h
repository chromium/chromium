// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_UNITTEST_UTILS_H_
#define ASH_SYSTEM_TIME_CALENDAR_UNITTEST_UTILS_H_

#include <string>

#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"

namespace ash {

namespace calendar_test_utils {

// A duration to let the animation finish and pass the cool down duration in
// tests.
constexpr base::TimeDelta kAnimationSettleDownDuration = base::Seconds(3);

// Creates a `google_apis::calendar::CalendarEvent` for testing.
std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
    const char* id,
    const char* summary,
    const char* start_time,
    const char* end_time);

}  // namespace calendar_test_utils

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_UNITTEST_UTILS_H_
