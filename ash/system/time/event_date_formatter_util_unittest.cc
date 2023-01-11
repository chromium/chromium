// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/event_date_formatter_util.h"
#include <string>

#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"

namespace ash {
namespace {

std::unique_ptr<google_apis::calendar::CalendarEvent> CreateEvent(
    const char* start_time,
    const char* end_time,
    bool all_day_event = false) {
  return calendar_test_utils::CreateEvent(
      "id_7", "summary_7", start_time, end_time,
      google_apis::calendar::CalendarEvent::EventStatus::kConfirmed,
      google_apis::calendar::CalendarEvent::ResponseStatus::kAccepted,
      all_day_event);
}

}  // namespace

using EventDateFormatterUtilTest = AshTestBase;

TEST_F(EventDateFormatterUtilTest,
       GetStartAndEndTimesAccessibleNames_24HourClock) {
  const char* start_time_string = "22 Nov 2021 23:30 GMT";
  const char* end_time_string = "23 Nov 2021 0:30 GMT";
  base::Time start_time, end_time;
  ash::system::ScopedTimezoneSettings timezone_settings(u"PST");
  Shell::Get()->system_tray_model()->SetUse24HourClock(true);

  EXPECT_TRUE(base::Time::FromString(start_time_string, &start_time));
  EXPECT_TRUE(base::Time::FromString(end_time_string, &end_time));

  const auto [actual_start, actual_end] =
      event_date_formatter_util::GetStartAndEndTimeAccessibleNames(start_time,
                                                                   end_time);

  EXPECT_EQ(actual_start, u"15:30");
  EXPECT_EQ(actual_end, u"16:30");
}

TEST_F(EventDateFormatterUtilTest,
       GetStartAndEndTimesAccessibleNames_12HourClock) {
  const char* start_time_string = "22 Nov 2021 23:30 GMT";
  const char* end_time_string = "23 Nov 2021 0:30 GMT";
  base::Time start_time, end_time;
  ash::system::ScopedTimezoneSettings timezone_settings(u"PST");
  Shell::Get()->system_tray_model()->SetUse24HourClock(false);

  EXPECT_TRUE(base::Time::FromString(start_time_string, &start_time));
  EXPECT_TRUE(base::Time::FromString(end_time_string, &end_time));

  const auto [actual_start, actual_end] =
      event_date_formatter_util::GetStartAndEndTimeAccessibleNames(start_time,
                                                                   end_time);

  EXPECT_EQ(actual_start, u"3:30\u202fPM");
  EXPECT_EQ(actual_end, u"4:30\u202fPM");
}

TEST_F(EventDateFormatterUtilTest, GetFormattedInterval_12HourClock) {
  const char* start_time_string = "22 Nov 2021 23:30 GMT";
  const char* end_time_string = "23 Nov 2021 0:30 GMT";
  base::Time start_time, end_time;
  ash::system::ScopedTimezoneSettings timezone_settings(u"PST");
  Shell::Get()->system_tray_model()->SetUse24HourClock(false);

  EXPECT_TRUE(base::Time::FromString(start_time_string, &start_time));
  EXPECT_TRUE(base::Time::FromString(end_time_string, &end_time));

  const auto actual =
      event_date_formatter_util::GetFormattedInterval(start_time, end_time);

  // \x2013 is unicode for dash i.e. '-'
  EXPECT_EQ(actual, u"3:30\u2009\x2013\u20094:30\u202fPM");
}

TEST_F(EventDateFormatterUtilTest, GetFormattedInterval_24HourClock) {
  const char* start_time_string = "22 Nov 2021 23:30 GMT";
  const char* end_time_string = "23 Nov 2021 0:30 GMT";
  base::Time start_time, end_time;
  ash::system::ScopedTimezoneSettings timezone_settings(u"PST");
  Shell::Get()->system_tray_model()->SetUse24HourClock(true);

  EXPECT_TRUE(base::Time::FromString(start_time_string, &start_time));
  EXPECT_TRUE(base::Time::FromString(end_time_string, &end_time));

  const auto actual =
      event_date_formatter_util::GetFormattedInterval(start_time, end_time);

  // \x2013 is unicode for dash i.e. '-'
  EXPECT_EQ(actual, u"15:30\u2009\x2013\u200916:30");
}

class EventDateFormatterAllDayEventTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::tuple<const char*, std::u16string>> {
 public:
  const char* GetSelectedDateString() { return std::get<0>(GetParam()); }
  std::u16string GetExpectedResult() { return std::get<1>(GetParam()); }

  // testing::Test:
  void SetUp() override { AshTestBase::SetUp(); }

