// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_list_model.h"

#include <iterator>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "ash/calendar/calendar_client.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/session_manager/session_manager_types.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/common/api_error_codes.h"

namespace ash {

namespace {

using ::google_apis::calendar::SingleCalendar;

constexpr char kId1[] = "3edk4w2test@group.calendar.google.com";
constexpr char kSummary1[] = "A-Team OOO";
constexpr char kColorId1[] = "c7";
bool kSelected1 = true;
bool kPrimary1 = false;

constexpr char kId2[] = "user1@test.com";
constexpr char kSummary2[] = "user1@test.com";
constexpr char kColorId2[] = "c12";
bool kSelected2 = true;
bool kPrimary2 = true;

constexpr char kId3[] = "9l9zh1z9test@group.calendar.google.com";
constexpr char kSummary3[] = "Birthdays";
constexpr char kColorId3[] = "c14";
bool kSelected3 = true;
bool kPrimary3 = false;

constexpr char kId4[] = "zu35dc5test@group.calendar.google.com";
constexpr char kSummary4[] = "NYC Food Pop Ups";
constexpr char kColorId4[] = "c3";
bool kSelected4 = true;
bool kPrimary4 = false;

constexpr char kId5[] = "o8dymp3test@group.calendar.google.com";
constexpr char kSummary5[] = "Happy Hour Events";
constexpr char kColorId5[] = "c5";
bool kSelected5 = true;
bool kPrimary5 = false;

constexpr char kId6[] = "hf0yme7test@group.calendar.google.com";
constexpr char kSummary6[] = "On-call Rotation";
constexpr char kColorId6[] = "c12";
bool kSelected6 = true;
bool kPrimary6 = false;

constexpr char kId7[] = "jc4eec9test@group.calendar.google.com";
constexpr char kSummary7[] = "Running Club";
constexpr char kColorId7[] = "c11";
bool kSelected7 = true;
bool kPrimary7 = false;

constexpr char kId8[] = "yi2oxo6test@group.calendar.google.com";
constexpr char kSummary8[] = "Company holidays";
constexpr char kColorId8[] = "c9";
bool kSelected8 = true;
bool kPrimary8 = false;

constexpr char kId9[] = "ziy36m8test@group.calendar.google.com";
constexpr char kSummary9[] = "Soccer Games";
constexpr char kColorId9[] = "c10";
bool kSelected9 = true;
bool kPrimary9 = false;

constexpr char kId10[] = "a34wrv5test@group.calendar.google.com";
constexpr char kSummary10[] = "Writing Club";
constexpr char kColorId10[] = "c1";
bool kSelected10 = true;
bool kPrimary10 = false;

constexpr char kId11[] = "bx5mybotest@group.calendar.google.com";
constexpr char kSummary11[] = "Family";
constexpr char kColorId11[] = "c5";
bool kSelected11 = true;
bool kPrimary11 = false;

constexpr char kId12[] = "dfc67a8test@group.calendar.google.com";
constexpr char kSummary12[] = "Band Rehearsal";
constexpr char kColorId12[] = "c7";
bool kSelected12 = false;
bool kPrimary12 = false;

std::unique_ptr<google_apis::calendar::CalendarList> CreateMockCalendarList() {
  std::list<std::unique_ptr<google_apis::calendar::SingleCalendar>> calendars;
  calendars.push_back(calendar_test_utils::CreateCalendar(
      kId1, kSummary1, kColorId1, kSelected1, kPrimary1));
  calendars.push_back(calendar_test_utils::CreateCalendar(
      kId2, kSummary2, kColorId2, kSelected2, kPrimary2));
  calendars.push_back(calendar_test_utils::CreateCalendar(
      kId3, kSummary3, kColorId3, kSelected3, kPrimary3));
  calendars.push_back(calendar_test_utils::CreateCalendar(
      kId12, kSummary12, kColorId12, kSelected12, kPrimary12));
  return calendar_test_utils::CreateMockCalendarList(std::move(calendars));
}

}  // namespace

class CalendarListModelTest : public AshTestBase {
 public:
  CalendarListModelTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  CalendarListModelTest(const CalendarListModelTest& other) = delete;
  CalendarListModelTest& operator=(const CalendarListModelTest& other) = delete;
  ~CalendarListModelTest() override = default;

  void SetUp() override {
    // Enable the Multi-Calendar feature.
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kMultiCalendarSupport);

    AshTestBase::SetUp();

