// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_month_view.h"

#include <memory>

#include "ash/calendar/calendar_client.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_list_model.h"
#include "ash/system/time/calendar_model.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/views_test_utils.h"

namespace ash {

namespace {

using ::google_apis::calendar::CalendarEvent;
using ::google_apis::calendar::EventList;
using ::google_apis::calendar::SingleCalendar;

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

const char* kCalendarId1 = "user1@email.com";
const char* kCalendarSummary1 = "user1@email.com";
const char* kCalendarColorId1 = "12";
bool kCalendarSelected1 = true;
bool kCalendarPrimary1 = true;

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

  void CreateMonthView(base::Time date) {
    AccountId user_account = AccountId::FromUserEmail("user@test");
    GetSessionControllerClient()->SwitchActiveUser(user_account);
    calendar_month_view_.reset();
    controller_.reset();
    controller_ = std::make_unique<CalendarViewController>();
    controller_->UpdateMonth(date);
    calendar_month_view_ = std::make_unique<CalendarMonthView>(
        controller_->GetOnScreenMonthFirstDayUTC(), controller_.get());
    views::test::RunScheduledLayout(calendar_month_view_.get());
  }

  CalendarMonthView* month_view() { return calendar_month_view_.get(); }
  CalendarViewController* controller() { return controller_.get(); }

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

  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");
  CreateMonthView(date);

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

  CreateMonthView(jun_date);

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
  ash::system::ScopedTimezoneSettings timezone_settings(u"Atlantic/Azores");
  CreateMonthView(date);

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

// Tests that the month view should be rendered correctly with any time zone.
// Using March to test since there's a DST change in several time zones in
// March. If any calculation is wrong, there could be duplicated dates and the
// last row will be rendered with one day off.
TEST_F(CalendarMonthViewTest, AllTimeZone) {
  // Create a monthview based on Mar,1st 2022.
  //
  // 27, 28, 1 , 2 , 3 , 4 , 5
  // 6,  7,  8 , 9 , 10, 11, 12
  // 13, 14, 15, 16, 17, 18, 19
  // 20, 21, 22, 23, 24, 25, 26
  // 27, 28, 29, 30, 31, 1 , 2
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("6 Mar 2022 10:00 GMT", &date));

  for (auto* timezone : kAllTimeZones) {
    // Creates a month view based on the current timezone.
    ash::system::ScopedTimezoneSettings timezone_settings(
        base::UTF8ToUTF16(timezone));
    CreateMonthView(date);

    // Checks some dates in the first row and last row of this month view.
    EXPECT_EQ(u"27",
              static_cast<views::LabelButton*>(month_view()->children()[0])
                  ->GetText());
    EXPECT_EQ(u"28",
              static_cast<views::LabelButton*>(month_view()->children()[1])
                  ->GetText());
    EXPECT_EQ(u"1",
              static_cast<views::LabelButton*>(month_view()->children()[2])
                  ->GetText());
    EXPECT_EQ(u"29",
              static_cast<views::LabelButton*>(month_view()->children()[30])
                  ->GetText());
    EXPECT_EQ(u"2",
              static_cast<views::LabelButton*>(month_view()->children()[34])
                  ->GetText());
  }
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

  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");
  CreateMonthView(date);

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

  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");
  CreateMonthView(date);

  // Today's row position is updated because today is in this month.
  int top = controller()->GetTodayRowTopHeight();
  int bottom = controller()->GetTodayRowBottomHeight();
  EXPECT_NE(0, top);
  EXPECT_NE(0, bottom);

