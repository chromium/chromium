// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/reporting/lock_unlock_reporter.h"

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper_testing.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/lock_unlock_event.pb.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"

using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::StrEq;

namespace ash {
namespace reporting {

constexpr char kFakeEmail[] = "user@managed.com";

struct LockUnlockReporterTestCase {
  session_manager::UnlockType unlock_type;
  UnlockType expected_unlock_type;
  bool success;
};

class LockUnlockTestHelper {
 public:
  LockUnlockTestHelper() = default;

  LockUnlockTestHelper(const LockUnlockTestHelper&) = delete;
  LockUnlockTestHelper& operator=(const LockUnlockTestHelper&) = delete;

  ~LockUnlockTestHelper() = default;

  void Init() {
    chromeos::PowerManagerClient::InitializeFake();
    SessionManagerClient::InitializeFake();
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
  }

  void Shutdown() { chromeos::PowerManagerClient::Shutdown(); }

  session_manager::SessionManager* session_manager() {
    return &session_manager_;
  }

  std::unique_ptr<TestingProfile> CreateRegularUserProfile() {
    AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
    auto* const user = fake_user_manager_->AddUser(account_id);
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(user->GetAccountId().GetUserEmail());
    auto profile = profile_builder.Build();
    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                            profile.get());
    fake_user_manager_->LoginUser(user->GetAccountId(), true);
    return profile;
  }

  std::unique_ptr<::reporting::UserEventReporterHelperTesting>
  GetReporterHelper(bool reporting_enabled,
                    bool should_report_user,
                    ::reporting::Status status = ::reporting::Status()) {
    record_.Clear();
    report_count_ = 0;
    auto mock_queue = std::unique_ptr<::reporting::MockReportQueue,
                                      base::OnTaskRunnerDeleter>(
        new ::reporting::MockReportQueue(),
        base::OnTaskRunnerDeleter(
            base::SequencedTaskRunner::GetCurrentDefault()));

    ON_CALL(*mock_queue, AddRecord(_, ::reporting::Priority::SECURITY, _))
        .WillByDefault(
            [this, status](std::string_view record_string,
                           ::reporting::Priority event_priority,
                           ::reporting::ReportQueue::EnqueueCallback cb) {
              ++report_count_;
              EXPECT_TRUE(record_.ParseFromArray(record_string.data(),
                                                 record_string.size()));
              std::move(cb).Run(status);
            });

    auto reporter_helper =
        std::make_unique<::reporting::UserEventReporterHelperTesting>(
            reporting_enabled, should_report_user, /*is_kiosk_user=*/false,
            std::move(mock_queue));
    return reporter_helper;
  }

  LockUnlockRecord GetRecord() { return record_; }

  int GetReportCount() { return report_count_; }

 private:
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  content::BrowserTaskEnvironment task_environment_;

  LockUnlockRecord record_;
  int report_count_ = 0;
  session_manager::SessionManager session_manager_;
};

class LockUnlockReporterTest
    : public ::testing::TestWithParam<LockUnlockReporterTestCase> {
 protected:
  LockUnlockReporterTest() {}

  void SetUp() override { test_helper_.Init(); }

  void TearDown() override { test_helper_.Shutdown(); }

  LockUnlockTestHelper test_helper_;
};

// When the device is locked/unlocked by an unaffiliated user a unique user ID
// for this device should be reported.
TEST_F(LockUnlockReporterTest, ReportUnaffiliatedUserId) {
  policy::ManagedSessionService managed_session_service;
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true,
      /*should_report_user=*/false);

  auto reporter = LockUnlockReporter::CreateForTest(std::move(reporter_helper),
                                                    &managed_session_service);

  auto profile = test_helper_.CreateRegularUserProfile();
  auto* const user = ProfileHelper::Get()->GetUserByProfile(profile.get());
  managed_session_service.OnUserProfileLoaded(user->GetAccountId());

  test_helper_.session_manager()->SetSessionState(
      session_manager::SessionState::LOCKED);
  managed_session_service.OnSessionStateChanged();

  const LockUnlockRecord& record = test_helper_.GetRecord();
  ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_FALSE(record.has_unlock_event());
  EXPECT_FALSE(record.has_affiliated_user());
  EXPECT_TRUE(record.has_unaffiliated_user());
  EXPECT_TRUE(record.unaffiliated_user().has_user_id());
  EXPECT_THAT(record.unaffiliated_user().user_id(), Not(IsEmpty()));
  EXPECT_TRUE(record.has_lock_event());
}

