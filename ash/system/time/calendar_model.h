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
#include "base/observer_list.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/common/api_error_codes.h"

namespace ash {

// A simple std::list of calendar events, used to store a single day's events
// in EventMap. Not to be confused with google_apis::calendar::EventList,
// which represents the return value of a query from the GoogleCalendar API.
using SingleDayEventList = std::list<google_apis::calendar::CalendarEvent>;

// Controller of the `CalendarView`.
class ASH_EXPORT CalendarModel {
 public:
  explicit CalendarModel(const std::set<base::Time> non_prunable_months);
  CalendarModel(const CalendarModel& other) = delete;
  CalendarModel& operator=(const CalendarModel& other) = delete;
  virtual ~CalendarModel();

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

  // Requests events that fall in |months|.
  void FetchEvents(const std::set<base::Time> months);

  // Requests events that fall in the set of non-prunable "base" months.
  void FetchEventsForBaseMonths();

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

  // Insert EventList |events| in the EventCache.
  void InsertEvents(
      const std::unique_ptr<google_apis::calendar::EventList>& events);

  // Free up months of events as needed to keep us within storage limits.
  void PruneEventCache();

  // Invoked when events requested via FetchEvents() are ready, or if the
  // request failed.
  void OnCalendarEventsFetched(
      google_apis::ApiErrorCode error,
      std::unique_ptr<google_apis::calendar::EventList> events);

  // Returns true if we've already fetched events for |start_of_month| since the
  // calendar was opened, false otherwise.
  bool IsMonthAlreadyFetched(base::Time start_of_month) const;

  // Fetch events for |start_of_month| if we haven't already done so since the
  // calendar was opened.  This registers our callback OnCalendarEventsFetched.
  virtual void MaybeFetchMonth(base::Time start_of_month);

  // Officially declare the month denoted by |start_of_month| as "fetched."
  // If the month is non-prunable then we won't attempt to fetch it again unless
  // the calendar is closed and re-opened.  If the month is prunable then we'll
  // attempt a re-fetch if it gets pruned and our visible window includes it
  // again.
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

  // Internal storage for fetched events, with each fetched month having a map
  // of days to events.
  MonthToEventsMap event_months_;

  // Months whose events we've fetched, that are eligible for pruning, in
  // most-recently-used (MRU) order.
  std::deque<base::Time> prunable_months_mru_;

  // The set of months exempt from pruning.
  const std::set<base::Time> non_prunable_months_;

  // The set of months exempt from pruning that have been fetched.
  std::set<base::Time> non_prunable_months_fetched_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<CalendarModel> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_MODEL_H_
