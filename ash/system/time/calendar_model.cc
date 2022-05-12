// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "base/bind.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
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

  // Bail out early if there is no CalendarClient.  This will be the case in
  // most unit tests.
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

void CalendarModel::CancelFetch(const base::Time& start_of_month) {
  if (base::Contains(pending_fetches_, start_of_month)) {
    // Note that the `CalendarEventFetch` here will be removed from
    // `pending_fetches_` in `OnEventsFetched`, which will receive an error code
    // of `google_apis::CANCELLED` and an empty event list, so there's no need
    // to remove it here.
    pending_fetches_[start_of_month]->Cancel();
  }
}

int CalendarModel::EventsNumberOfDayInternal(base::Time day,
                                             SingleDayEventList* events) const {
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

int CalendarModel::EventsNumberOfDay(base::Time day,
                                     SingleDayEventList* events) {
  const base::Time start_of_month = calendar_utils::GetStartOfMonthUTC(day);
  if (base::Contains(event_months_, start_of_month))
    PromoteMonth(start_of_month);
  return EventsNumberOfDayInternal(day, events);
}

void CalendarModel::OnEventsFetched(
    base::Time start_of_month,
    google_apis::ApiErrorCode error,
    const google_apis::calendar::EventList* events) {
  base::UmaHistogramSparse("Ash.Calendar.FetchEvents.Result", error);
  if (error != google_apis::HTTP_SUCCESS) {
    // Request is no longer outstanding, so it can be destroyed.
    pending_fetches_.erase(start_of_month);
    // TODO(https://crbug.com/1298187): Possibly respond further based on the
    // specific error code, retry in some cases, etc. Or notify observers e.g.
    // observer.OnEventsFetched(kError, start_of_month, events);
    return;
  }

  if (ash::features::IsCalendarModelDebugModeEnabled() && events) {
    VLOG(1) << __FUNCTION__ << " month " << start_of_month << " num events "
            << events->items().size();

    // It is possible for incoming events to have a start date (adjusted for
    // timezone differences) that's not in `start_of_month`. The code below
    // outputs a breakdown of the events by month.
    if (events->items().size() > 0) {
      std::map<base::Time, int> included_months;
      for (auto& event : events->items()) {
        base::Time adjusted_start = GetStartTimeAdjusted(event.get());
        base::Time adjusted_start_of_month =
            calendar_utils::GetStartOfMonthUTC(adjusted_start);
        if (included_months.find(adjusted_start_of_month) ==
            included_months.end()) {
          included_months[adjusted_start_of_month] = 1;
        } else {
          included_months[adjusted_start_of_month]++;
        }
      }

      if (included_months.size() > 1) {
        VLOG(1) << __FUNCTION__ << " breakdown:";
        for (auto& included_month : included_months) {
          VLOG(1) << __FUNCTION__ << "   " << included_month.first << " ("
                  << included_month.second << ")";
        }
      }
    }
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
  // the calendar first opened, then update the max distance.
  UpdateMaxDistanceBrowsed(start_of_month);
}

void CalendarModel::OnEventFetchFailedInternalError(
    base::Time start_of_month,
    CalendarEventFetchInternalErrorCode error) {
  // Request is no longer outstanding, so it can be destroyed.
  pending_fetches_.erase(start_of_month);
  // TODO(https://crbug.com/1298187): May need to respond further based on the
  // specific error code, retry in some cases, etc.
}

void CalendarModel::UpdateMaxDistanceBrowsed(const base::Time& start_of_month) {
  max_distance_browsed_ =
      std::max(max_distance_browsed_,
               static_cast<size_t>(abs(calendar_utils::GetMonthsBetween(
                   first_on_screen_month_, start_of_month))));
}

bool CalendarModel::ShouldInsertEvent(const CalendarEvent* event) const {
  if (!event)
    return false;

  return base::Contains(kAllowedEventStatuses, event->status()) &&
         base::Contains(kAllowedResponseStatuses,
                        event->self_response_status());
}

void CalendarModel::InsertEvent(
    const google_apis::calendar::CalendarEvent* event) {
  DCHECK(event);

  base::Time start_of_month =
      calendar_utils::GetStartOfMonthUTC(GetStartTimeMidnightAdjusted(event));

  if (ash::features::IsCalendarModelDebugModeEnabled()) {
    VLOG(1) << __FUNCTION__ << " start_of_month " << start_of_month;
    DebugDumpEventLarge(__FUNCTION__, event);
  }

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
  if (!ShouldInsertEvent(event))
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

base::Time CalendarModel::GetStartTimeAdjusted(
    const google_apis::calendar::CalendarEvent* event) const {
  if (time_difference_minutes_.has_value()) {
    return event->start_time().date_time() +
           base::Minutes(time_difference_minutes_.value());
  }
  return event->start_time().date_time();
}

base::Time CalendarModel::GetEndTimeAdjusted(
    const google_apis::calendar::CalendarEvent* event) const {
  if (time_difference_minutes_.has_value()) {
    return event->end_time().date_time() +
           base::Minutes(time_difference_minutes_.value());
  }
  return event->start_time().date_time();
}

base::Time CalendarModel::GetStartTimeMidnightAdjusted(
    const google_apis::calendar::CalendarEvent* event) const {
  return GetStartTimeAdjusted(event).UTCMidnight();
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

  // Early return if there are no events for this month.
  base::Time start_of_month = calendar_utils::GetStartOfMonthUTC(day);
  auto it = event_months_.find(start_of_month);
  if (it == event_months_.end())
    return event_list;

  // Early return if there are no events for this day.
  base::Time midnight = day.UTCMidnight();
  const SingleMonthEventMap& month = it->second;
  auto it2 = month.find(midnight);
  if (it2 == month.end())
    return event_list;

  return it2->second;
}

CalendarModel::FetchingStatus CalendarModel::FindFetchingStatus(
    base::Time start_time) const {
  if (pending_fetches_.count(start_time))
    return kFetching;

  if (event_months_.count(start_time))
    return kSuccess;

  return kNever;
}

void CalendarModel::DebugDumpEventSmall(
    std::ostringstream* out,
    const char* prefix,
    const google_apis::calendar::CalendarEvent* event) {
  if (!event)
    return;

  *out << prefix << "      "
       << calendar_utils::GetTwelveHourClockTime(
              event->start_time().date_time())
       << " -> "
       << calendar_utils::GetTwelveHourClockTime(event->end_time().date_time())
       << " (" << event->summary().substr(0, 6) << "...)"
       << "\n";
}

void CalendarModel::DebugDumpEventLarge(
    const char* prefix,
    const google_apis::calendar::CalendarEvent* event) {
  if (!event)
    return;

  VLOG(1) << prefix << " ID: " << event->id();
  VLOG(1) << prefix << "  summary: \"" << event->summary().substr(0, 6)
          << "...\"";
  VLOG(1) << prefix << "  st/et: " << event->start_time().date_time() << " => "
          << event->end_time().date_time();
  VLOG(1) << prefix << "  (adj): " << GetStartTimeAdjusted(event) << " => "
          << GetEndTimeAdjusted(event);
}

void CalendarModel::DebugDumpEvents(std::ostringstream* out,
                                    const char* prefix) {
  *out << prefix << " event_months_ START size: " << event_months_.size()
       << "\n";
  for (auto& month : event_months_) {
    *out << prefix << " month: " << month.first << "\n";
    for (auto& day : month.second) {
      *out << prefix << "   day: " << day.first << "\n";
      for (auto it = day.second.begin(); it != day.second.end(); ++it) {
        google_apis::calendar::CalendarEvent event = *it;
        DebugDumpEventSmall(out, prefix, &event);
      }
    }
  }
  *out << prefix << " event_months_ END"
       << "\n";
}

void CalendarModel::DebugDumpMruMonths(std::ostringstream* out,
                                       const char* prefix) {
  *out << prefix << " mru_months_ START size: " << mru_months_.size() << "\n";
  for (auto& month : mru_months_) {
    *out << prefix << "   " << month << "\n";
  }
  *out << prefix << " mru_months_ END"
       << "\n";
}

void CalendarModel::DebugDumpNonPrunableMonths(std::ostringstream* out,
                                               const char* prefix) {
  *out << prefix
       << " non_prunable_months_ START size: " << non_prunable_months_.size()
       << "\n";
  for (auto& month : non_prunable_months_) {
    *out << prefix << "   " << month << "\n";
  }
  *out << prefix << " non_prunable_months_ END"
       << "\n";
}

void CalendarModel::DebugDumpMonthsFetched(std::ostringstream* out,
                                           const char* prefix) {
  *out << prefix << " months_fetched_ START size: " << months_fetched_.size()
       << "\n";
  for (auto& month : months_fetched_) {
    *out << prefix << "   " << month << "\n";
  }
  *out << prefix << " months_fetched_ END"
       << "\n";
}

void CalendarModel::DebugDump() {
  std::ostringstream out;
  const char* kDebugDumpPrefix = "CalendarModelDump: ";
  out << __FUNCTION__ << " START"
      << "\n";
  DebugDumpEvents(&out, kDebugDumpPrefix);
  DebugDumpMruMonths(&out, kDebugDumpPrefix);
  DebugDumpNonPrunableMonths(&out, kDebugDumpPrefix);
  DebugDumpMonthsFetched(&out, kDebugDumpPrefix);
  out << __FUNCTION__ << " END"
      << "\n";
  VLOG(1) << out.str();
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
