// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_model.h"

#include <cstddef>
#include <iterator>
#include <list>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "ash/calendar/calendar_client.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_list_model.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"
#include "components/user_manager/user_type.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/common/api_error_codes.h"

namespace ash {

namespace {

using ::google_apis::calendar::CalendarEvent;
using ::google_apis::calendar::EventList;
using ::google_apis::calendar::SingleCalendar;

const char* kStartTime0 = "23 Oct 2009 11:30 GMT";
const char* kEndTime0 = "23 Oct 2009 12:30 GMT";
const char* kId0 = "id_0";
const char* kSummary0 = "summary_0";
const char* kStartTime1 = "19 Oct 2009 07:30 GMT";
const char* kEndTime1 = "19 Oct 2009 08:30 GMT";
const char* kId1 = "id_1";
const char* kSummary1 = "summary_1";
const char* kStartTime2 = "15 Oct 2009 11:30 GMT";
const char* kEndTime2 = "15 Oct 2009 12:30 GMT";
const char* kId2 = "id_2";
const char* kSummary2 = "summary_2";
const char* kStartTime3 = "14 Oct 2009 11:30 GMT";
const char* kEndTime3 = "14 Oct 2009 12:30 GMT";
const char* kId3 = "id_3";
const char* kSummary3 = "summary_3";
const char* kStartTime4 = "23 Feb 2010 11:30 GMT";
const char* kEndTime4 = "23 Feb 2010 12:30 GMT";
const char* kId4 = "id_4";
const char* kSummary4 = "summary_4";
const char* kStartTime5 = "23 Mar 2010 11:30 GMT";
const char* kEndTime5 = "23 Mar 2010 12:30 GMT";
const char* kId5 = "id_5";
const char* kSummary5 = "summary_5";
const char* kStartTime12 = "24 Oct 2009 07:10 GMT";
const char* kEndTime12 = "24 Oct 2009 08:00 GMT";
const char* kId12 = "id_12";
const char* kSummary12 = "summary_12";
const char* kStartTime13 = "24 Oct 2009 07:30 GMT";
const char* kEndTime13 = "25 Oct 2009 08:30 GMT";
const char* kId13 = "id_13";
const char* kSummary13 = "summary_13";
const char* kBaseStartTime = "01 Oct 2009 00:00 GMT";

const char* kCalendarId1 = "user1@email.com";
const char* kCalendarSummary1 = "user1@email.com";
const char* kCalendarColorId1 = "12";
bool kCalendarSelected1 = true;
bool kCalendarPrimary1 = true;

}  // namespace

class CalendarModelUtilsTest : public AshTestBase {
 public:
  CalendarModelUtilsTest() = default;
  CalendarModelUtilsTest(const CalendarModelUtilsTest& other) = delete;
  CalendarModelUtilsTest& operator=(const CalendarModelUtilsTest& other) =
      delete;
  ~CalendarModelUtilsTest() override = default;

  static void SetFakeNow(base::Time fake_now) { fake_time_ = fake_now; }
  static base::Time FakeTimeNow() { return fake_time_; }

  static base::Time fake_time_;
};

base::Time CalendarModelUtilsTest::fake_time_;

TEST_F(CalendarModelUtilsTest, SurroundingMonths) {
  std::set<base::Time> months;

  // Set current date.
  base::Time current_date, start_of_month;
  current_date =
      calendar_test_utils::GetTimeFromString("23 Oct 2009 11:30 GMT");
  start_of_month =
      calendar_test_utils::GetTimeFromString("01 Oct 2009 00:00 GMT");
  CalendarModelUtilsTest::SetFakeNow(current_date);
  base::subtle::ScopedTimeClockOverrides time_override(
      &CalendarModelUtilsTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  // 0 months out.
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 0);
  EXPECT_EQ(1UL, months.size());
  EXPECT_TRUE(base::Contains(months, start_of_month));

  // 1 month out.
  base::Time start_of_previous_month =
      calendar_test_utils::GetTimeFromString("01 Sep 2009 00:00 GMT");
  base::Time start_of_next_month =
      calendar_test_utils::GetTimeFromString("01 Nov 2009 00:00 GMT");
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 1);
  EXPECT_EQ(3UL, months.size());
  EXPECT_TRUE(base::Contains(months, start_of_month));
  EXPECT_TRUE(base::Contains(months, start_of_previous_month));
  EXPECT_TRUE(base::Contains(months, start_of_next_month));

  // 2 months out.
  base::Time start_of_previous_month_2 =
      calendar_test_utils::GetTimeFromString("01 Aug 2009 00:00 GMT");
  base::Time start_of_next_month_2 =
      calendar_test_utils::GetTimeFromString("01 Dec 2009 00:00 GMT");
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 2);
  EXPECT_EQ(5UL, months.size());
  EXPECT_TRUE(base::Contains(months, start_of_month));
  EXPECT_TRUE(base::Contains(months, start_of_previous_month));
  EXPECT_TRUE(base::Contains(months, start_of_next_month));
  EXPECT_TRUE(base::Contains(months, start_of_previous_month_2));
  EXPECT_TRUE(base::Contains(months, start_of_next_month_2));

  // 3 months out, which takes us into the next year.
  base::Time start_of_previous_month_3 =
      calendar_test_utils::GetTimeFromString("01 Jul 2009 00:00 GMT");
  base::Time start_of_next_month_3 =
      calendar_test_utils::GetTimeFromString("01 Jan 2010 00:00 GMT");
  months = calendar_utils::GetSurroundingMonthsUTC(current_date, 3);
  EXPECT_EQ(7UL, months.size());
  EXPECT_TRUE(base::Contains(months, start_of_month));
  EXPECT_TRUE(base::Contains(months, start_of_previous_month));
  EXPECT_TRUE(base::Contains(months, start_of_next_month));
  EXPECT_TRUE(base::Contains(months, start_of_previous_month_2));
  EXPECT_TRUE(base::Contains(months, start_of_next_month_2));
  EXPECT_TRUE(base::Contains(months, start_of_previous_month_3));
  EXPECT_TRUE(base::Contains(months, start_of_next_month_3));
}

