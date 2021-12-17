// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_TIME_CALENDAR_VIEW_CONTROLLER_H_

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
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

// A simple std::list of calendar events, used to store a single day's events
// in EventMap. Not to be confused with google_apis::calendar::EventList,
// which represents the return value of a query from the GoogleCalendar API.
using SingleDayEventList = std::list<google_apis::calendar::CalendarEvent>;

}  // namespace

// Controller of the `CalendarView`.
class ASH_EXPORT CalendarViewController {
 public:
  CalendarViewController();
  CalendarViewController(const CalendarViewController& other) = delete;
  CalendarViewController& operator=(const CalendarViewController& other) =
      delete;
  virtual ~CalendarViewController();

  // Maps a day, i.e. midnight on the day of the event's start_time, to a
  // SingleDayEventList.
  using SingleMonthEventMap = std::map<base::Time, SingleDayEventList>;

  // Maps a month, i.e. midnight on the first day of the month, to a
  // SingleMonthEventMap.
  using MonthToEventsMap = std::map<base::Time, SingleMonthEventMap>;

  class Observer : public base::CheckedObserver {
   public:
    // Gets called when `current_date_ ` changes.
    virtual void OnMonthChanged(const base::Time::Exploded current_month) {}

    // Invoked when a set of events has been fetched.
    virtual void OnEventsFetched(
        const google_apis::calendar::EventList* events) {}

    // Invoked when a date cell is clicked to open the event list.
    virtual void OpenEventList() {}

    // Invoked when the close button is clicked to close the event list.
    virtual void CloseEventList() {}

    // Invoked when the selected date is updated in the
    // `CalendarViewController`.
    virtual void OnSelectedDateUpdated() {}
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Updates the `current_date_`.
  void UpdateMonth(const base::Time current_month_first_date);

  // Gets the first day of the `current_date_`'s month, in local time.
  base::Time GetOnScreenMonthFirstDayLocal() const;

  // Gets the first day of the nth-previous month based on the `current_date_`'s
  // month, in local time.
  base::Time GetPreviousMonthFirstDayLocal(unsigned int num_months) const;

  // Gets the first day of the nth-next month based on the `current_date_`'s
  // month, in local time.
  base::Time GetNextMonthFirstDayLocal(unsigned int num_months) const;

  // Gets the first day of the `current_date_`'s month, in UTC time.
  base::Time GetOnScreenMonthFirstDayUTC() const;

  // Gets the first day of the nth-previous month based on the `current_date_`'s
  // month, in UTC time.
  base::Time GetPreviousMonthFirstDayUTC(unsigned int num_months) const;

  // Gets the first day of the nth-next month based on the `current_date_`'s
  // month, in UTC time.
  base::Time GetNextMonthFirstDayUTC(unsigned int num_months) const;

  // Gets the month name of the `current_date_`'s month.
  std::u16string GetOnScreenMonthName() const;

  // Gets the month name of the next month based on the `current_date_`'s month.
  std::u16string GetNextMonthName() const;

  // Gets the month name of the previous month based `current_date_`'s month.
  std::u16string GetPreviousMonthName() const;

  // Get the current date, which can be today or the first day of the current
  // month if current month is not today's month.
  base::Time current_date() { return current_date_; }

  // Returns true if before getting to the on-screen-month, it was showing a
  // later month; returns false if it was showing an earlier month. This is used
  // to define the animation directions for updating the header and month views.
  bool was_on_later_month() { return was_on_later_month_; }

  // The currently selected date to show the event list.
  absl::optional<base::Time::Exploded> selected_date() {
    return selected_date_;
  }

  // The row index of the currently selected date. This is used for auto
  // scrolling to this row when the event list is expanded.
  int selected_date_row_index() { return selected_date_row_index_; }

  // Getters and setters: the row index when the event list view is showing,
  // today's row number and today's row height.
  int GetExpandedRowIndex() const;
  void set_expanded_row_index(int row_index) {
    expanded_row_index_ = row_index;
  }
  int today_row() const { return today_row_; }
  void set_today_row(int row) { today_row_ = row; }
  int row_height() const { return row_height_; }
  void set_row_height(int height) { row_height_ = height; }

  // Getters of the today's row position, top and bottom.
  int GetTodayRowTopHeight() const;
  int GetTodayRowBottomHeight() const;

  // Requests more events as needed.
  void FetchEvents();

  // The calendar events of the selected date.
  SingleDayEventList SelectedDateEvents();

  // Same as `EventsNumberOfDayInternal`, except that return of any events on
  // `day` constitutes "use" in the most-recently-used sense, so the month that
  // includes day will then be promoted to most-recently-used status.
  int EventsNumberOfDay(base::Time day, SingleDayEventList* events);

  // A callback passed into the`CalendarDateCellView`, which is called when the
  // cell is clicked to show the event list view.
  void ShowEventListView(base::Time::Exploded selected_date, int row_index);

  // A callback passed into the`CalendarEventListView`, which is called when the
  // close button is clicked to close the event list view.
  void CloseEventListView();

  // Gets called when the `CalendarEventListView` is opened.
  void OnEventListOpened();

  // Gets called when the `CalendarEventListView` is closed.
  void OnEventListClosed();

  // If the selected date in the current month. This is used to inform the
  // `CalendarView` if the month should be updated when a date is selected.
  bool IsSelectedDateInCurrentMonth();

 private:
  // For unit tests.
  friend class MockCalendarViewController;
  friend class CalendarViewControllerEventsTest;
  friend class CalendarViewEventListViewTest;

  // Insert a single |event| in the EventCache.
  void InsertEvent(const google_apis::calendar::CalendarEvent* event);

  // Insert a single |event| in the EventMap for the month that contains its
  // start date.
  void InsertEventInMonth(SingleMonthEventMap& month,
                          const google_apis::calendar::CalendarEvent* event);

  // Insert EventList |events| in the EventCache.
  void InsertEvents(
      const std::unique_ptr<google_apis::calendar::EventList>& events);

  // Find the event list of the given day.
  SingleDayEventList FindEvents(base::Time day) const;

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

  // The current date, which can be today or the first day of the current month
  // if current month is not today's month.
  base::Time current_date_;

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

  // The today's date cell row number (which is index +1) in its
  // `CalendarMonthView`.
  int today_row_ = 0;

  // Each row's height. Every row should have the same height, so this height is
  // only updated once with today's row.
  int row_height_ = 0;

  // If before getting to the on-screen-month, it was showing a later month.
  bool was_on_later_month_ = false;

  // If the event list is expanded.
  bool is_event_list_showing_ = false;

  // The currently selected date.
  absl::optional<base::Time::Exploded> selected_date_;

  // The row index of the currently selected date.
  int selected_date_row_index_;

  // The current row index when the event list view is shown.
  int expanded_row_index_;

  // The event list of the currently selected date.
  SingleDayEventList* selected_date_events_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<CalendarViewController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_VIEW_CONTROLLER_H_
