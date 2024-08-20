// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/ash/login/reporting/login_logout_reporter_test_delegate.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper_testing.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"

using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::StrEq;

namespace ash {
namespace reporting {

constexpr char user_email[] = "user@managed.com";

class LoginLogoutTestHelper {
 public:
  LoginLogoutTestHelper() = default;

  LoginLogoutTestHelper(const LoginLogoutTestHelper&) = delete;
  LoginLogoutTestHelper& operator=(const LoginLogoutTestHelper&) = delete;

  ~LoginLogoutTestHelper() = default;

  void Init() {
    chromeos::PowerManagerClient::InitializeFake();
    session_termination_manager_ =
        std::make_unique<SessionTerminationManager>();
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
  }

  void Shutdown() { chromeos::PowerManagerClient::Shutdown(); }

  std::unique_ptr<TestingProfile> CreateProfile(user_manager::User* user) {
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(user->GetAccountId().GetUserEmail());
    auto profile = profile_builder.Build();
    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                            profile.get());
    fake_user_manager_->LoginUser(user->GetAccountId(), true);
    return profile;
  }

  std::unique_ptr<TestingProfile> CreateRegularUserProfile() {
    AccountId account_id = AccountId::FromUserEmail(user_email);
    auto* const user = fake_user_manager_->AddUser(account_id);
    return CreateProfile(user);
  }

  std::unique_ptr<TestingProfile> CreatePublicAccountProfile() {
    AccountId account_id =
        AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
            "managed_guest", policy::DeviceLocalAccountType::kPublicSession));
    auto* const user = fake_user_manager_->AddPublicAccountUser(account_id);
    return CreateProfile(user);
  }

  std::unique_ptr<TestingProfile> CreateGuestProfile() {
    auto* const user = fake_user_manager_->AddGuestUser();
    return CreateProfile(user);
  }

  std::unique_ptr<TestingProfile> CreateKioskAppProfile() {
    AccountId account_id =
        AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
            "kiosk", policy::DeviceLocalAccountType::kKioskApp));
    auto* const user = fake_user_manager_->AddKioskAppUser(account_id);
    return CreateProfile(user);
  }

  std::unique_ptr<TestingProfile> CreateWebKioskAppProfile() {
    AccountId account_id =
        AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
            "webkiosk", policy::DeviceLocalAccountType::kWebKioskApp));
    auto* const user = fake_user_manager_->AddWebKioskAppUser(account_id);
    return CreateProfile(user);
  }

  std::unique_ptr<TestingProfile> CreateProfileByType(
      user_manager::UserType user_type) {
    switch (user_type) {
      case user_manager::UserType::kRegular:
        return CreateRegularUserProfile();
      case user_manager::UserType::kGuest:
        return CreateGuestProfile();
      case user_manager::UserType::kPublicAccount:
        return CreatePublicAccountProfile();
      case user_manager::UserType::kKioskApp:
        return CreateKioskAppProfile();
      case user_manager::UserType::kWebKioskApp:
        return CreateWebKioskAppProfile();
      default:
        NOTREACHED_IN_MIGRATION();
        return nullptr;
    }
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

  LoginLogoutRecord GetRecord() { return record_; }

  int GetReportCount() { return report_count_; }

 private:
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<SessionTerminationManager> session_termination_manager_;

  LoginLogoutRecord record_;
  int report_count_ = 0;
};

struct LoginLogoutReporterTestCase {
  user_manager::UserType user_type;
  LoginLogoutSessionType expected_session_type;
};

class LoginLogoutReporterTest
    : public ::testing::TestWithParam<LoginLogoutReporterTestCase> {
 protected:
  LoginLogoutReporterTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { test_helper_.Init(); }

  void TearDown() override { test_helper_.Shutdown(); }

  LoginLogoutTestHelper test_helper_;

  ScopedTestingLocalState local_state_;
};

