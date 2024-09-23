// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_view_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/system/time/calendar_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"

namespace ash {

using CalendarViewControllerUnittest = AshTestBase;

TEST_F(CalendarViewControllerUnittest, UtilFunctions) {
  auto controller = std::make_unique<CalendarViewController>();

  base::Time date;
  ASSERT_TRUE(base::Time::FromString("24 Aug 2021 10:00 GMT", &date));

  controller->UpdateMonth(date);

  base::Time::Exploded first_day_exploded;
  base::Time first_day = controller->GetOnScreenMonthFirstDayUTC();
  first_day.LocalExplode(&first_day_exploded);
  std::u16string month_name = controller->GetOnScreenMonthName();

  EXPECT_EQ(8, first_day_exploded.month);
  EXPECT_EQ(1, first_day_exploded.day_of_month);
  EXPECT_EQ(2021, first_day_exploded.year);
  EXPECT_EQ(u"August", month_name);

  base::Time::Exploded previous_first_day_exploded;
  base::Time previous_first_day = controller->GetPreviousMonthFirstDayUTC(1);
  previous_first_day.LocalExplode(&previous_first_day_exploded);
  std::u16string previous_month_name = controller->GetPreviousMonthName();

  EXPECT_EQ(7, previous_first_day_exploded.month);
  EXPECT_EQ(1, previous_first_day_exploded.day_of_month);
  EXPECT_EQ(2021, previous_first_day_exploded.year);
  EXPECT_EQ(u"July", previous_month_name);

  base::Time::Exploded next_first_day_exploded;
  base::Time next_first_day = controller->GetNextMonthFirstDayUTC(1);
  next_first_day.LocalExplode(&next_first_day_exploded);
  std::u16string next_month_name = controller->GetNextMonthName();

  EXPECT_EQ(9, next_first_day_exploded.month);
  EXPECT_EQ(1, next_first_day_exploded.day_of_month);
  EXPECT_EQ(2021, next_first_day_exploded.year);
  EXPECT_EQ(u"September", next_month_name);
}

TEST_F(CalendarViewControllerUnittest, CornerCases) {
  auto controller = std::make_unique<CalendarViewController>();

  // Next month of Dec should be Jan of next year.
  base::Time last_month_date;
  ASSERT_TRUE(
      base::Time::FromString("24 Dec 2021 10:00 GMT", &last_month_date));

  controller->UpdateMonth(last_month_date);

  base::Time::Exploded january_first_day_exploded;
  base::Time january_first_day = controller->GetNextMonthFirstDayUTC(1);
  january_first_day.LocalExplode(&january_first_day_exploded);
  std::u16string january_month_name = controller->GetNextMonthName();

  EXPECT_EQ(1, january_first_day_exploded.month);
  EXPECT_EQ(1, january_first_day_exploded.day_of_month);
  EXPECT_EQ(2022, january_first_day_exploded.year);
  EXPECT_EQ(u"January", january_month_name);

  // Previous month of Jan should be Dec of last year
  base::Time first_month_date;
  ASSERT_TRUE(
      base::Time::FromString("24 Jan 2021 10:00 GMT", &first_month_date));

  controller->UpdateMonth(first_month_date);

  base::Time::Exploded dec_first_day_exploded;
  base::Time dec_first_day = controller->GetPreviousMonthFirstDayUTC(1);
  dec_first_day.LocalExplode(&dec_first_day_exploded);
  std::u16string dec_month_name = controller->GetPreviousMonthName();

  EXPECT_EQ(12, dec_first_day_exploded.month);
  EXPECT_EQ(1, dec_first_day_exploded.day_of_month);
  EXPECT_EQ(2020, dec_first_day_exploded.year);
  EXPECT_EQ(u"December", dec_month_name);
}

// Tests the date function can return the correct date when DST starts/ends.
TEST_F(CalendarViewControllerUnittest, GetDatesWithDaylightSaving) {
  auto controller = std::make_unique<CalendarViewController>();

  // Set the timezone to GMT.
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  // Set current month to 4/1/2022 00:00 PST, which is 4/1/2022 07:00 GMT.
  base::Time current_month_date;
  ASSERT_TRUE(
      base::Time::FromString("1 Apr 2022 00:00 PST", &current_month_date));

  controller->UpdateMonth(current_month_date);

  base::Time previous_first_day = controller->GetPreviousMonthFirstDayUTC(1);
  std::u16string previous_month_name = controller->GetPreviousMonthName();
  base::Time next_first_day = controller->GetNextMonthFirstDayUTC(1);
  std::u16string next_month_name = controller->GetNextMonthName();

  EXPECT_EQ(u"March 1, 2022",
            calendar_utils::GetMonthDayYear(previous_first_day));
  EXPECT_EQ(u"March", previous_month_name);
  EXPECT_EQ(u"May 1, 2022", calendar_utils::GetMonthDayYear(next_first_day));
  EXPECT_EQ(u"May", next_month_name);

  // Set timezone to Pacific Daylight Time. Mar 13th is the daylight saving
  // starts day.
  timezone_settings.SetTimezoneFromID(u"PST");
  previous_first_day = controller->GetPreviousMonthFirstDayUTC(1);
  previous_month_name = controller->GetPreviousMonthName();
  next_first_day = controller->GetNextMonthFirstDayUTC(1);
  next_month_name = controller->GetNextMonthName();

  EXPECT_EQ(u"March 1, 2022",
            calendar_utils::GetMonthDayYear(previous_first_day));
  EXPECT_EQ(u"March", previous_month_name);
  EXPECT_EQ(u"May 1, 2022", calendar_utils::GetMonthDayYear(next_first_day));
  EXPECT_EQ(u"May", next_month_name);

  // Set current month to 4/1/2022 00:00 GMT, which should be 3/31/2022 17:00
  // PST.
  base::Time current_month_date2;
  ASSERT_TRUE(
      base::Time::FromString("1 Apr 2022 00:00 GMT", &current_month_date2));

  controller->UpdateMonth(current_month_date2);

  previous_first_day = controller->GetPreviousMonthFirstDayUTC(1);
  previous_month_name = controller->GetPreviousMonthName();
  next_first_day = controller->GetNextMonthFirstDayUTC(1);
  next_month_name = controller->GetNextMonthName();

  EXPECT_EQ(u"February 1, 2022",
            calendar_utils::GetMonthDayYear(previous_first_day));
  EXPECT_EQ(u"February", previous_month_name);
  EXPECT_EQ(u"April 1, 2022", calendar_utils::GetMonthDayYear(next_first_day));
  EXPECT_EQ(u"April", next_month_name);

  // Set the timezone back to GMT.
  timezone_settings.SetTimezoneFromID(u"GMT");
  previous_first_day = controller->GetPreviousMonthFirstDayUTC(1);
  previous_month_name = controller->GetPreviousMonthName();
  next_first_day = controller->GetNextMonthFirstDayUTC(1);
  next_month_name = controller->GetNextMonthName();

  EXPECT_EQ(u"March 1, 2022",
            calendar_utils::GetMonthDayYear(previous_first_day));
  EXPECT_EQ(u"March", previous_month_name);
  EXPECT_EQ(u"May 1, 2022", calendar_utils::GetMonthDayYear(next_first_day));
  EXPECT_EQ(u"May", next_month_name);

  // Set current month to 11/1/2022 00:00 PST, which is 11/1/2022 07:00 GMT.
  base::Time current_month_date3;
  ASSERT_TRUE(
      base::Time::FromString("1 Nov 2022 00:00 PST", &current_month_date3));

  controller->UpdateMonth(current_month_date3);

  previous_first_day = controller->GetPreviousMonthFirstDayUTC(1);
  previous_month_name = controller->GetPreviousMonthName();
  next_first_day = controller->GetNextMonthFirstDayUTC(1);
  next_month_name = controller->GetNextMonthName();

  EXPECT_EQ(u"October 1, 2022",
            calendar_utils::GetMonthDayYear(previous_first_day));
  EXPECT_EQ(u"October", previous_month_name);
  EXPECT_EQ(u"December 1, 2022",
            calendar_utils::GetMonthDayYear(next_first_day));
  EXPECT_EQ(u"December", next_month_name);

  // Set timezone to Pacific Daylight Time. Nov 6th is the daylight saving
  // ends day.
  timezone_settings.SetTimezoneFromID(u"PST");
  previous_first_day = controller->GetPreviousMonthFirstDayUTC(1);
  previous_month_name = controller->GetPreviousMonthName();
  next_first_day = controller->GetNextMonthFirstDayUTC(1);
  next_month_name = controller->GetNextMonthName();

  EXPECT_EQ(u"October 1, 2022",
            calendar_utils::GetMonthDayYear(previous_first_day));
  EXPECT_EQ(u"October", previous_month_name);
  EXPECT_EQ(u"December 1, 2022",
            calendar_utils::GetMonthDayYear(next_first_day));
  EXPECT_EQ(u"December", next_month_name);

  // Set current month to 11/1/2022 00:00 GMT, which should be 10/31/2022 17:00
  // PST.
  base::Time current_month_date4;
  ASSERT_TRUE(
      base::Time::FromString("1 Nov 2022 00:00 GMT", &current_month_date4));

  controller->UpdateMonth(current_month_date4);

  previous_first_day = controller->GetPreviousMonthFirstDayUTC(1);
  previous_month_name = controller->GetPreviousMonthName();
  next_first_day = controller->GetNextMonthFirstDayUTC(1);
  next_month_name = controller->GetNextMonthName();

  EXPECT_EQ(u"September 1, 2022",
            calendar_utils::GetMonthDayYear(previous_first_day));
  EXPECT_EQ(u"September", previous_month_name);
  EXPECT_EQ(u"November 1, 2022",
            calendar_utils::GetMonthDayYear(next_first_day));
  EXPECT_EQ(u"November", next_month_name);

  // Set the timezone back to GMT.
  timezone_settings.SetTimezoneFromID(u"GMT");
  previous_first_day = controller->GetPreviousMonthFirstDayUTC(1);
  previous_month_name = controller->GetPreviousMonthName();
  next_first_day = controller->GetNextMonthFirstDayUTC(1);
  next_month_name = controller->GetNextMonthName();

  EXPECT_EQ(u"October 1, 2022",
            calendar_utils::GetMonthDayYear(previous_first_day));
  EXPECT_EQ(u"October", previous_month_name);
  EXPECT_EQ(u"December 1, 2022",
            calendar_utils::GetMonthDayYear(next_first_day));
  EXPECT_EQ(u"December", next_month_name);
}

// Tests that Ash.Calendar.MaxDistanceBrowsed records once on destruction of
// CalendarViewController.
TEST_F(CalendarViewControllerUnittest, MaxDistanceBrowsedRecordedOnClose) {
  base::HistogramTester histogram_tester;
  auto controller = std::make_unique<CalendarViewController>();

  histogram_tester.ExpectTotalCount("Ash.Calendar.MaxDistanceBrowsed", 0);

  // Destroy the controller (this happens when the calendar is closed). The
  // metric should be recorded once.
  controller.reset();

  histogram_tester.ExpectTotalCount("Ash.Calendar.MaxDistanceBrowsed", 1);
}

TEST_F(
    CalendarViewControllerUnittest,
    ShouldRecordEventListViewJoinMeetingButtonPressed_WhenEventListIsShowing) {
  base::HistogramTester histogram_tester;
  auto controller = std::make_unique<CalendarViewController>();

  controller->OnEventListOpened();
  const std::unique_ptr<ui::Event> test_event = std::make_unique<ui::KeyEvent>(
      ui::EventType::kMousePressed, ui::VKEY_UNKNOWN, ui::EF_NONE);
  controller->RecordJoinMeetingButtonPressed(*test_event);

  histogram_tester.ExpectTotalCount(
      "Ash.Calendar.EventListView.JoinMeetingButton.Pressed", 1);
}

TEST_F(
    CalendarViewControllerUnittest,
    ShouldRecordUpNextViewJoinMeetingButtonPressed_WhenEventListIsNotShowing) {
  base::HistogramTester histogram_tester;
  auto controller = std::make_unique<CalendarViewController>();

  const std::unique_ptr<ui::Event> test_event = std::make_unique<ui::KeyEvent>(
      ui::EventType::kMousePressed, ui::VKEY_UNKNOWN, ui::EF_NONE);
  controller->RecordJoinMeetingButtonPressed(*test_event);

  histogram_tester.ExpectTotalCount(
      "Ash.Calendar.UpNextView.JoinMeetingButton.Pressed", 1);
}

}  // namespace ash
