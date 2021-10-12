// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_TIME_CALENDAR_VIEW_CONTROLLER_H_

#include <list>
#include <map>
#include <memory>
#include <set>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/common/api_error_codes.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// Controller of the `CalendarView`.
class ASH_EXPORT CalendarViewController {
 public:
  CalendarViewController();
  CalendarViewController(const CalendarViewController& other) = delete;
  CalendarViewController& operator=(const CalendarViewController& other) =
      delete;
  virtual ~CalendarViewController();

  // A simple std::list of calendar events, used to store a single day's events
  // in EventMap.  Not to be confused with google_apis::calendar::EventList,
  // which represents the return value of a query from the GoogleCalendar API.
  using SingleDayEventList = std::list<google_apis::calendar::CalendarEvent>;

  // Maps a day, i.e. midnight on the day of the event's start_time, to a
  // SingleDayEventList.
  using SingleMonthEventMap = std::map<base::Time, SingleDayEventList>;

  // Maps a month, i.e. midnight on the first day of the month, to a
  // SingleMonthEventMap.
  using MonthToEventsMap = std::map<base::Time, SingleMonthEventMap>;

  // The set of months whose events we've already fetched.
  using MonthSet = std::set<base::Time>;

  class Observer : public base::CheckedObserver {
   public:
    // Gets called when `current_date_ ` changes.
    virtual void OnMonthChanged(const base::Time::Exploded current_month) = 0;

    // Invoked when a set of events has been fetched.
    virtual void OnEventsFetched(
        const google_apis::calendar::EventList* events) {}
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Updates the `current_date_`.
  void UpdateMonth(const base::Time current_month_first_date);

  // Gets the first day of the `current_date_`'s month.
  base::Time GetOnScreenMonthFirstDay() const;

  // Gets the first day of the nth-previous month based on the `current_date_`'s
  // month.
  base::Time GetPreviousMonthFirstDay(unsigned int num_months) const;

  // Gets the first day of the nth-next month based on the `current_date_`'s
  // month.
  base::Time GetNextMonthFirstDay(unsigned int num_months) const;

  // Gets the month name of the `current_date_`'s month.
  std::u16string GetOnScreenMonthName() const;

  // Gets the month name of the next month based on the `current_date_`'s month.
  std::u16string GetNextMonthName() const;

  // Gets the month name of the previous month based `current_date_`'s month.
  std::u16string GetPreviousMonthName() const;

  // Get the current date, which can be today or the first day of the current
  // month if current month is not today's month.
  base::Time current_date() { return current_date_; }

  // Getters of the today's row position, top and bottom.
  int GetTodayRowTopHeight() const;
  int GetTodayRowBottomHeight() const;

  // Getters and setters of the today's row number and row height.
  int today_row() const { return today_row_; }
  void set_today_row(int row) { today_row_ = row; }
  int row_height() const { return row_height_; }
  void set_row_height(int height) { row_height_ = height; }

  // Requests more events as needed.
  virtual void FetchEvents();

  // Returns true if `day` contains any events we've previously fetched, false
  // otherwise. If `events` is non-nullptr then we assign it to the EventList
  // for `day`. Callers should NOT cache `events` themselves, and should
  // instead just call this method again if they need to.
  bool IsDayWithEvents(base::Time day, SingleDayEventList* events) const;

 protected:
  // Invoked when events requested via FetchEvents() are ready, or if the
  // request failed.
  void OnCalendarEventsFetched(
      google_apis::ApiErrorCode error,
      std::unique_ptr<google_apis::calendar::EventList> events);

 private:
  // Insert a single |event| in the EventCache.
  void InsertEvent(const google_apis::calendar::CalendarEvent* event);

  // Insert a single |event| in the EventMap for the month that contains its
  // start date.
  void InsertEventInMonth(SingleMonthEventMap& month,
                          const google_apis::calendar::CalendarEvent* event);

  // Insert EventList |events| in the EventCache.
  void InsertEvents(
      const std::unique_ptr<google_apis::calendar::EventList>& events);

  // Returns true if we've already fetched events for |start_of_month| since the
  // calendar was opened, false otherwise.
  bool IsMonthAlreadyFetched(base::Time start_of_month) const;

  // Fetch events for |start_of_month| if we haven't already done so since the
  // calendar was opened.  This registers our callback OnCalendarEventsFetched.
  void MaybeFetchMonth(base::Time start_of_month);

  // The current date, which can be today or the first day of the current month
  // if current month is not today's month.
  base::Time current_date_;

  base::ObserverList<Observer> observers_;

  // Internal storage for fetched events, with each fetched month having a map
  // of days to events.
  MonthToEventsMap event_months_;

  // The today's date cell row number (which is index +1) in its
  // `CalendarMonthView`.
  int today_row_ = 0;

  // Each row's height. Every row should have the same height, so this height is
  // only updated once with today's row.
  int row_height_ = 0;

  base::WeakPtrFactory<CalendarViewController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_VIEW_CONTROLLER_H_
