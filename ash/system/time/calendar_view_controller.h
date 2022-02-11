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
#include "ash/system/time/calendar_model.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/common/api_error_codes.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// Controller of the `CalendarView`.
class ASH_EXPORT CalendarViewController {
 public:
  CalendarViewController(UnifiedSystemTrayController* controller);
  CalendarViewController(const CalendarViewController& other) = delete;
  CalendarViewController& operator=(const CalendarViewController& other) =
      delete;
  virtual ~CalendarViewController();

  class Observer : public base::CheckedObserver {
   public:
    // Gets called when `current_date_ ` changes.
    virtual void OnMonthChanged(const base::Time::Exploded current_month) {}

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

  // The currently selected date to show the event list.
  absl::optional<base::Time> selected_date() { return selected_date_; }

  // The row index of the currently selected date. This is used for auto
  // scrolling to this row when the event list is expanded.
  int selected_date_row_index() { return selected_date_row_index_; }

  // Getters and setters: the row index when the event list view is showing,
  // today's row number, today's row height and expanded area height.
  int GetExpandedRowIndex() const;
  void set_expanded_row_index(int row_index) {
    expanded_row_index_ = row_index;
  }
  int today_row() const { return today_row_; }
  void set_today_row(int row) { today_row_ = row; }
  int row_height() const { return row_height_; }
  void set_row_height(int height) { row_height_ = height; }
  int expanded_area_available_height() const {
    return expanded_area_available_height_;
  }
  void set_expanded_area_available_height(int height) {
    expanded_area_available_height_ = height;
  }

  int time_difference_hours() { return time_difference_hours_; }

  UnifiedSystemTrayController* unified_system_tray_controller() {
    return unified_system_tray_controller_;
  }

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
  void ShowEventListView(base::Time selected_date, int row_index);

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
  friend class CalendarMonthViewTest;
  friend class CalendarViewEventListViewTest;

  // Find the event list of the given day.
  SingleDayEventList FindEvents(base::Time day) const;

  // The current date, which can be today or the first day of the current month
  // if current month is not today's month.
  base::Time current_date_;

  // The today's date cell row number (which is index +1) in its
  // `CalendarMonthView`.
  int today_row_ = 0;

  // Each row's height. Every row should have the same height, so this height is
  // only updated once with today's row.
  int row_height_ = 0;

  // The expanded area available height, which will be used to set the expanded
  // event list min height.
  int expanded_area_available_height_ = 0;

  // If the event list is expanded.
  bool is_event_list_showing_ = false;

  // The currently selected date.
  absl::optional<base::Time> selected_date_;

  // The row index of the currently selected date.
  int selected_date_row_index_ = 0;

  // The current row index when the event list view is shown.
  int expanded_row_index_ = 0;

  // The time difference between UTC and local time in hour.
  int time_difference_hours_ = 0;

  // The event list of the currently selected date.
  SingleDayEventList* selected_date_events_;

  UnifiedSystemTrayController* unified_system_tray_controller_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<CalendarViewController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_VIEW_CONTROLLER_H_