class CalendarModelTest
    : public AshTestBase,
      public testing::WithParamInterface</*multi_calendar_enabled=*/bool> {
 public:
  CalendarModelTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatureState(
        ash::features::kMultiCalendarSupport, IsMultiCalendarEnabled());
  }

  CalendarModelTest(const CalendarModelTest& other) = delete;
  CalendarModelTest& operator=(const CalendarModelTest& other) = delete;
  ~CalendarModelTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Register a mock `CalendarClient` to the `CalendarController`.
    const std::string email = "user1@email.com";
    AccountId account_id = AccountId::FromUserEmail(email);
    Shell::Get()->calendar_controller()->SetActiveUserAccountIdForTesting(
        account_id);
    calendar_model_ = std::make_unique<CalendarModel>();
    calendar_client_ =
        std::make_unique<calendar_test_utils::CalendarClientTestImpl>();
    Shell::Get()->calendar_controller()->RegisterClientForUser(
        account_id, calendar_client_.get());
    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        ash::prefs::kCalendarIntegrationEnabled, true);

    if (IsMultiCalendarEnabled()) {
      FetchCalendars();
    }
  }

  void TearDown() override {
    time_overrides_.reset();
    calendar_model_.reset();
    scoped_feature_list_.Reset();

    AshTestBase::TearDown();
  }

  bool IsMultiCalendarEnabled() { return GetParam(); }

  void FetchCalendars() {
    // Set a mock calendar list.
    std::list<std::unique_ptr<google_apis::calendar::SingleCalendar>> calendars;
    calendars.push_back(calendar_test_utils::CreateCalendar(
        kCalendarId1, kCalendarSummary1, kCalendarColorId1, kCalendarSelected1,
        kCalendarPrimary1));
    calendar_client_->SetCalendarList(
        calendar_test_utils::CreateMockCalendarList(std::move(calendars)));

    // Start the calendar list fetch and fast forward until it is complete.
    calendar_list_model()->FetchCalendars();
    WaitUntilFetched();
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

  int EventsNumberOfDayInternal(const char* day,
                                SingleDayEventList* events) const {
    base::Time day_base = calendar_test_utils::GetTimeFromString(day);

    if (events) {
      events->clear();
    }

    return calendar_model_->EventsNumberOfDay(day_base, events);
  }

  base::Time GetStartTimeMidnightAdjusted(
      const google_apis::calendar::CalendarEvent* event) {
    return calendar_utils::GetStartTimeMidnightAdjusted(event);
  }

  bool EventsPresentAtIndex(std::vector<base::Time>& months, int index) {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, static_cast<int>(months.size()));
    return base::Contains(calendar_model_->event_months_, months[index]);
  }

  bool EventsPresentInRange(std::vector<base::Time>& months,
                            int start_index,
                            int end_index) {
    DCHECK_GE(start_index, 0);
    DCHECK_GE(end_index, start_index);

    for (int i = start_index; i < end_index; ++i) {
      if (!EventsPresentAtIndex(months, i)) {
        return false;
      }
    }

    return true;
  }

  bool NoEventsPresentInRange(std::vector<base::Time>& months,
                              int start_index,
                              int end_index) {
    DCHECK_GE(start_index, 0);
    DCHECK_GE(end_index, start_index);

    for (int i = start_index; i < end_index; ++i) {
      const base::Time& start_of_month =
          calendar_utils::GetFirstDayOfMonth(months[i]).UTCMidnight();
      if (base::Contains(non_prunable_months(), start_of_month)) {
        continue;
      }

      if (EventsPresentAtIndex(months, i)) {
        return false;
      }
    }

    return true;
  }

  void UpdateSession(uint32_t session_id,
                     const std::string& email,
                     bool is_child = false) {
    UserSession session;
    session.session_id = session_id;
    session.user_info.type = is_child ? user_manager::UserType::kChild
                                      : user_manager::UserType::kRegular;
    session.user_info.account_id = AccountId::FromUserEmail(email);
    session.user_info.display_name = email;
    session.user_info.display_email = email;
    session.user_info.is_new_profile = false;

    SessionController::Get()->UpdateUserSession(session);
  }

  void TestMultiDayEvent(SingleDayEventList events,
                         const char* start_date_str,
                         const char* end_date_str) {
    base::Time start_time_midnight =
        calendar_test_utils::GetTimeFromString(start_date_str).UTCMidnight();
    base::Time end_time_midnight =
        calendar_test_utils::GetTimeFromString(end_date_str).UTCMidnight();

    // Each day inside the event's time range should have an event.
    while (start_time_midnight <= end_time_midnight) {
      EXPECT_EQ(1, EventsNumberOfDay(start_time_midnight, &events));
      // Add more than 24 hours to consider daylight savings.
      start_time_midnight =
          (start_time_midnight + base::Hours(30)).UTCMidnight();
    }
  }

  // Wait until the response is back. Since we used `PostDelayedTask` with 1
  // second to mimic the behavior of fetching, duration of 1 minute should be
  // enough.
  void WaitUntilFetched() {
    task_environment()->FastForwardBy(base::Minutes(1));
    base::RunLoop().RunUntilIdle();
  }

  // Set today's date to add non-prunable months in the model.
  void SetTodayFromStr(const char* date_str) {
    bool result = base::Time::FromString(date_str, &now_);
    DCHECK(result);
    SetTodayFromTime(now_);
  }

  void SetTodayFromTime(base::Time date) {
    now_ = date;
    std::set<base::Time> months = calendar_utils::GetSurroundingMonthsUTC(
        now_, calendar_utils::kNumSurroundingMonthsCached);

    calendar_model_->non_prunable_months_.clear();
    // Non-prunable months are today's date and the two surrounding months.
    calendar_model()->AddNonPrunableMonths(months);
  }

  void SetEventList(std::unique_ptr<google_apis::calendar::EventList> events) {
    calendar_client_->SetEventList(std::move(events));
  }

  void SetError(google_apis::ApiErrorCode error) {
    calendar_client_->SetError(error);
  }

  void MockOnEventsFetched(base::Time start_of_month,
                           google_apis::ApiErrorCode error,
                           const google_apis::calendar::EventList* events) {
    calendar_model_->OnEventsFetched(start_of_month,
                                     google_apis::calendar::kPrimaryCalendarId,
                                     error, events);
  }

  base::Time now() { return now_; }

  std::set<base::Time> non_prunable_months() {
    return calendar_model_->non_prunable_months_;
  }

  CalendarModel::MonthToEventsMap& event_months() {
    return calendar_model_->event_months_;
  }

  CalendarListModel* calendar_list_model() {
    return Shell::Get()->system_tray_model()->calendar_list_model();
  }

  CalendarModel* calendar_model() { return calendar_model_.get(); }

  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_overrides_;

  std::unique_ptr<CalendarModel> calendar_model_;
  std::unique_ptr<calendar_test_utils::CalendarClientTestImpl> calendar_client_;
  base::Time now_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(MultiCalendar, CalendarModelTest, testing::Bool());

TEST_P(CalendarModelTest, FetchingSuccessfullyWithOneEvent) {
  // All events will be distributed by the system timezone. If no timezone is
  // set, the test will run with the local default timezone which might cause a
  // test failure. So here sets the timezone to "GMT", and the same for all the
  // tests in this file.
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  if (IsMultiCalendarEnabled()) {
    EXPECT_TRUE(calendar_list_model()->get_is_cached());
    EXPECT_EQ(1u, calendar_list_model()->GetCachedCalendarList().size());
  }

  // Set current date to `kStartTime0`.
  SetTodayFromStr(kStartTime0);
  base::Time start_of_month = calendar_utils::GetStartOfMonthUTC(
      calendar_test_utils::GetTimeFromString(kStartTime0));

  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);

  // Set up list of events as the mock response.
  auto event_list = std::make_unique<google_apis::calendar::EventList>();
  event_list->InjectItemForTesting(std::move(event));

  // Haven't fetched anything yet, so no events on `kStartTime0`.
  SingleDayEventList events;
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());
  EXPECT_EQ(CalendarModel::kNever,
            calendar_model()->FindFetchingStatus(start_of_month));

  // Set this event list as the response;
  SetEventList(std::move(event_list));

  // Now fetch the events, the status changes to `kFetching` before the events
  // are fetched.
  calendar_model()->FetchEvents(calendar_utils::GetStartOfMonthUTC(now()));
  EXPECT_EQ(CalendarModel::kFetching,
            calendar_model()->FindFetchingStatus(start_of_month));

  WaitUntilFetched();

  // Now we have an event on kStartTime0.
  EXPECT_EQ(CalendarModel::kSuccess,
            calendar_model()->FindFetchingStatus(start_of_month));
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);

  // Set an empty event list as the mock response.
  auto event_list2 = std::make_unique<google_apis::calendar::EventList>();

  // Set the event list as the response;
  SetEventList(std::move(event_list2));

  // Now we do a refetch.
  calendar_model()->FetchEvents(calendar_utils::GetStartOfMonthUTC(now()));

  EXPECT_EQ(CalendarModel::kRefetching,
            calendar_model()->FindFetchingStatus(start_of_month));
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));

  WaitUntilFetched();

  EXPECT_EQ(CalendarModel::kSuccess,
            calendar_model()->FindFetchingStatus(start_of_month));
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));

  std::unique_ptr<google_apis::calendar::CalendarEvent> event2 =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);

  // Set up list of events as the mock response.
  auto event_list3 = std::make_unique<google_apis::calendar::EventList>();
  event_list3->InjectItemForTesting(std::move(event2));

  // Set the event list as the response;
  SetEventList(std::move(event_list3));

  // Now we do a refetch.
  calendar_model()->FetchEvents(calendar_utils::GetStartOfMonthUTC(now()));

  EXPECT_EQ(CalendarModel::kRefetching,
            calendar_model()->FindFetchingStatus(start_of_month));
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));

  WaitUntilFetched();

  EXPECT_EQ(CalendarModel::kSuccess,
            calendar_model()->FindFetchingStatus(start_of_month));
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));
}

