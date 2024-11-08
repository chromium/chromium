// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/user_session_activity/user_session_activity_reporter_delegate.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper_testing.h"
#include "chrome/browser/ash/power/ml/idle_event_notifier.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/user_session_activity.pb.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;

namespace reporting {

namespace {

constexpr std::string_view kUserEmail = "user@example.com";

const AccountId kAccountId = AccountId::FromUserEmail(kUserEmail);

std::unique_ptr<ash::power::ml::IdleEventNotifier> GetIdleEventNotifier() {
  mojo::PendingRemote<viz::mojom::VideoDetectorObserver> observer;
  return std::make_unique<ash::power::ml::IdleEventNotifier>(
      chromeos::PowerManagerClient::Get(), ui::UserActivityDetector::Get(),
      observer.InitWithNewPipeAndPassReceiver());
}

}  // namespace

class UserSessionActivityReporterDelegateTest : public ::testing::Test {
 protected:
  std::unique_ptr<::reporting::UserEventReporterHelperTesting>
  GetReporterHelper(
      bool reporting_enabled,
      ::reporting::Status status = ::reporting::Status::StatusOK()) {
    auto mock_queue = std::unique_ptr<::reporting::MockReportQueue,
                                      base::OnTaskRunnerDeleter>(
        new ::reporting::MockReportQueue(),
        base::OnTaskRunnerDeleter(
            base::SequencedTaskRunner::GetCurrentDefault()));

    ON_CALL(*mock_queue, AddRecord(_, _, _))
        .WillByDefault(
            [this, status](std::string_view record_string,
                           ::reporting::Priority event_priority,
                           ::reporting::ReportQueue::EnqueueCallback cb) {
              UserSessionActivityRecord record;
              EXPECT_TRUE(record.ParseFromArray(record_string.data(),
                                                record_string.size()));
              records_.AddValue(std::move(record));
              std::move(cb).Run(status);
            });

    auto reporter_helper =
        std::make_unique<::reporting::UserEventReporterHelperTesting>(
            reporting_enabled, /*should_report_user=*/true,
            /*is_kiosk_user=*/false, std::move(mock_queue));
    return reporter_helper;
  }

  UserSessionActivityRecord GetReportedRecord() { return records_.Take(); }

  bool RecordWasReported() const { return !records_.IsEmpty(); }

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();

    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

    // Create delegate with reporting policy enabled via the reporter helper.
    delegate_ = std::make_unique<UserSessionActivityReporterDelegate>(
        GetReporterHelper(/*reporting_enabled=*/true), GetIdleEventNotifier());
  }

