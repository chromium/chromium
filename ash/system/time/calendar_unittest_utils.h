// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_UNITTEST_UTILS_H_
#define ASH_SYSTEM_TIME_CALENDAR_UNITTEST_UTILS_H_

#include <string>

#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"

namespace ash {

namespace {
// This list is from "ash/components/settings/timezone_settings.cc"
const char* kAllTimeZones[] = {"Pacific/Midway",
                               "Pacific/Honolulu",
                               "America/Anchorage",
                               "America/Los_Angeles",
                               "America/Vancouver",
                               "America/Tijuana",
                               "America/Phoenix",
                               "America/Chihuahua",
                               "America/Denver",
                               "America/Edmonton",
                               "America/Mazatlan",
                               "America/Regina",
                               "America/Costa_Rica",
                               "America/Chicago",
                               "America/Mexico_City",
                               "America/Tegucigalpa",
                               "America/Winnipeg",
                               "Pacific/Easter",
                               "America/Bogota",
                               "America/Lima",
                               "America/New_York",
                               "America/Toronto",
                               "America/Caracas",
                               "America/Barbados",
                               "America/Halifax",
                               "America/Manaus",
                               "America/Santiago",
                               "America/St_Johns",
                               "America/Araguaina",
                               "America/Argentina/Buenos_Aires",
                               "America/Argentina/San_Luis",
                               "America/Montevideo",
                               "America/Santiago",
                               "America/Sao_Paulo",
                               "America/Godthab",
                               "Atlantic/South_Georgia",
                               "Atlantic/Cape_Verde",
                               "Etc/GMT",
                               "Atlantic/Azores",
                               "Atlantic/Reykjavik",
                               "Atlantic/St_Helena",
                               "Africa/Casablanca",
                               "Atlantic/Faroe",
                               "Europe/Dublin",
                               "Europe/Lisbon",
                               "Europe/London",
                               "Europe/Amsterdam",
                               "Europe/Belgrade",
                               "Europe/Berlin",
                               "Europe/Bratislava",
                               "Europe/Brussels",
                               "Europe/Budapest",
                               "Europe/Copenhagen",
                               "Europe/Ljubljana",
                               "Europe/Madrid",
                               "Europe/Malta",
                               "Europe/Oslo",
                               "Europe/Paris",
                               "Europe/Prague",
                               "Europe/Rome",
                               "Europe/Stockholm",
                               "Europe/Sarajevo",
                               "Europe/Tirane",
                               "Europe/Vaduz",
                               "Europe/Vienna",
                               "Europe/Warsaw",
                               "Europe/Zagreb",
                               "Europe/Zurich",
                               "Africa/Windhoek",
                               "Africa/Lagos",
                               "Africa/Brazzaville",
                               "Africa/Cairo",
                               "Africa/Harare",
                               "Africa/Maputo",
                               "Africa/Johannesburg",
                               "Europe/Kaliningrad",
                               "Europe/Athens",
                               "Europe/Bucharest",
                               "Europe/Chisinau",
                               "Europe/Helsinki",
                               "Europe/Istanbul",
                               "Europe/Kiev",
                               "Europe/Riga",
                               "Europe/Sofia",
                               "Europe/Tallinn",
                               "Europe/Vilnius",
                               "Asia/Amman",
                               "Asia/Beirut",
                               "Asia/Jerusalem",
                               "Africa/Nairobi",
                               "Asia/Baghdad",
                               "Asia/Riyadh",
                               "Asia/Kuwait",
                               "Europe/Minsk",
                               "Europe/Moscow",
                               "Asia/Tehran",
                               "Europe/Samara",
                               "Asia/Dubai",
                               "Asia/Tbilisi",
                               "Indian/Mauritius",
                               "Asia/Baku",
                               "Asia/Yerevan",
                               "Asia/Kabul",
                               "Asia/Karachi",
                               "Asia/Aqtobe",
                               "Asia/Ashgabat",
                               "Asia/Oral",
                               "Asia/Yekaterinburg",
                               "Asia/Calcutta",
                               "Asia/Colombo",
                               "Asia/Katmandu",
                               "Asia/Omsk",
                               "Asia/Almaty",
                               "Asia/Dhaka",
                               "Asia/Novosibirsk",
                               "Asia/Rangoon",
                               "Asia/Bangkok",
                               "Asia/Jakarta",
                               "Asia/Krasnoyarsk",
                               "Asia/Novokuznetsk",
                               "Asia/Ho_Chi_Minh",
                               "Asia/Phnom_Penh",
                               "Asia/Vientiane",
                               "Asia/Shanghai",
                               "Asia/Hong_Kong",
                               "Asia/Kuala_Lumpur",
                               "Asia/Singapore",
                               "Asia/Manila",
                               "Asia/Taipei",
                               "Asia/Ulaanbaatar",
                               "Asia/Makassar",
                               "Asia/Irkutsk",
                               "Asia/Yakutsk",
                               "Australia/Perth",
                               "Australia/Eucla",
                               "Asia/Seoul",
                               "Asia/Tokyo",
                               "Asia/Jayapura",
                               "Asia/Sakhalin",
                               "Asia/Vladivostok",
                               "Asia/Magadan",
                               "Australia/Darwin",
                               "Australia/Adelaide",
                               "Pacific/Guam",
                               "Australia/Brisbane",
                               "Australia/Hobart",
                               "Australia/Sydney",
                               "Asia/Anadyr",
                               "Pacific/Port_Moresby",
                               "Asia/Kamchatka",
                               "Pacific/Fiji",
                               "Pacific/Majuro",
                               "Pacific/Auckland",
                               "Pacific/Tongatapu",
                               "Pacific/Apia",
                               "Pacific/Kiritimati"};
}  // namespace

namespace calendar_test_utils {

// A duration to let the animation finish and pass the cool down duration in
// tests.
constexpr base::TimeDelta kAnimationSettleDownDuration = base::Seconds(3);

// A duration which is smaller than any of the animation duration. So if there's
// an animation, the view should be in the middle of the animation.
constexpr base::TimeDelta kAnimationStartBufferDuration =
    base::Milliseconds(90);

// Creates a `google_apis::calendar::CalendarEvent` for testing, that converts
// start/end time strings to `google_apis::calendar::DateTime`.
std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
    const char* id,
    const char* summary,
    const char* start_time,
    const char* end_time,
    const google_apis::calendar::CalendarEvent::EventStatus event_status =
        google_apis::calendar::CalendarEvent::EventStatus::kConfirmed,
    const google_apis::calendar::CalendarEvent::ResponseStatus
        self_response_status =
            google_apis::calendar::CalendarEvent::ResponseStatus::kAccepted);

// Creates a `google_apis::calendar::CalendarEvent` for testing, that converts
// start/end `base::Time` objects to `google_apis::calendar::DateTime`.
std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
    const char* id,
    const char* summary,
    base::Time start_time,
    base::Time end_time,
    const google_apis::calendar::CalendarEvent::EventStatus event_status =
        google_apis::calendar::CalendarEvent::EventStatus::kConfirmed,
    const google_apis::calendar::CalendarEvent::ResponseStatus
        self_response_status =
            google_apis::calendar::CalendarEvent::ResponseStatus::kAccepted);

// Checks if the two exploded are in the same month.
bool IsTheSameMonth(const base::Time& date_a, const base::Time& date_b);

// Returns the `base:Time` from the given string.
base::Time GetTimeFromString(const char* start_time);

}  // namespace calendar_test_utils

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_UNITTEST_UTILS_H_