TEST_P(CalendarModelTest, FetchingSuccessfullyWithMultiEvents) {
  // Sets the timezone to "GMT".
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  if (IsMultiCalendarEnabled()) {
    EXPECT_TRUE(calendar_list_model()->get_is_cached());
    EXPECT_EQ(1u, calendar_list_model()->GetCachedCalendarList().size());
  }

  // Set current date to `kStartTime0`.
  SetTodayFromStr(kStartTime0);

  // Set up list of events from the same month as the mock response.
  std::unique_ptr<google_apis::calendar::CalendarEvent> event0 =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event1 =
      calendar_test_utils::CreateEvent(kId1, kSummary1, kStartTime1, kEndTime1);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event2 =
      calendar_test_utils::CreateEvent(kId2, kSummary2, kStartTime2, kEndTime2);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event3 =
      calendar_test_utils::CreateEvent(kId3, kSummary3, kStartTime3, kEndTime3);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event13 =
      calendar_test_utils::CreateEvent(kId13, kSummary13, kStartTime13,
                                       kEndTime13);
  auto event_list = std::make_unique<google_apis::calendar::EventList>();
  event_list->InjectItemForTesting(std::move(event0));
  event_list->InjectItemForTesting(std::move(event1));
  event_list->InjectItemForTesting(std::move(event2));
  event_list->InjectItemForTesting(std::move(event3));
  event_list->InjectItemForTesting(std::move(event13));
  base::Time start_of_month0 = calendar_utils::GetStartOfMonthUTC(
      calendar_test_utils::GetTimeFromString(kStartTime0));
  base::Time start_of_month1 = calendar_utils::GetStartOfMonthUTC(
      calendar_test_utils::GetTimeFromString(kStartTime1));
  base::Time start_of_month2 = calendar_utils::GetStartOfMonthUTC(
      calendar_test_utils::GetTimeFromString(kStartTime2));
  base::Time start_of_month3 = calendar_utils::GetStartOfMonthUTC(
      calendar_test_utils::GetTimeFromString(kStartTime3));
  base::Time start_of_month13 = calendar_utils::GetStartOfMonthUTC(
      calendar_test_utils::GetTimeFromString(kStartTime13));

  // Haven't fetch anything yet, so no events are fetched.
  SingleDayEventList events;
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());
  EXPECT_EQ(CalendarModel::kNever,
            calendar_model()->FindFetchingStatus(start_of_month0));
  EXPECT_EQ(CalendarModel::kNever,
            calendar_model()->FindFetchingStatus(start_of_month1));
  EXPECT_EQ(CalendarModel::kNever,
            calendar_model()->FindFetchingStatus(start_of_month2));
  EXPECT_EQ(CalendarModel::kNever,
            calendar_model()->FindFetchingStatus(start_of_month3));
  EXPECT_EQ(CalendarModel::kNever,
            calendar_model()->FindFetchingStatus(start_of_month13));

  // Set this event list as the response;
  SetEventList(std::move(event_list));

  // Now fetch the events, the status changes to `kFetching` before the events
  // are fetched. We put all events in one mock response list with one fetch. So
  // here we only call `FetchEvents` one time.
  calendar_model()->FetchEvents(calendar_utils::GetStartOfMonthUTC(now()));
  EXPECT_EQ(CalendarModel::kFetching,
            calendar_model()->FindFetchingStatus(start_of_month0));

  WaitUntilFetched();

  // Now we have an event on kStartTime0.
  EXPECT_EQ(CalendarModel::kSuccess,
            calendar_model()->FindFetchingStatus(start_of_month0));
  EXPECT_EQ(CalendarModel::kSuccess,
            calendar_model()->FindFetchingStatus(start_of_month1));
  EXPECT_EQ(CalendarModel::kSuccess,
            calendar_model()->FindFetchingStatus(start_of_month2));
  EXPECT_EQ(CalendarModel::kSuccess,
            calendar_model()->FindFetchingStatus(start_of_month3));
  EXPECT_EQ(CalendarModel::kSuccess,
            calendar_model()->FindFetchingStatus(start_of_month13));
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime1, &events));
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime2, &events));
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime3, &events));
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime13, &events));

  // Set up an empty event list.
  auto event_list2 = std::make_unique<google_apis::calendar::EventList>();

  // Set this event list as the response;
  SetEventList(std::move(event_list2));

  // Now we do a refetch.
  calendar_model()->FetchEvents(calendar_utils::GetStartOfMonthUTC(now()));

  EXPECT_EQ(CalendarModel::kRefetching,
            calendar_model()->FindFetchingStatus(start_of_month0));
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));

  WaitUntilFetched();

  EXPECT_EQ(CalendarModel::kSuccess,
            calendar_model()->FindFetchingStatus(start_of_month0));
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
}

TEST_P(CalendarModelTest, ChangeTimeDifference) {
  // Sets the timezone to "America/Los_Angeles".
  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");
  calendar_test_utils::ScopedLibcTimeZone scoped_libc_timezone(
      "America/Los_Angeles");
  ASSERT_TRUE(scoped_libc_timezone.is_success());

  if (IsMultiCalendarEnabled()) {
    EXPECT_TRUE(calendar_list_model()->get_is_cached());
    EXPECT_EQ(1u, calendar_list_model()->GetCachedCalendarList().size());
  }

  // Set today to`kStartTime0`.
  SetTodayFromStr(kStartTime0);

  // Creates 3 events:
  // (1) kStartTime0 = "23 Oct 2009 11:30 GMT";
  //     kEndTime0 = "23 Oct 2009 12:30 GMT";
  //
  // (2) kStartTime12 = "24 Oct 2009 07:10 GMT";
  //     kEndTime12 = "24 Oct 2009 08:00 GMT";
  //
  // (3) kStartTime13 = "24 Oct 2009 07:30 GMT";
  //     kEndTime13 = "25 Oct 2009 08:30 GMT";
  std::unique_ptr<google_apis::calendar::CalendarEvent> event0 =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event12 =
      calendar_test_utils::CreateEvent(kId12, kSummary12, kStartTime12,
                                       kEndTime12);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event13 =
      calendar_test_utils::CreateEvent(kId13, kSummary13, kStartTime13,
                                       kEndTime13);

  // Prepare mock events.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->InjectItemForTesting(std::move(event0));
  event_list->InjectItemForTesting(std::move(event12));
  event_list->InjectItemForTesting(std::move(event13));

  // Set this event list as the mock response;
  SetEventList(std::move(event_list));
  calendar_model()->FetchEvents(calendar_utils::GetStartOfMonthUTC(now()));
  WaitUntilFetched();

  // Based on the tesing timezone "America/Los_Angeles" (-7hrs) these 3 events
  // are distributed into 3 days, since one of them is a multi day event. See
  // details as below:
  //
  // (1) kStartTime0 = "23 Oct 2009 11:30 GMT";
  //     kEndTime0 = "23 Oct 2009 12:30 GMT";
  //  => event1  23 Oct 2009 4:30 ~5:30
  //
  // (2) kStartTime12 = "24 Oct 2009 07:10 GMT";
  //     kEndTime12 = "24 Oct 2009 08:00 GMT";
  //  => event2   24 Oct 2009 00:10 ~1:00
  //
  // (3) kStartTime13 = "24 Oct 2009 07:30 GMT";
  //     kEndTime13 = "25 Oct 2009 08:30 GMT";
  //  => event3   24 Oct 2009 00:30 ~23:59
  //     event4   25 Oct 2009 00:00 ~1:30
  SingleDayEventList events;
  EXPECT_EQ(1, EventsNumberOfDay("23 Oct 2009 00:00", &events));
  EXPECT_EQ(2, EventsNumberOfDay("24 Oct 2009 00:00", &events));
  EXPECT_EQ(1, EventsNumberOfDay("25 Oct 2009 00:00", &events));
  EXPECT_EQ(0, EventsNumberOfDay("26 Oct 2009 00:00", &events));

  // Sets the timezone to "Pacific/Honolulu" which has -10 hours time
  // difference.
  timezone_settings.SetTimezoneFromID(u"Pacific/Honolulu");

  // (1) kStartTime0 = "23 Oct 2009 11:30 GMT";
  //     kEndTime0 = "23 Oct 2009 12:30 GMT";
  //  => event1  23 Oct 2009 1:30 ~2:30
  //
  // (2) kStartTime12 = "24 Oct 2009 07:10 GMT";
  //     kEndTime12 = "24 Oct 2009 08:00 GMT";
  //  => event2   23 Oct 2009 21:10 ~22:00
  //
  // (3) kStartTime13 = "24 Oct 2009 07:30 GMT";
  //     kEndTime13 = "25 Oct 2009 08:30 GMT";
  //  => event3   23 Oct 2009 21:30 ~23:59
  //     event4   24 Oct 2009 00:00 ~22:30
  calendar_model()->RedistributeEvents();
  EXPECT_EQ(3, EventsNumberOfDay("23 Oct 2009 00:00", &events));
  EXPECT_EQ(1, EventsNumberOfDay("24 Oct 2009 00:00", &events));
  EXPECT_EQ(0, EventsNumberOfDay("25 Oct 2009 00:00", &events));
  EXPECT_EQ(0, EventsNumberOfDay("26 Oct 2009 00:00", &events));

  // Sets the timezone to "Pacific/Kiritimatis" which has +14 hours time
  // difference;
  timezone_settings.SetTimezoneFromID(u"Pacific/Kiritimati");

  // (1) kStartTime0 = "23 Oct 2009 11:30 GMT";
  //     kEndTime0 = "23 Oct 2009 12:30 GMT";
  //  => event1  24 Oct 2009 1:30 ~2:30
  //
  // (2) kStartTime12 = "24 Oct 2009 07:10 GMT";
  //     kEndTime12 = "24 Oct 2009 08:00 GMT";
  //  => event2   24 Oct 2009 21:10 ~22:00
  //
  // (3) kStartTime13 = "24 Oct 2009 07:30 GMT";
  //     kEndTime13 = "25 Oct 2009 08:30 GMT";
  //  => event4   24 Oct 2009 21:30 ~23:59
  //     event5   25 Oct 2009 00:00 ~22:30
  calendar_model()->RedistributeEvents();
  EXPECT_EQ(0, EventsNumberOfDay("23 Oct 2009 00:00", &events));
  EXPECT_EQ(3, EventsNumberOfDay("24 Oct 2009 00:00", &events));
  EXPECT_EQ(1, EventsNumberOfDay("25 Oct 2009 00:00", &events));
  EXPECT_EQ(0, EventsNumberOfDay("26 Oct 2009 00:00", &events));
}

