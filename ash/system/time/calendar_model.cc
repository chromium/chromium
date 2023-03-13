// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_model.h"

#include <stdlib.h>
#include <cstddef>
#include <memory>

#include "ash/calendar/calendar_client.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/time/calendar_event_fetch.h"
#include "ash/system/time/calendar_utils.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/common/api_error_codes.h"

#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace {

using ::google_apis::calendar::CalendarEvent;

constexpr auto kAllowedEventStatuses =
    base::MakeFixedFlatSet<CalendarEvent::EventStatus>(
        {CalendarEvent::EventStatus::kConfirmed,
         CalendarEvent::EventStatus::kTentative});

constexpr auto kAllowedResponseStatuses =
    base::MakeFixedFlatSet<CalendarEvent::ResponseStatus>(
        {CalendarEvent::ResponseStatus::kAccepted,
         CalendarEvent::ResponseStatus::kNeedsAction,
         CalendarEvent::ResponseStatus::kTentative});

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

auto SplitEventsIntoMultiDayAndSameDay(const ash::SingleDayEventList& list) {
  std::list<CalendarEvent> multi_day_events;
  std::list<CalendarEvent> same_day_events;

  for (const CalendarEvent& event : list) {
    if (event.all_day_event() || ash::calendar_utils::IsMultiDayEvent(&event))
      multi_day_events.push_back(std::move(event));
    else
      same_day_events.push_back(std::move(event));
  }

  return std::make_tuple(std::move(multi_day_events),
                         std::move(same_day_events));
}

void SortByDateAscending(
    std::list<google_apis::calendar::CalendarEvent>& events) {
  events.sort([](google_apis::calendar::CalendarEvent& a,
                 google_apis::calendar::CalendarEvent& b) {
    if (a.start_time().date_time() == b.start_time().date_time()) {
      return a.end_time().date_time() < b.end_time().date_time();
    }
    return a.start_time().date_time() < b.start_time().date_time();
  });
}

bool EventStartsInTenMins(const CalendarEvent& event,
                          const base::Time& now_local) {
  const int start_time_difference_in_mins =
      (ash::calendar_utils::GetStartTimeAdjusted(&event) - now_local)
          .InMinutes();

  return (0 <= start_time_difference_in_mins &&
          start_time_difference_in_mins <= 10);
}

bool EventStartedLessThanOneHourAgo(const CalendarEvent& event,
                                    const base::Time& now_local) {
  const int start_time_difference_in_mins =
      (ash::calendar_utils::GetStartTimeAdjusted(&event) - now_local)
          .InMinutes();
  const int end_time_difference_in_mins =
      (ash::calendar_utils::GetEndTimeAdjusted(&event) - now_local).InMinutes();

  return (0 <= end_time_difference_in_mins &&
          0 > start_time_difference_in_mins &&
          start_time_difference_in_mins >= -60);
}

