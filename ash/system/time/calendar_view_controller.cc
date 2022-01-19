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
#include "ash/system/time/calendar_utils.h"
#include "base/check.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Absolute minimum number of months to cache, which includes the
// current/previous/next on-screen months and current/prev/next months including
// today's date.
constexpr int kMinNumberOfMonthsCached = 6;

// Number of additional months to cache, to be adjusted as needed for optimal
// UX.
constexpr int kAdditionalNumberOfMonthsCached = 1;

// Maximum number of months to cache.
constexpr int kMaxNumberOfMonthsCached =
    kMinNumberOfMonthsCached + kAdditionalNumberOfMonthsCached;

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

}  // namespace

namespace ash {

CalendarViewController::CalendarViewController()
    : current_date_(base::Time::Now()),
      non_prunable_months_{GetPreviousMonthFirstDayUTC(1).UTCMidnight(),
                           GetOnScreenMonthFirstDayUTC().UTCMidnight(),
                           GetNextMonthFirstDayUTC(1).UTCMidnight()} {}

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
  if (calendar_utils::GetExplodedLocal(current_date_).month ==
          calendar_utils::GetExplodedLocal(current_month_first_date).month &&
      calendar_utils::GetExplodedLocal(current_date_).year ==
          calendar_utils::GetExplodedLocal(current_month_first_date).year) {
    return;
  }