    // Register a mock `CalendarClient` to the `CalendarController`.
    const std::string email = "test1@google.com";
    AccountId account_id = AccountId::FromUserEmail(email);
    Shell::Get()->calendar_controller()->SetActiveUserAccountIdForTesting(
        account_id);
    calendar_list_model_ = std::make_unique<CalendarListModel>();
    calendar_client_ =
        std::make_unique<calendar_test_utils::CalendarClientTestImpl>();
    Shell::Get()->calendar_controller()->RegisterClientForUser(
        account_id, calendar_client_.get());
    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        ash::prefs::kCalendarIntegrationEnabled, true);
  }

  void TearDown() override {
    calendar_list_model_.reset();
    scoped_feature_list_.Reset();
    AshTestBase::TearDown();
  }

  // Wait until the response is back. Since we used `PostDelayedTask` with 1
  // second to mimic the behavior of fetching, duration of 1 minute should be
  // enough.
  void WaitUntilFetched() {
    task_environment()->FastForwardBy(base::Minutes(1));
    base::RunLoop().RunUntilIdle();
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

  CalendarListModel* calendar_list_model() {
    return calendar_list_model_.get();
  }

  calendar_test_utils::CalendarClientTestImpl* client() {
    return calendar_client_.get();
  }

  std::unique_ptr<CalendarListModel> calendar_list_model_;
  std::unique_ptr<calendar_test_utils::CalendarClientTestImpl> calendar_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CalendarListModelTest, FetchShortCalendarList) {
  // Set up list of calendars as the mock response.
  client()->SetCalendarList(CreateMockCalendarList());

  // Fetching should not be in progress and the calendar list should not be
  // cached.
  EXPECT_FALSE(calendar_list_model()->get_fetch_in_progress());
  EXPECT_FALSE(calendar_list_model()->get_is_cached());

  // Fetch the calendars. The model should report that a fetch is in progress
  // and a calendar list is not yet cached.
  calendar_list_model()->FetchCalendars();
  EXPECT_TRUE(calendar_list_model()->get_fetch_in_progress());
  EXPECT_FALSE(calendar_list_model()->get_is_cached());

  WaitUntilFetched();

  // The model should now report that a fetch is not in progress and a calendar
  // list is cached.
  EXPECT_FALSE(calendar_list_model()->get_fetch_in_progress());
  EXPECT_TRUE(calendar_list_model()->get_is_cached());

  CalendarList calendar_list = calendar_list_model()->GetCachedCalendarList();

  // Verify that the length of the result matches the number of calendars in
  // the mock list that are selected.
  EXPECT_EQ(3u, calendar_list.size());

  // Verify that the next calendars in the result are sorted in alphabetical
  // order.
  ash::CalendarList::iterator calendar_2_it = std::next(calendar_list.begin());
  EXPECT_EQ(calendar_2_it->summary(), kSummary1);
  ash::CalendarList::iterator calendar_3_it = std::next(calendar_2_it);
  EXPECT_EQ(calendar_3_it->summary(), kSummary3);

  // Set up list of calendars as the mock response.
  client()->SetCalendarList(CreateMockCalendarList());

  // Trigger a refetch. The model should report that a fetch is in progress and
  // a calendar list is already cached.
  calendar_list_model()->FetchCalendars();
  EXPECT_TRUE(calendar_list_model()->get_fetch_in_progress());
  EXPECT_TRUE(calendar_list_model()->get_is_cached());

  WaitUntilFetched();

  // The model should now report that a fetch is not in progress and a calendar
  // list is cached. The size of the result should be as expected.
  EXPECT_FALSE(calendar_list_model()->get_fetch_in_progress());
  EXPECT_TRUE(calendar_list_model()->get_is_cached());
  calendar_list = calendar_list_model()->GetCachedCalendarList();
  EXPECT_EQ(3u, calendar_list.size());
}

