// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_unittest_utils.h"

#include <string>

#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"

namespace ash {

namespace calendar_test_utils {

std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
    const char* id,
    const char* summary,
    const char* start_time,
    const char* end_time) {
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      std::make_unique<google_apis::calendar::CalendarEvent>();
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

}  // namespace calendar_test_utils

}  // namespace ash