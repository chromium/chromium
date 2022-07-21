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
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/login_logout_event.pb.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using chromeos::MissiveClient;
using chromeos::MissiveClientTestObserver;
using enterprise_management::ChromeDeviceSettingsProto;
using enterprise_management::DeviceLocalAccountInfoProto;
using reporting::Destination;
using reporting::Priority;
using reporting::Record;
using testing::Eq;
using testing::InvokeWithoutArgs;
using testing::SizeIs;

namespace ash::reporting {
namespace {

constexpr char kPublicSessionUserEmail[] = "public_session_user@localhost";

Record GetNextLoginLogoutRecord(MissiveClientTestObserver* observer) {
  std::tuple<Priority, Record> enqueued_record =
      observer->GetNextEnqueuedRecord();
  Priority priority = std::get<0>(enqueued_record);
  Record record = std::get<1>(enqueued_record);

  EXPECT_THAT(priority, Eq(Priority::SECURITY));
  return record;
}

absl::optional<Record> MaybeGetEnqueudLoginLogoutRecord() {
  const std::vector<Record>& records =
      MissiveClient::Get()->GetTestInterface()->GetEnqueuedRecords(
          Priority::SECURITY);
  for (const Record& record : records) {
    if (record.destination() == Destination::LOGIN_LOGOUT_EVENTS) {
      return record;
    }
  }
  return absl::nullopt;
}

class PublicSessionUserCreationWaiter
    : public user_manager::UserManager::Observer {
 public:
  PublicSessionUserCreationWaiter() = default;

  PublicSessionUserCreationWaiter(const PublicSessionUserCreationWaiter&) =
      delete;
  PublicSessionUserCreationWaiter& operator=(
      const PublicSessionUserCreationWaiter&) = delete;

  ~PublicSessionUserCreationWaiter() override = default;

  void Wait(const AccountId& public_session_account_id) {
    if (user_manager::UserManager::Get()->IsKnownUser(
            public_session_account_id)) {
      return;
    }

    local_state_changed_run_loop_ = std::make_unique<base::RunLoop>();
    user_manager::UserManager::Get()->AddObserver(this);
    local_state_changed_run_loop_->Run();
    user_manager::UserManager::Get()->RemoveObserver(this);
  }

  // user_manager::UserManager::Observer:
  void LocalStateChanged(user_manager::UserManager* user_manager) override {
    local_state_changed_run_loop_->Quit();
  }

 private:
  std::unique_ptr<base::RunLoop> local_state_changed_run_loop_;
};

class LoginLogoutReporterBrowserTest
    : public policy::DevicePolicyCrosBrowserTest {
 public:
  LoginLogoutReporterBrowserTest(const LoginLogoutReporterBrowserTest&) =
      delete;
  LoginLogoutReporterBrowserTest& operator=(
      const LoginLogoutReporterBrowserTest&) = delete;

 protected:
  LoginLogoutReporterBrowserTest() {
    login_manager_.set_session_restore_enabled();
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        kReportDeviceLoginLogout, true);
  }

  ~LoginLogoutReporterBrowserTest() override = default;

  void SetUpOnMainThread() override {
    login_manager_.set_should_launch_browser(true);
    FakeSessionManagerClient::Get()->set_supports_browser_restart(true);
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

  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kFakeUserEmail,
                                     FakeGaiaMixin::kFakeUserGaiaId)};

