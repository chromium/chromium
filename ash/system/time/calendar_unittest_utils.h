// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_UNITTEST_UTILS_H_
#define ASH_SYSTEM_TIME_CALENDAR_UNITTEST_UTILS_H_

#include <list>
#include <memory>
#include <set>
#include <string>

#include "ash/calendar/calendar_client.h"
#include "ash/system/time/calendar_utils.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"

namespace ash {

namespace {
// This list is from "chromeos/ash/components/settings/timezone_settings.cc".
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

// These are from "third_party/fontconfig/include/fc-lang/fclang.h" data.
// Locales "und-zmth" and "und-zsye" are omitted since they cannot be set.
const char* kLocales[] = {
    "aa",     "ab",  "af",    "ak",    "am",    "an",    "ar",    "as",
    "ast",    "av",  "ay",    "az-az", "az-ir", "ba",    "be",    "ber-dz",
    "ber-ma", "bg",  "bh",    "bho",   "bi",    "bin",   "bm",    "bn",
    "bo",     "br",  "brx",   "bs",    "bua",   "byn",   "ca",    "ce",
    "ch",     "chm", "chr",   "co",    "crh",   "cs",    "csb",   "cu",
    "cv",     "cy",  "da",    "de",    "doi",   "dv",    "dz",    "ee",
    "el",     "en",  "eo",    "es",    "et",    "eu",    "fa",    "fat",
    "ff",     "fi",  "fil",   "fj",    "fo",    "fr",    "fur",   "fy",
    "ga",     "gd",  "gez",   "gl",    "gn",    "gu",    "gv",    "ha",
    "haw",    "he",  "hi",    "hne",   "ho",    "hr",    "ht",    "hu",
    "hy",     "hz",  "ia",    "id",    "ie",    "ig",    "ii",    "ik",
    "io",     "is",  "it",    "iu",    "ja",    "jv",    "ka",    "kaa",
    "kab",    "ki",  "kj",    "kk",    "kl",    "km",    "kn",    "ko",
    "kok",    "kr",  "ks",    "ku-am", "ku-iq", "ku-ir", "ku-tr", "kum",
    "kv",     "kw",  "kwm",   "ky",    "la",    "lah",   "lb",    "lez",
    "lg",     "li",  "ln",    "lo",    "lt",    "lv",    "mai",   "mg",
    "mh",     "mi",  "mk",    "ml",    "mn-cn", "mn-mn", "mni",   "mo",
    "mr",     "ms",  "mt",    "my",    "na",    "nb",    "nds",   "ne",
    "ng",     "nl",  "nn",    "no",    "nr",    "nso",   "nv",    "ny",
    "oc",     "om",  "or",    "os",    "ota",   "pa",    "pa-pk", "pap-an",
    "pap-aw", "pl",  "ps-af", "ps-pk", "pt",    "qu",    "quz",   "rm",
    "rn",     "ro",  "ru",    "rw",    "sa",    "sah",   "sat",   "sc",
    "sco",    "sd",  "se",    "sel",   "sg",    "sh",    "shs",   "si",
    "sid",    "sk",  "sl",    "sm",    "sma",   "smj",   "smn",   "sms",
    "sn",     "so",  "sq",    "sr",    "ss",    "st",    "su",    "sv",
    "sw",     "syr", "ta",    "te",    "tg",    "th",    "ti-er", "ti-et",
    "tig",    "tk",  "tl",    "tn",    "to",    "tr",    "ts",    "tt",
    "tw",     "ty",  "tyv",   "ug",    "uk",    "ur",    "uz",    "ve",
    "vi",     "vo",  "vot",   "wa",    "wal",   "wen",   "wo",    "xh",
    "yap",    "yi",  "yo",    "za",    "zh-cn", "zh-hk", "zh-mo", "zh-sg",
    "zh-tw",  "zu"};

std::set<std::string> kLocalesWithUniqueNumerals{"bn", "fa", "mr", "pa-pk"};

}  // namespace

namespace calendar_test_utils {

// Used for over-riding the locale that `base::Time` uses. Copied from
// `time_unittest.cc`.
class ScopedLibcTimeZone {
 public:
  explicit ScopedLibcTimeZone(const std::string& timezone);
  ~ScopedLibcTimeZone();

  ScopedLibcTimeZone(const ScopedLibcTimeZone& other) = delete;
  ScopedLibcTimeZone& operator=(const ScopedLibcTimeZone& other) = delete;

