// Copyright 2022 The Chromium Authors
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

namespace ash {

namespace {
// A cmp function is needed to create a set of
// `google_apis::calendar::CalendarEvent`.
struct CmpEvent {
  bool operator()(const google_apis::calendar::CalendarEvent& event1,
                  const google_apis::calendar::CalendarEvent& event2) const {
    return event1.start_time().date_time() < event2.start_time().date_time();
  }
};
}  // namespace

class CalendarEventFetch;

// A simple std::list of calendar events, used to store a single day's events
// in EventMap. Not to be confused with google_apis::calendar::EventList,
// which represents the return value of a query from the GoogleCalendar API.
using SingleDayEventList = std::list<google_apis::calendar::CalendarEvent>;

// Controller of the `CalendarView`.
class ASH_EXPORT CalendarModel : public SessionObserver {
 public:
  // `kNa` is used to represent the fetching status when on the non-logged-in
  // screens. If `kNa` is returned, the loading bar won't be shown since events
  // are not being fetched.
  // `kRefetching` is used to represent the fetching status when there're cached
  // events for a month but another fetching request has been sent. In this
  // case, the event indicator dots and the tooltip will show the cached events.
  // `kNever` is used as the default state when we experience an error or
  // timeout or haven't fetched anything.
  enum FetchingStatus { kNever, kFetching, kRefetching, kSuccess, kError, kNa };

  CalendarModel();
  CalendarModel(const CalendarModel& other) = delete;
  CalendarModel& operator=(const CalendarModel& other) = delete;
  ~CalendarModel() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // Maps a day, i.e. midnight on the day of the event's start_time, to a
  // SingleDayEventList.
  using SingleMonthEventMap = std::map<base::Time, SingleDayEventList>;

  // Maps a month, i.e. midnight on the first day of the month, to a
  // SingleMonthEventMap.
  using MonthToEventsMap = std::map<base::Time, SingleMonthEventMap>;

  class Observer : public base::CheckedObserver {
   public:
    // Invoked when a month of events has been fetched.
    virtual void OnEventsFetched(const FetchingStatus status,
                                 const base::Time start_time) {}
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Completely, unconditionally clears out any cached events. Intended for when
  // we log out or switch users.
  void ClearAllCachedEvents();

  // Clears out all events that start in a non-prunable month.
  void ClearAllPrunableEvents();

  // Returns true if the event storage for a given month is populated with at
  // least one event.
  bool MonthHasEvents(const base::Time start_of_month);

  // Logs to UMA all event fetch metrics recorded over the lifetime of a
  // calendar session.
  void UploadLifetimeMetrics();

  // Adds `month` to the set of non-prunable months.
  void AddNonPrunableMonth(const base::Time& month);

  // Adds every month in `months` to the set of non-prunable months.
  void AddNonPrunableMonths(const std::set<base::Time>& months);

  // Requests events for the month starting at `start_of_month` if there is not
  // currently a calendar list fetch in progress.
  void MaybeFetchEvents(base::Time start_of_month);

  // Requests events for the month starting at `start_of_month`. If there is no
  // calendar list, or Multi-Calendar is disabled, only primary calendar events
  // are fetched.
  void FetchEvents(base::Time start_of_month);

  // Requests events for the month starting at `start_of_month` for the primary
  // calendar.
  void FetchPrimaryCalendarEvents(const base::Time start_of_month);

  // Cancels any pending event fetch for `start_of_month`.
  void CancelFetch(const base::Time& start_of_month);

  // Gets the number of events of the passed in day.
  int EventsNumberOfDay(base::Time day, SingleDayEventList* events);

  // Finds the event list of the given day, with no impact on our MRU list.  Use
  // this if you don't care about making the month in which `day` resides less
  // likely to be pruned if we need to trim down to stay within storage limits.
  SingleDayEventList FindEvents(base::Time day) const;

  // Uses the `FindEvents` method to get events for that day and then filters
  // the result into two lists of multi-day and same day events.
  std::tuple<SingleDayEventList, SingleDayEventList>
  FindEventsSplitByMultiDayAndSameDay(base::Time day) const;

