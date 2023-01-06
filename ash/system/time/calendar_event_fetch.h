// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_EVENT_FETCH_H_
#define ASH_SYSTEM_TIME_CALENDAR_EVENT_FETCH_H_

#include "ash/ash_export.h"
#include "ash/system/time/calendar_event_fetch_types.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/common/api_error_codes.h"

namespace base {

class TickClock;

}  // namespace base

namespace ash {
// Represents a single fetch of a given month's calendar events. Fetch begins
// immediately on construction.
class ASH_EXPORT CalendarEventFetch {
 public:
  // A callback invoked when a fetch of calendar events is complete.
  using FetchCompleteCallback =
      base::OnceCallback<void(base::Time start_of_month,
                              google_apis::ApiErrorCode error,
                              const google_apis::calendar::EventList* events)>;

  // A callback invoked when a fetch of calendar events did not complete, due
  // to an internal error.
  using FetchInternalErrorCallback =
      base::OnceCallback<void(base::Time start_of_month,
                              CalendarEventFetchInternalErrorCode error)>;

  CalendarEventFetch(const base::Time& start_of_month,
                     FetchCompleteCallback complete_callback,
                     FetchInternalErrorCallback internal_error_callback,
                     const base::TickClock* tick_clock);
  CalendarEventFetch(const CalendarEventFetch& other) = delete;
  CalendarEventFetch& operator=(const CalendarEventFetch& other) = delete;
  ~CalendarEventFetch();

  // Cancels the fetch request, invokes `cancel_closure_`.
  void Cancel();

 private:
  // Sends the request for an event list. Cancels any in-progress fetch request.
  void SendFetchRequest();

  // Callback invoked when results of a fetch are available.
  void OnResultReceived(
      google_apis::ApiErrorCode error,
      std::unique_ptr<google_apis::calendar::EventList> events);

  // Callback invoked when we've gone too long without receiving a response.
  void OnTimeout();

  // Start of the month whose events we're fetching.
  const base::Time start_of_month_;

  // Fetch start/end times.
  const std::pair<base::Time, base::Time> time_range_;

  // Callback invoked when the fetch is complete.
  FetchCompleteCallback complete_callback_;

  // Callback invoked when the fetch failed with an internal error.
  FetchInternalErrorCallback internal_error_callback_;

  // Timestamp of the start of the fetch, used for duration metrics.
  const base::Time fetch_start_time_;

  // Timer we run at the start of a fetch, to ensure that we terminate if we
  // go too long without a response.
  base::OneShotTimer timeout_;

  // Closure to be invoked if the request needs to be canceled.
  base::OnceClosure cancel_closure_;

  base::WeakPtrFactory<CalendarEventFetch> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_EVENT_FETCH_H_