TEST_F(CalendarListModelTest, FetchLongCalendarList) {
  // Set up list of calendars as the mock response.
  std::list<std::unique_ptr<google_apis::calendar::SingleCalendar>> calendars;
  calendars.push_back(calendar_test_utils::CreateCalendar(
      kId1, kSummary1, kColorId1, kSelected1, kPrimary1));
  calendars.push_back(calendar_test_utils::CreateCalendar(
      kId2, kSummary2, kColorId2, kSelected2, kPrimary2));
  calendars.push_back(calendar_test_utils::CreateCalendar(
      kId3, kSummary3, kColorId3, kSelected3, kPrimary3));
  calendars.push_back(calendar_test_utils::CreateCalendar(
      kId4, kSummary4, kColorId4, kSelected4, kPrimary4));
  calendars.push_back(calendar_test_utils::CreateCalendar(
      kId5, kSummary5, kColorId5, kSelected5, kPrimary5));
  calendars.push_back(calendar_test_utils::CreateCalendar(
      kId6, kSummary6, kColorId6, kSelected6, kPrimary6));
  calendars.push_back(calendar_test_utils::CreateCalendar(
      kId7, kSummary7, kColorId7, kSelected7, kPrimary7));
  calendars.push_back(calendar_test_utils::CreateCalendar(
      kId8, kSummary8, kColorId8, kSelected8, kPrimary8));
  calendars.push_back(calendar_test_utils::CreateCalendar(
      kId9, kSummary9, kColorId9, kSelected9, kPrimary9));
  calendars.push_back(calendar_test_utils::CreateCalendar(
      kId10, kSummary10, kColorId10, kSelected10, kPrimary10));
  calendars.push_back(calendar_test_utils::CreateCalendar(
      kId11, kSummary11, kColorId11, kSelected11, kPrimary11));
  calendars.push_back(calendar_test_utils::CreateCalendar(
      kId12, kSummary12, kColorId12, kSelected12, kPrimary12));
  client()->SetCalendarList(
      calendar_test_utils::CreateMockCalendarList(std::move(calendars)));

  // Fetching should not be in progress and the calendar list should not be
  // cached.
  EXPECT_FALSE(calendar_list_model()->get_fetch_in_progress());
  EXPECT_FALSE(calendar_list_model()->get_is_cached());

  // Fetch the calendars. The model should report that a fetch is in progress
  // and a calendar list is not yet cached.
  calendar_list_model()->FetchCalendars();
  EXPECT_TRUE(calendar_list_model()->get_fetch_in_progress());
  EXPECT_FALSE(calendar_list_model()->get_is_cached());

  WaitUntilFetched();

  // The model should now report that a fetch is not in progress and a calendar
  // list is cached.
  EXPECT_FALSE(calendar_list_model()->get_fetch_in_progress());
  EXPECT_TRUE(calendar_list_model()->get_is_cached());

  CalendarList calendar_list = calendar_list_model()->GetCachedCalendarList();

  // Verify that the length of the result equals `kMultipleCalendarsLimit`.
  EXPECT_EQ(10u, calendar_list.size());

  // Verify that the primary calendar is the first entry in the result.
  EXPECT_TRUE(calendar_list.front().primary());

  // Verify that the next calendars in the result are sorted in the expected
  // order (alphabetically, with the unselected calendar absent).
  ash::CalendarList::iterator calendar_2_it = std::next(calendar_list.begin());
  EXPECT_EQ(calendar_2_it->summary(), kSummary1);
  ash::CalendarList::iterator calendar_3_it = std::next(calendar_2_it);
  EXPECT_EQ(calendar_3_it->summary(), kSummary3);
  ash::CalendarList::iterator calendar_4_it = std::next(calendar_3_it);
  EXPECT_EQ(calendar_4_it->summary(), kSummary8);
  ash::CalendarList::iterator calendar_5_it = std::next(calendar_4_it);
  EXPECT_EQ(calendar_5_it->summary(), kSummary11);
  ash::CalendarList::iterator calendar_6_it = std::next(calendar_5_it);
  EXPECT_EQ(calendar_6_it->summary(), kSummary5);
  ash::CalendarList::iterator calendar_7_it = std::next(calendar_6_it);
  EXPECT_EQ(calendar_7_it->summary(), kSummary4);
  ash::CalendarList::iterator calendar_8_it = std::next(calendar_7_it);
  EXPECT_EQ(calendar_8_it->summary(), kSummary6);
  ash::CalendarList::iterator calendar_9_it = std::next(calendar_8_it);
  EXPECT_EQ(calendar_9_it->summary(), kSummary7);
  ash::CalendarList::iterator calendar_10_it = std::next(calendar_9_it);
  EXPECT_EQ(calendar_10_it->summary(), kSummary9);
}

TEST_F(CalendarListModelTest, ActiveUserChange) {
  // Set up two users, user1 is the active user.
  UpdateSession(1u, "user1@test.com");
  UpdateSession(2u, "user2@test.com");
  std::vector<uint32_t> order = {1u, 2u};
  SessionController::Get()->SetUserSessionOrder(order);
  base::RunLoop().RunUntilIdle();

  // Set up list of calendars as the mock response.
  client()->SetCalendarList(CreateMockCalendarList());

  calendar_list_model()->FetchCalendars();
  WaitUntilFetched();

  // Verify that the length of the result matches the number of calendars in
  // the mock list that are selected.
  EXPECT_TRUE(calendar_list_model()->get_is_cached());
  CalendarList calendar_list = calendar_list_model()->GetCachedCalendarList();
  EXPECT_EQ(3u, calendar_list.size());

  // Make user2 the active user, and the cached calendars should be cleared.
  order = {2u, 1u};
  SessionController::Get()->SetUserSessionOrder(order);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(calendar_list_model()->get_is_cached());
  calendar_list = calendar_list_model()->GetCachedCalendarList();
  EXPECT_EQ(0u, calendar_list.size());
}