TEST_F(LoginLogoutReporterTest, ReportAffiliatedLogin) {
  policy::ManagedSessionService managed_session_service;
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true,
      /*should_report_user=*/true);

  auto reporter = LoginLogoutReporter::CreateForTest(
      std::move(reporter_helper),
      std::make_unique<LoginLogoutReporterTestDelegate>(),
      &managed_session_service);

  auto profile = test_helper_.CreateRegularUserProfile();
  auto* const user = ProfileHelper::Get()->GetUserByProfile(profile.get());
  managed_session_service.OnUserProfileLoaded(user->GetAccountId());
  const LoginLogoutRecord& record = test_helper_.GetRecord();

  ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_FALSE(record.is_guest_session());
  EXPECT_FALSE(record.has_logout_event());
  ASSERT_TRUE(record.has_affiliated_user());
  ASSERT_TRUE(record.affiliated_user().has_user_email());
  EXPECT_THAT(record.affiliated_user().user_email(), StrEq(user_email));
  ASSERT_TRUE(record.has_session_type());
  EXPECT_THAT(record.session_type(),
              Eq(LoginLogoutSessionType::REGULAR_USER_SESSION));
  ASSERT_TRUE(record.has_login_event());
  EXPECT_FALSE(record.login_event().has_failure());
}

TEST_P(LoginLogoutReporterTest, ReportUnaffiliatedLogin) {
  const auto test_case = GetParam();
  const bool is_guest_session =
      test_case.user_type == user_manager::UserType::kPublicAccount ||
      test_case.user_type == user_manager::UserType::kGuest;

  policy::ManagedSessionService managed_session_service;
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true,
      /*should_report_user=*/false);

  auto reporter = LoginLogoutReporter::CreateForTest(
      std::move(reporter_helper),
      std::make_unique<LoginLogoutReporterTestDelegate>(),
      &managed_session_service);

  auto profile = test_helper_.CreateProfileByType(test_case.user_type);
  auto* const user = ProfileHelper::Get()->GetUserByProfile(profile.get());
  managed_session_service.OnUserProfileLoaded(user->GetAccountId());
  const LoginLogoutRecord& record = test_helper_.GetRecord();

  ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_THAT(record.is_guest_session(), Eq(is_guest_session));
  EXPECT_FALSE(record.has_logout_event());
  EXPECT_FALSE(record.has_affiliated_user());
  ASSERT_TRUE(record.has_session_type());
  EXPECT_THAT(record.session_type(), Eq(test_case.expected_session_type));
  ASSERT_TRUE(record.has_login_event());
  EXPECT_FALSE(record.login_event().has_failure());
  if (test_case.expected_session_type ==
      LoginLogoutSessionType::REGULAR_USER_SESSION) {
    EXPECT_TRUE(record.has_unaffiliated_user());
    EXPECT_TRUE(record.unaffiliated_user().has_user_id());
    EXPECT_THAT(record.unaffiliated_user().user_id(), Not(IsEmpty()));
  }
}

TEST_F(LoginLogoutReporterTest, ReportAffiliatedLogout) {
  policy::ManagedSessionService managed_session_service;
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true,
      /*should_report_user=*/true);

  auto reporter = LoginLogoutReporter::CreateForTest(
      std::move(reporter_helper),
      std::make_unique<LoginLogoutReporterTestDelegate>(),
      &managed_session_service);

  auto profile = test_helper_.CreateRegularUserProfile();
  managed_session_service.OnSessionWillBeTerminated();
  const LoginLogoutRecord& record = test_helper_.GetRecord();

  ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_FALSE(record.is_guest_session());
  EXPECT_FALSE(record.has_login_event());
  EXPECT_TRUE(record.has_logout_event());
  ASSERT_TRUE(record.has_affiliated_user());
  ASSERT_TRUE(record.affiliated_user().has_user_email());
  EXPECT_THAT(record.affiliated_user().user_email(), StrEq(user_email));
  ASSERT_TRUE(record.has_session_type());
  EXPECT_THAT(record.session_type(),
              Eq(LoginLogoutSessionType::REGULAR_USER_SESSION));
}

