// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_list_model.h"

#include <iterator>
#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "ash/calendar/calendar_client.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/shell.h"
#include "ash/system/time/calendar_event_fetch_types.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/time/calendar_utils.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/common/api_error_codes.h"

namespace {

// Compares two calendars by primary field first, then alphabetically by
// summary (i.e. Calendar name) if neither calendar is primary.
bool CompareByPrimaryAndSummary(
    google_apis::calendar::SingleCalendar calendar_one,
    google_apis::calendar::SingleCalendar calendar_two) {
  if (calendar_one.primary()) {
    return true;
  }
  if (calendar_two.primary()) {
    return false;
  }
  return base::CompareCaseInsensitiveASCII(calendar_one.summary(),
                                           calendar_two.summary()) < 0;
}

void FilterForSelectedCalendars(ash::CalendarList& calendars) {
  calendars.remove_if([](google_apis::calendar::SingleCalendar calendar) {
    return !calendar.selected();
  });
}

}  // namespace

namespace ash {

CalendarListModel::CalendarListModel()
    : timeout_(nullptr), session_observer_(this) {}

CalendarListModel::~CalendarListModel() = default;

void CalendarListModel::OnSessionStateChanged(
    session_manager::SessionState state) {
  ClearCacheAndCancelFetch();
}

void CalendarListModel::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  ClearCacheAndCancelFetch();
}

void CalendarListModel::AddObserver(Observer* observer) {
  if (observer) {
    observers_.AddObserver(observer);
  }
}

void CalendarListModel::RemoveObserver(Observer* observer) {
  if (observer) {
    observers_.RemoveObserver(observer);
  }
}

void CalendarListModel::FetchCalendars() {
  if (!calendar_utils::ShouldFetchCalendarData()) {
    return;
  }

  CancelFetch();

  fetch_in_progress_ = true;
  fetch_start_time_ = base::TimeTicks::Now();

  CalendarClient* client = Shell::Get()->calendar_controller()->GetClient();

  // Bail out early if there is no CalendarClient. This will be the case in
  // most unit tests.
  if (!client) {
    CHECK_IS_TEST();
    return;
  }

  cancel_closure_ = client->GetCalendarList(base::BindOnce(
      &CalendarListModel::OnCalendarListFetched, weak_factory_.GetWeakPtr()));
  CHECK(cancel_closure_);

  timeout_.Start(FROM_HERE, calendar_utils::kCalendarDataFetchTimeout,
                 base::BindOnce(&CalendarListModel::OnCalendarListFetchTimeout,
                                weak_factory_.GetWeakPtr()));
}

void CalendarListModel::CancelFetch() {
  timeout_.Stop();
  if (cancel_closure_) {
    std::move(cancel_closure_).Run();
  }
}

CalendarList CalendarListModel::GetCachedCalendarList() {
  // Since the calendar list is kept until it is replaced during a re-fetch
  // (or removed during a session change), we check is_cached_ before returning
  // the list.
  if (get_is_cached()) {
    return calendar_list_;
  }
  CalendarList empty_calendar_list;
  return empty_calendar_list;
}

void CalendarListModel::OnCalendarListFetched(
    google_apis::ApiErrorCode error,
    std::unique_ptr<google_apis::calendar::CalendarList> calendars) {
  // Cancel timeout timer unless it was previously stopped on Cancel.
  if (error != google_apis::CANCELLED) {
    timeout_.Stop();
  }

  calendar_metrics::RecordCalendarListFetchDuration(base::TimeTicks::Now() -
                                                    fetch_start_time_);
  calendar_metrics::RecordCalendarListFetchErrorCode(error);
  calendar_metrics::RecordCalendarListFetchTimeout(false);

  if (error == google_apis::HTTP_SUCCESS) {
    if (calendars && !calendars->items().empty()) {
      ClearCachedCalendarList();
      for (auto& calendar : calendars->items()) {
        calendar_list_.push_back(*calendar.get());
      }
      FilterForSelectedCalendars(calendar_list_);

      calendar_metrics::RecordTotalSelectedCalendars(calendar_list_.size());

      // The ordering of the calendar list is not always consistent between
      // API calls, so calendar lists are sorted to maintain consistency of
      // which events are shown. The sorter also moves the primary calendar
      // to the top of the list.
      calendar_list_.sort(CompareByPrimaryAndSummary);
      if (calendar_list_.size() > calendar_utils::kMultipleCalendarsLimit) {
        calendar_list_.resize(calendar_utils::kMultipleCalendarsLimit);
      }
      is_cached_ = true;
    }
  }
  // In case of error, we fallback to a previously cached calendar list if it
  // exists. So we still notify observers of completion in all cases.
  fetch_in_progress_ = false;
  for (auto& observer : observers_) {
    observer.OnCalendarListFetchComplete();
  }
}

void CalendarListModel::OnCalendarListFetchTimeout() {
  calendar_metrics::RecordCalendarListFetchTimeout(true);

  fetch_in_progress_ = false;
  for (auto& observer : observers_) {
    observer.OnCalendarListFetchComplete();
  }
}

void CalendarListModel::ClearCachedCalendarList() {
  calendar_list_.clear();
  is_cached_ = false;
}

void CalendarListModel::ClearCacheAndCancelFetch() {
  CancelFetch();
  fetch_in_progress_ = false;
  ClearCachedCalendarList();
}

}  // namespace ash
