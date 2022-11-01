// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_EVENT_DATE_FORMATTER_UTIL_H_
#define ASH_SYSTEM_TIME_EVENT_DATE_FORMATTER_UTIL_H_

#include <tuple>

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"

namespace ash::event_date_formatter_util {

ASH_EXPORT const std::tuple<std::u16string, std::u16string>
GetStartAndEndTimeAccessibleNames(base::Time start_time, base::Time end_time);

// Returns a string containing the event start and end times "nn:nn - nn:nn".
ASH_EXPORT const std::u16string GetFormattedInterval(base::Time start_time,
                                                     base::Time end_time);

// Returns "Starts [Ends] at hh:nn (Day n / n)" for multi-day events.
// Returns "(Day n / n)" for all day events.
ASH_EXPORT const std::u16string GetMultiDayText(
    const google_apis::calendar::CalendarEvent* event,
    const base::Time& selected_date_midnight,
    const base::Time& selected_date_midnight_utc);
}  // namespace ash::event_date_formatter_util

#endif  // ASH_SYSTEM_TIME_EVENT_DATE_FORMATTER_UTIL_H_