TEST_P(LoginLogoutReporterTest, ReportUnaffiliatedLogout) {
  const auto test_case = GetParam();
  const bool is_guest_session =
      test_case.user_type == user_manager::UserType::kPublicAccount ||
      test_case.user_type == user_manager::UserType::kGuest;

  policy::ManagedSessionService managed_session_service;
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true,
      /*should_report_user=*/false);

  auto reporter = LoginLogoutReporter::CreateForTest(
      std::move(reporter_helper),
      std::make_unique<LoginLogoutReporterTestDelegate>(),
      &managed_session_service);

  auto profile = test_helper_.CreateProfileByType(test_case.user_type);
  managed_session_service.OnSessionWillBeTerminated();
  const LoginLogoutRecord& record = test_helper_.GetRecord();

  ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_THAT(record.is_guest_session(), Eq(is_guest_session));
  EXPECT_FALSE(record.has_login_event());
  EXPECT_TRUE(record.has_logout_event());
  EXPECT_FALSE(record.has_affiliated_user());
  ASSERT_TRUE(record.has_session_type());
  EXPECT_THAT(record.session_type(), Eq(test_case.expected_session_type));
  if (test_case.expected_session_type ==
      LoginLogoutSessionType::REGULAR_USER_SESSION) {
    EXPECT_TRUE(record.has_unaffiliated_user());
    EXPECT_TRUE(record.unaffiliated_user().has_user_id());
    EXPECT_THAT(record.unaffiliated_user().user_id(), Not(IsEmpty()));
  }
}

TEST_P(LoginLogoutReporterTest, ReportLoginLogoutDisabled) {
  const auto test_case = GetParam();
  policy::ManagedSessionService managed_session_service;
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/false,
      /*should_report_user=*/false);

  auto reporter = LoginLogoutReporter::CreateForTest(
      std::move(reporter_helper),
      std::make_unique<LoginLogoutReporterTestDelegate>(),
      &managed_session_service);

  auto profile = test_helper_.CreateProfileByType(test_case.user_type);
  auto* const user = ProfileHelper::Get()->GetUserByProfile(profile.get());
  managed_session_service.OnUserProfileLoaded(user->GetAccountId());
  managed_session_service.OnSessionWillBeTerminated();

  ASSERT_THAT(test_helper_.GetReportCount(), Eq(0));
}

INSTANTIATE_TEST_SUITE_P(All,
                         LoginLogoutReporterTest,
                         ::testing::ValuesIn<LoginLogoutReporterTestCase>(
                             {{user_manager::UserType::kRegular,
                               LoginLogoutSessionType::REGULAR_USER_SESSION},
                              {user_manager::UserType::kGuest,
                               LoginLogoutSessionType::GUEST_SESSION},
                              {user_manager::UserType::kPublicAccount,
                               LoginLogoutSessionType::PUBLIC_ACCOUNT_SESSION},
                              {user_manager::UserType::kKioskApp,
                               LoginLogoutSessionType::KIOSK_SESSION},
                              {user_manager::UserType::kWebKioskApp,
                               LoginLogoutSessionType::KIOSK_SESSION}}));

class LoginFailureReporterTest : public ::testing::TestWithParam<AuthFailure> {
 protected:
  LoginFailureReporterTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { test_helper_.Init(); }

  void TearDown() override { test_helper_.Shutdown(); }

  LoginLogoutTestHelper test_helper_;

  ScopedTestingLocalState local_state_;
};

TEST_F(LoginFailureReporterTest, ReportAffiliatedLoginFailure_OwnerRequired) {
  policy::ManagedSessionService managed_session_service;
  auto delegate = std::make_unique<LoginLogoutReporterTestDelegate>(
      AccountId::FromUserEmail(user_email));
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true,
      /*should_report_user=*/true);

  auto reporter = LoginLogoutReporter::CreateForTest(std::move(reporter_helper),
                                                     std::move(delegate),
                                                     &managed_session_service);

  managed_session_service.OnAuthFailure(
      AuthFailure(AuthFailure::OWNER_REQUIRED));
  const LoginLogoutRecord& record = test_helper_.GetRecord();

  ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_FALSE(record.is_guest_session());
  EXPECT_FALSE(record.has_logout_event());
  ASSERT_TRUE(record.has_affiliated_user());
  ASSERT_TRUE(record.affiliated_user().has_user_email());
  EXPECT_THAT(record.affiliated_user().user_email(), StrEq(user_email));
  ASSERT_TRUE(record.has_session_type());
  EXPECT_THAT(record.session_type(),
              Eq(LoginLogoutSessionType::REGULAR_USER_SESSION));
  ASSERT_TRUE(record.has_login_event());
  ASSERT_TRUE(record.login_event().has_failure());
  ASSERT_THAT(record.login_event().failure().reason(),
              Eq(LoginFailureReason::OWNER_REQUIRED));
}

