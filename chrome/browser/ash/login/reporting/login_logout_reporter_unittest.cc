// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/reporting/login_logout_reporter_test_delegate.h"

#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper_testing.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"

namespace ash {
namespace reporting {

using ::testing::_;

class LoginLogoutReporterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    auto user_manager = std::make_unique<FakeChromeUserManager>();
    user_manager_ = user_manager.get();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
  }

  void TearDown() override { chromeos::PowerManagerClient::Shutdown(); }

  std::unique_ptr<TestingProfile> CreateRegularProfile(
      base::StringPiece user_email) {
    AccountId account_id = AccountId::FromUserEmail(std::string(user_email));
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(account_id.GetUserEmail());
    auto profile = profile_builder.Build();
    user_manager_->AddUser(account_id);
    user_manager_->LoginUser(account_id, true);
    return profile;
  }

  std::unique_ptr<TestingProfile> CreatePublicAccountProfile() {
    AccountId account_id =
        AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
            "guest", policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION));
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(account_id.GetUserEmail());
    auto profile = profile_builder.Build();
    user_manager_->AddPublicAccountUser(account_id);
    user_manager_->LoginUser(account_id, true);
    return profile;
  }

  std::unique_ptr<TestingProfile> CreateKioskAppProfile() {
    AccountId account_id =
        AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
            "kiosk", policy::DeviceLocalAccount::TYPE_KIOSK_APP));
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(account_id.GetUserEmail());
    auto profile = profile_builder.Build();
    user_manager_->AddKioskAppUser(account_id);
    user_manager_->LoginUser(account_id, true);
    return profile;
  }

 private:
  FakeChromeUserManager* user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(LoginLogoutReporterTest, ReportAffiliatedLogin) {
  static constexpr char user_email[] = "affiliated@managed.org";
  auto mock_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new testing::StrictMock<::reporting::MockReportQueue>(),
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));

  LoginLogoutRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord(_, _, _))
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            EXPECT_TRUE(record.ParseFromArray(record_string.data(),
                                              record_string.size()));
            priority = event_priority;
          });

  auto reporter_helper =
      std::make_unique<::reporting::UserEventReporterHelperTesting>(
          /*reporting_enabled=*/true,
          /*should_report_user=*/true, std::move(mock_queue));

  auto reporter = LoginLogoutReporter::CreateForTest(
      std::move(reporter_helper),
      std::make_unique<LoginLogoutReporterTestDelegate>());
  auto profile = CreateRegularProfile(user_email);
  reporter->OnLogin(profile.get());

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::SECURITY));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_FALSE(record.is_guest_session());
  EXPECT_FALSE(record.has_logout_event());
  ASSERT_TRUE(record.has_affiliated_user());
  ASSERT_TRUE(record.affiliated_user().has_user_email());
  EXPECT_THAT(record.affiliated_user().user_email(), testing::Eq(user_email));
  ASSERT_TRUE(record.has_login_event());
  EXPECT_FALSE(record.login_event().has_failure());
}

TEST_F(LoginLogoutReporterTest, ReportUnaffiliatedLogin) {
  static constexpr char user_email[] = "unaffiliated@unmanaged.org";
  auto mock_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new testing::StrictMock<::reporting::MockReportQueue>(),
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));

  LoginLogoutRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord(_, _, _))
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            EXPECT_TRUE(record.ParseFromArray(record_string.data(),
                                              record_string.size()));
            priority = event_priority;
          });

  auto reporter_helper =
      std::make_unique<::reporting::UserEventReporterHelperTesting>(
          /*reporting_enabled=*/true,
          /*should_report_user=*/false, std::move(mock_queue));

  auto reporter = LoginLogoutReporter::CreateForTest(
      std::move(reporter_helper),
      std::make_unique<LoginLogoutReporterTestDelegate>());
  auto profile = CreateRegularProfile(user_email);
  reporter->OnLogin(profile.get());

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::SECURITY));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_FALSE(record.is_guest_session());
  EXPECT_FALSE(record.has_logout_event());
  EXPECT_FALSE(record.has_affiliated_user());
  ASSERT_TRUE(record.has_login_event());
  EXPECT_FALSE(record.login_event().has_failure());
}

