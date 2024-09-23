// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/impression_history_tracker.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/scheduler/test/fake_clock.h"
#include "chrome/browser/notifications/scheduler/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using StoreEntries = std::vector<std::unique_ptr<notifications::ClientState>>;

namespace notifications {
namespace {

const char kGuid1[] = "guid1";
const char kGuid2[] = "guid2";
const char kButtonId[] = "button_id_1";
const char kTimeStr[] = "04/25/20 01:00:00 AM";

struct TestCase {
  // Input data that will be pushed to the target class.
  std::vector<test::ImpressionTestData> input;

  // List of registered clients.
  std::vector<SchedulerClientType> registered_clients;

  // Expected output data.
  std::vector<test::ImpressionTestData> expected;
};

Impression CreateImpression(const base::Time& create_time,
                            const std::string& guid) {
  Impression impression(SchedulerClientType::kTest1, guid, create_time);
  return impression;
}

Impression CreateImpression(const base::Time& create_time,
                            const std::string& guid,
                            UserFeedback feedback) {
  Impression impression(SchedulerClientType::kTest1, guid, create_time);
  impression.feedback = feedback;
  return impression;
}

Impression CreateImpression(
    const base::Time& create_time,
    const std::string& guid,
    UserFeedback feedback,
    base::TimeDelta ignore_timeout_duration,
    Impression::ImpressionResultMap impression_mapping) {
  Impression impression(SchedulerClientType::kTest1, guid, create_time);
  impression.feedback = feedback;
  impression.ignore_timeout_duration = ignore_timeout_duration;
  impression.impression_mapping = impression_mapping;
  return impression;
}

TestCase CreateDefaultTestCase() {
  TestCase test_case;
  test_case.input = {{SchedulerClientType::kTest1,
                      2 /* current_max_daily_show */,
                      {} /* impressions */,
                      std::nullopt /* suppression_info */,
                      0 /* negative_events_count */,
                      std::nullopt /* last_negative_event_ts */,
                      std::nullopt /* last_shown_ts */}};
  test_case.registered_clients = {SchedulerClientType::kTest1};
  test_case.expected = test_case.input;
  return test_case;
}

class MockImpressionStore : public CollectionStore<ClientState> {
 public:
  MockImpressionStore() {}
  MockImpressionStore(const MockImpressionStore&) = delete;
  MockImpressionStore& operator=(const MockImpressionStore&) = delete;

  MOCK_METHOD1(InitAndLoad, void(CollectionStore<ClientState>::LoadCallback));
  MOCK_METHOD3(Add,
               void(const std::string&,
                    const ClientState&,
                    base::OnceCallback<void(bool)>));
  MOCK_METHOD3(Update,
               void(const std::string&,
                    const ClientState&,
                    base::OnceCallback<void(bool)>));
  MOCK_METHOD2(Delete,
               void(const std::string&, base::OnceCallback<void(bool)>));
};

class MockDelegate final : public ImpressionHistoryTracker::Delegate {
 public:
  MockDelegate() = default;
  MockDelegate(const MockDelegate&) = delete;
  MockDelegate& operator=(const MockDelegate&) = delete;
  ~MockDelegate() override = default;
  MOCK_METHOD2(GetThrottleConfig,
               void(SchedulerClientType,
                    base::OnceCallback<void(std::unique_ptr<ThrottleConfig>)>));
};

// TODO(xingliu): Add more test cases following the test doc.
class ImpressionHistoryTrackerTest : public ::testing::Test {
 public:
  ImpressionHistoryTrackerTest() : store_(nullptr), delegate_(nullptr) {}
  ImpressionHistoryTrackerTest(const ImpressionHistoryTrackerTest&) = delete;
  ImpressionHistoryTrackerTest& operator=(const ImpressionHistoryTrackerTest&) =
      delete;
  ~ImpressionHistoryTrackerTest() override = default;

  void SetUp() override {
    config_.impression_expiration = base::Days(28);
    config_.suppression_duration = base::Days(56);
    config_.initial_daily_shown_per_type = 2;
  }

