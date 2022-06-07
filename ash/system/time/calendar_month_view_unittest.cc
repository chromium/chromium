// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_month_view.h"

#include <memory>

#include "ash/components/settings/timezone_settings.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

namespace {

std::unique_ptr<google_apis::calendar::EventList> CreateMockEventList() {
  auto event_list = std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("Greenwich Mean Time");
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_0", "summary_0", "18 Aug 2021 8:30 GMT", "18 Nov 2021 9:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_1", "summary_1", "18 Aug 2021 8:15 GMT", "18 Nov 2021 11:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_2", "summary_2", "18 Aug 2021 11:30 GMT", "18 Nov 2021 12:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_3", "summary_3", "18 Aug 2021 8:30 GMT", "19 Nov 2021 10:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_4", "summary_4", "2 Sep 2021 8:30 GMT", "21 Nov 2021 9:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_5", "summary_5", "2 Sep 2021 10:30 GMT", "21 Nov 2021 11:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_6", "summary_6", "10 Aug 2021 4:30 GMT", "10 Aug 2021 5:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_7", "summary_7", "10 Aug 2021 7:30 GMT", "10 Aug 2021 9:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_8", "summary_8", "10 Aug 2021 10:30 GMT", "10 Aug 2021 11:30 GMT"));

  return event_list;
}

}  // namespace

class CalendarMonthViewTest : public AshTestBase {
 public:
  CalendarMonthViewTest() = default;
  CalendarMonthViewTest(const CalendarMonthViewTest&) = delete;
  CalendarMonthViewTest& operator=(const CalendarMonthViewTest&) = delete;
  ~CalendarMonthViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<CalendarViewController>();
  }

  void TearDown() override {
    calendar_month_view_.reset();
    controller_.reset();

    AshTestBase::TearDown();
  }

  void CreateMonthView(base::Time date, const std::u16string& timezone) {
    AccountId user_account = AccountId::FromUserEmail("user@test");
    GetSessionControllerClient()->SwitchActiveUser(user_account);
    calendar_month_view_.reset();
    controller_.reset();
    ash::system::TimezoneSettings::GetInstance()->SetTimezoneFromID(timezone);
    controller_ = std::make_unique<CalendarViewController>();
    controller_->UpdateMonth(date);
    calendar_month_view_ =
        std::make_unique<CalendarMonthView>(date, controller_.get());
    calendar_month_view_->Layout();
  }

  void UploadEvents() {
    calendar_month_view_->calendar_model_->InsertEventsForTesting(
        CreateMockEventList().get());
  }

  void TriggerPaint() {
    gfx::Canvas canvas;
    for (auto* cell : calendar_month_view_->children())
      static_cast<CalendarDateCellView*>(cell)->PaintButtonContents(&canvas);
  }

  CalendarMonthView* month_view() { return calendar_month_view_.get(); }
  CalendarViewController* controller() { return controller_.get(); }

  void InsertPendingFetches(base::Time start_of_month) {
    calendar_month_view_->calendar_model_->InsertPendingFetchesForTesting(
        start_of_month);
  }

  void DeletePendingFetches(base::Time start_of_month) {
    calendar_month_view_->calendar_model_->DeletePendingFetchesForTesting(
        start_of_month);
  }

  void NotifyObservers(base::Time start_of_month) {
    for (auto& observer : calendar_month_view_->calendar_model_->observers_)
      observer.OnEventsFetched(CalendarModel::kSuccess, start_of_month,
                               nullptr);
  }

  CalendarModel::MonthToEventsMap event_months() {
    return calendar_month_view_->calendar_model_->event_months_;
  }

  static base::Time FakeTimeNow() { return fake_time_; }
  static void SetFakeNow(base::Time fake_now) { fake_time_ = fake_now; }

 private:
  std::unique_ptr<CalendarMonthView> calendar_month_view_;
  std::unique_ptr<CalendarViewController> controller_;
  static base::Time fake_time_;
};

base::Time CalendarMonthViewTest::fake_time_;

