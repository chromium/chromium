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
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/time/calendar_utils.h"
#include "base/check.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

CalendarViewController::CalendarViewController()
    : currently_shown_date_(base::Time::Now()),
      calendar_open_time_(base::TimeTicks::Now()),
      month_dwell_time_(base::TimeTicks::Now()) {
  // Using the local time format to get the local `base::Time`, which is used to
  // generate the exploded everywhere, since the LocalExplode doesn't use the
  // manually set timezone.
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
  base::Time local_time;
  bool result = base::Time::FromString(
      converter
          .to_bytes(base::TimeFormatWithPattern(currently_shown_date_,
                                                "MMMMdyyyy HH:mm") +
                    u" GMT")
          .c_str(),
      &local_time);
  DCHECK(result);
  int difference_in_minutes = (local_time - currently_shown_date_).InMinutes();
  // Gives it an extra 1 minute to consider the processing time. Adjust the
  // mintes by using the remainder of 15, since there're half an hour and 45
  // minutes timezone.
  difference_in_minutes += difference_in_minutes > 0 ? 1 : (-1);
  time_difference_minutes_ = difference_in_minutes - difference_in_minutes % 15;
  Shell::Get()->system_tray_model()->calendar_model()->RedistributeEvents(
      time_difference_minutes_);
}

CalendarViewController::~CalendarViewController() {
  calendar_metrics::RecordMonthDwellTime(base::TimeTicks::Now() -
                                         month_dwell_time_);

  if (user_journey_time_recorded_)
    return;

  UmaHistogramMediumTimes("Ash.Calendar.UserJourneyTime.EventNotLaunched",
                          base::TimeTicks::Now() - calendar_open_time_);
}

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
  base::Time::Exploded currently_shown_date_exploded =
      calendar_utils::GetExplodedUTC(currently_shown_date_);
  base::Time::Exploded current_month_first_date_exploded =
      calendar_utils::GetExplodedUTC(current_month_first_date);
  if (currently_shown_date_exploded.year ==
          current_month_first_date_exploded.year &&
      currently_shown_date_exploded.month ==
          current_month_first_date_exploded.month) {
    return;
  }

  calendar_metrics::RecordMonthDwellTime(base::TimeTicks::Now() -
                                         month_dwell_time_);
  month_dwell_time_ = base::TimeTicks::Now();

  currently_shown_date_ = current_month_first_date;
  for (auto& observer : observers_) {
    observer.OnMonthChanged(
        calendar_utils::GetExplodedLocal(current_month_first_date));
  }
}

base::Time CalendarViewController::GetOnScreenMonthFirstDayLocal() const {
  return calendar_utils::GetFirstDayOfMonth(
             currently_shown_date_ + base::Minutes(time_difference_minutes_)) -
         base::Minutes(time_difference_minutes_);
}

base::Time CalendarViewController::GetPreviousMonthFirstDayLocal(
    unsigned int num_months) const {
  base::Time prev, current = GetOnScreenMonthFirstDayLocal() +
                             base::Minutes(time_difference_minutes_);

  DCHECK_GE(num_months, 1UL);

  for (unsigned int i = 0; i < num_months; i++, current = prev) {
    prev = calendar_utils::GetStartOfPreviousMonthLocal(current);
  }

  return prev - base::Minutes(time_difference_minutes_);
}

base::Time CalendarViewController::GetNextMonthFirstDayLocal(
    unsigned int num_months) const {
  base::Time next, current = GetOnScreenMonthFirstDayLocal() +
                             base::Minutes(time_difference_minutes_);

  DCHECK_GE(num_months, 1UL);

  for (unsigned int i = 0; i < num_months; i++, current = next) {
    next = calendar_utils::GetStartOfNextMonthLocal(current);
  }
  return next - base::Minutes(time_difference_minutes_);
}

base::Time CalendarViewController::GetOnScreenMonthFirstDayUTC() const {
  return calendar_utils::GetStartOfMonthUTC(currently_shown_date_);
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
  return calendar_utils::GetMonthName(currently_shown_date_);
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
  std::set<base::Time> months;
  calendar_utils::GetSurroundingMonthsUTC(
      GetOnScreenMonthFirstDayUTC().UTCMidnight(),
      CalendarModel::kNumSurroundingMonthsCached, months);
  Shell::Get()->system_tray_model()->calendar_model()->FetchEvents(months);
}

SingleDayEventList CalendarViewController::SelectedDateEvents() {
  if (!selected_date_.has_value())
    return std::list<google_apis::calendar::CalendarEvent>();

  return Shell::Get()->system_tray_model()->calendar_model()->FindEvents(
      selected_date_.value() + base::Minutes(time_difference_minutes_));
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

void CalendarViewController::OnCalendarEventWillLaunch() {
  UmaHistogramMediumTimes("Ash.Calendar.UserJourneyTime.EventLaunched",
                          base::TimeTicks::Now() - calendar_open_time_);
  user_journey_time_recorded_ = true;
}

bool CalendarViewController::IsSelectedDateInCurrentMonth() {
  if (!selected_date_.has_value())
    return false;

  base::Time::Exploded currently_shown_date_exploded =
      calendar_utils::GetExplodedUTC(currently_shown_date_);
  base::Time::Exploded selected_date_exploded =
      calendar_utils::GetExplodedUTC(selected_date_.value());
  return currently_shown_date_exploded.year == selected_date_exploded.year &&
         currently_shown_date_exploded.month == selected_date_exploded.month;
}

}  // namespace ash
