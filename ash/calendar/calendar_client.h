// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CALENDAR_CALENDAR_CLIENT_H_
#define ASH_CALENDAR_CALENDAR_CLIENT_H_

#include "ash/ash_export.h"
#include "google_apis/calendar/calendar_api_requests.h"

namespace ash {

// Interface for the calendar browser client. Provides calendar api service for
// this client.
class ASH_EXPORT CalendarClient {
 public:
  // Fetches calendar events based on the current client's account.
  //
  // `callback` will be called when response or google_apis's ERROR (if the call
  // is not successful) is returned. `google_apis::OTHER_ERROR` will be passed
  // in the `callback` if the current client has no valid calendar service,
  // e.g. a guest user.
  //
  // `end_time` must be greater than `start_time`.
  //
  // The returned `base::OnceClosure` callback can cancel the api call and get
  // the `google_apis::CANCEL` error before the response is back.
  virtual base::OnceClosure GetEventList(
      google_apis::calendar::CalendarEventListCallback callback,
      const base::Time& start_time,
      const base::Time& end_time) = 0;

 protected:
  virtual ~CalendarClient() = default;
};

}  // namespace ash

#endif  // ASH_CALENDAR_CALENDAR_CLIENT_H_