TEST_F(LoginFailureReporterTest,
       ReportAffiliatedLoginFailure_UnrecoverableCryptohome) {
  policy::ManagedSessionService managed_session_service;
  auto delegate = std::make_unique<LoginLogoutReporterTestDelegate>(
      AccountId::FromUserEmail(user_email));
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true,
      /*should_report_user=*/true);

  auto reporter = LoginLogoutReporter::CreateForTest(std::move(reporter_helper),
                                                     std::move(delegate),
                                                     &managed_session_service);

  managed_session_service.OnAuthFailure(
      AuthFailure(AuthFailure::UNRECOVERABLE_CRYPTOHOME));
  const LoginLogoutRecord& record = test_helper_.GetRecord();

  ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_FALSE(record.is_guest_session());
  EXPECT_FALSE(record.has_logout_event());
  ASSERT_TRUE(record.has_affiliated_user());
  ASSERT_TRUE(record.affiliated_user().has_user_email());
  EXPECT_THAT(record.affiliated_user().user_email(), StrEq(user_email));
  ASSERT_TRUE(record.has_session_type());
  EXPECT_THAT(record.session_type(),
              Eq(LoginLogoutSessionType::REGULAR_USER_SESSION));
  ASSERT_TRUE(record.has_login_event());
  ASSERT_TRUE(record.login_event().has_failure());
  ASSERT_THAT(record.login_event().failure().reason(),
              Eq(LoginFailureReason::UNRECOVERABLE_CRYPTOHOME));
}

TEST_F(LoginFailureReporterTest, ReportUnaffiliatedLoginFailure_TpmError) {
  policy::ManagedSessionService managed_session_service;
  auto delegate = std::make_unique<LoginLogoutReporterTestDelegate>(
      AccountId::FromUserEmail(user_email));
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true,
      /*should_report_user=*/false);

  auto reporter = LoginLogoutReporter::CreateForTest(std::move(reporter_helper),
                                                     std::move(delegate),
                                                     &managed_session_service);

  managed_session_service.OnAuthFailure(AuthFailure(AuthFailure::TPM_ERROR));
  const LoginLogoutRecord& record = test_helper_.GetRecord();

  ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_FALSE(record.is_guest_session());
  EXPECT_FALSE(record.has_logout_event());
  EXPECT_FALSE(record.has_affiliated_user());
  EXPECT_TRUE(record.has_unaffiliated_user());
  EXPECT_TRUE(record.unaffiliated_user().has_user_id());
  EXPECT_THAT(record.unaffiliated_user().user_id(), Not(IsEmpty()));
  ASSERT_TRUE(record.has_session_type());
  EXPECT_THAT(record.session_type(),
              Eq(LoginLogoutSessionType::REGULAR_USER_SESSION));
  ASSERT_TRUE(record.has_login_event());
  ASSERT_TRUE(record.login_event().has_failure());
  ASSERT_THAT(record.login_event().failure().reason(),
              Eq(LoginFailureReason::TPM_ERROR));
}