// Test for pruning of events.
TEST_P(CalendarModelTest, PruneEvents) {
  // Sets the timezone to "GMT".
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  if (IsMultiCalendarEnabled()) {
    EXPECT_TRUE(calendar_list_model()->get_is_cached());
    EXPECT_EQ(1u, calendar_list_model()->GetCachedCalendarList().size());
  }

  // The number of event is exactly the max cached capacity. No events should be
  // removed when init the mock response.
  constexpr int kNumEvents = calendar_utils::kMaxNumPrunableMonths +
                             calendar_utils::kMaxNumNonPrunableMonths;  // 25

  // Loop from base start time to mock the initial cached months.
  base::Time current_month =
      calendar_test_utils::GetTimeFromString(kBaseStartTime);

  // Set the next next month as today. The default surrounding months number is
  // 2. This sets the first 5 months as the non_prunable months.
  // NOTE: We must set today before injecting any events because if we set it
  // at the i == 2 loop, the first two months will be added to mru_months_ and
  // will be prunable.
  base::Time next_month = calendar_utils::GetStartOfNextMonthUTC(current_month);
  SetTodayFromTime(calendar_utils::GetStartOfNextMonthUTC(next_month));

  for (int i = 0; i < kNumEvents; ++i) {
    // Inject events.
    MockOnEventsFetched(current_month, google_apis::ApiErrorCode::HTTP_SUCCESS,
                        nullptr);

    current_month = calendar_utils::GetStartOfNextMonthUTC(current_month);
  }

  std::vector<base::Time> init_prunable_months;
  for (auto& month : event_months()) {
    if (!base::Contains(non_prunable_months(), month.first)) {
      init_prunable_months.push_back(month.first);
    }
  }

  EXPECT_EQ((int)init_prunable_months.size(),
            calendar_utils::kMaxNumPrunableMonths);

  // Loop from start time reversely to mock scroll up form the current month to
  // the previous months.
  base::Time on_screen_month =
      calendar_test_utils::GetTimeFromString(kBaseStartTime);
  for (int i = 0; i < calendar_utils::kMaxNumPrunableMonths; ++i) {
    auto months = calendar_utils::GetSurroundingMonthsUTC(on_screen_month, 1);

    // Fetch events.
    for (auto& month : months) {
      calendar_model()->FetchEvents(month);
    }

    WaitUntilFetched();

    if (i > 1) {
      EXPECT_LE((int)event_months().size(), kNumEvents + 1);
      EXPECT_TRUE(EventsPresentInRange(
          init_prunable_months, i, calendar_utils::kMaxNumPrunableMonths - 1));

      EXPECT_TRUE(NoEventsPresentInRange(init_prunable_months, 0, i));
    }

    on_screen_month =
        calendar_utils::GetStartOfPreviousMonthUTC(on_screen_month);
  }

  // No `non_prunable_months` is delected.
  for (auto month : non_prunable_months()) {
    EXPECT_TRUE(base::Contains(event_months(), month));
  }

  EXPECT_EQ((int)event_months().size(), kNumEvents + 1);
}

TEST_P(CalendarModelTest, RecordFetchResultHistogram_Success) {
  // Sets the timezone to "GMT".
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  if (IsMultiCalendarEnabled()) {
    EXPECT_TRUE(calendar_list_model()->get_is_cached());
    EXPECT_EQ(1u, calendar_list_model()->GetCachedCalendarList().size());
  }

  base::HistogramTester histogram_tester;

  // Current date is just `kStartTime0`.
  SetTodayFromStr(kStartTime0);

  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchEvents.Result",
                                     google_apis::HTTP_SUCCESS,
                                     /*expected_count=*/0);

  // Now fetch the events, which will get all events from the current month,
  // as well as next/prev months.
  for (auto& month : non_prunable_months()) {
    calendar_model()->FetchEvents(month);
  }

  WaitUntilFetched();

  // We should have recorded "success" for all fetches.
  histogram_tester.ExpectBucketCount(
      "Ash.Calendar.FetchEvents.Result", google_apis::HTTP_SUCCESS,
      /*expected_count=*/calendar_utils::kMaxNumNonPrunableMonths);
}

TEST_P(CalendarModelTest, RecordFetchResultHistogram_Failure) {
  // Sets the timezone to "GMT".
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  if (IsMultiCalendarEnabled()) {
    EXPECT_TRUE(calendar_list_model()->get_is_cached());
    EXPECT_EQ(1u, calendar_list_model()->GetCachedCalendarList().size());
  }

  base::HistogramTester histogram_tester;

  // Current date is just `kStartTime0`.
  base::Time current_date = calendar_test_utils::GetTimeFromString(kStartTime0);

  SetTodayFromTime(current_date);

  // Now fetch the events.
  google_apis::ApiErrorCode error;
  int i = 0;
  for (auto month : non_prunable_months()) {
    switch (i) {
      case 0:
        error = google_apis::HTTP_UNAUTHORIZED;
        break;
      case 1:
      case 2:
        error = google_apis::NO_CONNECTION;
        break;
      default:
        error = google_apis::PARSE_ERROR;
        break;
    }
    SetError(error);
    calendar_model()->FetchEvents(month);

    WaitUntilFetched();
    ++i;
  }

  // We should have recorded "success" for no fetches.
  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchEvents.Result",
                                     google_apis::HTTP_SUCCESS,
                                     /*expected_count=*/0);
  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchEvents.Result",
                                     google_apis::HTTP_UNAUTHORIZED,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchEvents.Result",
                                     google_apis::NO_CONNECTION,
                                     /*expected_count=*/2);
  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchEvents.Result",
                                     google_apis::PARSE_ERROR,
                                     /*expected_count=*/2);
}

TEST_P(CalendarModelTest, SessionStateChange) {
  // Sets the timezone to "GMT".
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  if (IsMultiCalendarEnabled()) {
    EXPECT_TRUE(calendar_list_model()->get_is_cached());
    EXPECT_EQ(1u, calendar_list_model()->GetCachedCalendarList().size());
  }

  // Current date is just `kStartTime0`.
  SetTodayFromStr(kStartTime0);

  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);

  // Haven't injected anything yet, so no events on `kStartTime0`.
  SingleDayEventList events;
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());

  // Set up list of events as the mock response and fetch the event list.
  auto event_list = std::make_unique<google_apis::calendar::EventList>();
  event_list->InjectItemForTesting(std::move(event));
  SetEventList(std::move(event_list));
  calendar_model()->FetchEvents(now());
  WaitUntilFetched();

  // Now we have an event on kStartTime0.
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);

  // Lets pretend the user locked the screen, which should clear all cached
  // events.
  SessionInfo session_info;
  session_info.state = session_manager::SessionState::LOCKED;
  SessionController::Get()->SetSessionInfo(session_info);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());
}

TEST_P(CalendarModelTest, ActiveUserChange) {
  // Sets the timezone to "GMT".
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  if (IsMultiCalendarEnabled()) {
    EXPECT_TRUE(calendar_list_model()->get_is_cached());
    EXPECT_EQ(1u, calendar_list_model()->GetCachedCalendarList().size());
  }

  // Set up two users, user1 is the active user.
  UpdateSession(1u, "user1@test.com");
  UpdateSession(2u, "user2@test.com");
  std::vector<uint32_t> order = {1u, 2u};
  SessionController::Get()->SetUserSessionOrder(order);
  base::RunLoop().RunUntilIdle();

  // Current date is just `kStartTime0`.
  SetTodayFromStr(kStartTime0);
  std::set<base::Time> months =
      calendar_utils::GetSurroundingMonthsUTC(base::Time::Now(), 1);

  // Haven't injected anything yet, so no events on `kStartTime0`.
  SingleDayEventList events;
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());

  // Set up list of events.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  event_list->InjectItemForTesting(std::move(event));
  SetEventList(std::move(event_list));

  // Now fetch the events.
  calendar_model()->FetchEvents(now());
  WaitUntilFetched();

  // Now we have an event on kStartTime0.
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);

  // Make user2 the active user, and we should clear the cached events.
  order = {2u, 1u};
  SessionController::Get()->SetUserSessionOrder(order);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());
  EXPECT_TRUE(event_months().empty());
}

