// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_MODEL_H_
#define ASH_SYSTEM_TIME_CALENDAR_MODEL_H_

#include <deque>
#include <list>
#include <map>
#include <memory>
#include <set>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/time/calendar_event_fetch.h"
#include "ash/system/time/calendar_event_fetch_types.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/common/api_error_codes.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class CalendarEventFetch;

// A simple std::list of calendar events, used to store a single day's events
// in EventMap. Not to be confused with google_apis::calendar::EventList,
// which represents the return value of a query from the GoogleCalendar API.
using SingleDayEventList = std::list<google_apis::calendar::CalendarEvent>;

// Controller of the `CalendarView`.
class ASH_EXPORT CalendarModel : public SessionObserver {
 public:
  CalendarModel();
  CalendarModel(const CalendarModel& other) = delete;
  CalendarModel& operator=(const CalendarModel& other) = delete;
  ~CalendarModel() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // Number of months, before and after the month currently on-display, that we
  // cache-ahead.
  static constexpr int kNumSurroundingMonthsCached = 2;

  // Maps a day, i.e. midnight on the day of the event's start_time, to a
  // SingleDayEventList.
  using SingleMonthEventMap = std::map<base::Time, SingleDayEventList>;

  // Maps a month, i.e. midnight on the first day of the month, to a
  // SingleMonthEventMap.
  using MonthToEventsMap = std::map<base::Time, SingleMonthEventMap>;

  class Observer : public base::CheckedObserver {
   public:
    // Invoked when a set of events has been fetched.
    virtual void OnEventsFetched(
        const google_apis::calendar::EventList* events) {}
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Completely, unconditionally clears out any cached events. Intended for when
  // we log out or switch users.
  void ClearAllCachedEvents();

  // Requests events that fall in |months|.
  void FetchEvents(const std::set<base::Time>& months);

  // Requests events that fall in |num_months| months surrounding |day|.
  void FetchEventsSurrounding(int num_months, const base::Time day);

  // Same as `FindEvents`, except that return of any events on `day` constitutes
  // "use" in the most-recently-used sense, so the month that includes day will
  // then be promoted to most-recently-used status.  Use this to get events if
  // you want to make the month in which |day| resides less likely to be pruned
  // if we need to trim down to stay within storage limits.
  int EventsNumberOfDay(base::Time day, SingleDayEventList* events);

  // Find the event list of the given day, with no impact on our MRU list.  Use
  // this if you don't care about making the month in which |day| resides less
  // likely to be pruned if we need to trim down to stay within storage limits.
  SingleDayEventList FindEvents(base::Time day) const;

  // Redistributes all the fetched events to the date map with the
  // `time_difference_minutes_`. This only happens once per calendar view's life
  // cycle.
  void RedistributeEvents(int time_difference_minutes);

  // Updates the time difference in minutes.
  void set_time_difference_minutes(int minutes) {
    time_difference_minutes_ = minutes;
  }

 protected:
  // Fetch events for |start_of_month| if we haven't already done so since the
  // calendar was opened.  This registers our callback OnCalendarEventsFetched.
  virtual void MaybeFetchMonth(base::Time start_of_month);

 private:
  // For unit tests.
  friend class TestableCalendarModel;
  friend class CalendarModelTest;
  friend class CalendarViewEventListViewTest;
  friend class CalendarMonthViewTest;

  // Insert a single |event| in the EventCache.
  void InsertEvent(const google_apis::calendar::CalendarEvent* event);

  // Insert a single |event| in the EventMap for the month that contains its
  // start date.
  void InsertEventInMonth(SingleMonthEventMap& month,
                          const google_apis::calendar::CalendarEvent* event);

  // Returns the event's `start_time` midnight adjusted by the
  // `time_difference_minutes_`. So the each event will be mapped to the date
  // map by the local device/set time.
  base::Time GetStartTimeMidnightAdjusted(
      const google_apis::calendar::CalendarEvent* event) const;

  // Insert EventList |events| in the EventCache.
  void InsertEvents(const google_apis::calendar::EventList* events);

  // Free up months of events as needed to keep us within storage limits.
  void PruneEventCache();

  // Returns true if we've already fetched events for |start_of_month| since the
  // calendar was opened, false otherwise.
  bool IsMonthAlreadyFetched(base::Time start_of_month) const;

  // Officially declare the month denoted by |start_of_month| as "fetched."
  void MarkMonthAsFetched(base::Time start_of_month);

  // Add a month to the queue of months eligible for pruning when we need to
  // limit the amount we cache.
  void QueuePrunableMonth(base::Time start_of_month);

  // Returns the number of events that this `day` contains. If `events` is
  // non-nullptr then we assign it to the EventList for `day`. Callers should
  // NOT cache `events` themselves, and should instead just call this method
  // again if they need to.
  int EventsNumberOfDayInternal(base::Time day,
                                SingleDayEventList* events) const;

  // Actual callback invoked when an event fetch is complete.
  void OnEventsFetched(base::Time start_of_month,
                       google_apis::ApiErrorCode error,
                       const google_apis::calendar::EventList* events);

  // Callback invoked when an event fetch failed with an internal error.
  void OnEventFetchFailedInternalError(
      base::Time start_of_month,
      CalendarEventFetchInternalErrorCode error);

  // Internal storage for fetched events, with each fetched month having a
  // map of days to events.
  MonthToEventsMap event_months_;

  // Months whose events we've fetched, that are eligible for pruning, in
  // most-recently-used (MRU) order.
  std::deque<base::Time> prunable_months_mru_;

  // All fetch requests that are still in-progress.
  std::map<base::Time, std::unique_ptr<CalendarEventFetch>> pending_fetches_;

  // Time difference between the UTC time and the local time in minutes.
  absl::optional<int> time_difference_minutes_;

  ScopedSessionObserver session_observer_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<CalendarModel> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_MODEL_H_
