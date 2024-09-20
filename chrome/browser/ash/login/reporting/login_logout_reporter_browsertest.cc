// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "apps/test/app_window_waiter.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/auto_reset.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_profile_load_failed_observer.h"
#include "chrome/browser/ash/app_mode/kiosk_test_helper.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_apps_mixin.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/embedded_test_server_setup_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/login_logout_event.pb.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/stub_authenticator_builder.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

std::optional<Record> MaybeGetEnqueuedLoginLogoutRecord() {
  const std::vector<Record>& records =
      MissiveClient::Get()->GetTestInterface()->GetEnqueuedRecords(
          Priority::SECURITY);
  for (const Record& record : records) {
    if (record.destination() == Destination::LOGIN_LOGOUT_EVENTS) {
      return record;
    }
  }
  return std::nullopt;
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

class KioskProfileLoadFailedWaiter : public KioskProfileLoadFailedObserver {
 public:
  KioskProfileLoadFailedWaiter() = default;

  KioskProfileLoadFailedWaiter(const KioskProfileLoadFailedWaiter&) = delete;
  KioskProfileLoadFailedWaiter& operator=(const KioskProfileLoadFailedWaiter&) =
      delete;

  ~KioskProfileLoadFailedWaiter() override = default;

  void Wait() {
    if (!LoginDisplayHost::default_host()) {
      // LoginDisplayHost instance is destroyed, this means the profile load
      // failure already took place.
      return;
    }
    KioskController::Get().AddProfileLoadFailedObserver(this);
    run_loop_.Run();
  }

  // KioskProfileLoadFailedObserver:
  void OnKioskProfileLoadFailed() override { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;
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
    login_manager_.SetShouldLaunchBrowser(true);
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
  ASSERT_TRUE(login_record.has_source_info());
  EXPECT_THAT(login_record.source_info().source(),
              Eq(::reporting::SourceInfo::ASH));

  LoginLogoutRecord login_record_data;
  ASSERT_TRUE(login_record_data.ParseFromString(login_record.data()));
  EXPECT_THAT(login_record_data.session_type(),
              Eq(LoginLogoutSessionType::REGULAR_USER_SESSION));
  EXPECT_FALSE(login_record_data.has_affiliated_user());
  ASSERT_TRUE(login_record_data.has_login_event());
  EXPECT_FALSE(login_record_data.login_event().has_failure());

  Shell::Get()->session_controller()->RequestSignOut();
  Record logout_record = GetNextLoginLogoutRecord(&observer);
  ASSERT_TRUE(logout_record.has_source_info());
  EXPECT_THAT(logout_record.source_info().source(),
              Eq(::reporting::SourceInfo::ASH));

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
  ASSERT_TRUE(login_record.has_source_info());
  EXPECT_THAT(login_record.source_info().source(),
              Eq(::reporting::SourceInfo::ASH));

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

  // Check if the record is already enqueued in case it was enqueued before the
  // |observer| initialization.
  std::optional<Record> login_record = MaybeGetEnqueuedLoginLogoutRecord();

  if (!login_record.has_value()) {
    // Record is not enqueued yet, so wait for it.
    login_record = GetNextLoginLogoutRecord(&observer);
  }
  ASSERT_TRUE(login_record.value().has_source_info());
  EXPECT_THAT(login_record.value().source_info().source(),
              Eq(::reporting::SourceInfo::ASH));

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
          policy::DeviceLocalAccountType::kPublicSession));

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
  ASSERT_TRUE(user_manager->IsLoggedInAsManagedGuestSession());

  Record login_record = GetNextLoginLogoutRecord(&observer);
  ASSERT_TRUE(login_record.has_source_info());
  EXPECT_THAT(login_record.source_info().source(),
              Eq(::reporting::SourceInfo::ASH));

  LoginLogoutRecord login_record_data;
  ASSERT_TRUE(login_record_data.ParseFromString(login_record.data()));
  EXPECT_THAT(login_record_data.session_type(),
              Eq(LoginLogoutSessionType::PUBLIC_ACCOUNT_SESSION));
  EXPECT_FALSE(login_record_data.has_affiliated_user());
  ASSERT_TRUE(login_record_data.has_login_event());
  EXPECT_FALSE(login_record_data.login_event().has_failure());
}

class LoginLogoutReporterKioskBrowserTest
    : public MixinBasedInProcessBrowserTest {
 protected:
  void SetUp() override {
    login_manager_.set_session_restore_enabled();

    MixinBasedInProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);

    fake_cws_.Init(embedded_test_server());
    fake_cws_.SetUpdateCrx(GetTestAppId(), GetTestAppId() + ".crx", "1.0.0");
  }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    host_resolver()->AddRule("*", "127.0.0.1");

    ChromeDeviceSettingsProto& proto(policy_helper_.device_policy()->payload());
    KioskAppsMixin::AppendAutoLaunchKioskAccount(&proto);
    proto.mutable_device_reporting()->set_report_login_logout(true);
    policy_helper_.RefreshDevicePolicy();
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    extensions::browsertest_util::CreateAndInitializeLocalCache();
  }

  std::string GetTestAppId() const { return KioskAppsMixin::kTestChromeAppId; }

 private:
  FakeCWS fake_cws_;
  policy::DevicePolicyCrosTestHelper policy_helper_;
  base::AutoReset<bool> skip_splash_wait_override_ =
      KioskTestHelper::SkipSplashScreenWait();
  EmbeddedTestServerSetupMixin embedded_test_server_{&mixin_host_,
                                                     embedded_test_server()};

  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  LoginManagerMixin login_manager_{&mixin_host_, {}};
};