TEST_P(CalendarModelTest, ActiveChildUserChange) {
  // Sets the timezone to "GMT".
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  if (IsMultiCalendarEnabled()) {
    EXPECT_TRUE(calendar_list_model()->get_is_cached());
    EXPECT_EQ(1u, calendar_list_model()->GetCachedCalendarList().size());
  }

  // Set up two users, user1 is the active user.
  UpdateSession(1u, "user1@test.com", /*is_child*/ true);
  UpdateSession(2u, "user2@test.com", /*is_child*/ true);
  std::vector<uint32_t> order = {1u, 2u};
  SessionController::Get()->SetUserSessionOrder(order);
  base::RunLoop().RunUntilIdle();

  // Current date is just `kStartTime0`.
  SetTodayFromStr(kStartTime0);
  std::set<base::Time> months =
      calendar_utils::GetSurroundingMonthsUTC(base::Time::Now(), 1);

  // Haven't injected anything yet, so no events on `kStartTime0`.
  SingleDayEventList events;
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());

  // Set up list of events.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  std::unique_ptr<google_apis::calendar::CalendarEvent> event =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  event_list->InjectItemForTesting(std::move(event));
  SetEventList(std::move(event_list));

  // Now fetch the events.
  calendar_model()->FetchEvents(now());
  WaitUntilFetched();

  // Now we have an event on kStartTime0 for chlid user1.
  EXPECT_EQ(1, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_FALSE(events.empty());
  EXPECT_TRUE(events.size() == 1);

  // Make user2 the active user, and we should clear the cached events.
  order = {2u, 1u};
  SessionController::Get()->SetUserSessionOrder(order);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());
  EXPECT_TRUE(event_months().empty());
}

TEST_P(CalendarModelTest, ClearEvents) {
  // Sets the timezone to "GMT".
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  std::unique_ptr<google_apis::calendar::CalendarEvent> event0 =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event1 =
      calendar_test_utils::CreateEvent(kId1, kSummary1, kStartTime1, kEndTime1);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event2 =
      calendar_test_utils::CreateEvent(kId2, kSummary2, kStartTime2, kEndTime2);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event3 =
      calendar_test_utils::CreateEvent(kId3, kSummary3, kStartTime3, kEndTime3);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event4 =
      calendar_test_utils::CreateEvent(kId4, kSummary4, kStartTime4, kEndTime4);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event5 =
      calendar_test_utils::CreateEvent(kId5, kSummary5, kStartTime5, kEndTime5);

  auto event_list = std::make_unique<google_apis::calendar::EventList>();

  event_list->InjectItemForTesting(std::move(event0));
  event_list->InjectItemForTesting(std::move(event1));
  event_list->InjectItemForTesting(std::move(event2));
  event_list->InjectItemForTesting(std::move(event3));
  event_list->InjectItemForTesting(std::move(event4));
  event_list->InjectItemForTesting(std::move(event5));

  // Current time is `kStartTime1`.
  SetTodayFromStr(kStartTime1);

  // Events from no months should now be present.
  SingleDayEventList events;
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime0, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime1, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime2, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime3, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime4, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime5, &events));

  MockOnEventsFetched(now(), google_apis::ApiErrorCode::HTTP_SUCCESS,
                      event_list.get());

  // Events from all months should now be present.
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime0, &events));
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime1, &events));
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime2, &events));
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime3, &events));
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime4, &events));
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime5, &events));

  // Clear out all non-prunable months.
  calendar_model()->ClearAllPrunableEvents();

  // Events from all non-prunable months should be present, but others not
  // present.
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime0, &events));
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime1, &events));
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime2, &events));
  EXPECT_EQ(1, EventsNumberOfDayInternal(kStartTime3, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime4, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime5, &events));

  // Now clear out all events.
  calendar_model()->ClearAllCachedEvents();

  // Events from all months prunable and non-prunable should not be present.
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime0, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime1, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime2, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime3, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime4, &events));
  EXPECT_EQ(0, EventsNumberOfDayInternal(kStartTime5, &events));
}

// Test for filtering of events based on their statuses. Cancelled or declined
// events shouldn't be inserted in a month.
TEST_P(CalendarModelTest, ShouldFilterEvents) {
  // Sets the timezone to "GMT".
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  SetTodayFromStr(kStartTime0);

  // Haven't injected anything yet, so no events on `kStartTime0`.
  SingleDayEventList events;
  EXPECT_EQ(0, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_TRUE(events.empty());

  // Inject events.
  std::vector<std::tuple<const char*, CalendarEvent::EventStatus,
                         CalendarEvent::ResponseStatus>>
      events_to_create = {
          std::make_tuple("cancelled+accepted",
                          CalendarEvent::EventStatus::kCancelled,
                          CalendarEvent::ResponseStatus::kAccepted),
          std::make_tuple("confirmed+accepted",
                          CalendarEvent::EventStatus::kConfirmed,
                          CalendarEvent::ResponseStatus::kAccepted),
          std::make_tuple("tentative+accepted",
                          CalendarEvent::EventStatus::kTentative,
                          CalendarEvent::ResponseStatus::kAccepted),
          std::make_tuple("unknown+accepted",
                          CalendarEvent::EventStatus::kUnknown,
                          CalendarEvent::ResponseStatus::kAccepted),
          std::make_tuple("confirmed+declined",
                          CalendarEvent::EventStatus::kConfirmed,
                          CalendarEvent::ResponseStatus::kDeclined),
          std::make_tuple("confirmed+needs_action",
                          CalendarEvent::EventStatus::kConfirmed,
                          CalendarEvent::ResponseStatus::kNeedsAction),
          std::make_tuple("confirmed+tentative",
                          CalendarEvent::EventStatus::kConfirmed,
                          CalendarEvent::ResponseStatus::kTentative),
          std::make_tuple("confirmed+unknown",
                          CalendarEvent::EventStatus::kConfirmed,
                          CalendarEvent::ResponseStatus::kUnknown)};

  std::unique_ptr<EventList> event_list = std::make_unique<EventList>();
  for (auto& event_to_create : events_to_create) {
    event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
        std::get<0>(event_to_create), std::get<0>(event_to_create), kStartTime0,
        kEndTime0, std::get<1>(event_to_create), std::get<2>(event_to_create)));
  }

  MockOnEventsFetched(now(), google_apis::ApiErrorCode::HTTP_SUCCESS,
                      event_list.get());

  // Verify that events were filtered by their statuses.
  EXPECT_EQ(4, EventsNumberOfDay(kStartTime0, &events));
  EXPECT_FALSE(events.empty());

  std::vector<std::string> filtered_event_ids;
  base::ranges::transform(events, std::back_inserter(filtered_event_ids),
                          &CalendarEvent::id);
  EXPECT_THAT(filtered_event_ids,
              testing::UnorderedElementsAreArray(std::vector<std::string>{
                  "confirmed+accepted", "tentative+accepted",
                  "confirmed+needs_action", "confirmed+tentative"}));
}

TEST_P(CalendarModelTest, EdgeOfMonthEvent) {
  // Will add event that's in the same month as kNow using PDT (UTC-7),
  // so the times will translate to next day (and month) on UTC.
  ash::system::ScopedTimezoneSettings timezone_settings(u"America/Los_Angeles");

  const char* kId = "id";
  const char* kSummary = "summary";
  const char* kNow = "10 May 2022 13:00 PDT";
  const char* kEdgeOfMonthEventStartTime = "31 May 2022 22:00 PDT";
  const char* kEdgeOfMonthEventEndTime = "31 May 2022 23:00 PDT";

  // Set current date and get surrounding months.
  SetTodayFromStr(kNow);

  // Insert an event in the edge of the month.
  std::unique_ptr<google_apis::calendar::CalendarEvent> end_of_month_event =
      calendar_test_utils::CreateEvent(
          kId, kSummary, kEdgeOfMonthEventStartTime, kEdgeOfMonthEventEndTime);

  // Haven't injected anything yet, so no events on the start time.
  SingleDayEventList events;
  base::Time start_time_adjusted =
      GetStartTimeMidnightAdjusted(end_of_month_event.get());
  EXPECT_EQ(0, EventsNumberOfDay(start_time_adjusted, &events));

  // Get ready to inject an event in the edge of the month.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->InjectItemForTesting(std::move(end_of_month_event));
  MockOnEventsFetched(now(), google_apis::ApiErrorCode::HTTP_SUCCESS,
                      event_list.get());

  // Now the day where the event started should have an event.
  EXPECT_EQ(1, EventsNumberOfDay(start_time_adjusted, &events));

  // The month to events map should insert the event in the current month.
  auto cur_month_map =
      event_months().find(calendar_utils::GetStartOfMonthUTC(now()));
  EXPECT_FALSE(cur_month_map->second.empty());

  // The month to events map should not insert the event in the next month.
  auto next_month_map =
      event_months().find(calendar_utils::GetStartOfNextMonthUTC(now()));

  EXPECT_TRUE(next_month_map->second.empty());
}