  void TearDown() override { AshTestBase::TearDown(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    EventDateFormatterAllDayEventTest,
    testing::Values(std::make_tuple("22 Nov 2021 00:00 UTC", u"(Day 1/2)"),
                    std::make_tuple("23 Nov 2021 00:00 UTC", u"(Day 2/2)")));

TEST_P(EventDateFormatterAllDayEventTest, GetMultiDayText_AllDayEvent) {
  const char* start_time_string = "22 Nov 2021 00:00 UTC";
  const char* end_time_string = "24 Nov 2021 00:00 UTC";
  const char* selected_date_string = GetSelectedDateString();
  const auto all_day_event =
      CreateEvent(start_time_string, end_time_string, true);
  base::Time selected_time;
  ash::system::ScopedTimezoneSettings timezone_settings(u"PST");

  EXPECT_TRUE(base::Time::FromUTCString(selected_date_string, &selected_time));

  const auto result = event_date_formatter_util::GetMultiDayText(
      all_day_event.get(), selected_time.UTCMidnight(),
      selected_time.LocalMidnight());

  EXPECT_EQ(result, GetExpectedResult());
}

struct MultiDayEventTestParams {
  const char* start_time_string;
  const char* end_time_string;
  const char* selected_date_string;
  const std::u16string expected_result;
};

class EventDateFormatterMultiDayEventTest
    : public AshTestBase,
      public testing::WithParamInterface<MultiDayEventTestParams> {
 public:
  const char* GetStartTimeString() { return GetParam().start_time_string; }
  const char* GetEndTimeString() { return GetParam().end_time_string; }
  const char* GetSelectedDateString() {
    return GetParam().selected_date_string;
  }
  const std::u16string GetExpectedResult() {
    return GetParam().expected_result;
  }

  // testing::Test:
  void SetUp() override { AshTestBase::SetUp(); }

  void TearDown() override { AshTestBase::TearDown(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    EventDateFormatterMultiDayEventTest,
    testing::Values(
        MultiDayEventTestParams{
            "22 Nov 2021 09:00 GMT", "24 Nov 2021 09:00 GMT",
            "22 Nov 2021 00:00 UTC", u"Starts at 10:00\u202fAM (Day 1/3)"},
        MultiDayEventTestParams{
            "22 Nov 2021 09:00 GMT", "24 Nov 2021 09:00 GMT",
            "23 Nov 2021 00:00 UTC", u"Starts at 10:00\u202fAM (Day 2/3)"},
        MultiDayEventTestParams{
            "22 Nov 2021 09:00 GMT", "24 Nov 2021 09:00 GMT",
            "24 Nov 2021 00:00 UTC", u"Ends at 10:00\u202fAM (Day 3/3)"},
        // Test edge case where a multi-day event falls into a single day in the
        // right timezone.
        MultiDayEventTestParams{
            "22 Nov 2021 23:00 GMT", "23 Nov 2021 23:00 GMT",
            "23 Nov 2021 00:00 UTC", u"Starts at 12:00\u202fAM (Day 1/1)"},
        // Test where a 2 hour event spans multiple days depending on timezone,
        // day 1.
        MultiDayEventTestParams{
            "22 Nov 2021 22:00 GMT", "23 Nov 2021 00:00 GMT",
            "22 Nov 2021 00:00 UTC", u"Starts at 11:00\u202fPM (Day 1/2)"},
        // Test where a 2 hour event spans multiple days depending on timezone,
        // day 2.
        MultiDayEventTestParams{
            "22 Nov 2021 22:00 GMT", "23 Nov 2021 00:00 GMT",
            "23 Nov 2021 00:00 UTC", u"Ends at 1:00\u202fAM (Day 2/2)"}));

TEST_P(EventDateFormatterMultiDayEventTest, GetMultiDayText_MultiDayEvent) {
  const char* start_time_string = GetStartTimeString();
  const char* end_time_string = GetEndTimeString();
  const char* selected_date_string = GetSelectedDateString();
  const auto event = CreateEvent(start_time_string, end_time_string);
  base::Time start_time, end_time, selected_time;
  // This is needed for the calendar util / `DateHelper` functions to use the
  // correct locale.
  ash::system::ScopedTimezoneSettings timezone_settings(u"Europe/Paris");
  // This is needed for `base::Time` to use the correct locale. Without this
  // override, it will ignore the `ash::system::ScopedTimezoneSettings` and use
  // the timezone of the environment it's running on.
  calendar_test_utils::ScopedLibcTimeZone scoped_libc_timezone("Europe/Paris");
  ASSERT_TRUE(scoped_libc_timezone.is_success());

  EXPECT_TRUE(base::Time::FromString(start_time_string, &start_time));
  EXPECT_TRUE(base::Time::FromString(end_time_string, &end_time));
  EXPECT_TRUE(base::Time::FromUTCString(selected_date_string, &selected_time));

  const auto result = event_date_formatter_util::GetMultiDayText(
      event.get(), selected_time.UTCMidnight(), selected_time.LocalMidnight());

  EXPECT_EQ(result, GetExpectedResult());
}

}  // namespace ash
