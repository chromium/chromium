// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_unittest_utils.h"

#include <string>

#include "ash/ash_export.h"
#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"

namespace ash {

namespace calendar_test_utils {

std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
    const char* id,
    const char* summary,
    const char* start_time,
    const char* end_time) {
  auto event = std::make_unique<google_apis::calendar::CalendarEvent>();
  base::Time start_time_base, end_time_base;
  google_apis::calendar::DateTime start_time_date, end_time_date;
  event->set_id(id);
  event->set_summary(summary);
  bool result = base::Time::FromString(start_time, &start_time_base);
  DCHECK(result);
  result = base::Time::FromString(end_time, &end_time_base);
  DCHECK(result);
  start_time_date.set_date_time(start_time_base);
  end_time_date.set_date_time(end_time_base);
  event->set_start_time(start_time_date);
  event->set_end_time(end_time_date);
  return event;
}

std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
    const char* id,
    const char* summary,
    base::Time start_time,
    base::Time end_time) {
  auto event = std::make_unique<google_apis::calendar::CalendarEvent>();
  google_apis::calendar::DateTime start_time_date, end_time_date;
  event->set_id(id);
  event->set_summary(summary);
  start_time_date.set_date_time(start_time);
  end_time_date.set_date_time(end_time);
  event->set_start_time(start_time_date);
  event->set_end_time(end_time_date);
  return event;
}

ASH_EXPORT bool IsTheSameMonth(const base::Time& date_a,
                               const base::Time& date_b) {
  return base::TimeFormatWithPattern(date_a, "MM YYYY") ==
         base::TimeFormatWithPattern(date_b, "MM YYYY");
}

}  // namespace calendar_test_utils

}  // namespace ash