TEST_F(LockUnlockReporterTest, ReportLockPolicyEnabled) {
  policy::ManagedSessionService managed_session_service;
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true,
      /*should_report_user=*/true);

  auto reporter = LockUnlockReporter::CreateForTest(std::move(reporter_helper),
                                                    &managed_session_service);

  auto profile = test_helper_.CreateRegularUserProfile();
  auto* const user = ProfileHelper::Get()->GetUserByProfile(profile.get());
  managed_session_service.OnUserProfileLoaded(user->GetAccountId());

  test_helper_.session_manager()->SetSessionState(
      session_manager::SessionState::LOCKED);
  managed_session_service.OnSessionStateChanged();

  const LockUnlockRecord& record = test_helper_.GetRecord();
  ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_FALSE(record.has_unlock_event());
  ASSERT_TRUE(record.has_affiliated_user());
  ASSERT_TRUE(record.affiliated_user().has_user_email());
  EXPECT_THAT(record.affiliated_user().user_email(), StrEq(kFakeEmail));
  EXPECT_TRUE(record.has_lock_event());
}

TEST_F(LockUnlockReporterTest, ReportLockPolicyDisabled) {
  policy::ManagedSessionService managed_session_service;
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/false,
      /*should_report_user=*/true);

  auto reporter = LockUnlockReporter::CreateForTest(std::move(reporter_helper),
                                                    &managed_session_service);

  auto profile = test_helper_.CreateRegularUserProfile();
  auto* const user = ProfileHelper::Get()->GetUserByProfile(profile.get());
  managed_session_service.OnUserProfileLoaded(user->GetAccountId());

  test_helper_.session_manager()->SetSessionState(
      session_manager::SessionState::LOCKED);
  managed_session_service.OnSessionStateChanged();

  ASSERT_THAT(test_helper_.GetReportCount(), Eq(0));
}

TEST_P(LockUnlockReporterTest, ReportUnlockPolicyDisabled) {
  const auto test_case = GetParam();
  policy::ManagedSessionService managed_session_service;
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/false,
      /*should_report_user=*/true);

  auto reporter = LockUnlockReporter::CreateForTest(std::move(reporter_helper),
                                                    &managed_session_service);

  auto profile = test_helper_.CreateRegularUserProfile();
  auto* const user = ProfileHelper::Get()->GetUserByProfile(profile.get());
  managed_session_service.OnUserProfileLoaded(user->GetAccountId());

  managed_session_service.OnUnlockScreenAttempt(test_case.success,
                                                test_case.unlock_type);

  ASSERT_THAT(test_helper_.GetReportCount(), Eq(0));
}

TEST_P(LockUnlockReporterTest, ReportUnlockPolicyEnabled) {
  const auto test_case = GetParam();
  policy::ManagedSessionService managed_session_service;
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true,
      /*should_report_user=*/true);

  auto reporter = LockUnlockReporter::CreateForTest(std::move(reporter_helper),
                                                    &managed_session_service);

  auto profile = test_helper_.CreateRegularUserProfile();
  auto* const user = ProfileHelper::Get()->GetUserByProfile(profile.get());
  managed_session_service.OnUserProfileLoaded(user->GetAccountId());

  managed_session_service.OnUnlockScreenAttempt(test_case.success,
                                                test_case.unlock_type);

  const LockUnlockRecord& record = test_helper_.GetRecord();
  ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_FALSE(record.has_lock_event());

  ASSERT_TRUE(record.has_affiliated_user());
  ASSERT_TRUE(record.affiliated_user().has_user_email());
  EXPECT_THAT(record.affiliated_user().user_email(), StrEq(kFakeEmail));
  EXPECT_TRUE(record.has_unlock_event());
  EXPECT_TRUE(record.unlock_event().has_unlock_type());
  EXPECT_THAT(record.unlock_event().unlock_type(),
              Eq(test_case.expected_unlock_type));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    LockUnlockReporterTest,
    ::testing::ValuesIn<LockUnlockReporterTestCase>(
        {{session_manager::UnlockType::PASSWORD, UnlockType::PASSWORD, true},
         {session_manager::UnlockType::PASSWORD, UnlockType::PASSWORD, false},
         {session_manager::UnlockType::PIN, UnlockType::PIN, true},
         {session_manager::UnlockType::PIN, UnlockType::PIN, false},
         {session_manager::UnlockType::FINGERPRINT, UnlockType::FINGERPRINT,
          true},
         {session_manager::UnlockType::FINGERPRINT, UnlockType::FINGERPRINT,
          false},
         {session_manager::UnlockType::CHALLENGE_RESPONSE,
          UnlockType::CHALLENGE_RESPONSE, true},
         {session_manager::UnlockType::CHALLENGE_RESPONSE,
          UnlockType::CHALLENGE_RESPONSE, false}}));

}  // namespace reporting
}  // namespace ash
