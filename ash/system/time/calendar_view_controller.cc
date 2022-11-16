// Copyright 2021 The Chromium Authors
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
      month_dwell_time_(base::TimeTicks::Now()),
      first_shown_date_(base::Time::Now()) {
  std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(
      base::Time::Now() +
          calendar_utils::GetTimeDifference(currently_shown_date_),
      calendar_utils::kNumSurroundingMonthsCached);
  Shell::Get()->system_tray_model()->calendar_model()->AddNonPrunableMonths(
      months);
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

  base::UmaHistogramMediumTimes("Ash.Calendar.UserJourneyTime.EventNotLaunched",
                                base::TimeTicks::Now() - calendar_open_time_);
  base::UmaHistogramCounts100000("Ash.Calendar.MaxDistanceBrowsed",
                                 max_distance_browsed_);
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

  max_distance_browsed_ =
      std::max(max_distance_browsed_,
               static_cast<size_t>(abs(calendar_utils::GetMonthsBetween(
                   first_shown_date_, current_month_first_date))));

  for (auto& observer : observers_)
    observer.OnMonthChanged();
}

base::Time CalendarViewController::GetPreviousMonthFirstDayUTC(
    unsigned int num_months) {
  base::Time prev, current = ApplyTimeDifference(GetOnScreenMonthFirstDayUTC());

  DCHECK_GE(num_months, 1UL);

  for (unsigned int i = 0; i < num_months; i++, current = prev) {
    prev = calendar_utils::GetStartOfPreviousMonthLocal(current);
  }

  return prev - calendar_utils::GetTimeDifference(prev);
}

base::Time CalendarViewController::GetNextMonthFirstDayUTC(
    unsigned int num_months) {
  base::Time next, current = ApplyTimeDifference(GetOnScreenMonthFirstDayUTC());

  DCHECK_GE(num_months, 1UL);

  for (unsigned int i = 0; i < num_months; i++, current = next) {
    next = calendar_utils::GetStartOfNextMonthLocal(current);
  }

  return next - calendar_utils::GetTimeDifference(next);
}

std::u16string CalendarViewController::GetPreviousMonthName() {
  return calendar_utils::GetMonthName(GetPreviousMonthFirstDayUTC(1));
}

std::u16string CalendarViewController::GetNextMonthName(int num_months) {
  return calendar_utils::GetMonthName(GetNextMonthFirstDayUTC(num_months));
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

SingleDayEventList CalendarViewController::SelectedDateEvents() {
  if (!selected_date_.has_value())
    return std::list<google_apis::calendar::CalendarEvent>();

  return Shell::Get()->system_tray_model()->calendar_model()->FindEvents(
      ApplyTimeDifference(selected_date_.value()));
}

std::tuple<SingleDayEventList, SingleDayEventList>
CalendarViewController::SelectedDateEventsSplitByMultiDayAndSameDay() {
  if (!selected_date_.has_value())
    return std::make_tuple(std::list<google_apis::calendar::CalendarEvent>(),
                           std::list<google_apis::calendar::CalendarEvent>());

  return Shell::Get()
      ->system_tray_model()
      ->calendar_model()
      ->FindEventsSplitByMultiDayAndSameDay(
          ApplyTimeDifference(selected_date_.value()));
}

SingleDayEventList CalendarViewController::UpcomingEvents() {
  return Shell::Get()
      ->system_tray_model()
      ->calendar_model()
      ->FindUpcomingEvents(
          ApplyTimeDifference(base::Time::NowFromSystemTime()));
}

int CalendarViewController::GetEventNumber(base::Time date) {
  return Shell::Get()->system_tray_model()->calendar_model()->EventsNumberOfDay(
      ApplyTimeDifference(date),
      /*events =*/nullptr);
}

void CalendarViewController::ShowEventListView(
    CalendarDateCellView* selected_calendar_date_cell_view,
    base::Time selected_date,
    int row_index) {
  // Do nothing if selecting on the same date.
  if (is_event_list_showing_ &&
      selected_calendar_date_cell_view == selected_date_cell_view_) {
    return;
  }
  selected_date_ = selected_date;
  set_selected_date_cell_view(selected_calendar_date_cell_view);
  selected_date_row_index_ = row_index;
  expanded_row_index_ = row_index;

  base::TimeDelta time_difference =
      calendar_utils::GetTimeDifference(selected_date);
  selected_date_midnight_ = (selected_date + time_difference).UTCMidnight();
  selected_date_midnight_utc_ = selected_date_midnight_ - time_difference;

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

void CalendarViewController::OnTodaysEventFetchComplete() {
  // Only record this once per lifetime of the CalendarView (and therefore the
  // controller).
  if (todays_date_cell_fetch_recorded_)
    return;

  UmaHistogramMediumTimes("Ash.Calendar.TimeToSeeTodaysEventDots",
                          base::TimeTicks::Now() - calendar_open_time_);
  todays_date_cell_fetch_recorded_ = true;
}

bool CalendarViewController::IsSelectedDateInCurrentMonth() {
  if (!selected_date_.has_value())
    return false;

  return calendar_utils::GetMonthNameAndYear(currently_shown_date_) ==
         calendar_utils::GetMonthNameAndYear(selected_date_.value());
}

base::Time CalendarViewController::GetOnScreenMonthFirstDayUTC() {
  base::TimeDelta time_difference =
      calendar_utils::GetTimeDifference(currently_shown_date_);
  return calendar_utils::GetFirstDayOfMonth(currently_shown_date_ +
                                            time_difference) -
         time_difference;
}

bool CalendarViewController::IsSuccessfullyFetched(base::Time start_of_month) {
  auto fetch_status =
      Shell::Get()->system_tray_model()->calendar_model()->FindFetchingStatus(
          start_of_month);
  return fetch_status == CalendarModel::kSuccess ||
         fetch_status == CalendarModel::kRefetching;
}

base::Time CalendarViewController::ApplyTimeDifference(base::Time date) {
  return date + calendar_utils::GetTimeDifference(date);
}

}  // namespace ash
