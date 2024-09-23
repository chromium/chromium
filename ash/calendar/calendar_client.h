// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CALENDAR_CALENDAR_CLIENT_H_
#define ASH_CALENDAR_CALENDAR_CLIENT_H_

#include <string>

#include "ash/ash_export.h"
#include "google_apis/calendar/calendar_api_requests.h"

namespace base {
class Time;
}  // namespace base

namespace ash {

// Interface for the calendar browser client. Provides calendar api service for
// this client.
class ASH_EXPORT CalendarClient {
 public:
  // Verifies if the Calendar integration is disabled by admin by checking:
  // 1) its own pref `prefs::kCalendarIntegrationEnabled`,
  // 2) that the Calendar web app is disabled by policy,
  // 3) that access to the Calendar web UI is blocked by policy.
  virtual bool IsDisabledByAdmin() const = 0;

  // Fetches a list of calendars based on the current client's account.
  //
  // `callback` will be called when response or google_apis's ERROR (if the call
  // is not successful) is returned. `google_apis::OTHER_ERROR` will be passed
  // in the `callback` if the current client has no valid calendar service,
  // e.g. a guest user.
  //
  // The returned `base::OnceClosure` callback can cancel the api call and get
  // the `google_apis::CANCEL` error before the response is back.
  virtual base::OnceClosure GetCalendarList(
      google_apis::calendar::CalendarListCallback callback) = 0;

  // Fetches primary calendar events based on the current client's account.
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
      const base::Time start_time,
      const base::Time end_time) = 0;

  // Fetches events belonging to the calendar with an ID matching `calendar_id`
  // for the current client's account.
  //
  // `callback` will be called when response or google_apis's ERROR (if the call
  // is not successful) is returned. `google_apis::OTHER_ERROR` will be passed
  // in the `callback` if the current client has no valid calendar service,
  // e.g. a guest user.
  //
  // `end_time` must be greater than `start_time`.
  //
  // Events in the retrieved list with an empty colorId member will take on the
  // value of `calendar_color_id`.
  //
  // The returned `base::OnceClosure` callback can cancel the api call and get
  // the `google_apis::CANCEL` error before the response is back.
  virtual base::OnceClosure GetEventList(
      google_apis::calendar::CalendarEventListCallback callback,
      const base::Time start_time,
      const base::Time end_time,
      const std::string& calendar_id,
      const std::string& calendar_color_id) = 0;

 protected:
  virtual ~CalendarClient() = default;
};

}  // namespace ash

#endif  // ASH_CALENDAR_CALENDAR_CLIENT_H_