  // The date 17th is on the 3rd row.
  EXPECT_EQ(3, bottom / (bottom - top));
}

// Regression test for b/276840405.
// Test the `CalendarMonthView` with time zone Central European Summer Time
// (Oslo) on a DST starting month.
TEST_F(CalendarMonthViewTest, OsloTimeDSTMonth) {
  // Create a monthview based on Apr,1st 2023. DST starts from 02:00 on Mar
  // 26th in 2023 with Central European Summer Time (Oslo).
  //
  // 26, 27, 28, 29, 30, 31, 1
  //  2,  3,  4,  5,  6,  7,  8
  //  9, 10, 11, 12, 13, 14, 15
  // 16, 17, 18, 19, 20, 21, 22
  // 23, 24, 25, 26, 27, 28, 29
  // 30,  1,  2,  3,  4,  5,  6
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Apr 2023 10:00 GMT", &date));

  // Sets the timezone to "Oslo";
  ash::system::ScopedTimezoneSettings timezone_settings(u"Europe/Oslo");
  CreateMonthView(date);

  // Check some dates in this month view.
  EXPECT_EQ(
      u"26",
      static_cast<views::LabelButton*>(month_view()->children()[0])->GetText());
  EXPECT_EQ(u"25",
            static_cast<views::LabelButton*>(month_view()->children()[30])
                ->GetText());
  EXPECT_EQ(u"1", static_cast<views::LabelButton*>(month_view()->children()[36])
                      ->GetText());
}

class CalendarMonthViewFetchTest
    : public AshTestBase,
      public testing::WithParamInterface</*multi_calendar_enabled=*/bool> {
 public:
  CalendarMonthViewFetchTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatureState(
        ash::features::kMultiCalendarSupport, IsMultiCalendarEnabled());
  }
  CalendarMonthViewFetchTest(const CalendarMonthViewFetchTest& other) = delete;
  CalendarMonthViewFetchTest& operator=(
      const CalendarMonthViewFetchTest& other) = delete;
  ~CalendarMonthViewFetchTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Register a mock `CalendarClient` to the `CalendarController`.
    const std::string email = "user1@email.com";
    account_id_ = AccountId::FromUserEmail(email);
    Shell::Get()->calendar_controller()->SetActiveUserAccountIdForTesting(
        account_id_);
    calendar_list_model_ =
        Shell::Get()->system_tray_model()->calendar_list_model();
    calendar_model_ = Shell::Get()->system_tray_model()->calendar_model();
    calendar_client_ =
        std::make_unique<calendar_test_utils::CalendarClientTestImpl>();
    controller_ = std::make_unique<CalendarViewController>();
    Shell::Get()->calendar_controller()->RegisterClientForUser(
        account_id_, calendar_client_.get());
    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        ash::prefs::kCalendarIntegrationEnabled, true);
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);

    if (IsMultiCalendarEnabled()) {
      // Sets a mock calendar list so the calendar list fetch returns
      // successfully.
      SetCalendarList();
    }
  }

  void TearDown() override {
    calendar_list_model_ = nullptr;
    calendar_model_ = nullptr;

    DestroyCalendarMonthViewWidget();
    time_overrides_.reset();
    controller_.reset();
    scoped_feature_list_.Reset();

    AshTestBase::TearDown();
  }

  bool IsMultiCalendarEnabled() { return GetParam(); }

  void CreateMonthView(base::Time date) {
    if (!widget_) {
      widget_ = CreateFramelessTestWidget();
      widget_->SetFullscreen(true);
    }
    GetSessionControllerClient()->SwitchActiveUser(account_id_);
    controller_.reset();
    controller_ = std::make_unique<CalendarViewController>();
    controller_->UpdateMonth(date);
    auto calendar_month_view = std::make_unique<CalendarMonthView>(
        controller_->GetOnScreenMonthFirstDayUTC(), controller_.get());
    calendar_month_view_ =
        widget_->SetContentsView(std::move(calendar_month_view));
    views::test::RunScheduledLayout(calendar_month_view_);
  }

  void DestroyCalendarMonthViewWidget() {
    calendar_month_view_ = nullptr;
    widget_.reset();
  }

  void SetCalendarList() {
    // Sets a mock calendar list.
    std::list<std::unique_ptr<google_apis::calendar::SingleCalendar>> calendars;
    calendars.push_back(calendar_test_utils::CreateCalendar(
        kCalendarId1, kCalendarSummary1, kCalendarColorId1, kCalendarSelected1,
        kCalendarPrimary1));
    calendar_client_->SetCalendarList(
        calendar_test_utils::CreateMockCalendarList(std::move(calendars)));
  }

  int EventsNumberOfDay(const char* day, SingleDayEventList* events) {
    base::Time day_base = calendar_test_utils::GetTimeFromString(day);

    if (events) {
      events->clear();
    }

    return calendar_model_->EventsNumberOfDay(day_base, events);
  }

  int EventsNumberOfDay(base::Time day, SingleDayEventList* events) {
    if (events) {
      events->clear();
    }

    return calendar_model_->EventsNumberOfDay(day, events);
  }

  // Wait until the response is back. Since we used `PostDelayedTask` with 1
  // second to mimic the behavior of fetching, duration of 1 minute should be
  // enough.
  void WaitUntilFetched() {
    task_environment()->FastForwardBy(base::Minutes(1));
    base::RunLoop().RunUntilIdle();
  }

  // Wait until the scheduled paint is done.
  void WaitUntilPainted() {
    task_environment()->FastForwardBy(base::Milliseconds(100));
    base::RunLoop().RunUntilIdle();
  }

  void SetTodayFromTime(base::Time date) {
    std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(
        date, calendar_utils::kNumSurroundingMonthsCached);

    calendar_model_->non_prunable_months_.clear();
    // Non-prunable months are today's date and the two surrounding months.
    calendar_model_->AddNonPrunableMonths(months);
  }

  void SetEventList(std::unique_ptr<google_apis::calendar::EventList> events) {
    calendar_client_->SetEventList(std::move(events));
  }

  bool is_events_indicator_drawn(int date_index) {
    return static_cast<CalendarDateCellView*>(
               calendar_month_view_->children()[date_index])
        ->is_events_indicator_drawn;
  }

  CalendarListModel* calendar_list_model() { return calendar_list_model_; }

  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_overrides_;

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<CalendarListModel> calendar_list_model_ = nullptr;
  raw_ptr<CalendarModel> calendar_model_ = nullptr;
  std::unique_ptr<calendar_test_utils::CalendarClientTestImpl> calendar_client_;
  raw_ptr<CalendarMonthView> calendar_month_view_ = nullptr;
  std::unique_ptr<CalendarViewController> controller_;
  AccountId account_id_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(MultiCalendar,
                         CalendarMonthViewFetchTest,
                         testing::Bool());

