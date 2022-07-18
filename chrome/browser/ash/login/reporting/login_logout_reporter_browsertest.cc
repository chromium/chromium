// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>
#include <vector>

#include "ash/components/login/auth/public/auth_failure.h"
#include "ash/components/login/auth/public/key.h"
#include "ash/components/login/auth/public/user_context.h"
#include "ash/components/login/auth/stub_authenticator_builder.h"
#include "ash/components/settings/cros_settings_names.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/login_logout_event.pb.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/account_id/account_id.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::MissiveClient;
using chromeos::MissiveClientTestObserver;
using reporting::Destination;
using reporting::Priority;
using reporting::Record;
using testing::Eq;
using testing::SizeIs;

namespace ash::reporting {

class LoginLogoutReporterBrowserTest
    : public policy::DevicePolicyCrosBrowserTest {
 public:
  LoginLogoutReporterBrowserTest() = default;

  LoginLogoutReporterBrowserTest(const LoginLogoutReporterBrowserTest&) =
      delete;
  LoginLogoutReporterBrowserTest& operator=(
      const LoginLogoutReporterBrowserTest&) = delete;

  ~LoginLogoutReporterBrowserTest() override = default;

  void SetUpOnMainThread() override {
    login_manager_.set_should_launch_browser(true);
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
  }

  void SetUpStubAuthenticatorAndAttemptLogin(
      AuthFailure::FailureReason failure_reason = AuthFailure::NONE) {
    const UserContext user_context =
        LoginManagerMixin::CreateDefaultUserContext(test_user_);

    auto authenticator_builder =
        std::make_unique<StubAuthenticatorBuilder>(user_context);
    if (failure_reason != AuthFailure::NONE) {
      authenticator_builder->SetUpAuthFailure(failure_reason);
    }

    test::UserSessionManagerTestApi(UserSessionManager::GetInstance())
        .InjectAuthenticatorBuilder(std::move(authenticator_builder));

    const std::string& password = user_context.GetKey()->GetSecret();
    LoginScreenTestApi::SubmitPassword(test_user_.account_id, password,
                                       /*check_if_submittable=*/true);
  }

 protected:
  void SetIsReportLoginLogoutPolicyEnabled(bool enabled) {
    policy_helper()
        ->device_policy()
        ->payload()
        .mutable_device_reporting()
        ->set_report_login_logout(enabled);
    policy_helper()->RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
        {ash::kReportDeviceLoginLogout});
  }

  Record GetNextLoginLogoutRecord(MissiveClientTestObserver* observer) {
    std::tuple<Priority, Record> enqueued_record =
        observer->GetNextEnqueuedRecord();
    Priority priority = std::get<0>(enqueued_record);
    Record record = std::get<1>(enqueued_record);

    EXPECT_THAT(priority, Eq(Priority::SECURITY));
    return record;
  }

  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kFakeUserEmail,
                                     FakeGaiaMixin::kFakeUserGaiaId)};

  LoginManagerMixin login_manager_{&mixin_host_, {test_user_}};
};

IN_PROC_BROWSER_TEST_F(LoginLogoutReporterBrowserTest,
                       LoginSuccessfulThenLogout) {
  SetIsReportLoginLogoutPolicyEnabled(true);

  MissiveClientTestObserver observer(Destination::LOGIN_LOGOUT_EVENTS);
  SetUpStubAuthenticatorAndAttemptLogin();
  test::WaitForPrimaryUserSessionStart();
  base::RunLoop().RunUntilIdle();

  Record login_record = GetNextLoginLogoutRecord(&observer);

  LoginLogoutRecord login_record_data;
  ASSERT_TRUE(login_record_data.ParseFromString(login_record.data()));
  EXPECT_THAT(login_record_data.session_type(),
              Eq(LoginLogoutSessionType::REGULAR_USER_SESSION));
  EXPECT_FALSE(login_record_data.has_affiliated_user());
  ASSERT_TRUE(login_record_data.has_login_event());
  EXPECT_FALSE(login_record_data.login_event().has_failure());

  ash::Shell::Get()->session_controller()->RequestSignOut();
  Record logout_record = GetNextLoginLogoutRecord(&observer);

  LoginLogoutRecord logout_record_data;
  ASSERT_TRUE(logout_record_data.ParseFromString(logout_record.data()));
  EXPECT_THAT(logout_record_data.session_type(),
              Eq(LoginLogoutSessionType::REGULAR_USER_SESSION));
  EXPECT_FALSE(logout_record_data.has_affiliated_user());
  EXPECT_TRUE(logout_record_data.has_logout_event());
}

IN_PROC_BROWSER_TEST_F(LoginLogoutReporterBrowserTest, LoginFailed) {
  SetIsReportLoginLogoutPolicyEnabled(true);

  MissiveClientTestObserver observer(Destination::LOGIN_LOGOUT_EVENTS);
  SetUpStubAuthenticatorAndAttemptLogin(
      AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);
  base::RunLoop().RunUntilIdle();

  Record login_record = GetNextLoginLogoutRecord(&observer);

  LoginLogoutRecord failed_login_record_data;
  ASSERT_TRUE(failed_login_record_data.ParseFromString(login_record.data()));
  EXPECT_THAT(failed_login_record_data.session_type(),
              Eq(LoginLogoutSessionType::REGULAR_USER_SESSION));
  EXPECT_FALSE(failed_login_record_data.has_affiliated_user());
  ASSERT_TRUE(failed_login_record_data.has_login_event());
  ASSERT_TRUE(failed_login_record_data.login_event().has_failure());
  EXPECT_THAT(failed_login_record_data.login_event().failure().reason(),
              LoginFailureReason::AUTHENTICATION_ERROR);
}

}  // namespace ash::reporting
