// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "ash/components/login/auth/auth_status_consumer.h"
#include "ash/components/login/auth/key.h"
#include "ash/components/login/auth/stub_authenticator_builder.h"
#include "ash/components/login/auth/user_context.h"
#include "ash/components/settings/cros_settings_names.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/login_logout_event.pb.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "components/account_id/account_id.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::MissiveClient;
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

  std::vector<Record> GetLoginLogoutRecords(
      const std::vector<Record>& all_records) {
    std::vector<Record> login_logout_records;
    for (const Record& record : all_records) {
      if (record.destination() == Destination::LOGIN_LOGOUT_EVENTS) {
        login_logout_records.push_back(record);
      }
    }
    return login_logout_records;
  }

  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kFakeUserEmail,
                                     FakeGaiaMixin::kFakeUserGaiaId)};

  LoginManagerMixin login_manager_{&mixin_host_, {test_user_}};
};

IN_PROC_BROWSER_TEST_F(LoginLogoutReporterBrowserTest, LoginSuccessful) {
  SetIsReportLoginLogoutPolicyEnabled(true);

  SetUpStubAuthenticatorAndAttemptLogin();
  test::WaitForPrimaryUserSessionStart();
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(MissiveClient::Get());
  MissiveClient::TestInterface* const fake_missive =
      MissiveClient::Get()->GetTestInterface();
  ASSERT_TRUE(fake_missive);

  const std::vector<Record>& security_records =
      fake_missive->GetEnqueuedRecords(Priority::SECURITY);

  ASSERT_FALSE(security_records.empty());
  std::vector<Record> login_logout_records =
      GetLoginLogoutRecords(security_records);
  ASSERT_THAT(login_logout_records, SizeIs(1));

  LoginLogoutRecord record_data;
  ASSERT_TRUE(record_data.ParseFromString(login_logout_records[0].data()));
  EXPECT_THAT(record_data.session_type(),
              Eq(LoginLogoutSessionType::REGULAR_USER_SESSION));
  EXPECT_FALSE(record_data.has_affiliated_user());
  ASSERT_TRUE(record_data.has_login_event());
  EXPECT_FALSE(record_data.login_event().has_failure());
}

IN_PROC_BROWSER_TEST_F(LoginLogoutReporterBrowserTest, LoginFailed) {
  SetIsReportLoginLogoutPolicyEnabled(true);

  SetUpStubAuthenticatorAndAttemptLogin(
      AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(MissiveClient::Get());
  MissiveClient::TestInterface* const fake_missive =
      MissiveClient::Get()->GetTestInterface();
  ASSERT_TRUE(fake_missive);

  const std::vector<Record>& security_records =
      fake_missive->GetEnqueuedRecords(Priority::SECURITY);

  ASSERT_FALSE(security_records.empty());
  std::vector<Record> login_logout_records =
      GetLoginLogoutRecords(security_records);
  ASSERT_THAT(login_logout_records, SizeIs(1));

  LoginLogoutRecord record_data;
  ASSERT_TRUE(record_data.ParseFromString(security_records[0].data()));
  EXPECT_THAT(record_data.session_type(),
              Eq(LoginLogoutSessionType::REGULAR_USER_SESSION));
  EXPECT_FALSE(record_data.has_affiliated_user());
  ASSERT_TRUE(record_data.has_login_event());
  ASSERT_TRUE(record_data.login_event().has_failure());
  EXPECT_THAT(record_data.login_event().failure().reason(),
              LoginFailureReason::AUTHENTICATION_ERROR);
}

}  // namespace ash::reporting
