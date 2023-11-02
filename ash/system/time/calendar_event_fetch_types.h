// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_EVENT_FETCH_TYPES_H_
#define ASH_SYSTEM_TIME_CALENDAR_EVENT_FETCH_TYPES_H_

namespace ash {

// Errors that indicate an event-fetch failure that did not come from Google
// Calendar API.
enum class CalendarEventFetchInternalErrorCode {
  // Went too long without a response.
  kTimeout = 0,

  kMaxValue = kTimeout,
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_EVENT_FETCH_TYPES_H_