TEST_F(LoginFailureReporterTest,
       ReportPublicAccountLoginFailure_TpmUpdateRequired) {
  policy::ManagedSessionService managed_session_service;
  AccountId account_id =
      AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
          "managed_guest", policy::DeviceLocalAccountType::kPublicSession));
  auto delegate = std::make_unique<LoginLogoutReporterTestDelegate>(account_id);
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true,
      /*should_report_user=*/false);

  auto reporter = LoginLogoutReporter::CreateForTest(std::move(reporter_helper),
                                                     std::move(delegate),
                                                     &managed_session_service);

  managed_session_service.OnAuthFailure(
      AuthFailure(AuthFailure::TPM_UPDATE_REQUIRED));
  const LoginLogoutRecord& record = test_helper_.GetRecord();

  ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_TRUE(record.is_guest_session());
  EXPECT_FALSE(record.has_logout_event());
  EXPECT_FALSE(record.has_affiliated_user());
  ASSERT_TRUE(record.has_session_type());
  EXPECT_THAT(record.session_type(),
              Eq(LoginLogoutSessionType::PUBLIC_ACCOUNT_SESSION));
  ASSERT_TRUE(record.has_login_event());
  ASSERT_TRUE(record.login_event().has_failure());
  ASSERT_THAT(record.login_event().failure().reason(),
              Eq(LoginFailureReason::TPM_UPDATE_REQUIRED));
}

TEST_F(LoginFailureReporterTest,
       ReportPublicAccountLoginFailure_CouldNotMountTmpfs) {
  policy::ManagedSessionService managed_session_service;
  AccountId account_id =
      AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
          "managed_guest", policy::DeviceLocalAccountType::kPublicSession));
  auto delegate = std::make_unique<LoginLogoutReporterTestDelegate>(account_id);
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true,
      /*should_report_user=*/false);

  auto reporter = LoginLogoutReporter::CreateForTest(std::move(reporter_helper),
                                                     std::move(delegate),
                                                     &managed_session_service);

  managed_session_service.OnAuthFailure(
      AuthFailure(AuthFailure::COULD_NOT_MOUNT_TMPFS));
  const LoginLogoutRecord& record = test_helper_.GetRecord();

  ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_TRUE(record.is_guest_session());
  EXPECT_FALSE(record.has_logout_event());
  EXPECT_FALSE(record.has_affiliated_user());
  ASSERT_TRUE(record.has_session_type());
  EXPECT_THAT(record.session_type(),
              Eq(LoginLogoutSessionType::PUBLIC_ACCOUNT_SESSION));
  ASSERT_TRUE(record.has_login_event());
  ASSERT_TRUE(record.login_event().has_failure());
  ASSERT_THAT(record.login_event().failure().reason(),
              Eq(LoginFailureReason::COULD_NOT_MOUNT_TMPFS));
}

TEST_F(LoginFailureReporterTest, ReportGuestLoginFailure_MissingCryptohome) {
  policy::ManagedSessionService managed_session_service;
  auto delegate = std::make_unique<LoginLogoutReporterTestDelegate>(
      user_manager::GuestAccountId());
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true,
      /*should_report_user=*/false);

  auto reporter = LoginLogoutReporter::CreateForTest(std::move(reporter_helper),
                                                     std::move(delegate),
                                                     &managed_session_service);

  managed_session_service.OnAuthFailure(
      AuthFailure(AuthFailure::MISSING_CRYPTOHOME));
  const LoginLogoutRecord& record = test_helper_.GetRecord();

  ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_TRUE(record.is_guest_session());
  EXPECT_FALSE(record.has_logout_event());
  EXPECT_FALSE(record.has_affiliated_user());
  ASSERT_TRUE(record.has_session_type());
  EXPECT_THAT(record.session_type(), Eq(LoginLogoutSessionType::GUEST_SESSION));
  ASSERT_TRUE(record.has_login_event());
  ASSERT_TRUE(record.login_event().has_failure());
  ASSERT_THAT(record.login_event().failure().reason(),
              Eq(LoginFailureReason::MISSING_CRYPTOHOME));
}

TEST_F(LoginFailureReporterTest, ReportLoginLogoutDisabled) {
  policy::ManagedSessionService managed_session_service;
  auto delegate = std::make_unique<LoginLogoutReporterTestDelegate>(
      AccountId::FromUserEmail(user_email));
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/false,
      /*should_report_user=*/true);

  auto reporter = LoginLogoutReporter::CreateForTest(std::move(reporter_helper),
                                                     std::move(delegate),
                                                     &managed_session_service);

  managed_session_service.OnAuthFailure(
      AuthFailure(AuthFailure::MISSING_CRYPTOHOME));

  ASSERT_THAT(test_helper_.GetReportCount(), Eq(0));
}