// Returns events that start in 10 minutes time, or events that are in progress
// and started less than one hour ago.
auto FilterEventsStartingSoonOrRecentlyInProgress(
    const ash::SingleDayEventList& list,
    const base::Time& now_local) {
  std::list<CalendarEvent> result;

  for (const CalendarEvent& event : list) {
    if (EventStartsInTenMins(event, now_local) ||
        EventStartedLessThanOneHourAgo(event, now_local)) {
      result.emplace_back(event);
    }
  }

  return result;
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

  // Destroy the set of months that have been fetched.
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

void CalendarModel::UploadLifetimeMetrics() {
  base::UmaHistogramCounts100000(
      "Ash.Calendar.FetchEvents.TotalCacheSizeMonths", event_months_.size());
}

void CalendarModel::FetchEvents(base::Time start_of_month) {
  // Early return if it's not a valid user/user-session.
  if (!calendar_utils::ShouldFetchEvents())
    return;

  // Bail out early if there is no CalendarClient.  This will be the case in
  // most unit tests.
  CalendarClient* client = Shell::Get()->calendar_controller()->GetClient();
  if (!client)
    return;

  // Bail out early if this is a prunable month that's already been fetched.
  if (non_prunable_months_.find(start_of_month) == non_prunable_months_.end() &&
      months_fetched_.find(start_of_month) != months_fetched_.end()) {
    PromoteMonth(start_of_month);
    return;
  }

  // Erase any outstanding fetch for this month.
  pending_fetches_.erase(start_of_month);

  // Construct a unique_ptr for the fetch request, and transfer ownership to
  // `pending_fetches_`.
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

void CalendarModel::CancelFetch(const base::Time& start_of_month) {
  if (base::Contains(pending_fetches_, start_of_month)) {
    // Note that the `CalendarEventFetch` here will be removed from
    // `pending_fetches_` in `OnEventsFetched`, which will receive an error code
    // of `google_apis::CANCELLED` and an empty event list, so there's no need
    // to remove it here.
    pending_fetches_[start_of_month]->Cancel();
  }
}

int CalendarModel::EventsNumberOfDay(base::Time day,
                                     SingleDayEventList* events) {
  const SingleDayEventList& list = FindEvents(day);

  if (list.empty())
    return 0;

  // There are events, and the destination should be empty.
  if (events) {
    DCHECK(events->empty());
    *events = list;
  }

  return list.size();
}

void CalendarModel::OnEventsFetched(
    base::Time start_of_month,
    google_apis::ApiErrorCode error,
    const google_apis::calendar::EventList* events) {
  base::UmaHistogramSparse("Ash.Calendar.FetchEvents.Result", error);
  if (error != google_apis::HTTP_SUCCESS) {
    // Request is no longer outstanding, so it can be destroyed.
    pending_fetches_.erase(start_of_month);
    // Notify observers that we had an error with the events fetched.
    // Right now the `kError` status isn't handled in any way so we just emit
    // `kNever` to stop the loading animation displaying.
    // TODO(https://crbug.com/1298187): Possibly respond further based on the
    // specific error code, retry in some cases, etc.
    for (auto& observer : observers_)
      observer.OnEventsFetched(kNever, start_of_month, events);

    return;
  }

  // Keep us within storage limits.
  PruneEventCache();

  // Clear out this month's events, we're about replace them.
  event_months_.erase(start_of_month);

  if (!events || events->items().empty()) {
    // Even though `start_of_month` has no events, insert an empty map to
    // indicate a successful fetch.
    SingleMonthEventMap empty_event_map;
    event_months_.emplace(start_of_month, empty_event_map);
    PromoteMonth(start_of_month);
  } else {
    // Store the incoming events.
    for (const auto& event : events->items()) {
      if (calendar_utils::IsMultiDayEvent(event.get()))
        InsertMultiDayEvent(event.get(), start_of_month);
      else {
        base::Time start_time_midnight =
            calendar_utils::GetStartTimeMidnightAdjusted(event.get());
        InsertEventInMonth(
            event.get(),
            calendar_utils::GetStartOfMonthUTC(start_time_midnight),
            start_time_midnight);
      }
    }
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
}

void CalendarModel::OnEventFetchFailedInternalError(
    base::Time start_of_month,
    CalendarEventFetchInternalErrorCode error) {
  // Request is no longer outstanding, so it can be destroyed.
  pending_fetches_.erase(start_of_month);
  // Notify observers of timeout fetching events for given `start_of_month`.
  // Right now timeouts aren't handled in any way so we just emit `onTimeout` to
  // stop the loading animation displaying.
  // TODO(https://crbug.com/1298187): May need to respond further based on the
  // specific error code, retry in some cases, etc.
  for (auto& observer : observers_)
    observer.OnTimeout(start_of_month);
}

bool CalendarModel::ShouldInsertEvent(const CalendarEvent* event) const {
  if (!event)
    return false;

  return base::Contains(kAllowedEventStatuses, event->status()) &&
         base::Contains(kAllowedResponseStatuses,
                        event->self_response_status());
}

void CalendarModel::InsertMultiDayEvent(
    const google_apis::calendar::CalendarEvent* event,
    const base::Time start_of_month) {
  DCHECK(event);

  if (event->all_day_event()) {
    auto current_day_utc = calendar_utils::GetMaxTime(
                               start_of_month, event->start_time().date_time())
                               .UTCMidnight();
    base::Time start_of_next_month =
        calendar_utils::GetStartOfNextMonthUTC(current_day_utc);
    // Don't go into the next month.
    auto last_day_utc = calendar_utils::GetMinTime(
                            start_of_next_month, event->end_time().date_time())
                            .UTCMidnight();

    // In the Calendar API, the end `base::Time` of an "all day" event will be
    // the following day at midnight. For example for a single "all day" event
    // on 1st October, the start `base::Time` will be 2022-10-01 00:00:00.000
    // UTC and the end `base::Time` will be 2022-10-02 00:00:00.000 UTC, so we
    // iterate until the day before the `last_day_utc` to ensure we don't show
    // the all day event incorrectly going on to the next day. As we are going
    // to always show these events by day rather than time, we don't care about
    // timezone here.
    while (current_day_utc < last_day_utc) {
      InsertEventInMonth(event, start_of_month, current_day_utc);
      current_day_utc = calendar_utils::GetNextDayMidnight(current_day_utc);
    }
    return;
  }

  base::Time start_time_midnight =
      calendar_utils::GetStartTimeMidnightAdjusted(event);
  base::Time end_time_midnight =
      calendar_utils::GetEndTimeMidnightAdjusted(event);
  base::Time end_time = calendar_utils::GetEndTimeAdjusted(event);

  base::Time current_day_midnight =
      calendar_utils::GetMaxTime(start_of_month, start_time_midnight)
          .UTCMidnight();
  base::Time start_of_next_month =
      calendar_utils::GetStartOfNextMonthUTC(current_day_midnight);
  base::Time last_day_midnight =
      calendar_utils::GetMinTime(start_of_next_month, end_time_midnight)
          .UTCMidnight();

  // If the event ends at midnight we don't add it to that last day.
  if (end_time == end_time_midnight)
    last_day_midnight =
        (last_day_midnight - calendar_utils::kDurationForGettingPreviousDay)
            .UTCMidnight();

  while (current_day_midnight <= last_day_midnight) {
    InsertEventInMonth(event, start_of_month, current_day_midnight);
    current_day_midnight =
        calendar_utils::GetNextDayMidnight(current_day_midnight);
  }
}

void CalendarModel::InsertEventInMonth(
    const google_apis::calendar::CalendarEvent* event,
    const base::Time start_of_month,
    const base::Time start_time_midnight) {
  DCHECK(event);

  // Check the event is in the month we're trying to insert it into.
  if (start_of_month != calendar_utils::GetStartOfMonthUTC(start_time_midnight))
    return;

  // Month is now the most-recently-used.
  PromoteMonth(start_of_month);

  auto it = event_months_.find(start_of_month);
  if (it == event_months_.end()) {
    // No events for this month, so add a map for it and insert.
    SingleMonthEventMap month;
    InsertEventInMonthEventList(month, event, start_time_midnight);
    event_months_.emplace(start_of_month, month);
  } else {
    // Insert in a pre-existing month.
    SingleMonthEventMap& month = it->second;
    InsertEventInMonthEventList(month, event, start_time_midnight);
  }
}

void CalendarModel::InsertEventInMonthEventList(
    SingleMonthEventMap& month,
    const google_apis::calendar::CalendarEvent* event,
    const base::Time start_time_midnight) {
  DCHECK(event);
  if (!ShouldInsertEvent(event))
    return;

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

SingleDayEventList CalendarModel::FindEvents(base::Time day) const {
  SingleDayEventList event_list;

  // Early return if there are no events for this month.
  base::Time start_of_month = calendar_utils::GetStartOfMonthUTC(day);
  auto it = event_months_.find(start_of_month);
  if (it == event_months_.end()) {
    return event_list;
  }

  // Early return if there are no events for this day.
  base::Time midnight = day.UTCMidnight();
  const SingleMonthEventMap& month = it->second;
  auto it2 = month.find(midnight);
  if (it2 == month.end()) {
    return event_list;
  }

  auto events = it2->second;
  SortByDateAscending(events);
  return events;
}

std::tuple<SingleDayEventList, SingleDayEventList>
CalendarModel::FindEventsSplitByMultiDayAndSameDay(base::Time day) const {
  return SplitEventsIntoMultiDayAndSameDay(FindEvents(day));
}

std::list<CalendarEvent> CalendarModel::FindUpcomingEvents(
    base::Time now_local) const {
  // First get today's events for `now_local`.
  auto upcoming_events = FindEvents(now_local);
  const base::Time now_in_ten_mins = now_local + base::Minutes(10);
  // If `now_in_ten_mins` is the subsequent day, include its events.
  if (now_in_ten_mins.UTCMidnight() != now_local.UTCMidnight()) {
    auto tomorrows_events = FindEvents(now_local + base::Days(1));
    upcoming_events.splice(upcoming_events.end(), tomorrows_events);
  }

  return FilterEventsStartingSoonOrRecentlyInProgress(upcoming_events,
                                                      now_local);
}

CalendarModel::FetchingStatus CalendarModel::FindFetchingStatus(
    base::Time start_time) const {
  if (!calendar_utils::ShouldFetchEvents())
    return kNa;

  if (pending_fetches_.count(start_time)) {
    if (event_months_.count(start_time))
      return kRefetching;

    return kFetching;
  }

  if (event_months_.count(start_time))
    return kSuccess;

  return kNever;
}

void CalendarModel::RedistributeEvents() {
  // Redistributes all the fetched events to the date map with the
  // time difference.
  std::set<google_apis::calendar::CalendarEvent, CmpEvent>
      to_be_redistributed_events;
  for (auto& month : event_months_) {
    SingleMonthEventMap& event_map = month.second;
    for (auto& it : event_map) {
      for (const google_apis::calendar::CalendarEvent& event : it.second) {
        to_be_redistributed_events.insert(event);
      }
    }
  }

  // Clear out the entire event store, freshly insert the redistributed
  // events.
  event_months_.clear();
  for (const google_apis::calendar::CalendarEvent& event :
       to_be_redistributed_events) {
    if (calendar_utils::IsMultiDayEvent(&event)) {
      // Only redistributes the multi-day events within the non-prunable months
      // scope. 1, This can avoid some coroner cases, e.g. some events that are
      // across several years. 2, we only cache the events for non-prunable
      // months.
      for (base::Time month : non_prunable_months_) {
        InsertMultiDayEvent(&event, month);
      }
    } else {
      base::Time start_time_midnight =
          calendar_utils::GetStartTimeMidnightAdjusted(&event);
      InsertEventInMonth(
          &event, calendar_utils::GetStartOfMonthUTC(start_time_midnight),
          start_time_midnight);
    }
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