// Test the basics of the `CalendarMonthView`.
TEST_F(CalendarMonthViewTest, Basics) {
  // Create a monthview based on Aug,1st 2021.
  // 1 , 2 , 3 , 4 , 5 , 6 , 7
  // 8 , 9 , 10, 11, 12, 13, 14
  // 15, 16, 17, 18, 19, 20, 21
  // 22, 23, 24, 25, 26, 27, 28
  // 29, 30, 31, 1 , 2 , 3 , 4
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 10:00 GMT", &date));

  CreateMonthView(date, u"America/Los_Angeles");

  // Randomly checks some dates in this month view.
  EXPECT_EQ(
      u"1",
      static_cast<views::LabelButton*>(month_view()->children()[0])->GetText());
  EXPECT_EQ(u"31",
            static_cast<views::LabelButton*>(month_view()->children()[30])
                ->GetText());
  EXPECT_EQ(u"4", static_cast<views::LabelButton*>(month_view()->children()[34])
                      ->GetText());

  // Create a monthview based on Jun,1st 2021, which has the previous month's
  // dates in the first row.
  // 30, 31, 1 , 2 , 3 , 4 , 5
  // 6 , 7 , 8 , 9 , 10, 11, 12
  // 13, 14, 15, 16, 17, 18, 19
  // 20, 21, 22, 23, 24, 25, 26
  // 27, 28, 29, 30, 1 , 2 , 3
  base::Time jun_date;
  ASSERT_TRUE(base::Time::FromString("1 Jun 2021 10:00 GMT", &jun_date));

  CreateMonthView(jun_date, u"America/Los_Angeles");

  // Randomly checks some dates in this month view.
  EXPECT_EQ(
      u"30",
      static_cast<views::LabelButton*>(month_view()->children()[0])->GetText());
  EXPECT_EQ(u"29",
            static_cast<views::LabelButton*>(month_view()->children()[30])
                ->GetText());
  EXPECT_EQ(u"3", static_cast<views::LabelButton*>(month_view()->children()[34])
                      ->GetText());
}

// Regression test for https://crbug.com/1325533
// Test the `CalendarMonthView` with time zone that has DST starts from 00:00.
TEST_F(CalendarMonthViewTest, AzoreSummerTime) {
  // Create a monthview based on Mar,1st 2022. DST starts from 00:00 on Mar
  // 27th in 2022 with Azore Summer Time.
  //
  // 27, 28, 1 , 2 , 3 , 4 , 5
  // 6,  7,  8 , 9 , 10, 11, 12
  // 13, 14, 15, 16, 17, 18, 19
  // 20, 21, 22, 23, 24, 25, 26
  // 27, 28, 29, 30, 31, 1 , 2
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Mar 2022 10:00 GMT", &date));

  // Sets the timezone to "Azore Summer Time";
  CreateMonthView(date, u"Atlantic/Azores");

  base::Time date_without_DST;
  ASSERT_TRUE(
      base::Time::FromString("26 Mar 2022 10:00 GMT", &date_without_DST));

  base::Time date_with_DST;
  ASSERT_TRUE(base::Time::FromString("28 Mar 2022 10:00 GMT", &date_with_DST));

  // Before daylight saving the time difference is 1 hour.
  EXPECT_EQ(base::Minutes(-60),
            calendar_utils::GetTimeDifference(date_without_DST));

  // After daylight saving the time difference is 0 hours.
  EXPECT_EQ(base::Minutes(0), calendar_utils::GetTimeDifference(date_with_DST));

  // Randomly checks some dates in this month view.
  EXPECT_EQ(
      u"27",
      static_cast<views::LabelButton*>(month_view()->children()[0])->GetText());
  EXPECT_EQ(u"29",
            static_cast<views::LabelButton*>(month_view()->children()[30])
                ->GetText());
  EXPECT_EQ(u"2", static_cast<views::LabelButton*>(month_view()->children()[34])
                      ->GetText());
}

// Tests that todays row position is not updated when today is not in the month.
TEST_F(CalendarMonthViewTest, TodayNotInMonth) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 10:00 GMT", &date));

  // Set "Now" to a date that is not in this month.
  base::Time today;
  ASSERT_TRUE(base::Time::FromString("17 Sep 2021 10:00 GMT", &today));
  SetFakeNow(today);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarMonthViewTest::FakeTimeNow, nullptr, nullptr);

  CreateMonthView(date, u"America/Los_Angeles");

  // Today's row position is not updated.
  EXPECT_EQ(0, controller()->GetTodayRowTopHeight());
  EXPECT_EQ(0, controller()->GetTodayRowBottomHeight());
}