TEST_F(LoginFailureReporterTest, ReportKioskLoginFailure) {
  const base::Time failure_time = base::Time::Now();
  // Kiosk login failure session.
  {
    base::SimpleTestClock test_clock;
    test_clock.SetNow(failure_time);
    policy::ManagedSessionService managed_session_service;
    auto reporter_helper = test_helper_.GetReporterHelper(
        /*reporting_enabled=*/true,
        /*should_report_user=*/false);

    auto reporter = LoginLogoutReporter::CreateForTest(
        std::move(reporter_helper),
        std::make_unique<LoginLogoutReporterTestDelegate>(),
        &managed_session_service, &test_clock);
    base::RunLoop().RunUntilIdle();

    managed_session_service.OnKioskProfileLoadFailed();

    ASSERT_THAT(test_helper_.GetReportCount(), Eq(0));
  }

  // Next session after kiosk login failure session.
  {
    base::SimpleTestClock test_clock;
    test_clock.SetNow(failure_time + base::Hours(10));
    policy::ManagedSessionService managed_session_service;
    // Only |reporting_enabled| value at the time of kiosk login failure matter.
    auto reporter_helper = test_helper_.GetReporterHelper(
        /*reporting_enabled=*/false,
        /*should_report_user=*/false);

    auto reporter = LoginLogoutReporter::CreateForTest(
        std::move(reporter_helper),
        std::make_unique<LoginLogoutReporterTestDelegate>(),
        &managed_session_service, &test_clock);
    base::RunLoop().RunUntilIdle();
    const LoginLogoutRecord& record = test_helper_.GetRecord();

    ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
    ASSERT_TRUE(record.has_event_timestamp_sec());
    EXPECT_THAT(record.event_timestamp_sec(), Eq(failure_time.ToTimeT()));
    EXPECT_FALSE(record.is_guest_session());
    EXPECT_FALSE(record.has_logout_event());
    EXPECT_FALSE(record.has_affiliated_user());
    ASSERT_TRUE(record.has_session_type());
    EXPECT_THAT(record.session_type(),
                Eq(LoginLogoutSessionType::KIOSK_SESSION));
    ASSERT_TRUE(record.has_login_event());
    ASSERT_TRUE(record.login_event().has_failure());
    EXPECT_FALSE(record.login_event().failure().has_reason());
  }

  // Next session after kiosk login failure reporting session.
  {
    base::SimpleTestClock test_clock;
    test_clock.SetNow(failure_time + base::Hours(20));
    policy::ManagedSessionService managed_session_service;
    auto reporter_helper = test_helper_.GetReporterHelper(
        /*reporting_enabled=*/true,
        /*should_report_user=*/false);

    auto reporter = LoginLogoutReporter::CreateForTest(
        std::move(reporter_helper),
        std::make_unique<LoginLogoutReporterTestDelegate>(),
        &managed_session_service, &test_clock);
    base::RunLoop().RunUntilIdle();

    ASSERT_THAT(test_helper_.GetReportCount(), Eq(0));
  }
}