TEST_P(CalendarMonthViewFetchTest, FetchedBeforeMonthViewIsCreated) {
  // Create a monthview based on Aug,1st 2021. Today is set to 18th.
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 10:00 GMT", &date));
  base::Time today;
  ASSERT_TRUE(base::Time::FromString("18 Aug 2021 10:00 GMT", &today));
  SetTodayFromTime(today);

  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  // Used to fetch events.
  base::Time month_start_midnight = calendar_utils::GetStartOfMonthUTC(today);

  if (IsMultiCalendarEnabled()) {
    calendar_list_model()->FetchCalendars();
    WaitUntilFetched();
  }

  // Sets the event list response and fetches the events.
  auto event_list = CreateMockEventList();
  SingleDayEventList events;
  EXPECT_EQ(0, EventsNumberOfDay(today, &events));
  EXPECT_TRUE(events.empty());
  SetEventList(std::move(event_list));
  calendar_model_->FetchEvents(month_start_midnight);

  EXPECT_EQ(CalendarModel::kFetching,
            calendar_model_->FindFetchingStatus(month_start_midnight));

  WaitUntilFetched();

  EXPECT_EQ(CalendarModel::kSuccess,
            calendar_model_->FindFetchingStatus(month_start_midnight));

  // Events have been fetched before the month view is constructed.
  EXPECT_EQ(4, EventsNumberOfDay(today, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 4);

  // Creates the month view and now the fetching status should be `kRefetching`.
  SetEventList(CreateMockEventList());
  CreateMonthView(month_start_midnight);

  EXPECT_EQ(CalendarModel::kRefetching,
            calendar_model_->FindFetchingStatus(month_start_midnight));

  EXPECT_EQ(4, EventsNumberOfDay(today, &events));

  WaitUntilPainted();

  EXPECT_FALSE(is_events_indicator_drawn(0));
  EXPECT_EQ(u"18", static_cast<views::LabelButton*>(
                       calendar_month_view_->children()[17])
                       ->GetText());
  EXPECT_TRUE(is_events_indicator_drawn(17));
}

TEST_P(CalendarMonthViewFetchTest, UpdateEvents) {
  // Create a monthview based on Aug,1st 2021. Today is set to 18th.
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 10:00 GMT", &date));
  base::Time today;
  ASSERT_TRUE(base::Time::FromString("18 Aug 2021 10:00 GMT", &today));
  SetTodayFromTime(today);

  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");

  CreateMonthView(date);
  WaitUntilPainted();

  // Grayed out cell. Sep 2nd is the 33 one in this calendar, which is with
  // index 32.
  EXPECT_EQ(u"2", static_cast<CalendarDateCellView*>(
                      calendar_month_view_->children()[32])
                      ->GetText());
  EXPECT_EQ(u"", static_cast<CalendarDateCellView*>(
                     calendar_month_view_->children()[32])
                     ->GetTooltipText());
  // Regular cell. The 18th child is the cell 18 which is with index 17.
  EXPECT_EQ(u"18", static_cast<CalendarDateCellView*>(
                       calendar_month_view_->children()[17])
                       ->GetText());
  EXPECT_EQ(
      u"Wednesday, August 18, 2021, Loading events.",
      static_cast<CalendarDateCellView*>(calendar_month_view_->children()[17])
          ->GetTooltipText());

  // Sets the event list response.
  auto event_list = CreateMockEventList();
  SingleDayEventList events;
  EXPECT_EQ(0, EventsNumberOfDay(today, &events));
  EXPECT_TRUE(events.empty());
  SetEventList(std::move(event_list));

  if (IsMultiCalendarEnabled()) {
    // Start a calendar list fetch, after which we expect an event fetch to be
    // triggered.
    calendar_list_model()->FetchCalendars();
  } else {
    calendar_model_->FetchEvents(calendar_utils::GetStartOfMonthUTC(today));
  }

  // After the fetch is triggered, before the response is back, the event
  // number is not updated.
  EXPECT_EQ(u"2", static_cast<CalendarDateCellView*>(
                      calendar_month_view_->children()[32])
                      ->GetText());
  EXPECT_EQ(u"", static_cast<CalendarDateCellView*>(
                     calendar_month_view_->children()[32])
                     ->GetTooltipText());
  // Regular cell. The 18th child is the cell 18 which is with index 17.
  EXPECT_EQ(u"18", static_cast<CalendarDateCellView*>(
                       calendar_month_view_->children()[17])
                       ->GetText());
  EXPECT_EQ(
      u"Wednesday, August 18, 2021, Loading events.",
      static_cast<CalendarDateCellView*>(calendar_month_view_->children()[17])
          ->GetTooltipText());

  // After the events are fetched, the event numbers are updated for regular
  // cells, not for grayed out cells.
  WaitUntilFetched();

  EXPECT_EQ(u"2", static_cast<CalendarDateCellView*>(
                      calendar_month_view_->children()[32])
                      ->GetText());
  EXPECT_EQ(u"", static_cast<CalendarDateCellView*>(
                     calendar_month_view_->children()[32])
                     ->GetTooltipText());

  EXPECT_EQ(u"18", static_cast<CalendarDateCellView*>(
                       calendar_month_view_->children()[17])
                       ->GetText());
  EXPECT_EQ(
      u"Wednesday, August 18, 2021, 4 events",
      static_cast<CalendarDateCellView*>(calendar_month_view_->children()[17])
          ->GetTooltipText());
}

TEST_P(CalendarMonthViewFetchTest, RecordEventsDisplayedToUserOnce) {
  base::HistogramTester histogram_tester;
  // Create a monthview based on Aug,1st 2021. Today is set to 18th.
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 10:00 GMT", &date));
  base::Time today;
  ASSERT_TRUE(base::Time::FromString("18 Aug 2021 10:00 GMT", &today));
  SetTodayFromTime(today);
  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");

  CreateMonthView(date);
  WaitUntilPainted();

  // Nothing logged before we've fetched events.
  histogram_tester.ExpectTotalCount("Ash.Calendar.EventsDisplayedToUser", 0);

  // Sets the event list response.
  auto event_list = CreateMockEventList();
  SetEventList(std::move(event_list));

  if (IsMultiCalendarEnabled()) {
    // Start a calendar list fetch, after which we expect an event fetch to be
    // triggered.
    calendar_list_model()->FetchCalendars();
  } else {
    calendar_model_->FetchEvents(calendar_utils::GetStartOfMonthUTC(today));
  }
  WaitUntilFetched();

  // After fetching, we expect the metric to be logged.
  histogram_tester.ExpectTotalCount("Ash.Calendar.EventsDisplayedToUser", 1);

  // Fetch new events.
  auto event_list_2 = CreateMockEventList();
  SetEventList(std::move(event_list_2));
  calendar_model_->FetchEvents(calendar_utils::GetStartOfMonthUTC(today));
  WaitUntilFetched();

  // After fetching again, we don't expect any additional logs of the metric.
  histogram_tester.ExpectTotalCount("Ash.Calendar.EventsDisplayedToUser", 1);
}

TEST_P(CalendarMonthViewFetchTest, TimeZone) {
  // Create a monthview based on Aug,1st 2021. Today is set to 18th.
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 10:00 GMT", &date));
  base::Time today;
  ASSERT_TRUE(base::Time::FromString("18 Aug 2021 10:00 GMT", &today));
  SetTodayFromTime(today);

  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");

  CreateMonthView(date);
  WaitUntilPainted();

  // Sets the event list response.
  auto event_list = CreateMockEventList();
  SingleDayEventList events;
  EXPECT_EQ(0, EventsNumberOfDay(today, &events));
  EXPECT_TRUE(events.empty());
  SetEventList(std::move(event_list));

  if (IsMultiCalendarEnabled()) {
    // Start a calendar list fetch, after which we expect an event fetch to be
    // triggered.
    calendar_list_model()->FetchCalendars();
  } else {
    calendar_model_->FetchEvents(calendar_utils::GetStartOfMonthUTC(today));
  }
  WaitUntilFetched();

  EXPECT_EQ(u"18", static_cast<CalendarDateCellView*>(
                       calendar_month_view_->children()[17])
                       ->GetText());
  EXPECT_EQ(
      u"Wednesday, August 18, 2021, 4 events",
      static_cast<CalendarDateCellView*>(calendar_month_view_->children()[17])
          ->GetTooltipText());

  EXPECT_EQ(u"10", static_cast<CalendarDateCellView*>(
                       calendar_month_view_->children()[9])
                       ->GetText());
  EXPECT_EQ(
      u"Tuesday, August 10, 2021, 2 events",
      static_cast<CalendarDateCellView*>(calendar_month_view_->children()[9])
          ->GetTooltipText());

  // Based on the timezone the event that happens on 10th GMT time is showing on
  // the 9th.
  EXPECT_EQ(u"9", static_cast<CalendarDateCellView*>(
                      calendar_month_view_->children()[8])
                      ->GetText());
  EXPECT_EQ(
      u"Monday, August 9, 2021, 1 event",
      static_cast<CalendarDateCellView*>(calendar_month_view_->children()[8])
          ->GetTooltipText());
}

TEST_P(CalendarMonthViewFetchTest, InactiveUserSession) {
  // Create a monthview based on Aug,1st 2021. Today is set to 18th.
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 10:00 GMT", &date));
  base::Time today;
  ASSERT_TRUE(base::Time::FromString("18 Aug 2021 10:00 GMT", &today));
  SetTodayFromTime(today);

  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");

  CreateMonthView(date);
  WaitUntilPainted();

  // Sets the event list response.
  auto event_list = CreateMockEventList();
  SingleDayEventList events;
  EXPECT_EQ(0, EventsNumberOfDay(today, &events));
  EXPECT_TRUE(events.empty());
  SetEventList(std::move(event_list));

  if (IsMultiCalendarEnabled()) {
    // Start a calendar list fetch, after which we expect an event fetch to be
    // triggered.
    calendar_list_model()->FetchCalendars();
  } else {
    calendar_model_->FetchEvents(calendar_utils::GetStartOfMonthUTC(today));
  }
  WaitUntilFetched();

  EXPECT_EQ(u"18", static_cast<CalendarDateCellView*>(
                       calendar_month_view_->children()[17])
                       ->GetText());
  EXPECT_EQ(
      u"Wednesday, August 18, 2021, 4 events",
      static_cast<CalendarDateCellView*>(calendar_month_view_->children()[17])
          ->GetTooltipText());

  // Destroys the month view widget before creating a new month view to avoid a
  // crash.
  DestroyCalendarMonthViewWidget();

  // Changes user session to inactive.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::OOBE);

  // Creates a new month view to simulate the behavior of opening the calendar
  // and check date cell tooltips.
  CreateMonthView(date);
  WaitUntilPainted();

  // Should not show event number.
  EXPECT_EQ(0, EventsNumberOfDay(today, &events));
  EXPECT_TRUE(events.empty());
  EXPECT_EQ(u"18", static_cast<CalendarDateCellView*>(
                       calendar_month_view_->children()[17])
                       ->GetText());
  EXPECT_EQ(
      u"Wednesday, August 18, 2021",
      static_cast<CalendarDateCellView*>(calendar_month_view_->children()[17])
          ->GetTooltipText());

  DestroyCalendarMonthViewWidget();

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  CreateMonthView(date);
  WaitUntilPainted();

  // Should not show event number.
  EXPECT_EQ(0, EventsNumberOfDay(today, &events));
  EXPECT_TRUE(events.empty());
  EXPECT_EQ(u"18", static_cast<CalendarDateCellView*>(
                       calendar_month_view_->children()[17])
                       ->GetText());
  EXPECT_EQ(
      u"Wednesday, August 18, 2021",
      static_cast<CalendarDateCellView*>(calendar_month_view_->children()[17])
          ->GetTooltipText());
}

}  // namespace ash
