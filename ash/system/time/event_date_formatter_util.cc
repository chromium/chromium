// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/event_date_formatter_util.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/date_helper.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::event_date_formatter_util {
namespace {

bool Is12HourClock() {
  return Shell::Get()->system_tray_model()->clock()->hour_clock_type() ==
         base::k12HourClock;
}

// Calculate the number of elapsed days so far.
// We add 1 as if 1 day has passed in the event, then we're on Day 2.
// `selected_date_midnight` will be the selected date at 00:00:00 UTC.
// `selected_date_midnight_utc` will be the selected date adjusted for local
// timezone in UTC.
int GetEventElapsedDayCount(const google_apis::calendar::CalendarEvent* event,
                            const base::Time& selected_date_midnight,
                            const base::Time& selected_date_midnight_utc) {
  // For all day events, we can just take selected midnight UTC minus the
  // event start time, as all day events start at midnight UTC.
  if (event->all_day_event()) {
    return (selected_date_midnight - event->start_time().date_time()).InDays() +
           1;
  }

  // For other events, we take the adjusted to local selected midnight minus the
  // adjusted to local midnight event start time.
  const auto start_time_local_midnight =
      DateHelper::GetInstance()->GetLocalMidnight(
          event->start_time().date_time());
  return (selected_date_midnight_utc - start_time_local_midnight).InDays() + 1;
}

int GetEventTotalDayCount(const google_apis::calendar::CalendarEvent* event) {
  const auto start_time = calendar_utils::GetStartTimeMidnightAdjusted(event);
  const auto end_time = calendar_utils::GetEndTimeMidnightAdjusted(event);

  const int total_day_count = (end_time - start_time).InDays();

  // Events ending at midnight of the following day that the event ends, i.e.
  // all day events or multi-day events that finish at midnight in the local
  // timezone, shouldn't be included in the total day count.
  // `base::Time::InDays()` will be correct for these events, e.g. a 2 day,
  // all day event with start and end times of 20220101 00:00:00 UTC - 20220103
  // 00:00:00 UTC will be calculated as 2 days in time. Technically the event
  // spans a 3 day period, but we want to show this as a 2 day event.
  const auto end_time_adjusted = calendar_utils::GetEndTimeAdjusted(event);
  base::Time::Exploded exploded_end_time;
  end_time_adjusted.UTCExplode(&exploded_end_time);

  auto event_ends_at_midnight =
      (exploded_end_time.hour == 0 && exploded_end_time.minute == 0);
  if (event->all_day_event() || event_ends_at_midnight)
    return total_day_count;

  // For multi-day events not ending at midnight, they'll span multiple days,
  // but the `base::Time::InDays()` function will return 1 less than the total
  // amount of days that an event might span e.g. for a 2 day, multi-day
  // event of 20220101 08:00:00 UTC - 20220102 08:00:00 UTC, the elapsed
  // time is 1 day, but it spans over 2 days.
  return total_day_count + 1;
}

// Calculates the total and elapsed number of days for the event.
// Returns "(Day n / n)".
const std::u16string GetEventDayText(
    const google_apis::calendar::CalendarEvent* event,
    const base::Time& selected_date_midnight,
    const base::Time& selected_date_midnight_utc) {
  const int elapsed_day_count = GetEventElapsedDayCount(
      event, selected_date_midnight, selected_date_midnight_utc);
  const int total_day_count = GetEventTotalDayCount(event);

  return l10n_util::GetStringFUTF16(IDS_ASH_CALENDAR_EVENT_ENTRY_DAYS_ELAPSED,
                                    base::FormatNumber(elapsed_day_count),
                                    base::FormatNumber(total_day_count));
}
}  // namespace

ASH_EXPORT const std::tuple<std::u16string, std::u16string>
GetStartAndEndTimeAccessibleNames(base::Time start_time, base::Time end_time) {
  if (Is12HourClock()) {
    return std::make_tuple(calendar_utils::GetTwelveHourClockTime(start_time),
                           calendar_utils::GetTwelveHourClockTime(end_time));
  }

  return std::make_tuple(calendar_utils::GetTwentyFourHourClockTime(start_time),
                         calendar_utils::GetTwentyFourHourClockTime(end_time));
}

ASH_EXPORT const std::u16string GetFormattedInterval(base::Time start_time,
                                                     base::Time end_time) {
  if (Is12HourClock()) {
    return calendar_utils::FormatTwelveHourClockTimeInterval(start_time,
                                                             end_time);
  }

  return calendar_utils::FormatTwentyFourHourClockTimeInterval(start_time,
                                                               end_time);
}

ASH_EXPORT const std::u16string GetMultiDayText(
    const google_apis::calendar::CalendarEvent* event,
    const base::Time& selected_date_midnight,
    const base::Time& selected_date_midnight_utc) {
  const auto day_text = GetEventDayText(event, selected_date_midnight,
                                        selected_date_midnight_utc);

  // Returns "(Day n / n)".
  if (event->all_day_event())
    return day_text;

  const auto end_time_local_midnight =
      calendar_utils::GetEndTimeMidnightAdjusted(event);
  const auto [start_time, end_time] = GetStartAndEndTimeAccessibleNames(
      event->start_time().date_time(), event->end_time().date_time());

  // Returns "Starts at `start_time` `day_text`.
  if (selected_date_midnight < end_time_local_midnight) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_CALENDAR_EVENT_ENTRY_STARTS_AT_TIME, start_time, day_text);
  }

  // Returns "Ends at `end_time` `day_text`.
  if (selected_date_midnight == end_time_local_midnight) {
    return l10n_util::GetStringFUTF16(IDS_ASH_CALENDAR_EVENT_ENTRY_ENDS_AT_TIME,
                                      end_time, day_text);
  }

  NOTREACHED()
      << "The `selected_date_midnight` is past the end of the event. Value is: "
      << selected_date_midnight;
}

}  // namespace ash::event_date_formatter_util