TEST_F(CalendarListModelTest, ActiveChildUserChange) {
  // Set up two child users, user1 is the active user.
  UpdateSession(1u, "user1@test.com", /*is_child=*/true);
  UpdateSession(2u, "user2@test.com", /*is_child=*/true);
  std::vector<uint32_t> order = {1u, 2u};
  SessionController::Get()->SetUserSessionOrder(order);
  base::RunLoop().RunUntilIdle();

  // Set up list of calendars as the mock response.
  client()->SetCalendarList(CreateMockCalendarList());

  calendar_list_model()->FetchCalendars();
  WaitUntilFetched();

  // Verify that the length of the result matches the number of calendars in
  // the mock list that are selected.
  EXPECT_TRUE(calendar_list_model()->get_is_cached());
  CalendarList calendar_list = calendar_list_model()->GetCachedCalendarList();
  EXPECT_EQ(3u, calendar_list.size());

  // Make user2 the active user, and the cached calendars should be cleared.
  order = {2u, 1u};
  SessionController::Get()->SetUserSessionOrder(order);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(calendar_list_model()->get_is_cached());
  calendar_list = calendar_list_model()->GetCachedCalendarList();
  EXPECT_EQ(0u, calendar_list.size());
}

TEST_F(CalendarListModelTest, ClearCalendars) {
  // Set up list of calendars as the mock response.
  client()->SetCalendarList(CreateMockCalendarList());

  calendar_list_model()->FetchCalendars();
  WaitUntilFetched();

  // Verify that the length of the result matches the number of calendars in
  // the mock list that are selected.
  EXPECT_TRUE(calendar_list_model()->get_is_cached());
  CalendarList calendar_list = calendar_list_model()->GetCachedCalendarList();
  EXPECT_EQ(3u, calendar_list.size());

  // Simulate a session change to clear the calendar list.
  calendar_list_model()->OnSessionStateChanged(
      session_manager::SessionState::LOCKED);

  // Verify that the list is empty after clearing the list and the model
  // indicates that there is no calendar list cached.
  EXPECT_FALSE(calendar_list_model()->get_is_cached());
  calendar_list = calendar_list_model()->GetCachedCalendarList();
  EXPECT_EQ(0u, calendar_list.size());
}

TEST_F(CalendarListModelTest, RecordFetchResultHistogram_Success) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchCalendars.Result",
                                     google_apis::HTTP_SUCCESS,
                                     /*expected_count=*/0);

  calendar_list_model()->FetchCalendars();

  WaitUntilFetched();

  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchCalendars.Result",
                                     google_apis::HTTP_SUCCESS,
                                     /*expected_count=*/1);
}

TEST_F(CalendarListModelTest, RecordFetchResultHistogram_Failure) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchCalendars.Result",
                                     google_apis::HTTP_SUCCESS,
                                     /*expected_count=*/0);

  client()->SetError(google_apis::NO_CONNECTION);
  calendar_list_model()->FetchCalendars();

  WaitUntilFetched();

  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchCalendars.Result",
                                     google_apis::NO_CONNECTION,
                                     /*expected_count=*/1);

  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchCalendars.Result",
                                     google_apis::HTTP_SUCCESS,
                                     /*expected_count=*/0);
}

TEST_F(CalendarListModelTest, RecordFetchResultHistogram_Cancelled) {
  base::HistogramTester histogram_tester;

  // Set mock calendar list and error code in the client.
  client()->SetCalendarList(CreateMockCalendarList());
  client()->SetError(google_apis::CANCELLED);
  calendar_list_model()->FetchCalendars();

  calendar_list_model()->CancelFetch();

  WaitUntilFetched();

  // There should be no calendar list cached despite a calendar list being set
  // in the client.
  EXPECT_FALSE(calendar_list_model()->get_is_cached());

  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchCalendars.Result",
                                     google_apis::CANCELLED,
                                     /*expected_count=*/1);

  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchCalendars.Result",
                                     google_apis::HTTP_SUCCESS,
                                     /*expected_count=*/0);
}

TEST_F(CalendarListModelTest, RecordFetchTimeout) {
  base::HistogramTester histogram_tester;

  // No timeout has been recorded yet.
  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchCalendars.Timeout",
                                     true,
                                     /*expected_count=*/0);

  client()->SetCalendarList(CreateMockCalendarList());

  // Delay the response until after the model declares a timeout.
  client()->SetResponseDelay(calendar_utils::kCalendarDataFetchTimeout +
                             base::Milliseconds(100));

  calendar_list_model()->FetchCalendars();

  task_environment()->FastForwardBy(calendar_utils::kCalendarDataFetchTimeout);

  // There should be no calendar list cached due to the timeout.
  EXPECT_FALSE(calendar_list_model()->get_is_cached());

  // A timeout should be recorded.
  histogram_tester.ExpectBucketCount("Ash.Calendar.FetchCalendars.Timeout",
                                     true,
                                     /*expected_count=*/1);
}

}  // namespace ash