TEST_F(LoginLogoutReporterTest, ReportManagedGuestLogin) {
  auto mock_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new testing::StrictMock<::reporting::MockReportQueue>(),
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));

  LoginLogoutRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord(_, _, _))
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            EXPECT_TRUE(record.ParseFromArray(record_string.data(),
                                              record_string.size()));
            priority = event_priority;
          });

  auto reporter_helper =
      std::make_unique<::reporting::UserEventReporterHelperTesting>(
          /*reporting_enabled=*/true,
          /*should_report_user=*/false, std::move(mock_queue));

  auto reporter = LoginLogoutReporter::CreateForTest(
      std::move(reporter_helper),
      std::make_unique<LoginLogoutReporterTestDelegate>());
  auto profile = CreatePublicAccountProfile();
  reporter->OnLogin(profile.get());

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::SECURITY));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_TRUE(record.is_guest_session());
  EXPECT_FALSE(record.has_logout_event());
  EXPECT_FALSE(record.has_affiliated_user());
  ASSERT_TRUE(record.has_login_event());
  EXPECT_FALSE(record.login_event().has_failure());
}

TEST_F(LoginLogoutReporterTest, KioskLogin) {
  auto mock_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new testing::StrictMock<::reporting::MockReportQueue>(),
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));

  EXPECT_CALL(*mock_queue, AddRecord).Times(0);

  auto reporter_helper =
      std::make_unique<::reporting::UserEventReporterHelperTesting>(
          /*reporting_enabled=*/true,
          /*should_report_user=*/false, std::move(mock_queue));

  auto reporter = LoginLogoutReporter::CreateForTest(
      std::move(reporter_helper),
      std::make_unique<LoginLogoutReporterTestDelegate>());
  auto profile = CreateKioskAppProfile();
  reporter->OnLogin(profile.get());
}

TEST_F(LoginLogoutReporterTest, ReportAffiliatedLogout) {
  static constexpr char user_email[] = "affiliated@managed.org";
  auto mock_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new testing::StrictMock<::reporting::MockReportQueue>(),
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));

  LoginLogoutRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord(_, _, _))
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            EXPECT_TRUE(record.ParseFromArray(record_string.data(),
                                              record_string.size()));
            priority = event_priority;
          });

  auto reporter_helper =
      std::make_unique<::reporting::UserEventReporterHelperTesting>(
          /*reporting_enabled=*/true,
          /*should_report_user=*/true, std::move(mock_queue));

  auto reporter = LoginLogoutReporter::CreateForTest(
      std::move(reporter_helper),
      std::make_unique<LoginLogoutReporterTestDelegate>());
  auto profile = CreateRegularProfile(user_email);
  reporter->OnSessionTerminationStarted(
      ProfileHelper::Get()->GetUserByProfile(profile.get()));

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::SECURITY));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_FALSE(record.is_guest_session());
  EXPECT_FALSE(record.has_login_event());
  EXPECT_TRUE(record.has_logout_event());
  ASSERT_TRUE(record.has_affiliated_user());
  ASSERT_TRUE(record.affiliated_user().has_user_email());
  EXPECT_THAT(record.affiliated_user().user_email(), testing::Eq(user_email));
}

TEST_F(LoginLogoutReporterTest, ReportUnaffiliatedLogout) {
  static constexpr char user_email[] = "unaffiliated@unmanaged.org";
  auto mock_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new testing::StrictMock<::reporting::MockReportQueue>(),
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));

  LoginLogoutRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord(_, _, _))
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            EXPECT_TRUE(record.ParseFromArray(record_string.data(),
                                              record_string.size()));
            priority = event_priority;
          });

  auto reporter_helper =
      std::make_unique<::reporting::UserEventReporterHelperTesting>(
          /*reporting_enabled=*/true,
          /*should_report_user=*/false, std::move(mock_queue));

  auto reporter = LoginLogoutReporter::CreateForTest(
      std::move(reporter_helper),
      std::make_unique<LoginLogoutReporterTestDelegate>());
  auto profile = CreateRegularProfile(user_email);
  reporter->OnSessionTerminationStarted(
      ProfileHelper::Get()->GetUserByProfile(profile.get()));

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::SECURITY));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_FALSE(record.is_guest_session());
  EXPECT_FALSE(record.has_login_event());
  EXPECT_TRUE(record.has_logout_event());
  EXPECT_FALSE(record.has_affiliated_user());
}

