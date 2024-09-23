// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_LIST_MODEL_H_
#define ASH_SYSTEM_TIME_CALENDAR_LIST_MODEL_H_

#include <list>
#include <optional>

#include "ash/ash_export.h"
#include "ash/calendar/calendar_client.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/time/calendar_event_fetch_types.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/common/api_error_codes.h"

namespace ash {

// A list of calendars used as an internal cache for the calendar list model.
using CalendarList = std::list<google_apis::calendar::SingleCalendar>;

// Retrieves the calendar list, to be persisted in an internal cache for the
// life of the Calendar View.
class ASH_EXPORT CalendarListModel : public SessionObserver {
 public:
  CalendarListModel();
  CalendarListModel(const CalendarListModel& other) = delete;
  CalendarListModel& operator=(const CalendarListModel& other) = delete;
  ~CalendarListModel() override;

  class Observer : public base::CheckedObserver {
   public:
    // Invoked when a calendar list fetch has been completed.
    virtual void OnCalendarListFetchComplete() {}
  };

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Fetches the user's calendar list. Cancels any calendar list fetch already
  // in progress.
  void FetchCalendars();

  // Cancels any calendar list fetch already in progress.
  void CancelFetch();

  // Returns true if the calendar list is currently cached.
  bool get_is_cached() { return is_cached_; }

  // Returns true if the calendar list is currently being fetched.
  bool get_fetch_in_progress() { return fetch_in_progress_; }

  // Returns true if the calendar list is currently cached and there is no
  // calendar list fetch in progress.
  bool list_cached_and_no_fetch_in_progress() {
    return (is_cached_ && !fetch_in_progress_);
  }

  // Returns the currently cached calendar list.
  // The calendar list should have at least one entry (for the primary calendar)
  // if the account that authorized the fetch uses Google Calendar.
  CalendarList GetCachedCalendarList();

 private:
  // For unit tests.
  friend class CalendarUpNextViewPixelTest;
  friend class CalendarViewWithUpNextViewAnimationTest;
  friend class CalendarViewWithUpNextViewTest;
  friend class PostLoginGlanceablesMetricsRecorderTest;

  // A callback invoked when the calendar list fetch is complete. If the fetch
  // was successful, the cached calendar list is replaced. If the fetch failed
  // and a calendar list is already cached, the calendar list is not modified.
  void OnCalendarListFetched(
      google_apis::ApiErrorCode error,
      std::unique_ptr<google_apis::calendar::CalendarList> calendars);

  // A callback invoked when the calendar list fetch timed out.
  void OnCalendarListFetchTimeout();

  // Clears out any existing cached calendar list. Intended for when we log out
  // or switch users.
  void ClearCachedCalendarList();

  // Cancels any fetch currently in progress and clears out any existing cache.
  void ClearCacheAndCancelFetch();

  // The most recently fetched calendar list. Gets overwritten after a
  // completed fetch.
  CalendarList calendar_list_;

  // Indicates whether there is a fetch in progress.
  bool fetch_in_progress_ = false;

  // Indicates whether the calendar list is currently cached.
  bool is_cached_ = false;

  // Timestamp of the start of the fetch, used for duration metrics.
  base::TimeTicks fetch_start_time_;

  // Timer we run at the start of a fetch, to ensure that we terminate if we
  // go too long without a response.
  base::OneShotTimer timeout_;

  // Closure to be invoked if the fetch needs to be canceled.
  base::OnceClosure cancel_closure_;

  ScopedSessionObserver session_observer_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<CalendarListModel> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_LIST_MODEL_H_