  void TearDown() override {
    delegate_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<UserSessionActivityReporterDelegate> delegate_;

  UserSessionActivityRecord record_;
  base::test::RepeatingTestFuture<UserSessionActivityRecord> records_;

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
};

TEST_F(UserSessionActivityReporterDelegateTest,
       ReportSessionActivity_PolicyDisabled) {
  delegate_ = std::make_unique<UserSessionActivityReporterDelegate>(
      GetReporterHelper(/*reporting_enabled=*/false), GetIdleEventNotifier());

  delegate_->ReportSessionActivity();

  EXPECT_FALSE(RecordWasReported());
}

TEST_F(UserSessionActivityReporterDelegateTest,
       ReportSessionActivity_PolicyEnabled) {
  delegate_->ReportSessionActivity();

  EXPECT_TRUE(RecordWasReported());

  // Verify record is empty since we didn't add events or active/idle states.
  const auto record = GetReportedRecord();
  EXPECT_THAT(record.active_idle_states().size(), Eq(0));
  EXPECT_FALSE(record.has_affiliated_user());
  EXPECT_FALSE(record.has_session_end());
  EXPECT_FALSE(record.has_session_start());
  EXPECT_FALSE(record.has_unaffiliated_user());
}

TEST_F(UserSessionActivityReporterDelegateTest,
       AddActiveIdleState_UserIsActive) {
  const auto* const affiliated_user =
      fake_user_manager_->AddUserWithAffiliation(kAccountId,
                                                 /*is_affiliated=*/true);

  delegate_->AddActiveIdleState(/*user_is_active=*/true, affiliated_user);

  delegate_->ReportSessionActivity();

  ASSERT_TRUE(RecordWasReported());
  const auto record = GetReportedRecord();
  ASSERT_THAT(record.active_idle_states(), SizeIs(1));
  EXPECT_THAT(record.active_idle_states(0).state(),
              Eq(ActiveIdleState::ACTIVE));
  EXPECT_THAT(record.active_idle_states(0).timestamp_micro(), Gt(0));
  EXPECT_TRUE(record.has_affiliated_user());
  EXPECT_THAT(record.affiliated_user().user_email(), Eq(kUserEmail));
}
TEST_F(UserSessionActivityReporterDelegateTest, AddActiveIdleState_UserIsIdle) {
  const auto* const affiliated_user =
      fake_user_manager_->AddUserWithAffiliation(kAccountId,
                                                 /*is_affiliated=*/true);

  delegate_->AddActiveIdleState(/*user_is_active=*/false,
                                /*user=*/affiliated_user);

  delegate_->ReportSessionActivity();

  ASSERT_TRUE(RecordWasReported());
  const auto record = GetReportedRecord();
  ASSERT_THAT(record.active_idle_states(), SizeIs(1));
  EXPECT_THAT(record.active_idle_states(0).state(), Eq(ActiveIdleState::IDLE));
  EXPECT_THAT(record.active_idle_states(0).timestamp_micro(), Gt(0));
  EXPECT_TRUE(record.has_affiliated_user());
  EXPECT_THAT(record.affiliated_user().user_email(), Eq(kUserEmail));
}

TEST_F(UserSessionActivityReporterDelegateTest, SetSessionStartEvent_Unlock) {
  const auto* const affiliated_user =
      fake_user_manager_->AddUserWithAffiliation(kAccountId,
                                                 /*is_affiliated=*/true);

  delegate_->SetSessionStartEvent(
      SessionStartEvent::Reason::SessionStartEvent_Reason_UNLOCK,
      affiliated_user);
  delegate_->ReportSessionActivity();

  ASSERT_TRUE(RecordWasReported());
  const auto record = GetReportedRecord();
  EXPECT_TRUE(record.has_session_start());
  EXPECT_THAT(record.session_start().reason(),
              Eq(SessionStartEvent_Reason_UNLOCK));
  EXPECT_THAT(record.session_start().timestamp_micro(), Gt(0));
  EXPECT_TRUE(record.has_affiliated_user());
  EXPECT_THAT(record.affiliated_user().user_email(), Eq(kUserEmail));
}

TEST_F(UserSessionActivityReporterDelegateTest, SetSessionStartEvent_Login) {
  const auto* const affiliated_user =
      fake_user_manager_->AddUserWithAffiliation(kAccountId,
                                                 /*is_affiliated=*/true);

  delegate_->SetSessionStartEvent(
      SessionStartEvent::Reason::SessionStartEvent_Reason_LOGIN,
      affiliated_user);
  delegate_->ReportSessionActivity();

  ASSERT_TRUE(RecordWasReported());
  const auto record = GetReportedRecord();
  EXPECT_TRUE(record.has_session_start());
  EXPECT_THAT(record.session_start().reason(),
              Eq(SessionStartEvent_Reason_LOGIN));
  EXPECT_THAT(record.session_start().timestamp_micro(), Gt(0));
  EXPECT_TRUE(record.has_affiliated_user());
  EXPECT_THAT(record.affiliated_user().user_email(), Eq(kUserEmail));
}

TEST_F(UserSessionActivityReporterDelegateTest, SetSessionEndEvent_Logout) {
  const auto* const affiliated_user =
      fake_user_manager_->AddUserWithAffiliation(kAccountId,
                                                 /*is_affiliated=*/true);

  delegate_->SetSessionEndEvent(
      SessionEndEvent::Reason::SessionEndEvent_Reason_LOGOUT, affiliated_user);
  delegate_->ReportSessionActivity();

  ASSERT_TRUE(RecordWasReported());
  const auto record = GetReportedRecord();
  EXPECT_TRUE(record.has_session_end());
  EXPECT_THAT(record.session_end().reason(), Eq(SessionEndEvent_Reason_LOGOUT));
  EXPECT_THAT(record.session_end().timestamp_micro(), Gt(0));
  EXPECT_TRUE(record.has_affiliated_user());
  EXPECT_THAT(record.affiliated_user().user_email(), Eq(kUserEmail));
}

TEST_F(UserSessionActivityReporterDelegateTest, SetSessionEndEvent_Lock) {
  const auto* const affiliated_user =
      fake_user_manager_->AddUserWithAffiliation(kAccountId,
                                                 /*is_affiliated=*/true);

  delegate_->SetSessionEndEvent(
      SessionEndEvent::Reason::SessionEndEvent_Reason_LOCK, affiliated_user);

  delegate_->ReportSessionActivity();

  ASSERT_TRUE(RecordWasReported());
  const auto record = GetReportedRecord();
  EXPECT_TRUE(record.has_session_end());
  EXPECT_THAT(record.session_end().reason(), Eq(SessionEndEvent_Reason_LOCK));
  EXPECT_THAT(record.session_end().timestamp_micro(), Gt(0));
  EXPECT_TRUE(record.has_affiliated_user());
  EXPECT_THAT(record.affiliated_user().user_email(), Eq(kUserEmail));
}

TEST_F(UserSessionActivityReporterDelegateTest, SetUser_Affiliated) {
  const auto* const affiliated_user =
      fake_user_manager_->AddUserWithAffiliation(kAccountId,
                                                 /*is_affiliated=*/true);

  UserSessionActivityRecord record;
  delegate_->SetUser(&record, affiliated_user);

  EXPECT_TRUE(record.has_affiliated_user());
  EXPECT_FALSE(record.has_unaffiliated_user());
  EXPECT_THAT(record.affiliated_user().user_email(), Eq(kUserEmail));
}

TEST_F(UserSessionActivityReporterDelegateTest, SetUser_Unaffiliated) {
  const auto* const unaffiliated_user =
      fake_user_manager_->AddUserWithAffiliation(kAccountId, false);

  UserSessionActivityRecord record;
  delegate_->SetUser(&record, unaffiliated_user);

  EXPECT_FALSE(record.has_affiliated_user());
  EXPECT_TRUE(record.has_unaffiliated_user());
  EXPECT_THAT(record.unaffiliated_user().user_id(), Not(IsEmpty()));
}

TEST_F(UserSessionActivityReporterDelegateTest, SetUser_ManagedGuest) {
  const auto* const managed_guest_user =
      fake_user_manager_->AddPublicAccountUser(
          AccountId::FromUserEmail(kUserEmail));

  UserSessionActivityRecord record;
  delegate_->SetUser(&record, managed_guest_user);

  EXPECT_FALSE(record.has_affiliated_user());
  EXPECT_FALSE(record.has_unaffiliated_user());
}

TEST_F(UserSessionActivityReporterDelegateTest, IsUserActive_RecentActivity) {
  auto delegate = std::make_unique<UserSessionActivityReporterDelegate>(
      GetReporterHelper(/*reporting_enabled=*/true), GetIdleEventNotifier());

  ash::power::ml::IdleEventNotifier::ActivityData activity_data;

  // Calculate local time of day because that's how
  // `activity_data.last_activity_time_of_day` is calculated.
  const base::TimeDelta local_time_of_day_now =
      base::Time::Now() - base::Time::Now().LocalMidnight();

  // Set user as active by setting last active time to be 1 second before the
  // idle timeout.
  activity_data.last_activity_time_of_day =
      local_time_of_day_now -
      (kActiveIdleStateCollectionFrequency - base::Seconds(1));

  EXPECT_TRUE(delegate->IsUserActive(activity_data));
}

TEST_F(UserSessionActivityReporterDelegateTest, IsUserActive_NoRecentActivity) {
  auto delegate = std::make_unique<UserSessionActivityReporterDelegate>(
      GetReporterHelper(/*reporting_enabled=*/true), GetIdleEventNotifier());

  ash::power::ml::IdleEventNotifier::ActivityData activity_data;

  // User is active because a video is playing.
  activity_data.is_video_playing = true;

  EXPECT_TRUE(delegate->IsUserActive(activity_data));
}

TEST_F(UserSessionActivityReporterDelegateTest, IsUserActive_VideoPlaying) {
  auto delegate = std::make_unique<UserSessionActivityReporterDelegate>(
      GetReporterHelper(/*reporting_enabled=*/true), GetIdleEventNotifier());

  ash::power::ml::IdleEventNotifier::ActivityData activity_data;

  // User is active because a video is playing.
  activity_data.is_video_playing = true;

  EXPECT_TRUE(delegate->IsUserActive(activity_data));
}
}  // namespace reporting