 protected:
  // Creates the impression tracker.
  void CreateTracker(const TestCase& test_case) {
    auto store = std::make_unique<MockImpressionStore>();
    store_ = store.get();
    delegate_ = std::make_unique<MockDelegate>();
    impression_trakcer_ = std::make_unique<ImpressionHistoryTrackerImpl>(
        config_, test_case.registered_clients, std::move(store), &clock_);
  }

  // Initializes the tracker with data defined in the |test_case|.
  void InitTrackerWithData(const TestCase& test_case) {
    // Initialize the store and call the callback.A
    StoreEntries entries;
    test::AddImpressionTestData(test_case.input, &entries);
    EXPECT_CALL(*store_, InitAndLoad(_))
        .WillOnce(
            Invoke([&entries](base::OnceCallback<void(bool, StoreEntries)> cb) {
              std::move(cb).Run(true, std::move(entries));
            }));
    base::RunLoop loop;
    impression_trakcer_->Init(
        delegate_.get(), base::BindOnce(
                             [](base::RepeatingClosure closure, bool success) {
                               EXPECT_TRUE(success);
                               std::move(closure).Run();
                             },
                             loop.QuitClosure()));
    loop.Run();
  }

  // Verifies the |expected_test_data| matches the internal states.
  void VerifyClientStates(const TestCase& test_case) {
    std::map<SchedulerClientType, const ClientState*> client_states;
    impression_trakcer_->GetClientStates(&client_states);

    ImpressionHistoryTracker::ClientStates expected_client_states;
    test::AddImpressionTestData(test_case.expected, &expected_client_states);

    DCHECK_EQ(expected_client_states.size(), client_states.size());
    for (const auto& expected : expected_client_states) {
      auto output_it = client_states.find(expected.first);
      CHECK(output_it != client_states.end());
      EXPECT_EQ(*expected.second, *output_it->second)
          << "Unmatch client states: \n"
          << "Expected: \n"
          << test::DebugString(expected.second.get()) << " \n"
          << "Acutual: \n"
          << test::DebugString(output_it->second);
    }
  }

  const SchedulerConfig& config() const { return config_; }
  MockImpressionStore* store() { return store_; }
  MockDelegate* delegate() { return delegate_.get(); }

  ImpressionHistoryTracker* tracker() { return impression_trakcer_.get(); }
  test::FakeClock* clock() { return &clock_; }

