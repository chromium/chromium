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
#include "ash/system/time/calendar_utils.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "google_apis/common/api_error_codes.h"

namespace {

// Absolute minimum number of months to cache, which includes the
// current/previous/next on-screen months and current/prev/next months including
// today's date.
constexpr int kMinNumberOfMonthsCached = 6;

// Number of additional months to cache, to be adjusted as needed for optimal
// UX.
constexpr int kAdditionalNumberOfMonthsCached = 4;

// Maximum number of months to cache.
constexpr int kMaxNumberOfMonthsCached =
    kMinNumberOfMonthsCached + kAdditionalNumberOfMonthsCached;

constexpr base::TimeDelta kTimeoutMs = base::Milliseconds(1000);

// Methods for debugging and gathering of metrics.

[[maybe_unused]] int GetEventMapSize(
    const ash::CalendarModel::SingleMonthEventMap& event_map) {
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

// Represents a single fetch of a given month's calendar events.
class CalendarEventFetch {
 public:
  // A callback invoked when a fetch of calendar events is complete.
  using FetchCompleteCallback =
      base::OnceCallback<void(base::Time start_of_month,
                              google_apis::ApiErrorCode error,
                              const google_apis::calendar::EventList* events)>;

  // A callback invoked when a fetch of calendar events did not complete, due to
  // an internal error.
  using FetchInternalErrorCallback =
      base::OnceCallback<void(base::Time start_of_month,
                              FetchInternalErrorCode error)>;

  // Fetch begins immediately on construction.
  CalendarEventFetch(base::Time start_of_month,
                     FetchCompleteCallback complete_callback,
                     FetchInternalErrorCallback internal_error_callback)
      : start_of_month_(start_of_month),
        complete_callback_(std::move(complete_callback)),
        internal_error_callback_(std::move(internal_error_callback)),
        fetch_start_time_(base::Time::Now()),
        timeout_(FROM_HERE,
                 kTimeoutMs,
                 base::BindRepeating(&CalendarEventFetch::OnTimeout,
                                     base::Unretained(this))) {
    CalendarClient* client = Shell::Get()->calendar_controller()->GetClient();
    if (!client) {
      base::UmaHistogramCounts100("Ash.Calendar.FetchEvents.NoCalendarClient",
                                  1);

      // IMPORTANT: 'this' is NOT safe to use after `internal_error_callback_`
      // has been executed, as the last thing it does is destroy its
      // std::unique_ptr<CalendarEventFetch> to this object.
      std::move(internal_error_callback_)
          .Run(start_of_month_, FetchInternalErrorCode::RESOURCE_UNAVAILABLE);
      return;
    }

    const base::Time start_of_next_month =
        calendar_utils::GetStartOfNextMonthUTC(start_of_month);
    client->GetEventList(base::BindOnce(&CalendarEventFetch::OnResultReceived,
                                        weak_factory_.GetWeakPtr()),
                         start_of_month, start_of_next_month);

    timeout_.Reset();
  }
  CalendarEventFetch(const CalendarEventFetch& other) = delete;
  CalendarEventFetch& operator=(const CalendarEventFetch& other) = delete;
  virtual ~CalendarEventFetch() = default;

 private:
  // Callback invoked when results of a fetch are available.
  void OnResultReceived(
      google_apis::ApiErrorCode error,
      std::unique_ptr<google_apis::calendar::EventList> events) {
    // Cancel timeout timer.
    timeout_.Stop();

    base::UmaHistogramTimes("Ash.Calendar.FetchEvents.FetchDuration",
                            base::Time::Now() - fetch_start_time_);

    // IMPORTANT: 'this' is NOT safe to use after `complete_callback_` has been
    // executed, as the last thing it does is destroy its
    // std::unique_ptr<CalendarEventFetch> to this object.
    std::move(complete_callback_).Run(start_of_month_, error, events.get());
  }

  void OnTimeout() {
    // IMPORTANT: 'this' is NOT safe to use after `internal_error_callback_` has
    // been executed, as the last thing it does is destroy its
    // std::unique_ptr<CalendarEventFetch> to this object.
    std::move(internal_error_callback_)
        .Run(start_of_month_, FetchInternalErrorCode::TIMEOUT);
  }

  // Start of the month whose events we're fetching.
  base::Time start_of_month_;

  // Callback invoked when the fetch is complete.
  FetchCompleteCallback complete_callback_;

  // Callback invoked when the fetch failed with an internal error.
  FetchInternalErrorCallback internal_error_callback_;

  const base::Time fetch_start_time_;

  // Timer we run at the start of a fetch, to ensure that we terminate if we go
  // too long without a response.
  base::RetainingOneShotTimer timeout_;

  base::WeakPtrFactory<CalendarEventFetch> weak_factory_{this};
};

// The calendar model itself.
CalendarModel::CalendarModel(const std::set<base::Time>& base_months)
    : session_observer_(this) {}

CalendarModel::~CalendarModel() {}

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

bool CalendarModel::IsMonthAlreadyFetched(base::Time start_of_month) const {
  for (auto& month : prunable_months_mru_) {
    if (month == start_of_month)
      return true;
  }

  return false;
}

void CalendarModel::MaybeFetchMonth(base::Time start_of_month) {
  // Early returns if it's not a valid user/user-session.
  if (!calendar_utils::IsActiveUser())
    return;

  // Bail out early if we have no CalendarClient.  This will be the case in most
  // unit tests.
  CalendarClient* client = Shell::Get()->calendar_controller()->GetClient();
  if (!client)
    return;

  // No need to fetch.
  if (IsMonthAlreadyFetched(start_of_month)) {
    base::UmaHistogramCounts100("Ash.Calendar.FetchEvents.PreFetched", 1);
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
          base::BindRepeating(&CalendarModel::OnEventsFetched,
                              base::Unretained(this)),
          base::BindRepeating(&CalendarModel::OnEventFetchFailedInternalError,
                              base::Unretained(this))));
}

void CalendarModel::MarkMonthAsFetched(base::Time start_of_month) {
  QueuePrunableMonth(start_of_month);
}

void CalendarModel::QueuePrunableMonth(base::Time start_of_month) {
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

void CalendarModel::ClearAllCachedEvents() {
  // Destroy all outstanding fetch requests.
  pending_fetches_.clear();

  // Destroy the list used to decide who gets pruned.
  prunable_months_mru_.clear();

  // Destroy the events themselves.
  event_months_.clear();
}

void CalendarModel::FetchEvents(const std::set<base::Time>& months) {
  for (auto& month : months)
    MaybeFetchMonth(month.UTCMidnight());
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
    QueuePrunableMonth(calendar_utils::GetStartOfMonthUTC(day));
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
    return;
  }

  // Keep us within storage limits.
  PruneEventCache();

  // Store the incoming events.
  InsertEvents(events);

  // Notify observers.
  for (auto& observer : observers_)
    observer.OnEventsFetched(events);

  // Month has officially been fetched.
  MarkMonthAsFetched(start_of_month);

  // Request is no longer outstanding, so it can be destroyed.
  pending_fetches_.erase(start_of_month);
}

void CalendarModel::OnEventFetchFailedInternalError(
    base::Time start_of_month,
    FetchInternalErrorCode error) {
  LOG(ERROR) << __FUNCTION__
             << " Event fetch received internal error: " << (int)error;

  // Request is no longer outstanding, so it can be destroyed.
  // TODO: https://crbug.com/1298187 We need to respond further based on the
  // specific error code, retry in some cases, etc.
  pending_fetches_.erase(start_of_month);
}

void CalendarModel::InsertEvent(
    const google_apis::calendar::CalendarEvent* event) {
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
}

void CalendarModel::InsertEventInMonth(
    SingleMonthEventMap& month,
    const google_apis::calendar::CalendarEvent* event) {
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
  for (const auto& event : events->items())
    InsertEvent(event.get());
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

  event_months_.clear();
  for (const google_apis::calendar::CalendarEvent& event :
       to_be_redistributed_events) {
    InsertEvent(&event);
  }
}

void CalendarModel::PruneEventCache() {
  while (event_months_.size() >= kMaxNumberOfMonthsCached &&
         !prunable_months_mru_.empty()) {
    base::Time lru_month = prunable_months_mru_.back();
    LOG(WARNING) << __FUNCTION__ << " pruning lru_month " << lru_month;
    event_months_.erase(lru_month);
    prunable_months_mru_.pop_back();
  }
}

}  // namespace ash
