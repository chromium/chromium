// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_view_controller.h"

#include "ash/calendar/calendar_client.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/time/calendar_utils.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Methods for debugging and gathering of metrics.

ALLOW_UNUSED_TYPE int GetEventMapSize(
    const ash::CalendarViewController::SingleMonthEventMap& event_map) {
  int total_bytes = 0;
  for (auto& event_list : event_map) {
    total_bytes += sizeof(event_list);
    for (auto& event : event_list.second) {
      total_bytes += event.GetApproximateSizeInBytes();
    }
  }

  return total_bytes;
}

// Methods to get the base::Time that is the start of the month in which
// |date| resides, or the month before/after |date|.
base::Time GetStartOfMonth(const base::Time& date) {
  base::Time::Exploded exp_date, exp_start_of_month;
  base::Time start_of_month;

  date.UTCExplode(&exp_date);
  exp_start_of_month.month = exp_date.month;
  exp_start_of_month.year = exp_date.year;
  exp_start_of_month.day_of_month = 1;
  exp_start_of_month.hour = 0;
  exp_start_of_month.minute = 0;
  exp_start_of_month.second = 0;
  exp_start_of_month.millisecond = 0;
  DCHECK(base::Time::FromUTCExploded(exp_start_of_month, &start_of_month));

  return start_of_month.UTCMidnight();
}

base::Time GetStartOfPreviousMonth(base::Time date) {
  base::Time start_of_month = GetStartOfMonth(date);
  base::Time last_day_of_previous_month, start_of_previous_month;

  last_day_of_previous_month = start_of_month - base::Days(1);
  start_of_previous_month =
      last_day_of_previous_month -
      base::Days(ash::calendar_utils::GetExploded(last_day_of_previous_month)
                     .day_of_month -
                 1);

  return start_of_previous_month;
}

base::Time GetStartOfNextMonth(base::Time date) {
  base::Time start_of_month = GetStartOfMonth(date);
  base::Time start_of_next_month, next_month_day;

  // Adds over 31 days to make sure it goes to the next month.
  next_month_day = start_of_month + base::Days(33);
  start_of_next_month =
      next_month_day -
      base::Days(ash::calendar_utils::GetExploded(next_month_day).day_of_month -
                 1);

  return start_of_next_month;
}

}  // namespace

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

base::Time CalendarViewController::GetPreviousMonthFirstDay(
    unsigned int num_months) const {
  base::Time prev, current = GetOnScreenMonthFirstDay();

  DCHECK_GE(num_months, 1UL);

  for (unsigned int i = 0; i < num_months; ++i) {
    prev = GetStartOfPreviousMonth(current);
    current = prev;
  }
  return prev;
}

base::Time CalendarViewController::GetNextMonthFirstDay(
    unsigned int num_months) const {
  base::Time next, current = GetOnScreenMonthFirstDay();

  DCHECK_GE(num_months, 1UL);

  for (unsigned int i = 0; i < num_months; ++i) {
    next = GetStartOfNextMonth(current);
    current = next;
  }
  return next;
}

std::u16string CalendarViewController::GetPreviousMonthName() const {
  return calendar_utils::GetMonthName(GetPreviousMonthFirstDay(1));
}

std::u16string CalendarViewController::GetNextMonthName() const {
  return calendar_utils::GetMonthName(GetNextMonthFirstDay(1));
}

std::u16string CalendarViewController::GetOnScreenMonthName() const {
  return calendar_utils::GetMonthName(current_date_);
}

int CalendarViewController::GetTodayRowTopHeight() const {
  return (today_row_ - 1) * row_height_;
}

int CalendarViewController::GetTodayRowBottomHeight() const {
  return today_row_ * row_height_;
}

bool CalendarViewController::IsMonthAlreadyFetched(
    base::Time start_of_month) const {
  return event_months_.find(start_of_month) != event_months_.end();
}

