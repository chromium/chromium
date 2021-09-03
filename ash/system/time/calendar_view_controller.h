// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_TIME_CALENDAR_VIEW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"

namespace ash {

// Controller of the `CalendarView`.
class ASH_EXPORT CalendarViewController {
 public:
  CalendarViewController();
  CalendarViewController(const CalendarViewController& other) = delete;
  CalendarViewController& operator=(const CalendarViewController& other) =
      delete;
  ~CalendarViewController();

  class Observer {
   public:
    // Gets called when `current_date_ ` changes.
    virtual void OnMonthChanged(const base::Time::Exploded current_month) = 0;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Updates the `current_date_`.
  void UpdateMonth(const base::Time current_month_first_date);

  // Gets the first day of the `current_date_`'s month.
  base::Time GetOnScreenMonthFirstDay() const;

  // Gets the first day of the previous month based on the `current_date_`'s
  // month.
  base::Time GetPreviousMonthFirstDay() const;

  // Gets the first day of the next month based on the `current_date_`'s month.
  base::Time GetNextMonthFirstDay() const;

  // Gets the month name of the `current_date_`'s month.
  std::u16string GetOnScreenMonthName();

  // Gets the month name of the next month based on the `current_date_`'s month.
  std::u16string GetNextMonthName();

  // Gets the month name of the previous month based `current_date_`'s month.
  std::u16string GetPreviousMonthName();

 private:
  // The current date, which can be today or the first day of the current month
  // if current month is not today's month.
  base::Time current_date_;

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_VIEW_CONTROLLER_H_