  // Uses the `FindEvents` method to get events for that day and then filters
  // the result into events that start or end in the next two hours.
  std::list<google_apis::calendar::CalendarEvent> FindUpcomingEvents(
      base::Time now_local) const;

  // Checks the `FetchingStatus` of a given start time.
  FetchingStatus FindFetchingStatus(base::Time start_time);

  // Redistributes all the fetched events to the date map with the
  // time difference. This method is only called when there's a timezone change.
  void RedistributeEvents();

 private:
  // For unit tests.
  friend class CalendarModelTest;
  friend class CalendarMonthViewFetchTest;
  friend class CalendarMonthViewTest;
  friend class CalendarUpNextViewAnimationTest;
  friend class CalendarUpNextViewPixelTest;
  friend class CalendarUpNextViewTest;
  friend class CalendarViewPixelTest;
  friend class CalendarViewAnimationTest;
  friend class CalendarViewEventListViewFetchTest;
  friend class CalendarViewEventListViewTest;
  friend class CalendarViewTest;
  friend class CalendarViewWithUpNextViewAnimationTest;
  friend class CalendarViewWithUpNextViewTest;

  // Checks if the event has allowed statuses and is eligible for insertion.
  bool ShouldInsertEvent(
      const google_apis::calendar::CalendarEvent* event) const;

  // Inserts a single `event` that spans more than one day in the EventCache.
  void InsertMultiDayEvent(const google_apis::calendar::CalendarEvent* event,
                           const base::Time start_of_month);

  // Finds or creates a new SingleMonthEventMap to then insert the `event` into
  // an event list of that month.
  void InsertEventInMonth(const google_apis::calendar::CalendarEvent* event,
                          const base::Time start_of_month,
                          const base::Time start_time_midnight);

  // Inserts a single `event` in a SingleDayEventList of a SingleMonthEventMap.
  void InsertEventInMonthEventList(
      SingleMonthEventMap& month,
      const google_apis::calendar::CalendarEvent* event,
      const base::Time start_time_midnight);

  // Frees up months of events as needed to keep us within storage limits.
  void PruneEventCache();

  // Moves this month to the top of our queue that's ordered from
  // most-recently-used to least-recently-used.
  void PromoteMonth(base::Time start_of_month);

  // Based on error(s) received during a month's fetch(es), notify observers.
  void NotifyObservers(base::Time start_of_month);

  // Actual callback invoked when an event fetch is complete.
  void OnEventsFetched(base::Time start_of_month,
                       std::string calendar_id,
                       google_apis::ApiErrorCode error,
                       const google_apis::calendar::EventList* events);

  // Callback invoked when an event fetch failed with an internal error.
  void OnEventFetchFailedInternalError(
      base::Time start_of_month,
      std::string calendar_id,
      CalendarEventFetchInternalErrorCode error);

  // Internal storage for fetched events, with each fetched month having a
  // map of days to events.
  MonthToEventsMap event_months_;

  // Months whose events we've fetched, in most-recently-used (MRU) order.
  std::deque<base::Time> mru_months_;

  // Set of months that are not prunable.
  std::set<base::Time> non_prunable_months_;

  // Set of months we've already fetched.
  std::set<base::Time> months_fetched_;

  // All fetch requests that are still in-progress. Maps each start of month
  // timestamp to a map linking each calendar ID to a CalendarEventFetch
  // object.
  std::map<base::Time,
           std::map<std::string, std::unique_ptr<CalendarEventFetch>>>
      pending_fetches_;

  // Maps a month to a set of error codes returned by the month's event
  // fetches.
  std::map<base::Time, std::set<google_apis::ApiErrorCode>> fetch_error_codes_;

  // Timestamp of the start of the first event fetch created, for use in
  // duration metrics when Multi-Calendar is enabled.
  base::TimeTicks fetches_start_time_;

  // Maps a non-prunable month to an indicator that equals true if new event
  // fetches for the month have completed successfully.
  std::map<base::Time, bool> events_have_fetched_;

  ScopedSessionObserver session_observer_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<CalendarModel> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_MODEL_H_
