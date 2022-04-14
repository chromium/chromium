// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_view_controller.h"

#include <stdlib.h>
#include <cstddef>

#include "ash/calendar/calendar_client.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/time/calendar_model.h"
#include "ash/system/time/calendar_utils.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

CalendarViewController::CalendarViewController()
    : currently_shown_date_(base::Time::Now()),
      calendar_open_time_(base::TimeTicks::Now()),
      month_dwell_time_(base::TimeTicks::Now()) {
  MaybeUpdateTimeDifference(currently_shown_date_);
  InitialFetchEvents();
  Shell::Get()->system_tray_model()->calendar_model()->ResetLifetimeMetrics(
      currently_shown_date_);
}

CalendarViewController::~CalendarViewController() {
  CalendarModel* calendar_model =
      Shell::Get()->system_tray_model()->calendar_model();
  DCHECK(calendar_model);
  calendar_model->UploadLifetimeMetrics();
  calendar_model->ClearAllPrunableEvents();

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
  if (calendar_utils::GetMonthNameAndYear(currently_shown_date_) ==
      calendar_utils::GetMonthNameAndYear(current_month_first_date)) {
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

void CalendarViewController::MaybeUpdateTimeDifference(base::Time date) {
  // Set the time difference, which is used to generate the exploded everywhere,
  // since the LocalExplode doesn't use the manually set timezone.
  int const new_time_difference =
      calendar_utils::GetTimeDifferenceInMinutes(date);
  if (time_difference_minutes_ == new_time_difference)
    return;

  time_difference_minutes_ = new_time_difference;
  Shell::Get()->system_tray_model()->calendar_model()->RedistributeEvents(
      time_difference_minutes_);
}

base::Time CalendarViewController::GetOnScreenMonthFirstDayLocal() {
  return calendar_utils::GetFirstDayOfMonth(
             ApplyTimeDifference(currently_shown_date_)) -
         base::Minutes(
             calendar_utils::GetTimeDifferenceInMinutes(currently_shown_date_));
}

base::Time CalendarViewController::GetPreviousMonthFirstDayLocal(
    unsigned int num_months) {
  base::Time prev,
      current = ApplyTimeDifference(GetOnScreenMonthFirstDayLocal());

  DCHECK_GE(num_months, 1UL);

  for (unsigned int i = 0; i < num_months; i++, current = prev) {
    prev = calendar_utils::GetStartOfPreviousMonthLocal(current);
  }

  return prev - base::Minutes(calendar_utils::GetTimeDifferenceInMinutes(prev));
}

base::Time CalendarViewController::GetNextMonthFirstDayLocal(
    unsigned int num_months) {
  base::Time next,
      current = ApplyTimeDifference(GetOnScreenMonthFirstDayLocal());

  DCHECK_GE(num_months, 1UL);

  for (unsigned int i = 0; i < num_months; i++, current = next) {
    next = calendar_utils::GetStartOfNextMonthLocal(current);
  }

  return next - base::Minutes(calendar_utils::GetTimeDifferenceInMinutes(next));
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

std::u16string CalendarViewController::GetPreviousMonthName() {
  return calendar_utils::GetMonthName(GetPreviousMonthFirstDayLocal(1));
}

std::u16string CalendarViewController::GetNextMonthName(int num_months) {
  return calendar_utils::GetMonthName(GetNextMonthFirstDayLocal(num_months));
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

void CalendarViewController::InitialFetchEvents() {
  std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(
      base::Time::Now() + base::Minutes(time_difference_minutes_),
      calendar_utils::kNumSurroundingMonthsCached);
  Shell::Get()->system_tray_model()->calendar_model()->AddNonPrunableMonths(
      months);
  Shell::Get()->system_tray_model()->calendar_model()->FetchEvents(months);
}

void CalendarViewController::FetchEvents() {
  Shell::Get()->system_tray_model()->calendar_model()->FetchEventsSurrounding(
      calendar_utils::kNumSurroundingMonthsCached,
      GetOnScreenMonthFirstDayUTC().UTCMidnight());
}

SingleDayEventList CalendarViewController::SelectedDateEvents() {
  if (!selected_date_.has_value())
    return std::list<google_apis::calendar::CalendarEvent>();

  return Shell::Get()->system_tray_model()->calendar_model()->FindEvents(
      ApplyTimeDifference(selected_date_.value()));
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

  return calendar_utils::GetMonthNameAndYear(currently_shown_date_) ==
         calendar_utils::GetMonthNameAndYear(selected_date_.value());
}

base::Time CalendarViewController::ApplyTimeDifference(base::Time date) {
  return date + base::Minutes(calendar_utils::GetTimeDifferenceInMinutes(date));
}

}  // namespace ash