 private:
  base::test::TaskEnvironment task_environment_;
  test::FakeClock clock_;
  SchedulerConfig config_;
  std::unique_ptr<ImpressionHistoryTracker> impression_trakcer_;
  raw_ptr<MockImpressionStore> store_;
  std::unique_ptr<MockDelegate> delegate_;
};

// New client data should be added to impression tracker.
TEST_F(ImpressionHistoryTrackerTest, NewReigstedClient) {
  TestCase test_case = CreateDefaultTestCase();
  test_case.registered_clients.emplace_back(SchedulerClientType::kTest2);
  test_case.expected.emplace_back(test::ImpressionTestData(
      SchedulerClientType::kTest2, config().initial_daily_shown_per_type, {},
      std::nullopt, 0, std::nullopt, std::nullopt));

  CreateTracker(test_case);
  EXPECT_CALL(*store(), Add(_, _, _));
  InitTrackerWithData(test_case);
  VerifyClientStates(test_case);
}

// Data for deprecated client should be deleted.
TEST_F(ImpressionHistoryTrackerTest, DeprecateClient) {
  TestCase test_case = CreateDefaultTestCase();
  test_case.registered_clients.clear();
  test_case.expected.clear();

  CreateTracker(test_case);
  EXPECT_CALL(*store(), Delete(_, _));
  InitTrackerWithData(test_case);
  VerifyClientStates(test_case);
}

// Verifies expired impression will be deleted.
TEST_F(ImpressionHistoryTrackerTest, DeleteExpiredImpression) {
  TestCase test_case = CreateDefaultTestCase();
  auto expired_create_time =
      clock()->Now() - base::Days(1) - config().impression_expiration;
  auto not_expired_time =
      clock()->Now() + base::Days(1) - config().impression_expiration;

  Impression expired = CreateImpression(expired_create_time, "guid1");
  Impression not_expired = CreateImpression(not_expired_time, "guid2");

  test_case.input.back().impressions = {expired, not_expired, expired};
  test_case.expected.back().impressions = {not_expired};

  CreateTracker(test_case);
  EXPECT_CALL(*store(), Update(_, _, _));
  InitTrackerWithData(test_case);
  VerifyClientStates(test_case);
}

// Verifies the state of new impression added to the tracker.
TEST_F(ImpressionHistoryTrackerTest, AddImpression) {
  TestCase test_case = CreateDefaultTestCase();
  CreateTracker(test_case);
  InitTrackerWithData(test_case);

  // No-op for unregistered client.
  tracker()->AddImpression(SchedulerClientType::kTest2, kGuid2,
                           Impression::ImpressionResultMap(),
                           Impression::CustomData(), std::nullopt);
  VerifyClientStates(test_case);

  clock()->SetNow(kTimeStr);

  Impression::ImpressionResultMap impression_mapping = {
      {UserFeedback::kDismiss, ImpressionResult::kNegative}};
  Impression::CustomData custom_data = {{"url", "https://www.example.com"}};
  EXPECT_CALL(*store(), Update(_, _, _));
  tracker()->AddImpression(SchedulerClientType::kTest1, kGuid1,
                           impression_mapping, custom_data, std::nullopt);
  Impression expected_impression(SchedulerClientType::kTest1, kGuid1,
                                 clock()->Now());
  expected_impression.impression_mapping = impression_mapping;
  expected_impression.custom_data = custom_data;
  test_case.expected.back().impressions.emplace_back(expected_impression);
  test_case.expected.back().last_shown_ts = clock()->Now();
  VerifyClientStates(test_case);
  EXPECT_EQ(*tracker()->GetImpression(SchedulerClientType::kTest1, kGuid1),
            expected_impression);
}

// Verifies that impression loaded from the database can be retrieved correctly.
TEST_F(ImpressionHistoryTrackerTest, GetImpressionLoadedFromDb) {
  TestCase test_case = CreateDefaultTestCase();
  Impression impression(SchedulerClientType::kTest1, kGuid1, clock()->Now());
  test_case.input.front().impressions.emplace_back(impression);
  CreateTracker(test_case);
  InitTrackerWithData(test_case);
  EXPECT_EQ(*tracker()->GetImpression(SchedulerClientType::kTest1, kGuid1),
            impression);
}

// If impression has been deleted, click should have no result.
TEST_F(ImpressionHistoryTrackerTest, ClickNoImpression) {
  TestCase test_case = CreateDefaultTestCase();
  CreateTracker(test_case);
  InitTrackerWithData(test_case);
  EXPECT_CALL(*store(), Update(_, _, _)).Times(0);
  UserActionData action_data(SchedulerClientType::kTest1,
                             UserActionType::kClick, kGuid1);
  tracker()->OnUserAction(action_data);
  VerifyClientStates(test_case);
}

// Verifies a consecutive dismiss will generate impression result.
TEST_F(ImpressionHistoryTrackerTest, ConsecutiveDismisses) {
  TestCase test_case = CreateDefaultTestCase();
  clock()->SetNow(kTimeStr);

  // Construct 3 dismisses in a row, which will generate neutral impression
  // result.
  auto dismiss_0 = CreateImpression(clock()->Now() - base::Days(1), "guid0",
                                    UserFeedback::kDismiss);
  auto dismiss_1 = CreateImpression(clock()->Now() - base::Minutes(30), "guid1",
                                    UserFeedback::kDismiss);
  auto dismiss_2 = CreateImpression(clock()->Now() - base::Minutes(15), "guid2",
                                    UserFeedback::kDismiss);
  test_case.input.front().impressions = {dismiss_0, dismiss_1, dismiss_2};
  test_case.expected.front().impressions = test_case.input.front().impressions;
  for (auto& impression : test_case.expected.front().impressions) {
    impression.feedback = UserFeedback::kDismiss;
    impression.impression = ImpressionResult::kNeutral;
    impression.integrated = true;
  }

  CreateTracker(test_case);
  InitTrackerWithData(test_case);
  EXPECT_CALL(*store(), Update(_, _, _));
  EXPECT_CALL(*delegate(), GetThrottleConfig(_, _))
      .Times(test_case.input.front().impressions.size())
      .WillRepeatedly(Invoke(
          [&](SchedulerClientType type,
              base::OnceCallback<void(std::unique_ptr<ThrottleConfig>)> cb) {
            std::move(cb).Run(nullptr);
          }));
  UserActionData action_data(SchedulerClientType::kTest1,
                             UserActionType::kDismiss, "guid2");
  tracker()->OnUserAction(action_data);
  VerifyClientStates(test_case);
}

// Verifies consecutive dismisses or timeout-ignored impressions will generate
// impression result with timeout configured.
TEST_F(ImpressionHistoryTrackerTest, ConsecutiveDismissesWithIgnoreTimeout) {
  TestCase test_case = CreateDefaultTestCase();
  clock()->SetNow(kTimeStr);

  // Config timeout duration and negative impression mapping.
  Impression::ImpressionResultMap impression_mapping;
  impression_mapping.emplace(UserFeedback::kDismiss,
                             ImpressionResult::kNegative);
  impression_mapping.emplace(UserFeedback::kIgnore,
                             ImpressionResult::kNegative);
  base::TimeDelta ignore_timeout_duration = base::Hours(12);

  // Construct 3 dismisses or timeout-ignored impressions in a row, which will
  // generate negative impression result.
  auto dismiss_0 = CreateImpression(
      clock()->Now() - base::Days(1), "guid0", UserFeedback::kNoFeedback,
      ignore_timeout_duration, impression_mapping);
  auto dismiss_1 = CreateImpression(
      clock()->Now() - base::Hours(16), "guid1", UserFeedback::kNoFeedback,
      ignore_timeout_duration, impression_mapping);
  auto dismiss_2 = CreateImpression(
      clock()->Now() - base::Minutes(15), "guid2", UserFeedback::kDismiss,
      ignore_timeout_duration, impression_mapping);
  test_case.input.front().impressions = {dismiss_0, dismiss_1, dismiss_2};
  test_case.expected.front().impressions = test_case.input.front().impressions;
  for (auto& impression : test_case.expected.front().impressions) {
    if (impression.guid != "guid2") {
      impression.feedback = UserFeedback::kIgnore;
    }
    impression.impression = ImpressionResult::kNegative;
    impression.integrated = true;
  }

  CreateTracker(test_case);
  InitTrackerWithData(test_case);
  EXPECT_CALL(*store(), Update(_, _, _));
  EXPECT_CALL(*delegate(), GetThrottleConfig(_, _))
      .Times(test_case.input.front().impressions.size())
      .WillRepeatedly(Invoke(
          [&](SchedulerClientType type,
              base::OnceCallback<void(std::unique_ptr<ThrottleConfig>)> cb) {
            std::move(cb).Run(nullptr);
          }));
  UserActionData action_data(SchedulerClientType::kTest1,
                             UserActionType::kDismiss, "guid2");
  tracker()->OnUserAction(action_data);
  VerifyClientStates(test_case);
}

// Defines the expected state of impression data after certain user action.
struct UserActionTestParam {
  ImpressionResult impression_result = ImpressionResult::kInvalid;
  UserFeedback user_feedback = UserFeedback::kNoFeedback;
  int current_max_daily_show = 0;
  std::optional<ActionButtonType> button_type;
  bool integrated = false;
  bool has_suppression = false;
  std::map<UserFeedback, ImpressionResult> impression_mapping;
};

class ImpressionHistoryTrackerUserActionTest
    : public ImpressionHistoryTrackerTest,
      public ::testing::WithParamInterface<UserActionTestParam> {
 public:
  ImpressionHistoryTrackerUserActionTest() = default;
  ImpressionHistoryTrackerUserActionTest(
      const ImpressionHistoryTrackerUserActionTest&) = delete;
  ImpressionHistoryTrackerUserActionTest& operator=(
      const ImpressionHistoryTrackerUserActionTest&) = delete;
  ~ImpressionHistoryTrackerUserActionTest() override = default;
};

const UserActionTestParam kUserActionTestParams[] = {
    // Suite 0: Click.
    {ImpressionResult::kPositive, UserFeedback::kClick, 3, std::nullopt,
     true /*integrated*/, false /*has_suppression*/},

    // Suite 1: Helpful button.
    {ImpressionResult::kPositive, UserFeedback::kHelpful, 3,
     ActionButtonType::kHelpful, true /*integrated*/,
     false /*has_suppression*/},

    // Suite 2: Unhelpful button.
    {ImpressionResult::kNegative, UserFeedback::kNotHelpful, 0,
     ActionButtonType::kUnhelpful, true /*integrated*/,
     true /*has_suppression*/},

    // Suite 3: One dismiss.
    {ImpressionResult::kInvalid, UserFeedback::kDismiss, 2, std::nullopt,
     false /*integrated*/, false /*has_suppression*/},

    // Suite 4: Click with negative impression result from impression mapping.
    {ImpressionResult::kNegative,
     UserFeedback::kClick,
     0,
     std::nullopt,
     true /*integrated*/,
     true /*has_suppression*/,
     {{UserFeedback::kClick,
       ImpressionResult::kNegative}} /*impression_mapping*/},

    // Suite 5: Click with negative impression result from impression mapping.
    {ImpressionResult::kNegative,
     UserFeedback::kClick,
     0,
     std::nullopt,
     true /*integrated*/,
     true /*has_suppression*/,
     {{UserFeedback::kClick,
       ImpressionResult::kNegative}} /*impression_mapping*/}};

// TODO(hesen): Add test for custom suppression duration from client.
// User actions like clicks should update the ClientState data accordingly.
TEST_P(ImpressionHistoryTrackerUserActionTest, UserAction) {
  clock()->SetNow(base::Time::UnixEpoch());
  TestCase test_case = CreateDefaultTestCase();
  Impression impression = CreateImpression(base::Time::Now(), kGuid1);
  DCHECK(!test_case.input.empty());
  impression.impression_mapping = GetParam().impression_mapping;
  test_case.input.front().impressions.emplace_back(impression);

  impression.impression = GetParam().impression_result;
  impression.integrated = GetParam().integrated;
  impression.feedback = GetParam().user_feedback;
  test_case.expected.front().current_max_daily_show =
      GetParam().current_max_daily_show;
  test_case.expected.front().impressions.emplace_back(impression);
  if (GetParam().has_suppression) {
    test_case.expected.front().suppression_info =
        SuppressionInfo(base::Time::UnixEpoch(), config().suppression_duration);
    test_case.expected.front().negative_events_count = 1;
    test_case.expected.front().last_negative_event_ts = base::Time::UnixEpoch();
  }
  CreateTracker(test_case);
  InitTrackerWithData(test_case);
  EXPECT_CALL(*store(), Update(_, _, _));
  if (GetParam().impression_result == ImpressionResult::kNegative ||
      GetParam().user_feedback == UserFeedback::kDismiss) {
    EXPECT_CALL(*delegate(), GetThrottleConfig(_, _))
        .WillOnce(Invoke(
            [&](SchedulerClientType type,
                base::OnceCallback<void(std::unique_ptr<ThrottleConfig>)> cb) {
              std::move(cb).Run(nullptr);
            }));
  }
  // Trigger user action.
  if (GetParam().user_feedback == UserFeedback::kClick) {
    UserActionData action_data(SchedulerClientType::kTest1,
                               UserActionType::kClick, kGuid1);
    tracker()->OnUserAction(action_data);
  } else if (GetParam().button_type.has_value()) {
    ButtonClickInfo button_click_info;
    button_click_info.button_id = kButtonId;
    button_click_info.type = GetParam().button_type.value();
    UserActionData action_data(SchedulerClientType::kTest1,
                               UserActionType::kButtonClick, kGuid1);
    action_data.button_click_info =
        std::make_optional(std::move(button_click_info));
    tracker()->OnUserAction(action_data);
  } else if (GetParam().user_feedback == UserFeedback::kDismiss) {
    UserActionData action_data(SchedulerClientType::kTest1,
                               UserActionType::kDismiss, kGuid1);
    tracker()->OnUserAction(action_data);
  }
  VerifyClientStates(test_case);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ImpressionHistoryTrackerUserActionTest,
                         testing::ValuesIn(kUserActionTestParams));

}  // namespace

}  // namespace notifications
