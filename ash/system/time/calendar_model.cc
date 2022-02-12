// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_model.h"

#include <stdlib.h>
#include <cstddef>

#include "ash/calendar/calendar_client.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/shell.h"
#include "ash/system/time/calendar_utils.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

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

CalendarModel::CalendarModel(const std::set<base::Time> non_prunable_months)
    : non_prunable_months_(non_prunable_months) {
  FetchEventsForBaseMonths();
}

CalendarModel::~CalendarModel() {}

void CalendarModel::AddObserver(Observer* observer) {
  if (observer)
    observers_.AddObserver(observer);
}

void CalendarModel::RemoveObserver(Observer* observer) {
  if (observer)
    observers_.RemoveObserver(observer);
}

bool CalendarModel::IsMonthAlreadyFetched(base::Time start_of_month) const {
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

void CalendarModel::MaybeFetchMonth(base::Time start_of_month) {
  // Bail out early if we have no CalendarClient.  This will be the case in most
  // unit tests.
  CalendarClient* client = Shell::Get()->calendar_controller()->GetClient();
  if (!client)
    return;

  // TODO https://crbug.com/1258002 Don't do any of this if the user is guest,
  // the screen is locked, or we're in OOBE or any other non-logged-in mode.
  if (!IsMonthAlreadyFetched(start_of_month)) {
    // We can't know whether the request will succeed (callback receives actual
    // events), fail (callback receives an error code), or not receive any
    // response (no events for that month), so the month is declared "fetched"
    // when we make the request for its events.
    MarkMonthAsFetched(start_of_month);

    // TODO https://crbug.com/1258179 the params passed to GetEventList() need
    // to be stored until the fetch request is complete in case of a failure, so
    // we know exactly which request failed.
    base::Time start_of_next_month =
        calendar_utils::GetStartOfNextMonthUTC(start_of_month);
    client->GetEventList(base::BindOnce(&CalendarModel::OnCalendarEventsFetched,
                                        weak_factory_.GetWeakPtr()),
                         start_of_month, start_of_next_month);
  }
}

void CalendarModel::MarkMonthAsFetched(base::Time start_of_month) {
  if (non_prunable_months_.find(start_of_month) != non_prunable_months_.end())
    non_prunable_months_fetched_.emplace(start_of_month);
  else
    QueuePrunableMonth(start_of_month);
}

void CalendarModel::QueuePrunableMonth(base::Time start_of_month) {
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

void CalendarModel::FetchEvents(const std::set<base::Time> months) {
  for (auto& month : months)
    MaybeFetchMonth(month.UTCMidnight());
}

void CalendarModel::FetchEventsForBaseMonths() {
  for (auto& month : non_prunable_months_)
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

void CalendarModel::OnCalendarEventsFetched(
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

void CalendarModel::InsertEvent(
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

void CalendarModel::InsertEventInMonth(
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

void CalendarModel::InsertEvents(
    const std::unique_ptr<google_apis::calendar::EventList>& events) {
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