TEST_P(CalendarModelTest, MultiDayEvents) {
  // Set timezone and fake now.
  const char* kNow = "10 Nov 2022 13:00 GMT";
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");
  SetTodayFromStr(kNow);

  // Generic event id and summary.
  const char* kId = "id";
  const char* kSummary = "summary";

  // Multi-day event ranges in the two surrounding months.
  const char* kMultiDayStartTime = "20 Nov 2022 22:00 GMT";
  const char* kMultiDayEndTime = "25 Nov 2022 10:00 GMT";
  const char* kMultiMonthStartTime = "10 Sep 2022 22:00 GMT";
  const char* kMultiMonthEndTime = "2 Nov 2022 10:00 GMT";
  const char* kMultiYearStartTime = "10 Dec 2022 22:00 GMT";
  const char* kMultiYearEndTime = "10 Jan 2023 10:00 GMT";

  // Create multi-day events.
  std::unique_ptr<google_apis::calendar::CalendarEvent> multi_day_event =
      calendar_test_utils::CreateEvent(kId, kSummary, kMultiDayStartTime,
                                       kMultiDayEndTime);
  std::unique_ptr<google_apis::calendar::CalendarEvent> multi_month_event =
      calendar_test_utils::CreateEvent(kId, kSummary, kMultiMonthStartTime,
                                       kMultiMonthEndTime);
  std::unique_ptr<google_apis::calendar::CalendarEvent> multi_year_event =
      calendar_test_utils::CreateEvent(kId, kSummary, kMultiYearStartTime,
                                       kMultiYearEndTime);

  // Haven't injected anything yet, so no events on the start times.
  SingleDayEventList events;
  EXPECT_EQ(0, EventsNumberOfDay(kMultiDayStartTime, &events));
  EXPECT_EQ(0, EventsNumberOfDay(kMultiMonthStartTime, &events));
  EXPECT_EQ(0, EventsNumberOfDay(kMultiYearStartTime, &events));

  // Prepare mock events list.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->InjectItemForTesting(std::move(multi_day_event));
  event_list->InjectItemForTesting(std::move(multi_month_event));
  event_list->InjectItemForTesting(std::move(multi_year_event));

  // Mock the events are fech by each related month.
  MockOnEventsFetched(
      calendar_utils::GetStartOfMonthUTC(
          calendar_test_utils::GetTimeFromString("10 Sep 2022 22:00 GMT")),
      google_apis::ApiErrorCode::HTTP_SUCCESS, event_list.get());
  MockOnEventsFetched(
      calendar_utils::GetStartOfMonthUTC(
          calendar_test_utils::GetTimeFromString("10 Oct 2022 22:00 GMT")),
      google_apis::ApiErrorCode::HTTP_SUCCESS, event_list.get());
  MockOnEventsFetched(
      calendar_utils::GetStartOfMonthUTC(
          calendar_test_utils::GetTimeFromString("10 Nov 2022 22:00 GMT")),
      google_apis::ApiErrorCode::HTTP_SUCCESS, event_list.get());
  MockOnEventsFetched(
      calendar_utils::GetStartOfMonthUTC(
          calendar_test_utils::GetTimeFromString("10 Dec 2022 22:00 GMT")),
      google_apis::ApiErrorCode::HTTP_SUCCESS, event_list.get());
  MockOnEventsFetched(
      calendar_utils::GetStartOfMonthUTC(
          calendar_test_utils::GetTimeFromString("10 Jan 2023 22:00 GMT")),
      google_apis::ApiErrorCode::HTTP_SUCCESS, event_list.get());

  // Test a multi-day event whose start and end are in the same month.
  TestMultiDayEvent(events, kMultiDayStartTime, kMultiDayEndTime);

  // Test a multi-day event whose start and end are in different months.
  TestMultiDayEvent(events, kMultiMonthStartTime, kMultiMonthEndTime);

  // Test a multi-day event whose start and end are in different years.
  TestMultiDayEvent(events, kMultiYearStartTime, kMultiYearEndTime);
}

TEST_P(CalendarModelTest, MultiAllDayEvents) {
  // Set timezone and fake now. We set this to be GMT+n as we previously
  // had a bug where all day events overflowed into the day after they were set
  // to end for GMT+ timezones.
  const char* kNow = "10 Nov 2022 13:00 GMT+5";
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT+5");
  SetTodayFromStr(kNow);

  // Generic event id and summary.
  const char* kId = "id";
  const char* kSummary = "summary";

  // Create 2 day "all day" event.  "All day" events technically don't have
  // start and end times but we set them to start and end at 00:00 local time in
  // the API response.
  const char* kMultiAllDayEventPreviousDayTime = "19 Nov 2022 00:00 GMT";
  const char* kMultiAllDayEventDayOneStartTime = "20 Nov 2022 00:00 GMT";
  const char* kMultiAllDayEventDayTwoStartTime = "21 Nov 2022 00:00 GMT";
  const char* kMultiAllDayEventEndTime = "22 Nov 2022 00:00 GMT";
  std::unique_ptr<google_apis::calendar::CalendarEvent> multi_all_day_event =
      calendar_test_utils::CreateEvent(
          kId, kSummary, kMultiAllDayEventDayOneStartTime,
          kMultiAllDayEventEndTime,
          google_apis::calendar::CalendarEvent::EventStatus::kConfirmed,
          google_apis::calendar::CalendarEvent::ResponseStatus::kAccepted,
          true);

  // Haven't injected anything yet, so no events on the start times.
  SingleDayEventList events;
  EXPECT_EQ(0, EventsNumberOfDay(kMultiAllDayEventDayOneStartTime, &events));

  // Prepare mock events list.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->InjectItemForTesting(std::move(multi_all_day_event));

  MockOnEventsFetched(calendar_utils::GetStartOfMonthUTC(
                          calendar_test_utils::GetTimeFromString(kNow)),
                      google_apis::ApiErrorCode::HTTP_SUCCESS,
                      event_list.get());

  // Assert that the correct number of all day events are stored. We should not
  // have events on days surrounding the all day events.
  EXPECT_EQ(0, EventsNumberOfDay(kMultiAllDayEventPreviousDayTime, &events));
  EXPECT_EQ(1, EventsNumberOfDay(kMultiAllDayEventDayOneStartTime, &events));
  EXPECT_EQ(1, EventsNumberOfDay(kMultiAllDayEventDayTwoStartTime, &events));
  EXPECT_EQ(0, EventsNumberOfDay(kMultiAllDayEventEndTime, &events));
}