TEST_F(LoginFailureReporterTest, ReportKioskLoginFailure_ReportingError) {
  const base::Time failure_time = base::Time::Now();
  // Kiosk login failure session.
  {
    base::SimpleTestClock test_clock;
    test_clock.SetNow(failure_time);
    policy::ManagedSessionService managed_session_service;
    auto reporter_helper = test_helper_.GetReporterHelper(
        /*reporting_enabled=*/true,
        /*should_report_user=*/false);

    auto reporter = LoginLogoutReporter::CreateForTest(
        std::move(reporter_helper),
        std::make_unique<LoginLogoutReporterTestDelegate>(),
        &managed_session_service, &test_clock);
    base::RunLoop().RunUntilIdle();

    managed_session_service.OnKioskProfileLoadFailed();

    ASSERT_THAT(test_helper_.GetReportCount(), Eq(0));
  }

  // Next session after kiosk login failure session.
  // Reporting error.
  {
    base::SimpleTestClock test_clock;
    test_clock.SetNow(failure_time + base::Hours(10));
    policy::ManagedSessionService managed_session_service;
    // Only |reporting_enabled| value at the time of kiosk login failure matter.
    auto reporter_helper = test_helper_.GetReporterHelper(
        /*reporting_enabled=*/true,
        /*should_report_user=*/false,
        ::reporting::Status(::reporting::error::INTERNAL, ""));

    auto reporter = LoginLogoutReporter::CreateForTest(
        std::move(reporter_helper),
        std::make_unique<LoginLogoutReporterTestDelegate>(),
        &managed_session_service, &test_clock);
    base::RunLoop().RunUntilIdle();

    ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
  }

  // Next session after reporting error session.
  // Report success.
  {
    base::SimpleTestClock test_clock;
    test_clock.SetNow(failure_time + base::Hours(20));
    policy::ManagedSessionService managed_session_service;
    auto reporter_helper = test_helper_.GetReporterHelper(
        /*reporting_enabled=*/true,
        /*should_report_user=*/false);

    auto reporter = LoginLogoutReporter::CreateForTest(
        std::move(reporter_helper),
        std::make_unique<LoginLogoutReporterTestDelegate>(),
        &managed_session_service, &test_clock);
    base::RunLoop().RunUntilIdle();
    const LoginLogoutRecord& record = test_helper_.GetRecord();

    ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
    ASSERT_TRUE(record.has_event_timestamp_sec());
    EXPECT_THAT(record.event_timestamp_sec(), Eq(failure_time.ToTimeT()));
    EXPECT_FALSE(record.is_guest_session());
    EXPECT_FALSE(record.has_logout_event());
    EXPECT_FALSE(record.has_affiliated_user());
    ASSERT_TRUE(record.has_session_type());
    EXPECT_THAT(record.session_type(),
                Eq(LoginLogoutSessionType::KIOSK_SESSION));
    ASSERT_TRUE(record.has_login_event());
    ASSERT_TRUE(record.login_event().has_failure());
    EXPECT_FALSE(record.login_event().failure().has_reason());
  }
}

TEST_F(LoginFailureReporterTest, ReportKioskLoginFailure_ReportingDisabled) {
  const base::Time failure_time = base::Time::Now();
  // Kiosk login failure session.
  {
    base::SimpleTestClock test_clock;
    test_clock.SetNow(failure_time);
    policy::ManagedSessionService managed_session_service;
    auto reporter_helper = test_helper_.GetReporterHelper(
        /*reporting_enabled=*/false,
        /*should_report_user=*/false);

    auto reporter = LoginLogoutReporter::CreateForTest(
        std::move(reporter_helper),
        std::make_unique<LoginLogoutReporterTestDelegate>(),
        &managed_session_service, &test_clock);
    base::RunLoop().RunUntilIdle();

    managed_session_service.OnKioskProfileLoadFailed();

    ASSERT_THAT(test_helper_.GetReportCount(), Eq(0));
  }

  // Next session after kiosk login failure session.
  {
    base::SimpleTestClock test_clock;
    test_clock.SetNow(failure_time + base::Hours(10));
    policy::ManagedSessionService managed_session_service;
    // Only |reporting_enabled| value at the time of kiosk login failure matter.
    auto reporter_helper = test_helper_.GetReporterHelper(
        /*reporting_enabled=*/true,
        /*should_report_user=*/false);

    auto reporter = LoginLogoutReporter::CreateForTest(
        std::move(reporter_helper),
        std::make_unique<LoginLogoutReporterTestDelegate>(),
        &managed_session_service, &test_clock);
    base::RunLoop().RunUntilIdle();

    ASSERT_THAT(test_helper_.GetReportCount(), Eq(0));
  }
}

