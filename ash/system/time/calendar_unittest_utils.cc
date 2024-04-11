// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_unittest_utils.h"

#include <string>

#include "ash/ash_export.h"
#include "base/environment.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"

namespace ash {

namespace calendar_test_utils {

ScopedLibcTimeZone::ScopedLibcTimeZone(const std::string& timezone) {
  auto env = base::Environment::Create();
  std::string old_timezone_value;
  if (env->GetVar(kTimeZoneEnvVarName, &old_timezone_value)) {
    old_timezone_ = old_timezone_value;
  }
  if (!env->SetVar(kTimeZoneEnvVarName, timezone)) {
    success_ = false;
  }
  tzset();
}

ScopedLibcTimeZone::~ScopedLibcTimeZone() {
  auto env = base::Environment::Create();
  if (old_timezone_.has_value()) {
    CHECK(env->SetVar(kTimeZoneEnvVarName, old_timezone_.value()));
  } else {
    CHECK(env->UnSetVar(kTimeZoneEnvVarName));
  }
}

std::unique_ptr<google_apis::calendar::SingleCalendar> CreateCalendar(
    const std::string& id,
    const std::string& summary,
    const std::string& color_id,
    bool selected,
    bool primary) {
  auto calendar = std::make_unique<google_apis::calendar::SingleCalendar>();
  calendar->set_id(id);
  calendar->set_summary(summary);
  calendar->set_color_id(color_id);
  calendar->set_selected(selected);
  calendar->set_primary(primary);
  return calendar;
}

std::unique_ptr<google_apis::calendar::CalendarList> CreateMockCalendarList(
    std::list<std::unique_ptr<google_apis::calendar::SingleCalendar>>
        calendars) {
  auto calendar_list = std::make_unique<google_apis::calendar::CalendarList>();

  for (auto& calendar : calendars) {
    calendar_list->InjectItemForTesting(std::move(calendar));
  }

  return calendar_list;
}

std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
    const char* id,
    const char* summary,
    const char* start_time,
    const char* end_time,
    const google_apis::calendar::CalendarEvent::EventStatus event_status,
    const google_apis::calendar::CalendarEvent::ResponseStatus
        self_response_status,
    bool all_day_event,
    GURL video_conference_url) {
  auto event = std::make_unique<google_apis::calendar::CalendarEvent>();
  base::Time start_time_base, end_time_base;
  google_apis::calendar::DateTime start_time_date, end_time_date;
  event->set_id(id);
  event->set_summary(summary);
  bool result;
  if (all_day_event)
    result = base::Time::FromUTCString(start_time, &start_time_base);
  else
    result = base::Time::FromString(start_time, &start_time_base);
  DCHECK(result);
  if (all_day_event)
    result = base::Time::FromUTCString(end_time, &end_time_base);
  else
    result = base::Time::FromString(end_time, &end_time_base);
  DCHECK(result);
  start_time_date.set_date_time(start_time_base);
  end_time_date.set_date_time(end_time_base);
  event->set_start_time(start_time_date);
  event->set_end_time(end_time_date);
  event->set_status(event_status);
  event->set_self_response_status(self_response_status);
  event->set_all_day_event(all_day_event);
  event->set_conference_data_uri(video_conference_url);
  return event;
}

std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
    const char* id,
    const char* summary,
    base::Time start_time,
    base::Time end_time,
    const google_apis::calendar::CalendarEvent::EventStatus event_status,
    const google_apis::calendar::CalendarEvent::ResponseStatus
        self_response_status,
    bool all_day_event,
    GURL video_conference_url) {
  auto event = std::make_unique<google_apis::calendar::CalendarEvent>();
  google_apis::calendar::DateTime start_time_date, end_time_date;
  event->set_id(id);
  event->set_summary(summary);
  start_time_date.set_date_time(start_time);
  end_time_date.set_date_time(end_time);
  event->set_start_time(start_time_date);
  event->set_end_time(end_time_date);
  event->set_status(event_status);
  event->set_self_response_status(self_response_status);
  event->set_all_day_event(all_day_event);
  event->set_conference_data_uri(video_conference_url);
  return event;
}

std::unique_ptr<google_apis::calendar::EventList> CreateMockEventList(
    std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events) {
  auto event_list = std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("Greenwich Mean Time");

  for (auto& event : events)
    event_list->InjectItemForTesting(std::move(event));

  return event_list;
}

ASH_EXPORT bool IsTheSameMonth(const base::Time date_a,
                               const base::Time date_b) {
  return base::UnlocalizedTimeFormatWithPattern(date_a, "MM YYYY") ==
         base::UnlocalizedTimeFormatWithPattern(date_b, "MM YYYY");
}

base::Time GetTimeFromString(const char* start_time) {
  base::Time date;
  bool result = base::Time::FromString(start_time, &date);
  DCHECK(result);
  return date;
}

CalendarClientTestImpl::CalendarClientTestImpl() = default;

CalendarClientTestImpl::~CalendarClientTestImpl() = default;

bool CalendarClientTestImpl::IsDisabledByAdmin() const {
  return is_disabled_by_admin_;
}

base::OnceClosure CalendarClientTestImpl::GetCalendarList(
    google_apis::calendar::CalendarListCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), error_, std::move(calendars_)),
      task_delay_);

  return base::DoNothing();
}

base::OnceClosure CalendarClientTestImpl::GetEventList(
    google_apis::calendar::CalendarEventListCallback callback,
    const base::Time start_time,
    const base::Time end_time) {
  // Give it a little bit of time to mock the api calling. This duration is a
  // little longer than the settle down duration, so in the test after the
  // animation settled down it can still be with `kFetching` status until
  // somemethod like `WaitUntilFetched` is called.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), error_, std::move(events_)),
      task_delay_);

  return base::DoNothing();
}

base::OnceClosure CalendarClientTestImpl::GetEventList(
    google_apis::calendar::CalendarEventListCallback callback,
    const base::Time start_time,
    const base::Time end_time,
    const std::string& calendar_id,
    const std::string& calendar_color_id) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), error_, std::move(events_)),
      task_delay_);

  return base::DoNothing();
}

void CalendarClientTestImpl::SetCalendarList(
    std::unique_ptr<google_apis::calendar::CalendarList> calendars) {
  calendars_ = std::move(calendars);
}

void CalendarClientTestImpl::SetEventList(
    std::unique_ptr<google_apis::calendar::EventList> events) {
  events_.reset();
  events_ = std::move(events);
}

}  // namespace calendar_test_utils

}  // namespace ash