TEST_P(CalendarModelTest, FindFetchingStatus) {
  // Sets the timezone to "GMT".
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  if (IsMultiCalendarEnabled()) {
    EXPECT_TRUE(calendar_list_model()->get_is_cached());
    EXPECT_EQ(1u, calendar_list_model()->GetCachedCalendarList().size());
  }

  std::unique_ptr<google_apis::calendar::CalendarEvent> event0 =
      calendar_test_utils::CreateEvent(kId0, kSummary0, kStartTime0, kEndTime0);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event1 =
      calendar_test_utils::CreateEvent(kId1, kSummary1, kStartTime1, kEndTime1);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event2 =
      calendar_test_utils::CreateEvent(kId2, kSummary2, kStartTime2, kEndTime2);
  std::unique_ptr<google_apis::calendar::CalendarEvent> event3 =
      calendar_test_utils::CreateEvent(kId3, kSummary3, kStartTime3, kEndTime3);

  auto event_list = std::make_unique<google_apis::calendar::EventList>();
  event_list->InjectItemForTesting(std::move(event0));
  event_list->InjectItemForTesting(std::move(event1));
  event_list->InjectItemForTesting(std::move(event2));
  event_list->InjectItemForTesting(std::move(event3));

  SetTodayFromStr(kStartTime0);

  base::Time start_of_month0 = calendar_utils::GetStartOfMonthUTC(
      calendar_test_utils::GetTimeFromString(kStartTime0));

  // Mock that events 0~3 has fetched.
  MockOnEventsFetched(start_of_month0, google_apis::ApiErrorCode::HTTP_SUCCESS,
                      event_list.get());

  // Starts fetching a date that is not in the cache.
  base::Time fetching_date =
      calendar_test_utils::GetTimeFromString("1 Apr 2022 00:00 GMT");
  calendar_model()->FetchEvents(fetching_date);

  // The request for `fetching_date` is just sent out.
  EXPECT_EQ(CalendarModel::kFetching,
            calendar_model()->FindFetchingStatus(
                calendar_utils::GetStartOfMonthUTC(fetching_date)));

  // The request for kStartTime 0,1,2,3 are already finished (since
  // `OnEventsFetched` is called).
  EXPECT_EQ(
      CalendarModel::kSuccess,
      calendar_model()->FindFetchingStatus(calendar_utils::GetStartOfMonthUTC(
          calendar_test_utils::GetTimeFromString(kStartTime0))));
  EXPECT_EQ(
      CalendarModel::kSuccess,
      calendar_model()->FindFetchingStatus(calendar_utils::GetStartOfMonthUTC(
          calendar_test_utils::GetTimeFromString(kStartTime1))));
  EXPECT_EQ(
      CalendarModel::kSuccess,
      calendar_model()->FindFetchingStatus(calendar_utils::GetStartOfMonthUTC(
          calendar_test_utils::GetTimeFromString(kStartTime2))));
  EXPECT_EQ(
      CalendarModel::kSuccess,
      calendar_model()->FindFetchingStatus(calendar_utils::GetStartOfMonthUTC(
          calendar_test_utils::GetTimeFromString(kStartTime3))));

  // The result of `kStartTime4` has never been fetched. And the request has
  // never been sent either.
  EXPECT_EQ(
      CalendarModel::kNever,
      calendar_model()->FindFetchingStatus(calendar_utils::GetStartOfMonthUTC(
          calendar_test_utils::GetTimeFromString(kStartTime4))));

  WaitUntilFetched();

  EXPECT_EQ(CalendarModel::kSuccess,
            calendar_model()->FindFetchingStatus(
                calendar_utils::GetStartOfMonthUTC(fetching_date)));
}

TEST_P(CalendarModelTest, FindEventsSplitByMultiDayAndSameDay) {
  // Set timezone and fake now.
  const char* kNow = "10 Nov 2022 13:00 GMT+5";
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT+5");
  SetTodayFromStr(kNow);

  const char* kSummary = "summary";

  const char* kMultiDayId = "multi-day";
  const char* kMultiDayEventStartTime = "10 Nov 2022 12:00 GMT";
  const char* kMultiDayEventEndTime = "12 Nov 2022 10:00 GMT";

  const char* kSameDayId = "same-day";
  const char* kSameDayEventStartTime = "10 Nov 2022 09:00 GMT";
  const char* kSameDayEventEndTime = "10 Nov 2022 10:00 GMT";

  auto multi_day_event = calendar_test_utils::CreateEvent(
      kMultiDayId, kSummary, kMultiDayEventStartTime, kMultiDayEventEndTime);
  auto same_day_event = calendar_test_utils::CreateEvent(
      kSameDayId, kSummary, kSameDayEventStartTime, kSameDayEventEndTime);

  // Prepare mock events list.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->InjectItemForTesting(std::move(multi_day_event));
  event_list->InjectItemForTesting(std::move(same_day_event));

  // Mock the events are fetched.
  MockOnEventsFetched(calendar_utils::GetStartOfMonthUTC(
                          calendar_test_utils::GetTimeFromString(kNow)),
                      google_apis::ApiErrorCode::HTTP_SUCCESS,
                      event_list.get());

  auto [multi_day_events, same_day_events] =
      calendar_model_->FindEventsSplitByMultiDayAndSameDay(now_);

  EXPECT_EQ(multi_day_events.size(), size_t(1));
  EXPECT_EQ(multi_day_events.back().id(), kMultiDayId);
  EXPECT_EQ(same_day_events.size(), size_t(1));
  EXPECT_EQ(same_day_events.back().id(), kSameDayId);
}

TEST_P(CalendarModelTest, FindUpcomingEvents_SameDay) {
  // Set timezone and fake now.
  const char* kNow = "10 Nov 2022 13:00 GMT";
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");
  SetTodayFromStr(kNow);

  const char* kSummary = "summary";
  const char* kEventStartingInTenMinsId = "event_starting_in_ten_mins";
  const char* kEventStartingInThirtyMinsId = "event_starting_in_thirty_mins";
  const char* kEventStartingInTwoHoursId = "event_starting_in_two_hours";
  const char* kEventInProgressStartedLessThanOneHourAgoId =
      "event_in_progress_started_less_than_one_hour_ago";
  const char* kEventInProgressStartedMoreThanOneHourAgoId =
      "event_in_progress_started_more_than_one_hour_ago";
  const char* kEventFinishedId = "event_finished";

  auto event_starting_in_ten_mins = calendar_test_utils::CreateEvent(
      kEventStartingInTenMinsId, kSummary, "10 Nov 2022 13:10 GMT",
      "10 Nov 2022 15:00 GMT");
  auto event_starting_in_thirty_mins = calendar_test_utils::CreateEvent(
      kEventStartingInThirtyMinsId, kSummary, "10 Nov 2022 13:30 GMT",
      "10 Nov 2022 15:00 GMT");
  auto event_starting_in_two_hours = calendar_test_utils::CreateEvent(
      kEventStartingInTwoHoursId, kSummary, "10 Nov 2022 15:00 GMT",
      "10 Nov 2022 16:00 GMT");
  auto event_in_progress_started_less_than_one_hour_ago =
      calendar_test_utils::CreateEvent(
          kEventInProgressStartedLessThanOneHourAgoId, kSummary,
          "10 Nov 2022 12:01:00 GMT", "10 Nov 2022 17:00 GMT");
  auto event_in_progress_started_more_than_one_hour_ago =
      calendar_test_utils::CreateEvent(
          kEventInProgressStartedMoreThanOneHourAgoId, kSummary,
          "10 Nov 2022 11:00 GMT", "10 Nov 2022 17:00 GMT");
  auto event_finished = calendar_test_utils::CreateEvent(
      kEventFinishedId, kSummary, "10 Nov 2022 12:30 GMT",
      "10 Nov 2022 12:59 GMT");

  // Prepare mock events list.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->InjectItemForTesting(std::move(event_starting_in_ten_mins));
  event_list->InjectItemForTesting(std::move(event_starting_in_thirty_mins));
  event_list->InjectItemForTesting(std::move(event_starting_in_two_hours));
  event_list->InjectItemForTesting(
      std::move(event_in_progress_started_less_than_one_hour_ago));
  event_list->InjectItemForTesting(
      std::move(event_in_progress_started_more_than_one_hour_ago));
  event_list->InjectItemForTesting(std::move(event_finished));

  // Mock the events are fetched.
  MockOnEventsFetched(calendar_utils::GetStartOfMonthUTC(
                          calendar_test_utils::GetTimeFromString(kNow)),
                      google_apis::ApiErrorCode::HTTP_SUCCESS,
                      event_list.get());

  auto events = calendar_model_->FindUpcomingEvents(now_);

  auto event_list_contains = [](auto& event_list, auto& id) {
    return base::Contains(event_list, id, &CalendarEvent::id);
  };

  // We should only get the 2 events back that start in 10 mins or were ongoing
  // with < 60 mins passed.
  EXPECT_EQ(events.size(), size_t(2));
  EXPECT_TRUE(event_list_contains(events, kEventStartingInTenMinsId));
  EXPECT_FALSE(event_list_contains(events, kEventStartingInThirtyMinsId));
  EXPECT_FALSE(event_list_contains(events, kEventStartingInTwoHoursId));
  EXPECT_TRUE(
      event_list_contains(events, kEventInProgressStartedLessThanOneHourAgoId));
  EXPECT_FALSE(
      event_list_contains(events, kEventInProgressStartedMoreThanOneHourAgoId));
  EXPECT_FALSE(event_list_contains(events, kEventFinishedId));
}

// If time now is 23:55 and we have an upcoming event starting at 00:05 the
// following day, we should only show today's events. This test is needed after
// we made the change to the logic of showing the up next view. Before the
// change, we would show the events starting in 10 mins even if it's in the next
// day. Now it shouldn't be shown.
TEST_P(CalendarModelTest, FindUpcomingEvents_NextDay) {
  // Set timezone and fake now.
  const char* kNow = "10 Nov 2022 23:55 GMT";
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");
  SetTodayFromStr(kNow);

  const char* kSummary = "summary";
  const char* kEventStartingInTenMinsTomorrowId =
      "event_starting_in_ten_mins_tomorrow";

  auto event_starting_in_ten_mins_tomorrow = calendar_test_utils::CreateEvent(
      kEventStartingInTenMinsTomorrowId, kSummary, "11 Nov 2022 00:05 GMT",
      "10 Nov 2022 15:00 GMT");
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->InjectItemForTesting(
      std::move(event_starting_in_ten_mins_tomorrow));

  MockOnEventsFetched(calendar_utils::GetStartOfMonthUTC(
                          calendar_test_utils::GetTimeFromString(kNow)),
                      google_apis::ApiErrorCode::HTTP_SUCCESS,
                      event_list.get());

  auto events = calendar_model_->FindUpcomingEvents(now_);

  auto event_list_contains = [](auto& event_list, auto& id) {
    return base::Contains(event_list, id, &CalendarEvent::id);
  };

  EXPECT_EQ(events.size(), size_t(0));
  EXPECT_FALSE(event_list_contains(events, kEventStartingInTenMinsTomorrowId));
}