void CalendarViewController::MaybeFetchMonth(base::Time start_of_month) {
  if (!IsMonthAlreadyFetched(start_of_month)) {
    CalendarClient* client = Shell::Get()->calendar_controller()->GetClient();
    DCHECK(client);

    base::Time start_of_next_month = GetStartOfNextMonth(start_of_month);
    client->GetEventList(
        base::BindOnce(&CalendarViewController::OnCalendarEventsFetched,
                       weak_factory_.GetWeakPtr()),
        start_of_month, start_of_next_month);

    // Insert an empty month in event_months, to indicate that we've fetched it.
    // If it turns out there are no events for this month, IsDayWithEvents()
    // will still return false for any day during that month.
    SingleMonthEventMap empty_month;
    event_months_.emplace(start_of_month, empty_month);
  }
}

void CalendarViewController::FetchEvents() {
  // For now, we fetch events for the current on-screen month and the months
  // before and after.  We can possibly fetch more if metrics analysis tells us
  // storage isn't getting out of hand.
  MaybeFetchMonth(GetPreviousMonthFirstDay(1).UTCMidnight());
  MaybeFetchMonth(GetOnScreenMonthFirstDay().UTCMidnight());
  MaybeFetchMonth(GetNextMonthFirstDay(1).UTCMidnight());
}

bool CalendarViewController::IsDayWithEvents(base::Time day,
                                             SingleDayEventList* events) const {
  // Early return if we know we have no events for this month.
  auto it = event_months_.find(GetStartOfMonth(day));
  if (it == event_months_.end())
    return false;

  // Early return if we know we have no events for this day.
  base::Time midnight = day.UTCMidnight();
  const SingleMonthEventMap& month = it->second;
  auto it2 = month.find(midnight);
  if (it2 == month.end())
    return false;

  // Early return if there was a chance that have some events for this day, but
  // in fact we don't.
  const SingleDayEventList& list = it2->second;
  if (list.empty())
    return false;

  // We have events, and we assume the destination is empty.
  if (events) {
    DCHECK(events->empty());
    *events = list;
  }

  return true;
}

void CalendarViewController::OnCalendarEventsFetched(
    google_apis::ApiErrorCode error,
    std::unique_ptr<google_apis::calendar::EventList> events) {
  if (error == google_apis::NOT_READY) {
    LOG(WARNING) << __FUNCTION__
                 << " Event fetch received google_apis::NOT_READY";
    return;
  }

  InsertEvents(events);

  // Notify observers.
  for (auto& observer : observers_)
    observer.OnEventsFetched(events.get());
}

void CalendarViewController::InsertEvent(
    const google_apis::calendar::CalendarEvent* event) {
  base::Time start_day = event->start_time().date_time().UTCMidnight();
  base::Time start_of_month = GetStartOfMonth(start_day);

  auto it = event_months_.find(start_of_month);
  if (it == event_months_.end()) {
    // No events for this month, so add a map for it and insert.
    SingleMonthEventMap month;
    InsertEventInMonth(month, event);
    event_months_.emplace(start_of_month, month);
  } else {
    // Insert in a pre-existing month.
    SingleMonthEventMap& month = it->second;
    InsertEventInMonth(month, event);
  }
}

void CalendarViewController::InsertEventInMonth(
    SingleMonthEventMap& month,
    const google_apis::calendar::CalendarEvent* event) {
  base::Time midnight = event->start_time().date_time().UTCMidnight();

  auto it = month.find(midnight);
  if (it == month.end()) {
    // No events stored for this day, so create a new list, add the event to
    // it, and insert the list in the map.
    SingleDayEventList list;
    list.push_back(*event);
    month.emplace(midnight, list);
  } else {
    // Already have some events for this day.
    SingleDayEventList& list = it->second;
    list.push_back(*event);
  }
}

void CalendarViewController::InsertEvents(
    const std::unique_ptr<google_apis::calendar::EventList>& events) {
  for (const auto& event : events->items())
    InsertEvent(event.get());
}

}  // namespace ash