  LoginManagerMixin login_manager_{&mixin_host_, {test_user_}};

  ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(LoginLogoutReporterBrowserTest,
                       LoginSuccessfulThenLogout) {
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

  Shell::Get()->session_controller()->RequestSignOut();
  Record logout_record = GetNextLoginLogoutRecord(&observer);

  LoginLogoutRecord logout_record_data;
  ASSERT_TRUE(logout_record_data.ParseFromString(logout_record.data()));
  EXPECT_THAT(logout_record_data.session_type(),
              Eq(LoginLogoutSessionType::REGULAR_USER_SESSION));
  EXPECT_FALSE(logout_record_data.has_affiliated_user());
  EXPECT_TRUE(logout_record_data.has_logout_event());
}

IN_PROC_BROWSER_TEST_F(LoginLogoutReporterBrowserTest, LoginFailed) {
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

IN_PROC_BROWSER_TEST_F(LoginLogoutReporterBrowserTest, PRE_GuestLogin) {
  base::RunLoop restart_job_waiter;
  FakeSessionManagerClient::Get()->set_restart_job_callback(
      restart_job_waiter.QuitClosure());

  ASSERT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
  ASSERT_TRUE(LoginScreenTestApi::ClickGuestButton());

  restart_job_waiter.Run();
  EXPECT_TRUE(FakeSessionManagerClient::Get()->restart_job_argv().has_value());
}

IN_PROC_BROWSER_TEST_F(LoginLogoutReporterBrowserTest, GuestLogin) {
  MissiveClientTestObserver observer(Destination::LOGIN_LOGOUT_EVENTS);
  test::WaitForPrimaryUserSessionStart();
  base::RunLoop().RunUntilIdle();

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  ASSERT_TRUE(user_manager->IsLoggedInAsGuest());

  absl::optional<Record> login_record = MaybeGetEnqueudLoginLogoutRecord();

  if (!login_record.has_value()) {
    // Record is not enqueued yet, so wait for it.
    login_record = GetNextLoginLogoutRecord(&observer);
  }

  LoginLogoutRecord login_record_data;
  ASSERT_TRUE(login_record_data.ParseFromString(login_record->data()));
  EXPECT_THAT(login_record_data.session_type(),
              Eq(LoginLogoutSessionType::GUEST_SESSION));
  EXPECT_FALSE(login_record_data.has_affiliated_user());
  ASSERT_TRUE(login_record_data.has_login_event());
  EXPECT_FALSE(login_record_data.login_event().has_failure());
}

class LoginLogoutReporterPublicSessionBrowserTest
    : public policy::DevicePolicyCrosBrowserTest {
 public:
  LoginLogoutReporterPublicSessionBrowserTest(
      const LoginLogoutReporterPublicSessionBrowserTest&) = delete;
  LoginLogoutReporterPublicSessionBrowserTest& operator=(
      const LoginLogoutReporterPublicSessionBrowserTest&) = delete;

 protected:
  LoginLogoutReporterPublicSessionBrowserTest() = default;
  ~LoginLogoutReporterPublicSessionBrowserTest() override = default;

  void SetUpOnMainThread() override {
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();

    // Wait for the public session user to be created.
    PublicSessionUserCreationWaiter public_session_waiter;
    public_session_waiter.Wait(public_session_account_id_);
    EXPECT_TRUE(user_manager::UserManager::Get()->IsKnownUser(
        public_session_account_id_));

    // Wait for the device local account policy to be installed.
    policy::CloudPolicyStore* const store =
        TestingBrowserProcess::GetGlobal()
            ->platform_part()
            ->browser_policy_connector_ash()
            ->GetDeviceLocalAccountPolicyService()
            ->GetBrokerForUser(public_session_account_id_.GetUserEmail())
            ->core()
            ->store();
    if (!store->has_policy()) {
      policy::MockCloudPolicyStoreObserver observer;

      base::RunLoop loop;
      store->AddObserver(&observer);
      EXPECT_CALL(observer, OnStoreLoaded(store))
          .Times(1)
          .WillOnce(InvokeWithoutArgs(&loop, &base::RunLoop::Quit));
      loop.Run();
      store->RemoveObserver(&observer);
    }
  }

  void SetUpInProcessBrowserTestFixture() override {
    policy::DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    // Setup the device policy.
    ChromeDeviceSettingsProto& proto(device_policy()->payload());
    DeviceLocalAccountInfoProto* account =
        proto.mutable_device_local_accounts()->add_account();
    account->set_account_id(kPublicSessionUserEmail);
    account->set_type(DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
    // Enable login/logout reporting.
    proto.mutable_device_reporting()->set_report_login_logout(true);
    RefreshDevicePolicy();

    // Setup the device local account policy.
    policy::UserPolicyBuilder device_local_account_policy;
    device_local_account_policy.policy_data().set_username(
        kPublicSessionUserEmail);
    device_local_account_policy.policy_data().set_policy_type(
        policy::dm_protocol::kChromePublicAccountPolicyType);
    device_local_account_policy.policy_data().set_settings_entity_id(
        kPublicSessionUserEmail);
    device_local_account_policy.Build();
    session_manager_client()->set_device_local_account_policy(
        kPublicSessionUserEmail, device_local_account_policy.GetBlob());
  }

  const AccountId public_session_account_id_ =
      AccountId::FromUserEmail(policy::GenerateDeviceLocalAccountUserId(
          kPublicSessionUserEmail,
          policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION));

  LoginManagerMixin login_manager_{&mixin_host_, {}};
};

IN_PROC_BROWSER_TEST_F(LoginLogoutReporterPublicSessionBrowserTest,
                       LoginSuccessful) {
  MissiveClientTestObserver observer(Destination::LOGIN_LOGOUT_EVENTS);

  ASSERT_TRUE(
      LoginScreenTestApi::ExpandPublicSessionPod(public_session_account_id_));
  LoginScreenTestApi::ClickPublicExpandedSubmitButton();
  test::WaitForPrimaryUserSessionStart();
  base::RunLoop().RunUntilIdle();

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  ASSERT_TRUE(user_manager->IsLoggedInAsPublicAccount());

  Record login_record = GetNextLoginLogoutRecord(&observer);

  LoginLogoutRecord login_record_data;
  ASSERT_TRUE(login_record_data.ParseFromString(login_record.data()));
  EXPECT_THAT(login_record_data.session_type(),
              Eq(LoginLogoutSessionType::PUBLIC_ACCOUNT_SESSION));
  EXPECT_FALSE(login_record_data.has_affiliated_user());
  ASSERT_TRUE(login_record_data.has_login_event());
  EXPECT_FALSE(login_record_data.login_event().has_failure());
}

}  // namespace
}  // namespace ash::reporting
