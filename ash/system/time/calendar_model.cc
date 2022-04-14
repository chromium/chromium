// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_model.h"

#include <stdlib.h>
#include <cstddef>
#include <memory>

#include "ash/calendar/calendar_client.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/shell.h"
#include "ash/system/time/calendar_event_fetch.h"
#include "ash/system/time/calendar_utils.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "google_apis/common/api_error_codes.h"

namespace {

// Methods for debugging and gathering of metrics.

[[maybe_unused]] size_t GetEventMapSize(
    const ash::CalendarModel::SingleMonthEventMap& event_map) {
  size_t total_bytes = 0;
  for (auto& event_list : event_map) {
    total_bytes += sizeof(event_list);
    for (auto& event : event_list.second) {
      total_bytes += event.GetApproximateSizeInBytes();
    }
  }

  return total_bytes;
}

[[maybe_unused]] size_t GetTotalCacheSize(
    const ash::CalendarModel::MonthToEventsMap& event_map) {
  size_t total_bytes = 0;
  for (auto& month : event_map)
    total_bytes += sizeof(month) + GetEventMapSize(month.second);

  return total_bytes;
}

}  // namespace

namespace ash {

CalendarModel::CalendarModel() : session_observer_(this) {}

CalendarModel::~CalendarModel() = default;

void CalendarModel::OnSessionStateChanged(session_manager::SessionState state) {
  ClearAllCachedEvents();
}

void CalendarModel::OnActiveUserSessionChanged(const AccountId& account_id) {
  ClearAllCachedEvents();
}

void CalendarModel::AddObserver(Observer* observer) {
  if (observer)
    observers_.AddObserver(observer);
}

void CalendarModel::RemoveObserver(Observer* observer) {
  if (observer)
    observers_.RemoveObserver(observer);
}

void CalendarModel::MaybeFetchMonth(base::Time start_of_month) {
  // Early return if it's not a valid user/user-session.
  if (!calendar_utils::IsActiveUser())
    return;

  // Bail out early if we have no CalendarClient.  This will be the case in most
  // unit tests.
  CalendarClient* client = Shell::Get()->calendar_controller()->GetClient();
  if (!client)
    return;

  // Bail out early if this is a prunable month that's already been fetched.
  if (non_prunable_months_.find(start_of_month) == non_prunable_months_.end() &&
      months_fetched_.find(start_of_month) != months_fetched_.end()) {
    return;
  }

  // Erase any outstanding fetch for this month.
  pending_fetches_.erase(start_of_month);

  // Construct a unique_ptr for the fetch request, and transfer ownership to
  // pending_fetches_.
  pending_fetches_.emplace(
      start_of_month,
      std::make_unique<CalendarEventFetch>(
          start_of_month,
          /*complete_callback=*/
          base::BindRepeating(&CalendarModel::OnEventsFetched,
                              base::Unretained(this)),
          /*internal_error_callback_=*/
          base::BindRepeating(&CalendarModel::OnEventFetchFailedInternalError,
                              base::Unretained(this)),
          /*tick_clock=*/nullptr));
}

void CalendarModel::PromoteMonth(base::Time start_of_month) {
  // If this month is non-prunable, nothing to do.
  if (non_prunable_months_.find(start_of_month) != non_prunable_months_.end())
    return;

  // If start_of_month is already most-recently-used, nothing to do.
  if (!mru_months_.empty() && mru_months_.front() == start_of_month)
    return;

  // Remove start_of_month from the queue if it's present.
  for (auto it = mru_months_.begin(); it != mru_months_.end(); ++it)
    if (*it == start_of_month) {
      mru_months_.erase(it);
      break;
    }

  // start_of_month is now the most-recently-used.
  mru_months_.push_front(start_of_month);
}

void CalendarModel::AddNonPrunableMonth(const base::Time& month) {
  // Early-return if `month` is present, to avoid the limits-check below.
  if (base::Contains(non_prunable_months_, month))
    return;

  if (non_prunable_months_.size() < calendar_utils::kMaxNumNonPrunableMonths)
    non_prunable_months_.emplace(month);
}

void CalendarModel::AddNonPrunableMonths(const std::set<base::Time>& months) {
  for (auto& month : months)
    AddNonPrunableMonth(month);
}

void CalendarModel::ClearAllCachedEvents() {
  // Destroy all outstanding fetch requests.
  pending_fetches_.clear();

  // Destroy the set of months we've fetched.
  months_fetched_.clear();

  // Destroy all prunable months.
  non_prunable_months_.clear();

  // Destroy the list used to decide who gets pruned.
  mru_months_.clear();

  // Destroy the events themselves.
  event_months_.clear();
}

void CalendarModel::ClearAllPrunableEvents() {
  // Destroy all outstanding fetch requests.
  pending_fetches_.clear();

  // Clear out all cached events that start in a prunable month, and any record
  // of having fetched it.
  for (auto& month : mru_months_) {
    event_months_.erase(month);
    months_fetched_.erase(month);
  }

  // Clear out the list of prunable months.
  mru_months_.clear();
}

void CalendarModel::ResetLifetimeMetrics(
    const base::Time& currently_shown_date) {
  max_distance_browsed_ = 0;
  first_on_screen_month_ =
      calendar_utils::GetFirstDayOfMonth(currently_shown_date);
}

void CalendarModel::UploadLifetimeMetrics() {
  base::UmaHistogramCounts100000("Ash.Calendar.FetchEvents.MaxDistanceBrowsed",
                                 max_distance_browsed_);
  base::UmaHistogramCounts100000(
      "Ash.Calendar.FetchEvents.TotalCacheSizeMonths", event_months_.size());
}

void CalendarModel::FetchEvents(const std::set<base::Time>& months) {
  for (auto& month : months)
    MaybeFetchMonth(month.UTCMidnight());
}

void CalendarModel::FetchEventsSurrounding(int num_months,
                                           const base::Time day) {
  std::set<base::Time> months =
      calendar_utils::GetSurroundingMonthsUTC(day, num_months);
  FetchEvents(months);
}

int CalendarModel::EventsNumberOfDayInternal(base::Time day,
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

int CalendarModel::EventsNumberOfDay(base::Time day,
                                     SingleDayEventList* events) {
  int event_number = EventsNumberOfDayInternal(day, events);
  if (event_number != 0) {
    PromoteMonth(calendar_utils::GetStartOfMonthUTC(day));
  }
  return event_number;
}

void CalendarModel::OnEventsFetched(
    base::Time start_of_month,
    google_apis::ApiErrorCode error,
    const google_apis::calendar::EventList* events) {
  base::UmaHistogramSparse("Ash.Calendar.FetchEvents.Result", error);
  if (error != google_apis::HTTP_SUCCESS) {
    LOG(ERROR) << __FUNCTION__ << " Event fetch received error: " << error;
    // Request is no longer outstanding, so it can be destroyed.
    // TODO: https://crbug.com/1298187 We need to respond further based on the
    // specific error code, retry in some cases, etc.
    pending_fetches_.erase(start_of_month);
    // TODO: https://crbug.com/1298187 maybe notify observers.
    // e.g. observer.OnEventsFetched(kError, start_of_month, events);
    return;
  }

  // Keep us within storage limits.
  PruneEventCache();

  // Clear out this month's events, we're about replace them.
  event_months_.erase(start_of_month);

  if (!events || events->items().empty()) {
    SingleMonthEventMap empty_event_list;
    event_months_.emplace(start_of_month, empty_event_list);
    PromoteMonth(start_of_month);
  } else {
    // Store the incoming events.
    InsertEvents(events);
  }

  // Notify observers.
  for (auto& observer : observers_)
    observer.OnEventsFetched(kSuccess, start_of_month, events);

  // Month has officially been fetched.
  months_fetched_.emplace(start_of_month);

  // Request is no longer outstanding, so it can be destroyed.
  pending_fetches_.erase(start_of_month);

  // Record the size of the month, and the total number of months.
  base::UmaHistogramCounts1M("Ash.Calendar.FetchEvents.SingleMonthSize",
                             GetEventMapSize(event_months_[start_of_month]));

  // If `start_of_month` is further, in months, from the on-screen month when
  // the calendar first opened, then update our max distance.
  UpdateMaxDistanceBrowsed(start_of_month);
}

void CalendarModel::OnEventFetchFailedInternalError(
    base::Time start_of_month,
    CalendarEventFetchInternalErrorCode error) {
  LOG(ERROR) << __FUNCTION__
             << " Event fetch received internal error: " << (int)error;

  // Request is no longer outstanding, so it can be destroyed.
  // TODO: https://crbug.com/1298187 We need to respond further based on the
  // specific error code, retry in some cases, etc.
  pending_fetches_.erase(start_of_month);
}

void CalendarModel::UpdateMaxDistanceBrowsed(const base::Time& start_of_month) {
  max_distance_browsed_ =
      std::max(max_distance_browsed_,
               static_cast<size_t>(abs(calendar_utils::GetMonthsBetween(
                   first_on_screen_month_, start_of_month))));
}

void CalendarModel::InsertEvent(
    const google_apis::calendar::CalendarEvent* event) {
  if (!event)
    return;

  base::Time start_of_month =
      calendar_utils::GetStartOfMonthUTC(GetStartTimeMidnightAdjusted(event));

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

  // Month is now the most-recently-used.
  PromoteMonth(start_of_month);
}

void CalendarModel::InsertEventInMonth(
    SingleMonthEventMap& month,
    const google_apis::calendar::CalendarEvent* event) {
  if (!event)
    return;

  base::Time start_time_midnight = GetStartTimeMidnightAdjusted(event);

  auto it = month.find(start_time_midnight);
  if (it == month.end()) {
    // No events stored for this day, so create a new list, add the event to
    // it, and insert the list in the map.
    SingleDayEventList list;
    list.push_back(*event);
    month.emplace(start_time_midnight, list);
  } else {
    // Already have some events for this day.
    SingleDayEventList& list = it->second;
    list.push_back(*event);
  }
}

base::Time CalendarModel::GetStartTimeMidnightAdjusted(
    const google_apis::calendar::CalendarEvent* event) const {
  if (time_difference_minutes_.has_value()) {
    return (event->start_time().date_time() +
            base::Minutes(time_difference_minutes_.value()))
        .UTCMidnight();
  }
  return event->start_time().date_time().UTCMidnight();
}

void CalendarModel::InsertEvents(
    const google_apis::calendar::EventList* events) {
  if (!events)
    return;

  for (const auto& event : events->items())
    InsertEvent(event.get());
}

void CalendarModel::InsertEventsForTesting(
    const google_apis::calendar::EventList* events) {
  if (!events)
    return;

  // Make sure the cache is empty.
  event_months_.clear();

  // Insert, and collect the set of months inserted.
  std::set<base::Time> months_inserted;
  for (const auto& event : events->items()) {
    base::Time month =
        calendar_utils::GetStartOfMonthUTC(event->start_time().date_time());
    months_inserted.emplace(month);
    InsertEvent(event.get());
  }
}

SingleDayEventList CalendarModel::FindEvents(base::Time day) const {
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

CalendarModel::FetchingStatus CalendarModel::FindFetchingStaus(
    base::Time start_time) const {
  if (pending_fetches_.count(start_time))
    return kFetching;

  if (event_months_.count(start_time))
    return kSuccess;

  return kNever;
}

void CalendarModel::RedistributeEvents(int time_difference_minutes) {
  // Early returns if the time difference is not changed.
  if (time_difference_minutes_.has_value() &&
      time_difference_minutes == time_difference_minutes_.value()) {
    return;
  }

  // Early returns if the `time_difference_minutes_` is not assigned and the
  // difference is 0.
  if (!time_difference_minutes_.has_value() && time_difference_minutes == 0) {
    time_difference_minutes_ = time_difference_minutes;
    return;
  }

  // Redistributes all the fetched events to the date map with the
  // `time_difference_minutes_`.
  time_difference_minutes_ = time_difference_minutes;
  SingleDayEventList to_be_redistributed_events;
  for (auto month = event_months_.begin(); month != event_months_.end();
       month++) {
    SingleMonthEventMap& event_map = month->second;
    for (auto it = event_map.begin(); it != event_map.end(); it++) {
      for (const google_apis::calendar::CalendarEvent& event : it->second) {
        to_be_redistributed_events.push_back(event);
      }
    }
  }

  // Clear out the entire event store, freshly insert the redistrubted events.
  event_months_.clear();
  for (const google_apis::calendar::CalendarEvent& event :
       to_be_redistributed_events) {
    InsertEvent(&event);
  }
}

void CalendarModel::PruneEventCache() {
  while (!mru_months_.empty() &&
         mru_months_.size() > calendar_utils::kMaxNumPrunableMonths) {
    base::Time lru_month = mru_months_.back();
    pending_fetches_.erase(lru_month);
    event_months_.erase(lru_month);
    months_fetched_.erase(lru_month);
    mru_months_.pop_back();
  }
}

}  // namespace ash