// The position of today should be updated when today is in the month.
TEST_F(CalendarMonthViewTest, TodayInMonth) {
  // Create a monthview based on Aug,1st 2021. Today is set to 17th.
  // 1 , 2 , 3 ,   4 , 5 , 6 , 7
  // 8 , 9 , 10,   11, 12, 13, 14
  // 15, 16, [17], 18, 19, 20, 21
  // 22, 23, 24,   25, 26, 27, 28
  // 29, 30, 31,   1 , 2 , 3 , 4

  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 10:00 GMT", &date));

  // Set "Now" to a date that is in this month.
  base::Time today;
  ASSERT_TRUE(base::Time::FromString("17 Aug 2021 10:00 GMT", &today));
  SetFakeNow(today);
  base::subtle::ScopedTimeClockOverrides in_month_time_override(
      &CalendarMonthViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateMonthView(date, u"America/Los_Angeles");

  // Today's row position is updated because today is in this month.
  int top = controller()->GetTodayRowTopHeight();
  int bottom = controller()->GetTodayRowBottomHeight();
  EXPECT_NE(0, top);
  EXPECT_NE(0, bottom);

  // The date 17th is on the 3rd row.
  EXPECT_EQ(3, bottom / (bottom - top));
}

TEST_F(CalendarMonthViewTest, UpdateEvents) {
  // Create a monthview based on Aug,1st 2021. Today is set to 18th.
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 10:00 GMT", &date));

  // Set "Now" to a date that is in this month.
  base::Time today;
  ASSERT_TRUE(base::Time::FromString("18 Aug 2021 10:00 GMT", &today));
  SetFakeNow(today);
  base::subtle::ScopedTimeClockOverrides in_month_time_override(
      &CalendarMonthViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateMonthView(date, u"America/Los_Angeles");
  // Used to fetch events and notify observers.
  base::Time date_midnight = date.UTCMidnight();

  TriggerPaint();
  // Grayed out cell. Sep 2nd is the 33 one in this calendar, which is with
  // index 32.
  EXPECT_EQ(u"2",
            static_cast<CalendarDateCellView*>(month_view()->children()[32])
                ->GetText());
  EXPECT_EQ(u"",
            static_cast<CalendarDateCellView*>(month_view()->children()[32])
                ->GetTooltipText());
  // Regular cell. The 18th child is the cell 18 which is with index 17.
  EXPECT_EQ(u"18",
            static_cast<CalendarDateCellView*>(month_view()->children()[17])
                ->GetText());
  EXPECT_EQ(u"Wednesday, August 18, 2021, Loading events.",
            static_cast<CalendarDateCellView*>(month_view()->children()[17])
                ->GetTooltipText());

  InsertPendingFetches(date_midnight);
  // Grayed out cell. Sep 2nd is the 33 one in this calendar, which is with
  // index 32.
  EXPECT_EQ(u"2",
            static_cast<CalendarDateCellView*>(month_view()->children()[32])
                ->GetText());
  EXPECT_EQ(u"",
            static_cast<CalendarDateCellView*>(month_view()->children()[32])
                ->GetTooltipText());
  // Regular cell. The 18th child is the cell 18 which is with index 17.
  EXPECT_EQ(u"18",
            static_cast<CalendarDateCellView*>(month_view()->children()[17])
                ->GetText());
  EXPECT_EQ(u"Wednesday, August 18, 2021, Loading events.",
            static_cast<CalendarDateCellView*>(month_view()->children()[17])
                ->GetTooltipText());

  // After events are fetched before the observers are notified the event number
  // is not updated.
  DeletePendingFetches(date_midnight);
  UploadEvents();
  // Grayed out cell. Sep 2nd is the 33 one in this calendar, which is with
  // index 32.
  EXPECT_EQ(u"2",
            static_cast<CalendarDateCellView*>(month_view()->children()[32])
                ->GetText());
  EXPECT_EQ(u"",
            static_cast<CalendarDateCellView*>(month_view()->children()[32])
                ->GetTooltipText());
  // Regular cell. The 18th child is the cell 18 which is with index 17.
  EXPECT_EQ(u"18",
            static_cast<CalendarDateCellView*>(month_view()->children()[17])
                ->GetText());
  EXPECT_EQ(u"Wednesday, August 18, 2021, Loading events.",
            static_cast<CalendarDateCellView*>(month_view()->children()[17])
                ->GetTooltipText());

  // After notifying observers and repainting, the event numbers are updated for
  // regular cells, not for grayed out cells.
  NotifyObservers(date_midnight);
  EXPECT_EQ(u"2",
            static_cast<CalendarDateCellView*>(month_view()->children()[32])
                ->GetText());
  EXPECT_EQ(u"",
            static_cast<CalendarDateCellView*>(month_view()->children()[32])
                ->GetTooltipText());

  EXPECT_EQ(u"18",
            static_cast<CalendarDateCellView*>(month_view()->children()[17])
                ->GetText());
  EXPECT_EQ(u"Wednesday, August 18, 2021, 4 events",
            static_cast<CalendarDateCellView*>(month_view()->children()[17])
                ->GetTooltipText());
}

TEST_F(CalendarMonthViewTest, TimeZone) {
  // Create a monthview based on Aug,1st 2021. Today is set to 18th.
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 10:00 GMT", &date));

  // Set "Now" to a date that is in this month.
  base::Time today;
  ASSERT_TRUE(base::Time::FromString("18 Aug 2021 10:00 GMT", &today));
  SetFakeNow(today);
  base::subtle::ScopedTimeClockOverrides in_month_time_override(
      &CalendarMonthViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  // Used to fetch events and notify observers.
  base::Time date_midnight = date.UTCMidnight();

  // Sets the timezone to "America/Los_Angeles";
  CreateMonthView(date, u"America/Los_Angeles");
  TriggerPaint();
  UploadEvents();
  NotifyObservers(date_midnight);

  EXPECT_EQ(u"18",
            static_cast<CalendarDateCellView*>(month_view()->children()[17])
                ->GetText());
  EXPECT_EQ(u"Wednesday, August 18, 2021, 4 events",
            static_cast<CalendarDateCellView*>(month_view()->children()[17])
                ->GetTooltipText());

  EXPECT_EQ(u"10",
            static_cast<CalendarDateCellView*>(month_view()->children()[9])
                ->GetText());
  EXPECT_EQ(u"Tuesday, August 10, 2021, 2 events",
            static_cast<CalendarDateCellView*>(month_view()->children()[9])
                ->GetTooltipText());

  // Based on the timezone the event that happens on 10th GMT time is showing on
  // the 9th.
  EXPECT_EQ(u"9",
            static_cast<CalendarDateCellView*>(month_view()->children()[8])
                ->GetText());
  EXPECT_EQ(u"Monday, August 9, 2021, 1 event",
            static_cast<CalendarDateCellView*>(month_view()->children()[8])
                ->GetTooltipText());

  // Set the timezone back to GMT.
  ash::system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"GMT");
}

TEST_F(CalendarMonthViewTest, InactiveUserSession) {
  // Create a monthview based on Aug,1st 2021. Today is set to 18th.
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 10:00 GMT", &date));

  // Set "Now" to a date that is in this month.
  base::Time today;
  ASSERT_TRUE(base::Time::FromString("18 Aug 2021 10:00 GMT", &today));
  SetFakeNow(today);
  base::subtle::ScopedTimeClockOverrides in_month_time_override(
      &CalendarMonthViewTest::FakeTimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  CreateMonthView(date, u"America/Los_Angeles");
  // Used to fetch events and notify observers.
  base::Time date_midnight = date.UTCMidnight();

  TriggerPaint();
  UploadEvents();
  NotifyObservers(date_midnight);
  EXPECT_EQ(u"18",
            static_cast<CalendarDateCellView*>(month_view()->children()[17])
                ->GetText());
  EXPECT_EQ(u"Wednesday, August 18, 2021, 4 events",
            static_cast<CalendarDateCellView*>(month_view()->children()[17])
                ->GetTooltipText());

  // Changes user session to inactive. Should not show event number.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::OOBE);
  month_view()->UpdateIsFetchedAndRepaint(false);
  TriggerPaint();
  EXPECT_EQ(u"18",
            static_cast<CalendarDateCellView*>(month_view()->children()[17])
                ->GetText());
  EXPECT_EQ(u"Wednesday, August 18, 2021",
            static_cast<CalendarDateCellView*>(month_view()->children()[17])
                ->GetTooltipText());

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  month_view()->UpdateIsFetchedAndRepaint(false);
  TriggerPaint();
  EXPECT_EQ(u"18",
            static_cast<CalendarDateCellView*>(month_view()->children()[17])
                ->GetText());
  EXPECT_EQ(u"Wednesday, August 18, 2021",
            static_cast<CalendarDateCellView*>(month_view()->children()[17])
                ->GetTooltipText());
}

}  // namespace ash
