// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_view_controller.h"

#include <stdlib.h>
#include <codecvt>
#include <cstddef>

#include "ash/calendar/calendar_client.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/time/calendar_utils.h"
#include "base/check.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

CalendarViewController::CalendarViewController(
    UnifiedSystemTrayController* controller)
    : current_date_(base::Time::Now()),
      unified_system_tray_controller_(controller) {
  // Using the local time format to get the local `base::Time`, which is used to
  // generate the exploded everywhere, since the LocalExplode doesn't use the
  // manually set timezone.
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
  base::Time local_time;
  bool result = base::Time::FromString(
      converter
          .to_bytes(
              base::TimeFormatWithPattern(current_date_, "MMMMdyyyy HH:mm") +
              u" GMT")
          .c_str(),
      &local_time);
  DCHECK(result);
  int difference_in_minutes = (local_time - current_date_).InMinutes();
  // Gives it an extra 1 minute to round the time difference to hours.
  difference_in_minutes += difference_in_minutes > 0 ? 1 : (-1);
  time_difference_hours_ = difference_in_minutes / 60;
}

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
  base::Time::Exploded current_date_exploded =
      calendar_utils::GetExplodedUTC(current_date_);
  base::Time::Exploded current_month_first_date_exploded =
      calendar_utils::GetExplodedUTC(current_month_first_date);
  if (current_date_exploded.year == current_month_first_date_exploded.year &&
      current_date_exploded.month == current_month_first_date_exploded.month) {
    return;
  }

  current_date_ = current_month_first_date;
  for (auto& observer : observers_) {
    observer.OnMonthChanged(
        calendar_utils::GetExplodedLocal(current_month_first_date));
  }
}

base::Time CalendarViewController::GetOnScreenMonthFirstDayLocal() const {
  return calendar_utils::GetStartOfMonthLocal(current_date_);
}

base::Time CalendarViewController::GetPreviousMonthFirstDayLocal(
    unsigned int num_months) const {
  base::Time prev, current = GetOnScreenMonthFirstDayLocal();

  DCHECK_GE(num_months, 1UL);

  for (unsigned int i = 0; i < num_months; i++, current = prev) {
    prev = calendar_utils::GetStartOfPreviousMonthLocal(current);
  }

  return prev;
}

base::Time CalendarViewController::GetNextMonthFirstDayLocal(
    unsigned int num_months) const {
  base::Time next, current = GetOnScreenMonthFirstDayLocal();

  DCHECK_GE(num_months, 1UL);

  for (unsigned int i = 0; i < num_months; i++, current = next) {
    next = calendar_utils::GetStartOfNextMonthLocal(current);
  }
  return next;
}

base::Time CalendarViewController::GetOnScreenMonthFirstDayUTC() const {
  return calendar_utils::GetStartOfMonthUTC(current_date_);
}

base::Time CalendarViewController::GetPreviousMonthFirstDayUTC(
    unsigned int num_months) const {
  base::Time prev, current = GetOnScreenMonthFirstDayUTC();

  DCHECK_GE(num_months, 1UL);

  for (unsigned int i = 0; i < num_months; i++, current = prev) {
    prev = calendar_utils::GetStartOfPreviousMonthUTC(current);
  }

  return prev;
}

base::Time CalendarViewController::GetNextMonthFirstDayUTC(
    unsigned int num_months) const {
  base::Time next, current = GetOnScreenMonthFirstDayUTC();

  DCHECK_GE(num_months, 1UL);

  for (unsigned int i = 0; i < num_months; i++, current = next) {
    next = calendar_utils::GetStartOfNextMonthUTC(current);
  }
  return next;
}

std::u16string CalendarViewController::GetPreviousMonthName() const {
  return calendar_utils::GetMonthName(GetPreviousMonthFirstDayLocal(1));
}

std::u16string CalendarViewController::GetNextMonthName() const {
  return calendar_utils::GetMonthName(GetNextMonthFirstDayLocal(1));
}

std::u16string CalendarViewController::GetOnScreenMonthName() const {
  return calendar_utils::GetMonthName(current_date_);
}

int CalendarViewController::GetExpandedRowIndex() const {
  DCHECK(is_event_list_showing_);
  return expanded_row_index_;
}

int CalendarViewController::GetTodayRowTopHeight() const {
  return (today_row_ - 1) * row_height_;
}

int CalendarViewController::GetTodayRowBottomHeight() const {
  return today_row_ * row_height_;
}

void CalendarViewController::FetchEvents() {
  std::set<base::Time> months{GetPreviousMonthFirstDayUTC(1).UTCMidnight(),
                              GetOnScreenMonthFirstDayUTC().UTCMidnight(),
                              GetNextMonthFirstDayUTC(1).UTCMidnight()};
  unified_system_tray_controller_->calendar_model()->FetchEvents(months);
}

SingleDayEventList CalendarViewController::SelectedDateEvents() {
  if (!selected_date_.has_value())
    return std::list<google_apis::calendar::CalendarEvent>();

  return unified_system_tray_controller_->calendar_model()->FindEvents(
      selected_date_.value());
}

void CalendarViewController::ShowEventListView(base::Time selected_date,
                                               int row_index) {
  // Do nothing if selecting on the same date.
  if (is_event_list_showing_ &&
      calendar_utils::IsTheSameDay(selected_date, selected_date_)) {
    return;
  }
  selected_date_ = selected_date;
  selected_date_row_index_ = row_index;
  expanded_row_index_ = row_index;

  // Notify observers.
  for (auto& observer : observers_)
    observer.OnSelectedDateUpdated();

  if (!is_event_list_showing_) {
    for (auto& observer : observers_)
      observer.OpenEventList();
  }
}

void CalendarViewController::CloseEventListView() {
  selected_date_ = absl::nullopt;
  for (auto& observer : observers_)
    observer.CloseEventList();
}

void CalendarViewController::OnEventListOpened() {
  is_event_list_showing_ = true;
}

void CalendarViewController::OnEventListClosed() {
  is_event_list_showing_ = false;
}

bool CalendarViewController::IsSelectedDateInCurrentMonth() {
  if (!selected_date_.has_value())
    return false;

  base::Time::Exploded current_date_exploded =
      calendar_utils::GetExplodedUTC(current_date_);
  base::Time::Exploded selected_date_exploded =
      calendar_utils::GetExplodedUTC(selected_date_.value());
  return current_date_exploded.year == selected_date_exploded.year &&
         current_date_exploded.month == selected_date_exploded.month;
}

}  // namespace ash