IN_PROC_BROWSER_TEST_F(LoginLogoutReporterKioskBrowserTest,
                       LoginSuccessfulThenLogout) {
  ASSERT_TRUE(::ash::LoginState::Get()->IsKioskSession());
  MissiveClientTestObserver observer(Destination::LOGIN_LOGOUT_EVENTS);

  // Check if the record is already enqueued in case it was enqueued before the
  // |observer| initialization.
  std::optional<Record> login_record = MaybeGetEnqueuedLoginLogoutRecord();
  if (!login_record.has_value()) {
    // Record is not enqueued yet, so wait for it.
    login_record = GetNextLoginLogoutRecord(&observer);
  }
  ASSERT_TRUE(login_record.value().has_source_info());
  EXPECT_THAT(login_record.value().source_info().source(),
              Eq(::reporting::SourceInfo::ASH));

  LoginLogoutRecord login_record_data;
  ASSERT_TRUE(login_record_data.ParseFromString(login_record->data()));
  EXPECT_THAT(login_record_data.session_type(),
              Eq(LoginLogoutSessionType::KIOSK_SESSION));
  EXPECT_FALSE(login_record_data.has_affiliated_user());
  ASSERT_TRUE(login_record_data.has_login_event());
  EXPECT_FALSE(login_record_data.login_event().has_failure());

  // Wait for the window to appear.
  extensions::AppWindow* const window =
      apps::AppWindowWaiter(extensions::AppWindowRegistry::Get(
                                ProfileManager::GetPrimaryUserProfile()),
                            GetTestAppId())
          .Wait();
  ASSERT_TRUE(window);

  // Terminate the app.
  window->GetBaseWindow()->Close();

  Record logout_record = GetNextLoginLogoutRecord(&observer);
  LoginLogoutRecord logout_record_data;
  ASSERT_TRUE(logout_record_data.ParseFromString(logout_record.data()));
  EXPECT_THAT(logout_record_data.session_type(),
              Eq(LoginLogoutSessionType::KIOSK_SESSION));
  EXPECT_FALSE(logout_record_data.has_affiliated_user());
  EXPECT_TRUE(logout_record_data.has_logout_event());
}

class LoginLogoutReporterKioskFailedBrowserTest
    : public LoginLogoutReporterKioskBrowserTest {
 public:
  LoginLogoutReporterKioskFailedBrowserTest(
      const LoginLogoutReporterKioskFailedBrowserTest&) = delete;
  LoginLogoutReporterKioskFailedBrowserTest& operator=(
      const LoginLogoutReporterKioskFailedBrowserTest&) = delete;

 protected:
  LoginLogoutReporterKioskFailedBrowserTest() = default;

  ~LoginLogoutReporterKioskFailedBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    LoginLogoutReporterKioskBrowserTest::SetUpInProcessBrowserTestFixture();

    UserDataAuthClient::InitializeFake();
    FakeUserDataAuthClient::Get()->SetNextOperationError(
        FakeUserDataAuthClient::Operation::kStartAuthSession,
        cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
            user_data_auth::CRYPTOHOME_ERROR_MOUNT_FATAL));
  }
};

// Kiosk login failure will cause shutdown and the failure will be reported in
// in the next session.
IN_PROC_BROWSER_TEST_F(LoginLogoutReporterKioskFailedBrowserTest,
                       PRE_ReportKioskLoginFailure) {
  KioskProfileLoadFailedWaiter profile_load_failed_waiter;
  profile_load_failed_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(LoginLogoutReporterKioskFailedBrowserTest,
                       ReportKioskLoginFailure) {
  MissiveClientTestObserver observer(Destination::LOGIN_LOGOUT_EVENTS);

  // Check if the record is already enqueued in case it was enqueued before the
  // |observer| initialization.
  std::optional<Record> login_record = MaybeGetEnqueuedLoginLogoutRecord();

  if (!login_record.has_value()) {
    // Record is not enqueued yet, so wait for it.
    login_record = GetNextLoginLogoutRecord(&observer);
  }
  ASSERT_TRUE(login_record.value().has_source_info());
  EXPECT_THAT(login_record.value().source_info().source(),
              Eq(::reporting::SourceInfo::ASH));

  LoginLogoutRecord login_record_data;
  ASSERT_TRUE(login_record_data.ParseFromString(login_record->data()));
  EXPECT_THAT(login_record_data.session_type(),
              Eq(LoginLogoutSessionType::KIOSK_SESSION));
  EXPECT_FALSE(login_record_data.has_affiliated_user());
  ASSERT_TRUE(login_record_data.has_login_event());
  ASSERT_TRUE(login_record_data.login_event().has_failure());
  EXPECT_FALSE(login_record_data.login_event().failure().has_reason());
}

}  // namespace
}  // namespace ash::reporting
