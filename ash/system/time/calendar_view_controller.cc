// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_view_controller.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/time/calendar_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

CalendarViewController::CalendarViewController()
    : current_date_(base::Time::Now()) {}

CalendarViewController::~CalendarViewController() = default;

void CalendarViewController::AddObserver(Observer* observer) {
  if (observer)
    observers_.AddObserver(observer);
}

void CalendarViewController::RemoveObserver(Observer* observer) {
  if (observer)
    observers_.RemoveObserver(observer);
}

void CalendarViewController::UpdateMonth(
    const base::Time current_month_first_date) {
  current_date_ = current_month_first_date;
  for (auto& observer : observers_) {
    observer.OnMonthChanged(
        calendar_utils::GetExploded(current_month_first_date));
  }
}

base::Time CalendarViewController::GetOnScreenMonthFirstDay() const {
  return current_date_ -
         base::Days(calendar_utils::GetExploded(current_date_).day_of_month -
                    1);
}

base::Time CalendarViewController::GetPreviousMonthFirstDay() const {
  const base::Time last_day_of_previous_month =
      GetOnScreenMonthFirstDay() - base::Days(1);
  return last_day_of_previous_month -
         base::Days(calendar_utils::GetExploded(last_day_of_previous_month)
                        .day_of_month -
                    1);
}

base::Time CalendarViewController::GetNextMonthFirstDay() const {
  // Adds over 31 days to make sure it goes to the next month.
  const base::Time next_month_day = GetOnScreenMonthFirstDay() + base::Days(33);
  return next_month_day -
         base::Days(calendar_utils::GetExploded(next_month_day).day_of_month -
                    1);
}

std::u16string CalendarViewController::GetPreviousMonthName() {
  return calendar_utils::GetMonthName(GetPreviousMonthFirstDay());
}

std::u16string CalendarViewController::GetNextMonthName() {
  return calendar_utils::GetMonthName(GetNextMonthFirstDay());
}

std::u16string CalendarViewController::GetOnScreenMonthName() {
  return calendar_utils::GetMonthName(current_date_);
}

int CalendarViewController::GetTodayRowTopHeight() {
  return (today_row_ - 1) * row_height_;
}

int CalendarViewController::GetTodayRowBottomHeight() {
  return today_row_ * row_height_;
}

}  // namespace ash