TEST_P(LoginFailureReporterTest,
       ReportUnaffiliatedLoginFailure_AuthenticationError) {
  policy::ManagedSessionService managed_session_service;
  auto delegate = std::make_unique<LoginLogoutReporterTestDelegate>(
      AccountId::FromUserEmail(user_email));
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true,
      /*should_report_user=*/false);

  auto reporter = LoginLogoutReporter::CreateForTest(std::move(reporter_helper),
                                                     std::move(delegate),
                                                     &managed_session_service);

  managed_session_service.OnAuthFailure(GetParam());
  const LoginLogoutRecord& record = test_helper_.GetRecord();

  ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_FALSE(record.is_guest_session());
  EXPECT_FALSE(record.has_logout_event());
  EXPECT_FALSE(record.has_affiliated_user());
  EXPECT_TRUE(record.has_unaffiliated_user());
  EXPECT_TRUE(record.unaffiliated_user().has_user_id());
  EXPECT_THAT(record.unaffiliated_user().user_id(), Not(IsEmpty()));
  ASSERT_TRUE(record.has_session_type());
  EXPECT_THAT(record.session_type(),
              Eq(LoginLogoutSessionType::REGULAR_USER_SESSION));
  ASSERT_TRUE(record.has_login_event());
  ASSERT_TRUE(record.login_event().has_failure());
  ASSERT_THAT(record.login_event().failure().reason(),
              Eq(LoginFailureReason::AUTHENTICATION_ERROR));
}

TEST_P(LoginFailureReporterTest,
       ReportPublicAccountLoginFailure_InternalLoginFailure) {
  policy::ManagedSessionService managed_session_service;
  AccountId account_id =
      AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
          "managed_guest", policy::DeviceLocalAccountType::kPublicSession));
  auto delegate = std::make_unique<LoginLogoutReporterTestDelegate>(account_id);
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true,
      /*should_report_user=*/false);

  auto reporter = LoginLogoutReporter::CreateForTest(std::move(reporter_helper),
                                                     std::move(delegate),
                                                     &managed_session_service);

  managed_session_service.OnAuthFailure(GetParam());
  const LoginLogoutRecord& record = test_helper_.GetRecord();

  ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_TRUE(record.is_guest_session());
  EXPECT_FALSE(record.has_logout_event());
  EXPECT_FALSE(record.has_affiliated_user());
  ASSERT_TRUE(record.has_session_type());
  EXPECT_THAT(record.session_type(),
              Eq(LoginLogoutSessionType::PUBLIC_ACCOUNT_SESSION));
  ASSERT_TRUE(record.has_login_event());
  ASSERT_TRUE(record.login_event().has_failure());
  ASSERT_THAT(record.login_event().failure().reason(),
              Eq(LoginFailureReason::INTERNAL_LOGIN_FAILURE_REASON));
}

TEST_P(LoginFailureReporterTest, ReportGuestLoginFailure_InternalLoginFailure) {
  policy::ManagedSessionService managed_session_service;
  auto delegate = std::make_unique<LoginLogoutReporterTestDelegate>(
      user_manager::GuestAccountId());
  auto reporter_helper = test_helper_.GetReporterHelper(
      /*reporting_enabled=*/true,
      /*should_report_user=*/false);

  auto reporter = LoginLogoutReporter::CreateForTest(std::move(reporter_helper),
                                                     std::move(delegate),
                                                     &managed_session_service);

  managed_session_service.OnAuthFailure(GetParam());
  const LoginLogoutRecord& record = test_helper_.GetRecord();

  ASSERT_THAT(test_helper_.GetReportCount(), Eq(1));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_TRUE(record.is_guest_session());
  EXPECT_FALSE(record.has_logout_event());
  EXPECT_FALSE(record.has_affiliated_user());
  ASSERT_TRUE(record.has_session_type());
  EXPECT_THAT(record.session_type(), Eq(LoginLogoutSessionType::GUEST_SESSION));
  ASSERT_TRUE(record.has_login_event());
  ASSERT_TRUE(record.login_event().has_failure());
  ASSERT_THAT(record.login_event().failure().reason(),
              Eq(LoginFailureReason::INTERNAL_LOGIN_FAILURE_REASON));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    LoginFailureReporterTest,
    ::testing::Values<AuthFailure>(
        AuthFailure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME),
        AuthFailure(AuthFailure::DATA_REMOVAL_FAILED),
        AuthFailure(AuthFailure::USERNAME_HASH_FAILED),
        AuthFailure(AuthFailure::FAILED_TO_INITIALIZE_TOKEN)));

}  // namespace reporting
}  // namespace ash