  bool is_success() const { return success_; }

 private:
  static constexpr char kTimeZoneEnvVarName[] = "TZ";

  bool success_ = true;
  std::optional<std::string> old_timezone_;
};

// A duration to let the animation finish and pass the cool down duration in
// tests.
constexpr base::TimeDelta kAnimationSettleDownDuration = base::Seconds(3);

// A duration which is smaller than any of the animation duration. So if there's
// an animation, the view should be in the middle of the animation.
constexpr base::TimeDelta kAnimationStartBufferDuration =
    base::Milliseconds(90);

// Creates a `google_apis::calendar::SingleCalendar` for testing only.
std::unique_ptr<google_apis::calendar::SingleCalendar> CreateCalendar(
    const std::string& id,
    const std::string& summary,
    const std::string& color_id,
    bool selected,
    bool primary);

std::unique_ptr<google_apis::calendar::CalendarList> CreateMockCalendarList(
    std::list<std::unique_ptr<google_apis::calendar::SingleCalendar>>
        calendars);

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
            google_apis::calendar::CalendarEvent::ResponseStatus::kAccepted,
    bool all_day_event = false,
    GURL video_conference_url = GURL());

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
            google_apis::calendar::CalendarEvent::ResponseStatus::kAccepted,
    bool all_day_event = false,
    GURL video_conference_url = GURL());

std::unique_ptr<google_apis::calendar::EventList> CreateMockEventList(
    std::list<std::unique_ptr<google_apis::calendar::CalendarEvent>> events);

// Checks if the two exploded are in the same month.
bool IsTheSameMonth(const base::Time date_a, const base::Time date_b);

// Returns the `base:Time` from the given string.
base::Time GetTimeFromString(const char* start_time);

// A mock `CalendarClient` which uses `SetEventList` and `SetError` to set the
// response. This mock client's `GetEventList` waits for a short duration to
// mock the fetching process. This client can be used in calendar unittests to
// mock the process of fetching events and getting back event list or error
// message. It needs to be registered to a calendar client in `SetUp`.
class CalendarClientTestImpl : public CalendarClient {
 public:
  CalendarClientTestImpl();
  CalendarClientTestImpl(const CalendarClientTestImpl& other) = delete;
  CalendarClientTestImpl& operator=(const CalendarClientTestImpl& other) =
      delete;
  ~CalendarClientTestImpl() override;

  void set_is_disabled_by_admin(bool is_disabled_by_admin) {
    is_disabled_by_admin_ = is_disabled_by_admin;
  }

  // CalendarClient:
  bool IsDisabledByAdmin() const override;
  base::OnceClosure GetCalendarList(
      google_apis::calendar::CalendarListCallback callback) override;
  base::OnceClosure GetEventList(
      google_apis::calendar::CalendarEventListCallback callback,
      const base::Time start_time,
      const base::Time end_time) override;
  base::OnceClosure GetEventList(
      google_apis::calendar::CalendarEventListCallback callback,
      const base::Time start_time,
      const base::Time end_time,
      const std::string& calendar_id,
      const std::string& calendar_color_id) override;

  // Sets `calendars` as the fetched calendar list.
  void SetCalendarList(
      std::unique_ptr<google_apis::calendar::CalendarList> calendars);

  // Sets `events` as the fetched event list.
  void SetEventList(std::unique_ptr<google_apis::calendar::EventList> events);

  // Sets `error` as the error message. The value of `error` is
  // `google_apis::HTTP_SUCCESS` by default.
  void SetError(google_apis::ApiErrorCode error) { error_ = error; }

  // Sets `delay` as the response delay. By default, the response delay is
  // `kAnimationSettleDownDuration` plus 2 seconds.
  void SetResponseDelay(const base::TimeDelta delay) { task_delay_ = delay; }

  // Force the task to take longer than the default timeout, causing an internal
  // error to be propagated.
  void ForceTimeout() {
    task_delay_ = calendar_utils::kCalendarDataFetchTimeout + base::Seconds(1);
  }

 private:
  bool is_disabled_by_admin_ = false;
  google_apis::ApiErrorCode error_ = google_apis::HTTP_SUCCESS;
  std::unique_ptr<google_apis::calendar::CalendarList> calendars_;
  std::unique_ptr<google_apis::calendar::EventList> events_;
  base::TimeDelta task_delay_ = kAnimationSettleDownDuration + base::Seconds(2);
};

}  // namespace calendar_test_utils

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_UNITTEST_UTILS_H_