TEST_F(LoginLogoutReporterTest, ReportManagedGuestLogout) {
  auto mock_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new testing::StrictMock<::reporting::MockReportQueue>(),
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));

  LoginLogoutRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord(_, _, _))
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            EXPECT_TRUE(record.ParseFromArray(record_string.data(),
                                              record_string.size()));
            priority = event_priority;
          });

  auto reporter_helper =
      std::make_unique<::reporting::UserEventReporterHelperTesting>(
          /*reporting_enabled=*/true,
          /*should_report_user=*/true, std::move(mock_queue));

  auto reporter = LoginLogoutReporter::CreateForTest(
      std::move(reporter_helper),
      std::make_unique<LoginLogoutReporterTestDelegate>());
  auto profile = CreatePublicAccountProfile();
  reporter->OnSessionTerminationStarted(
      ProfileHelper::Get()->GetUserByProfile(profile.get()));

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::SECURITY));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_TRUE(record.is_guest_session());
  EXPECT_FALSE(record.has_login_event());
  EXPECT_TRUE(record.has_logout_event());
  EXPECT_FALSE(record.has_affiliated_user());
}

TEST_F(LoginLogoutReporterTest, KioskLogout) {
  auto mock_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new testing::StrictMock<::reporting::MockReportQueue>(),
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));

  EXPECT_CALL(*mock_queue, AddRecord).Times(0);

  auto reporter_helper =
      std::make_unique<::reporting::UserEventReporterHelperTesting>(
          /*reporting_enabled=*/true,
          /*should_report_user=*/false, std::move(mock_queue));

  auto reporter = LoginLogoutReporter::CreateForTest(
      std::move(reporter_helper),
      std::make_unique<LoginLogoutReporterTestDelegate>());
  auto profile = CreateKioskAppProfile();
  reporter->OnSessionTerminationStarted(
      ProfileHelper::Get()->GetUserByProfile(profile.get()));
}

TEST_F(LoginLogoutReporterTest, ReportAffiliatedLoginFailure) {
  static constexpr char user_email[] = "affiliated@managed.org";
  auto mock_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new testing::StrictMock<::reporting::MockReportQueue>(),
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));

  LoginLogoutRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord(_, _, _))
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            EXPECT_TRUE(record.ParseFromArray(record_string.data(),
                                              record_string.size()));
            priority = event_priority;
          });

  auto reporter_helper =
      std::make_unique<::reporting::UserEventReporterHelperTesting>(
          /*reporting_enabled=*/true,
          /*should_report_user=*/true, std::move(mock_queue));

  auto reporter = LoginLogoutReporter::CreateForTest(
      std::move(reporter_helper),
      std::make_unique<LoginLogoutReporterTestDelegate>(
          AccountId::FromUserEmail(std::string(user_email))));
  reporter->OnLoginFailure(AuthFailure(AuthFailure::OWNER_REQUIRED));

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::SECURITY));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_FALSE(record.is_guest_session());
  EXPECT_FALSE(record.has_logout_event());
  ASSERT_TRUE(record.has_affiliated_user());
  ASSERT_TRUE(record.affiliated_user().has_user_email());
  EXPECT_THAT(record.affiliated_user().user_email(), testing::Eq(user_email));
  ASSERT_TRUE(record.has_login_event());
  ASSERT_TRUE(record.login_event().has_failure());
  ASSERT_THAT(record.login_event().failure().reason(),
              testing::Eq(LoginFailureReason::OWNER_REQUIRED));
}

TEST_F(LoginLogoutReporterTest, ReportAffiliatedLoginAuthenticationFailure) {
  static constexpr char user_email[] = "affiliated@managed.org";
  auto mock_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new testing::StrictMock<::reporting::MockReportQueue>(),
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));

  LoginLogoutRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord(_, _, _))
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            EXPECT_TRUE(record.ParseFromArray(record_string.data(),
                                              record_string.size()));
            priority = event_priority;
          });

  auto reporter_helper =
      std::make_unique<::reporting::UserEventReporterHelperTesting>(
          /*reporting_enabled=*/true,
          /*should_report_user=*/true, std::move(mock_queue));

  auto reporter = LoginLogoutReporter::CreateForTest(
      std::move(reporter_helper),
      std::make_unique<LoginLogoutReporterTestDelegate>(
          AccountId::FromUserEmail(std::string(user_email))));
  reporter->OnLoginFailure(
      AuthFailure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME));

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::SECURITY));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_FALSE(record.is_guest_session());
  EXPECT_FALSE(record.has_logout_event());
  ASSERT_TRUE(record.has_affiliated_user());
  ASSERT_TRUE(record.affiliated_user().has_user_email());
  EXPECT_THAT(record.affiliated_user().user_email(), testing::Eq(user_email));
  ASSERT_TRUE(record.has_login_event());
  ASSERT_TRUE(record.login_event().has_failure());
  ASSERT_THAT(record.login_event().failure().reason(),
              testing::Eq(LoginFailureReason::AUTHENTICATION_ERROR));
}