  was_on_later_month_ = current_date_ > current_month_first_date;
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

bool CalendarViewController::IsMonthAlreadyFetched(
    base::Time start_of_month) const {
  if (non_prunable_months_fetched_.find(start_of_month) !=
      non_prunable_months_fetched_.end()) {
    return true;
  }

  for (auto& month : prunable_months_mru_) {
    if (month == start_of_month)
      return true;
  }

  return false;
}

void CalendarViewController::MaybeFetchMonth(base::Time start_of_month) {
  // TODO https://crbug.com/1258002 Don't do any of this if the user is guest,
  // the screen is locked, or we're in OOBE or any other non-logged-in mode.
  if (!IsMonthAlreadyFetched(start_of_month)) {
    // We can't know whether the request will succeed (callback receives actual
    // events), fail (callback receives an error code), or not receive any
    // response (no events for that month), so the month is declared "fetched"
    // when we make the request for its events.
    MarkMonthAsFetched(start_of_month);

    CalendarClient* client = Shell::Get()->calendar_controller()->GetClient();
    if (!client)
      return;

    // TODO https://crbug.com/1258179 the params passed to GetEventList() need
    // to be stored until the fetch request is complete in case of a failure, so
    // we know exactly which request failed.
    base::Time start_of_next_month =
        calendar_utils::GetStartOfNextMonthUTC(start_of_month);
    client->GetEventList(
        base::BindOnce(&CalendarViewController::OnCalendarEventsFetched,
                       weak_factory_.GetWeakPtr()),
        start_of_month, start_of_next_month);
  }
}

void CalendarViewController::MarkMonthAsFetched(base::Time start_of_month) {
  if (non_prunable_months_.find(start_of_month) != non_prunable_months_.end())
    non_prunable_months_fetched_.emplace(start_of_month);
  else
    QueuePrunableMonth(start_of_month);
}

void CalendarViewController::QueuePrunableMonth(base::Time start_of_month) {
  // For safety, make sure we aren't queuing a non-prunable month.
  if (non_prunable_months_.find(start_of_month) != non_prunable_months_.end())
    return;

  // If start_of_month is already most-recently-used, nothing to do.
  if (!prunable_months_mru_.empty() &&
      prunable_months_mru_.front() == start_of_month)
    return;

  // Remove start_of_month from the queue if it's present.
  for (auto it = prunable_months_mru_.begin(); it != prunable_months_mru_.end();
       ++it)
    if (*it == start_of_month) {
      prunable_months_mru_.erase(it);
      break;
    }

  // start_of_month is now the most-recently-used.
  prunable_months_mru_.push_front(start_of_month);
}

void CalendarViewController::FetchEvents() {
  // Fetch the current on-screen month +/-1.  We can fetch more as storage
  // allows.
  MaybeFetchMonth(GetPreviousMonthFirstDayUTC(1).UTCMidnight());
  MaybeFetchMonth(GetOnScreenMonthFirstDayUTC().UTCMidnight());
  MaybeFetchMonth(GetNextMonthFirstDayUTC(1).UTCMidnight());
}

SingleDayEventList CalendarViewController::SelectedDateEvents() {
  if (!selected_date_.has_value())
    return std::list<google_apis::calendar::CalendarEvent>();

  base::Time date;
  const bool result =
      base::Time::FromLocalExploded(selected_date_.value(), &date);
  DCHECK(result);
  return FindEvents(date);
}

int CalendarViewController::EventsNumberOfDayInternal(
    base::Time day,
    SingleDayEventList* events) const {
  const SingleDayEventList& list = FindEvents(day);

  if (list.empty())
    return 0;

  // We have events, and we assume the destination is empty.
  if (events) {
    DCHECK(events->empty());
    *events = list;
  }

  return list.size();
}

int CalendarViewController::EventsNumberOfDay(base::Time day,
                                              SingleDayEventList* events) {
  int event_number = EventsNumberOfDayInternal(day, events);
  if (event_number != 0) {
    QueuePrunableMonth(calendar_utils::GetStartOfMonthUTC(day));
  }
  return event_number;
}

void CalendarViewController::ShowEventListView(
    base::Time::Exploded selected_date,
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

  auto current = calendar_utils::GetExplodedLocal(current_date_);
  return current.month == selected_date_->month &&
         current.year == selected_date_->year;
}

void CalendarViewController::OnCalendarEventsFetched(
    google_apis::ApiErrorCode error,
    std::unique_ptr<google_apis::calendar::EventList> events) {
  // TODO https://crbug.com/1258179 we need to handle the other error codes we
  // can possibly receive, and know for certain which fetch request failed.
  base::UmaHistogramSparse("Ash.Calendar.FetchEvents.Result", error);
  if (error != google_apis::HTTP_SUCCESS) {
    LOG(ERROR) << __FUNCTION__ << " Event fetch received error: " << error;
    return;
  }

  // Keep us within storage limits.
  PruneEventCache();

  // Store the incoming events.
  InsertEvents(events);

  // Notify observers.
  for (auto& observer : observers_)
    observer.OnEventsFetched(events.get());
}

void CalendarViewController::InsertEvent(
    const google_apis::calendar::CalendarEvent* event) {
  base::Time start_day = event->start_time().date_time().UTCMidnight();
  base::Time start_of_month = calendar_utils::GetStartOfMonthUTC(start_day);

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

SingleDayEventList CalendarViewController::FindEvents(base::Time day) const {
  SingleDayEventList event_list;

  // Early return if we know we have no events for this month.
  base::Time start_of_month = calendar_utils::GetStartOfMonthUTC(day);
  auto it = event_months_.find(start_of_month);
  if (it == event_months_.end())
    return event_list;

  // Early return if we know we have no events for this day.
  base::Time midnight = day.UTCMidnight();
  const SingleMonthEventMap& month = it->second;
  auto it2 = month.find(midnight);
  if (it2 == month.end())
    return event_list;

  return it2->second;
}

void CalendarViewController::PruneEventCache() {
  while (event_months_.size() >= kMaxNumberOfMonthsCached &&
         !prunable_months_mru_.empty()) {
    base::Time lru_month = prunable_months_mru_.back();
    LOG(WARNING) << __FUNCTION__ << " pruning lru_month " << lru_month;
    event_months_.erase(lru_month);
    prunable_months_mru_.pop_back();
  }
}

}  // namespace ash