// If time now is 00:10 and we have an event that started <1 hour ago, then we
// should get in progress events from the previous day back.
TEST_P(CalendarModelTest, FindUpcomingEvents_PreviousDay) {
  // Set timezone and fake now.
  const char* kNow = "10 Nov 2022 00:10 GMT";
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");
  SetTodayFromStr(kNow);

  const char* kSummary = "summary";
  const char* kEventInProgressStartedYesterdayId =
      "event_in_progress_started_yesterday_id";

  auto event_in_progress_started_yesterday = calendar_test_utils::CreateEvent(
      kEventInProgressStartedYesterdayId, kSummary, "09 Nov 2022 23:15 GMT",
      "10 Nov 2022 00:15 GMT");
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->InjectItemForTesting(
      std::move(event_in_progress_started_yesterday));

  MockOnEventsFetched(calendar_utils::GetStartOfMonthUTC(
                          calendar_test_utils::GetTimeFromString(kNow)),
                      google_apis::ApiErrorCode::HTTP_SUCCESS,
                      event_list.get());

  auto events = calendar_model_->FindUpcomingEvents(now_);

  auto event_list_contains = [](auto& event_list, auto& id) {
    return base::Contains(event_list, id, &CalendarEvent::id);
  };

  EXPECT_EQ(events.size(), size_t(1));
  EXPECT_TRUE(event_list_contains(events, kEventInProgressStartedYesterdayId));
}

// If the next event doesn't start in the next 10 mins, we'll still show it.
// This is needed after we changed the logic of showing the up next view.
TEST_P(CalendarModelTest, FindUpcomingEvents_ShowTheNextEvent) {
  // Set timezone and fake now.
  const char* kNow = "10 Nov 2022 13:00 GMT";
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");
  SetTodayFromStr(kNow);

  const char* kSummary = "summary";
  const char* kEventStartingInThirtyMinsId = "event_starting_in_thirty_mins";

  auto event_starting_in_thirty_mins = calendar_test_utils::CreateEvent(
      kEventStartingInThirtyMinsId, kSummary, "10 Nov 2022 13:30 GMT",
      "10 Nov 2022 15:00 GMT");

  // Prepare mock events list.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->InjectItemForTesting(std::move(event_starting_in_thirty_mins));

  // Mock the events are fetched.
  MockOnEventsFetched(calendar_utils::GetStartOfMonthUTC(
                          calendar_test_utils::GetTimeFromString(kNow)),
                      google_apis::ApiErrorCode::HTTP_SUCCESS,
                      event_list.get());

  auto events = calendar_model_->FindUpcomingEvents(now_);

  auto event_list_contains = [](auto& event_list, auto& id) {
    return base::Contains(event_list, id, &CalendarEvent::id);
  };

  EXPECT_EQ(events.size(), size_t(1));
  EXPECT_TRUE(event_list_contains(events, kEventStartingInThirtyMinsId));
}

// If two events start at the same time, show the one finishing earlier first.
// Returns:
// First event: 13:00 - 13:45
// Second event: 13:00 - 14:00
TEST_P(CalendarModelTest, EventsSortingWithSameStartTime) {
  // Set timezone and fake now.
  const char* kNow = "10 Nov 2022 13:00 GMT";
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");
  SetTodayFromStr(kNow);

  const char* kSummary = "summary";
  const char* kFirstEventId = "first_event";
  const char* kSecondEventId = "second_event";

  auto first_event = calendar_test_utils::CreateEvent(kFirstEventId, kSummary,
                                                      "10 Nov 2022 13:00 GMT",
                                                      "10 Nov 2022 13:45 GMT");
  auto second_event = calendar_test_utils::CreateEvent(kSecondEventId, kSummary,
                                                       "10 Nov 2022 13:00 GMT",
                                                       "10 Nov 2022 14:00 GMT");

  // Prepare mock events list. Note we add the second event first, which should
  // later be sorted differently.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->InjectItemForTesting(std::move(second_event));
  event_list->InjectItemForTesting(std::move(first_event));

  // Mock the events are fetched.
  MockOnEventsFetched(calendar_utils::GetStartOfMonthUTC(
                          calendar_test_utils::GetTimeFromString(kNow)),
                      google_apis::ApiErrorCode::HTTP_SUCCESS,
                      event_list.get());

  auto events = calendar_model_->FindUpcomingEvents(now_);

  EXPECT_EQ(events.size(), size_t(2));
  EXPECT_EQ(kFirstEventId, events.front().id());
  EXPECT_EQ(kSecondEventId, events.back().id());
}

// Shows all events that start in 10 mins.
TEST_P(CalendarModelTest, ShowEventsStartIn10MinsAsUpNext) {
  // Set timezone and fake now.
  const char* kNow = "10 Nov 2022 13:00 GMT";
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");
  SetTodayFromStr(kNow);

  const char* kSummary = "summary";
  const char* kFirstEventId = "first_event";
  const char* kSecondEventId = "second_event";

  auto first_event = calendar_test_utils::CreateEvent(kFirstEventId, kSummary,
                                                      "10 Nov 2022 13:05 GMT",
                                                      "10 Nov 2022 13:45 GMT");
  auto second_event = calendar_test_utils::CreateEvent(kSecondEventId, kSummary,
                                                       "10 Nov 2022 13:00 GMT",
                                                       "10 Nov 2022 14:00 GMT");

  // Prepare mock events list. Note we add the first event first, which should
  // later be sorted differently.
  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->InjectItemForTesting(std::move(first_event));
  event_list->InjectItemForTesting(std::move(second_event));

  // Mock the events are fetched.
  MockOnEventsFetched(calendar_utils::GetStartOfMonthUTC(
                          calendar_test_utils::GetTimeFromString(kNow)),
                      google_apis::ApiErrorCode::HTTP_SUCCESS,
                      event_list.get());

  auto events = calendar_model_->FindUpcomingEvents(now_);

  auto event_list_contains = [](auto& event_list, auto& id) {
    return base::Contains(event_list, id, &CalendarEvent::id);
  };

  EXPECT_EQ(events.size(), size_t(2));
  EXPECT_TRUE(event_list_contains(events, kSecondEventId));
}

// Shows the first event if there's no events that start in 10 mins.
TEST_P(CalendarModelTest, ShowTheFirstEventAsUpNext) {
  // Set timezone and fake now.
  const char* kNow = "10 Nov 2022 13:00 GMT";
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");
  SetTodayFromStr(kNow);

  const char* kSummary = "summary";
  const char* kFirstEventId = "first_event";
  const char* kSecondEventId = "second_event";

  auto first_event = calendar_test_utils::CreateEvent(kFirstEventId, kSummary,
                                                      "10 Nov 2022 15:05 GMT",
                                                      "10 Nov 2022 15:45 GMT");
  auto second_event = calendar_test_utils::CreateEvent(kSecondEventId, kSummary,
                                                       "10 Nov 2022 16:00 GMT",
                                                       "10 Nov 2022 17:00 GMT");

  std::unique_ptr<google_apis::calendar::EventList> event_list =
      std::make_unique<google_apis::calendar::EventList>();
  event_list->InjectItemForTesting(std::move(first_event));
  event_list->InjectItemForTesting(std::move(second_event));

  // Mock the events are fetched.
  MockOnEventsFetched(calendar_utils::GetStartOfMonthUTC(
                          calendar_test_utils::GetTimeFromString(kNow)),
                      google_apis::ApiErrorCode::HTTP_SUCCESS,
                      event_list.get());

  auto events = calendar_model_->FindUpcomingEvents(now_);

  auto event_list_contains = [](auto& event_list, auto& id) {
    return base::Contains(event_list, id, &CalendarEvent::id);
  };

  EXPECT_EQ(events.size(), size_t(1));
  EXPECT_TRUE(event_list_contains(events, kFirstEventId));
  EXPECT_FALSE(event_list_contains(events, kSecondEventId));
}
}  // namespace ash