TEST_F(LoginLogoutReporterTest, ReportUnaffiliatedLoginFailure) {
  static constexpr char user_email[] = "unaffiliated@unmanaged.org";
  auto mock_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new testing::StrictMock<::reporting::MockReportQueue>(),
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));

  LoginLogoutRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord(_, _, _))
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            EXPECT_TRUE(record.ParseFromArray(record_string.data(),
                                              record_string.size()));
            priority = event_priority;
          });

  auto reporter_helper =
      std::make_unique<::reporting::UserEventReporterHelperTesting>(
          /*reporting_enabled=*/true,
          /*should_report_user=*/false, std::move(mock_queue));

  auto reporter = LoginLogoutReporter::CreateForTest(
      std::move(reporter_helper),
      std::make_unique<LoginLogoutReporterTestDelegate>(
          AccountId::FromUserEmail(std::string(user_email))));
  reporter->OnLoginFailure(AuthFailure(AuthFailure::TPM_ERROR));

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::SECURITY));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_FALSE(record.is_guest_session());
  EXPECT_FALSE(record.has_logout_event());
  EXPECT_FALSE(record.has_affiliated_user());
  ASSERT_TRUE(record.has_login_event());
  ASSERT_TRUE(record.login_event().has_failure());
  ASSERT_THAT(record.login_event().failure().reason(),
              testing::Eq(LoginFailureReason::TPM_ERROR));
}

TEST_F(LoginLogoutReporterTest, ReportManagedGuestLoginFailure) {
  auto mock_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new testing::StrictMock<::reporting::MockReportQueue>(),
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));

  LoginLogoutRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord(_, _, _))
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            EXPECT_TRUE(record.ParseFromArray(record_string.data(),
                                              record_string.size()));
            priority = event_priority;
          });

  auto reporter_helper =
      std::make_unique<::reporting::UserEventReporterHelperTesting>(
          /*reporting_enabled=*/true,
          /*should_report_user=*/false, std::move(mock_queue));

  auto reporter = LoginLogoutReporter::CreateForTest(
      std::move(reporter_helper),
      std::make_unique<LoginLogoutReporterTestDelegate>(
          AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
              "guest", policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION))));
  reporter->OnLoginFailure(AuthFailure(AuthFailure::COULD_NOT_MOUNT_TMPFS));

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::SECURITY));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_TRUE(record.is_guest_session());
  EXPECT_FALSE(record.has_logout_event());
  EXPECT_FALSE(record.has_affiliated_user());
  ASSERT_TRUE(record.has_login_event());
  ASSERT_TRUE(record.login_event().has_failure());
  ASSERT_THAT(record.login_event().failure().reason(),
              testing::Eq(LoginFailureReason::COULD_NOT_MOUNT_TMPFS));
}

TEST_F(LoginLogoutReporterTest, ShouldNotReportEvent) {
  static constexpr char user_email[] = "affiliated@managed.org";
  auto mock_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new testing::StrictMock<::reporting::MockReportQueue>(),
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));

  EXPECT_CALL(*mock_queue, AddRecord).Times(0);

  auto reporter_helper =
      std::make_unique<::reporting::UserEventReporterHelperTesting>(
          /*reporting_enabled=*/false,
          /*should_report_user=*/true, std::move(mock_queue));

  auto reporter = LoginLogoutReporter::CreateForTest(
      std::move(reporter_helper),
      std::make_unique<LoginLogoutReporterTestDelegate>());
  auto profile = CreateRegularProfile(user_email);
  reporter->OnLogin(profile.get());
}

}  // namespace reporting
}  // namespace ash
