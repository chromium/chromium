// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "ash/components/arc/enterprise/arc_data_snapshotd_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/authpolicy/authpolicy_credentials_manager.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/screens/user_selection_screen.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/ui/mock_login_display.h"
#include "chrome/browser/ash/login/ui/mock_login_display_host.h"
#include "chrome/browser/ash/login/ui/mock_signin_ui.h"
#include "chrome/browser/ash/login/ui/signin_ui.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/locale_switch_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/tpm_error_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/authpolicy/fake_authpolicy_client.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/stub_authenticator_builder.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/account_id/account_id.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

namespace em = ::enterprise_management;

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::Return;

const char kObjectGuid[] = "12345";
const char kAdUsername[] = "test_user@ad-domain.com";
const char kNewUser[] = "new_test_user@gmail.com";
const char kNewGaiaID[] = "11111";
const char kExistingUser[] = "existing_test_user@gmail.com";
const char kExistingGaiaID[] = "22222";
const char kManagedUser[] = "user@example.com";
const char kManagedGaiaID[] = "33333";
const char kManager[] = "admin@example.com";
const char kManagedDomain[] = "example.com";
const char kUserAllowlist[] = "*@ad-domain.com";
const char kUserNotMatchingAllowlist[] = "user@another_mail.com";
const char kPassword[] = "test_password";

const char kPublicSessionUserEmail[] = "public_session_user@localhost";
const char kPublicSessionSecondUserEmail[] =
    "public_session_second_user@localhost";
const int kAutoLoginNoDelay = 0;
const int kAutoLoginShortDelay = 1;
const int kAutoLoginLongDelay = 10000;

// Wait for cros settings to become permanently untrusted and run `callback`.
void WaitForPermanentlyUntrustedStatusAndRun(base::OnceClosure callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  while (true) {
    const CrosSettingsProvider::TrustedStatus status =
        CrosSettings::Get()->PrepareTrustedValues(
            base::BindOnce(&WaitForPermanentlyUntrustedStatusAndRun,
                           std::move(split_callback.first)));
    switch (status) {
      case CrosSettingsProvider::PERMANENTLY_UNTRUSTED:
        std::move(split_callback.second).Run();
        return;
      case CrosSettingsProvider::TEMPORARILY_UNTRUSTED:
        return;
      case CrosSettingsProvider::TRUSTED:
        content::RunAllPendingInMessageLoop();
        break;
    }
  }
}

base::FilePath GetKerberosConfigPath() {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_HOME, &path));
  return path.Append("kerberos").Append("krb5.conf");
}

base::FilePath GetKerberosCredentialsCachePath() {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_HOME, &path));
  return path.Append("kerberos").Append("krb5cc");
}

void EnableArcForProfile(Profile* profile) {
  arc::SetArcAvailableCommandLineForTesting(
      base::CommandLine::ForCurrentProcess());
  arc::ResetArcAllowedCheckForTesting(profile);
  arc::SetArcPlayStoreEnabledForProfile(profile, true);

  ArcAppListPrefsFactory::GetInstance()->RecreateServiceInstanceForTesting(
      profile);
  arc::ArcSessionManager::Get()->SetProfile(profile);
}

arc::data_snapshotd::ArcDataSnapshotdManager* arc_data_snapshotd_manager() {
  return arc::data_snapshotd::ArcDataSnapshotdManager::Get();
}

class UserProfileLoadedObserver
    : public session_manager::SessionManagerObserver {
 public:
  UserProfileLoadedObserver() {
    session_observation_.Observe(session_manager::SessionManager::Get());
  }

  // session_manager::SessionManagerObserver:
  void OnUserProfileLoaded(const AccountId& account_id) override {
    session_observation_.Reset();
    account_id_ = account_id;
    run_loop_.Quit();
  }

  AccountId Wait() {
    run_loop_.Run();
    DCHECK(!account_id_.empty());
    return account_id_;
  }

 private:
  base::RunLoop run_loop_;
  AccountId account_id_ = EmptyAccountId();
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};
};

}  // namespace

class ExistingUserControllerTest : public policy::DevicePolicyCrosBrowserTest {
 protected:
  ExistingUserControllerTest() = default;

  ExistingUserControllerTest(const ExistingUserControllerTest&) = delete;
  ExistingUserControllerTest& operator=(const ExistingUserControllerTest&) =
      delete;

  ExistingUserController* existing_user_controller() {
    return ExistingUserController::current_controller();
  }

  const ExistingUserController* existing_user_controller() const {
    return ExistingUserController::current_controller();
  }

  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    mock_login_display_host_ = std::make_unique<MockLoginDisplayHost>();
    mock_login_display_ = std::make_unique<MockLoginDisplay>();
    mock_signin_ui_ = std::make_unique<MockSigninUI>();
    SetUpLoginDisplay();
  }

  virtual void SetUpLoginDisplay() {
    EXPECT_CALL(*mock_login_display_host_, GetLoginDisplay())
        .Times(AnyNumber())
        .WillRepeatedly(Return(mock_login_display_.get()));
    EXPECT_CALL(*mock_login_display_host_, GetSigninUI())
        .Times(AnyNumber())
        .WillRepeatedly(Return(mock_signin_ui_.get()));
    EXPECT_CALL(*mock_login_display_, Init(_, true)).Times(1);
  }

  void SetUpOnMainThread() override {
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    existing_user_controller_ = std::make_unique<ExistingUserController>();
    EXPECT_CALL(*mock_login_display_host_, GetExistingUserController())
        .Times(AnyNumber())
        .WillRepeatedly(Return(existing_user_controller_.get()));
    ASSERT_EQ(existing_user_controller(), existing_user_controller_.get());

    existing_user_controller_->Init(user_manager::UserList());

    // Prevent browser start in user session so that we do not need to wait
    // for its initialization.
    test::UserSessionManagerTestApi(UserSessionManager::GetInstance())
        .SetShouldLaunchBrowserInTests(false);
  }

  void TearDownOnMainThread() override {
    DevicePolicyCrosBrowserTest::InProcessBrowserTest::TearDownOnMainThread();

    // `existing_user_controller_` has data members that are CrosSettings
    // observers. They need to be destructed before CrosSettings.
    Mock::VerifyAndClear(mock_login_display_host_.get());
    existing_user_controller_.reset();
    mock_login_display_host_.reset();

    // Test case may be configured with the real user manager but empty user
    // list initially. So network OOBE screen is initialized.
    // Need to reset it manually so that we don't end up with CrosSettings
    // observer that wasn't removed.
    WizardController* controller = WizardController::default_controller();
    if (controller && controller->current_screen())
      controller->current_screen()->Hide();
  }

  void ExpectLoginFailure() {
    EXPECT_CALL(*mock_login_display_, SetUIEnabled(false)).Times(1);
    EXPECT_CALL(*mock_signin_ui_,
                ShowSigninError(SigninError::kOwnerKeyLost, std::string()))
        .Times(1);
    EXPECT_CALL(*mock_login_display_, SetUIEnabled(true)).Times(1);
  }

  void MakeCrosSettingsPermanentlyUntrusted() {
    device_policy()->policy().set_policy_data_signature("bad signature");
    session_manager_client()->set_device_policy(device_policy()->GetBlob());
    session_manager_client()->OnPropertyChangeComplete(true);

    base::RunLoop run_loop;
    WaitForPermanentlyUntrustedStatusAndRun(run_loop.QuitClosure());
    run_loop.Run();
  }

  // ExistingUserController private member accessors.
  base::OneShotTimer* auto_login_timer() {
    return existing_user_controller()->auto_login_timer_.get();
  }

  AccountId auto_login_account_id() const {
    return existing_user_controller()->public_session_auto_login_account_id_;
  }

  int auto_login_delay() const {
    return existing_user_controller()->auto_login_delay_;
  }

  bool is_login_in_progress() const {
    return existing_user_controller()->is_login_in_progress_;
  }

  std::unique_ptr<ExistingUserController> existing_user_controller_;

  std::unique_ptr<MockSigninUI> mock_signin_ui_;
  std::unique_ptr<MockLoginDisplay> mock_login_display_;
  std::unique_ptr<MockLoginDisplayHost> mock_login_display_host_;

  const AccountId ad_account_id_ =
      AccountId::AdFromUserEmailObjGuid(kAdUsername, kObjectGuid);

  const LoginManagerMixin::TestUserInfo new_user_{
      AccountId::FromUserEmailGaiaId(kNewUser, kNewGaiaID)};
  const LoginManagerMixin::TestUserInfo existing_user_{
      AccountId::FromUserEmailGaiaId(kExistingUser, kExistingGaiaID)};

  CryptohomeMixin cryptohome_mixin_{&mixin_host_};
  LoginManagerMixin login_manager_{&mixin_host_,
                                   {existing_user_},
                                   nullptr,
                                   &cryptohome_mixin_};
};

IN_PROC_BROWSER_TEST_F(ExistingUserControllerTest, ExistingUserLogin) {
  EXPECT_CALL(*mock_login_display_, SetUIEnabled(false)).Times(2);
  EXPECT_CALL(*mock_login_display_, SetUIEnabled(true)).Times(1);
  EXPECT_CALL(*mock_login_display_host_,
              StartWizard(TermsOfServiceScreenView::kScreenId.AsId()))
      .Times(0);

  UserProfileLoadedObserver profile_loaded_observer;
  login_manager_.LoginWithDefaultContext(existing_user_);
  profile_loaded_observer.Wait();
}

// Verifies that when the cros settings are untrusted, no new session can be
// started.
class ExistingUserControllerUntrustedTest : public ExistingUserControllerTest {
 public:
  ExistingUserControllerUntrustedTest() = default;

  ExistingUserControllerUntrustedTest(
      const ExistingUserControllerUntrustedTest&) = delete;
  ExistingUserControllerUntrustedTest& operator=(
      const ExistingUserControllerUntrustedTest&) = delete;

  void SetUpOnMainThread() override {
    ExistingUserControllerTest::SetUpOnMainThread();
    MakeCrosSettingsPermanentlyUntrusted();
    ExpectLoginFailure();
  }
};

IN_PROC_BROWSER_TEST_F(ExistingUserControllerUntrustedTest,
                       ExistingUserLoginForbidden) {
  UserContext user_context(
      LoginManagerMixin::CreateDefaultUserContext(existing_user_));
  existing_user_controller()->Login(user_context, SigninSpecifics());
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerUntrustedTest,
                       NewUserLoginForbidden) {
  UserContext user_context(
      LoginManagerMixin::CreateDefaultUserContext(new_user_));
  existing_user_controller()->CompleteLogin(user_context);
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerUntrustedTest,
                       GuestLoginForbidden) {
  existing_user_controller()->Login(
      UserContext(user_manager::USER_TYPE_GUEST, EmptyAccountId()),
      SigninSpecifics());
}

MATCHER_P(HasDetails, expected, "") {
  return expected == *content::Details<const std::string>(arg).ptr();
}

class ExistingUserControllerPublicSessionTest
    : public ExistingUserControllerTest,
      public user_manager::UserManager::Observer {
 public:
  ExistingUserControllerPublicSessionTest(
      const ExistingUserControllerPublicSessionTest&) = delete;
  ExistingUserControllerPublicSessionTest& operator=(
      const ExistingUserControllerPublicSessionTest&) = delete;

 protected:
  ExistingUserControllerPublicSessionTest() {}

  void SetUpOnMainThread() override {
    ExistingUserControllerTest::SetUpOnMainThread();

    // By default ArcDataSnapshotdManager does not influence an auto login
    // flow.
    EXPECT_TRUE(arc_data_snapshotd_manager());
    EXPECT_TRUE(arc_data_snapshotd_manager()->IsAutoLoginAllowed());
    EXPECT_FALSE(arc_data_snapshotd_manager()->IsAutoLoginConfigured());

    // Wait for the public session user to be created.
    if (!user_manager::UserManager::Get()->IsKnownUser(
            public_session_account_id_)) {
      user_manager::UserManager::Get()->AddObserver(this);
      local_state_changed_run_loop_ = std::make_unique<base::RunLoop>();
      local_state_changed_run_loop_->Run();
      user_manager::UserManager::Get()->RemoveObserver(this);
    }
    EXPECT_TRUE(user_manager::UserManager::Get()->IsKnownUser(
        public_session_account_id_));

    // Wait for the device local account policy to be installed.
    policy::CloudPolicyStore* store =
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
    ExistingUserControllerTest::SetUpInProcessBrowserTestFixture();
    // Setup the device policy.
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    em::DeviceLocalAccountInfoProto* account =
        proto.mutable_device_local_accounts()->add_account();
    account->set_account_id(kPublicSessionUserEmail);
    account->set_type(
        em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
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

  void SetUpLoginDisplay() override {
    EXPECT_CALL(*mock_login_display_host_, GetLoginDisplay())
        .Times(AnyNumber())
        .WillRepeatedly(Return(mock_login_display_.get()));
    EXPECT_CALL(*mock_login_display_host_, GetSigninUI())
        .Times(AnyNumber())
        .WillRepeatedly(Return(mock_signin_ui_.get()));
    EXPECT_CALL(*mock_login_display_, Init(_, _)).Times(AnyNumber());
  }

  void TearDownOnMainThread() override {
    ExistingUserControllerTest::TearDownOnMainThread();

    // Test case may be configured with the real user manager but empty user
    // list initially. So network OOBE screen is initialized.
    // Need to reset it manually so that we don't end up with CrosSettings
    // observer that wasn't removed.
    WizardController* controller = WizardController::default_controller();
    if (controller && controller->current_screen())
      controller->current_screen()->Hide();
  }

  // user_manager::UserManager::Observer:
  void LocalStateChanged(user_manager::UserManager* user_manager) override {
    local_state_changed_run_loop_->Quit();
  }

  void ExpectSuccessfulLogin(const UserContext& user_context) {
    test::UserSessionManagerTestApi session_manager_test_api(
        UserSessionManager::GetInstance());
    session_manager_test_api.InjectStubUserContext(user_context);
    // There may be in-session oobe or an initial login screen created from
    // --login-manager.
    EXPECT_CALL(*mock_login_display_host_,
                StartWizard(LocaleSwitchView::kScreenId.AsId()))
        .Times(AnyNumber());
    EXPECT_CALL(*mock_login_display_, SetUIEnabled(false)).Times(AnyNumber());
    EXPECT_CALL(*mock_login_display_, SetUIEnabled(true)).Times(AnyNumber());
  }

  void SetAutoLoginPolicy(const std::string& user_email, int delay) {
    // Wait until ExistingUserController has finished auto-login
    // configuration by observing the same settings that trigger
    // ConfigureAutoLogin.

    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());

    // If both settings have changed we need to wait for both to
    // propagate, so check the new values against the old ones.
    scoped_refptr<content::MessageLoopRunner> runner1;
    base::CallbackListSubscription subscription1;
    if (!proto.has_device_local_accounts() ||
        !proto.device_local_accounts().has_auto_login_id() ||
        proto.device_local_accounts().auto_login_id() != user_email) {
      runner1 = new content::MessageLoopRunner;
      subscription1 = CrosSettings::Get()->AddSettingsObserver(
          kAccountsPrefDeviceLocalAccountAutoLoginId,
          base::BindLambdaForTesting([&]() { runner1->Quit(); }));
    }
    scoped_refptr<content::MessageLoopRunner> runner2;
    base::CallbackListSubscription subscription2;
    if (!proto.has_device_local_accounts() ||
        !proto.device_local_accounts().has_auto_login_delay() ||
        proto.device_local_accounts().auto_login_delay() != delay) {
      runner2 = new content::MessageLoopRunner;
      subscription2 = CrosSettings::Get()->AddSettingsObserver(
          kAccountsPrefDeviceLocalAccountAutoLoginDelay,
          base::BindLambdaForTesting([&]() { runner2->Quit(); }));
    }

    // Update the policy.
    proto.mutable_device_local_accounts()->set_auto_login_id(user_email);
    proto.mutable_device_local_accounts()->set_auto_login_delay(delay);
    RefreshDevicePolicy();

    // Wait for ExistingUserController to read the updated settings.
    if (runner1.get())
      runner1->Run();
    if (runner2.get())
      runner2->Run();
  }

  void ConfigureAutoLogin() {
    existing_user_controller()->ConfigureAutoLogin();
  }

  void FireAutoLogin() {
    existing_user_controller()->OnPublicSessionAutoLoginTimerFire();
  }

  const AccountId public_session_account_id_ =
      AccountId::FromUserEmail(policy::GenerateDeviceLocalAccountUserId(
          kPublicSessionUserEmail,
          policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION));

 private:
  std::unique_ptr<base::RunLoop> local_state_changed_run_loop_;
};

IN_PROC_BROWSER_TEST_F(ExistingUserControllerPublicSessionTest,
                       ConfigureAutoLoginUsingPolicy) {
  EXPECT_TRUE(!auto_login_account_id().is_valid());
  EXPECT_EQ(0, auto_login_delay());
  EXPECT_FALSE(auto_login_timer());

  // Set the policy.
  SetAutoLoginPolicy(kPublicSessionUserEmail, kAutoLoginLongDelay);
  EXPECT_EQ(public_session_account_id_, auto_login_account_id());
  EXPECT_EQ(kAutoLoginLongDelay, auto_login_delay());
  ASSERT_TRUE(auto_login_timer());
  EXPECT_TRUE(auto_login_timer()->IsRunning());

  // Unset the policy.
  SetAutoLoginPolicy("", 0);
  EXPECT_TRUE(!auto_login_account_id().is_valid());
  EXPECT_EQ(0, auto_login_delay());
  ASSERT_TRUE(auto_login_timer());
  EXPECT_FALSE(auto_login_timer()->IsRunning());
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerPublicSessionTest,
                       AutoLoginNoDelay) {
  // Set up mocks to check login success.
  UserContext user_context(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                           public_session_account_id_);
  user_context.SetUserIDHash(user_context.GetAccountId().GetUserEmail());
  ExpectSuccessfulLogin(user_context);

  // Start auto-login and wait for login tasks to complete.
  SetAutoLoginPolicy(kPublicSessionUserEmail, kAutoLoginNoDelay);
  content::RunAllPendingInMessageLoop();
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerPublicSessionTest,
                       AutoLoginShortDelay) {
  // Set up mocks to check login success.
  UserContext user_context(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                           public_session_account_id_);
  user_context.SetUserIDHash(user_context.GetAccountId().GetUserEmail());
  ExpectSuccessfulLogin(user_context);

  UserProfileLoadedObserver profile_loaded_observer;

  SetAutoLoginPolicy(kPublicSessionUserEmail, kAutoLoginShortDelay);
  ASSERT_TRUE(auto_login_timer());
  // Don't assert that timer is running: with the short delay sometimes
  // the trigger happens before the assert.  We've already tested that
  // the timer starts when it should.

  // Wait for the timer to fire.
  base::RunLoop runner;
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::Milliseconds(kAutoLoginShortDelay + 1),
              runner.QuitClosure());
  runner.Run();

  profile_loaded_observer.Wait();

  // Wait for login tasks to complete.
  content::RunAllPendingInMessageLoop();
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerPublicSessionTest,
                       LoginStopsAutoLogin) {
  // Set up mocks to check login success.
  UserContext user_context(
      LoginManagerMixin::CreateDefaultUserContext(new_user_));
  ExpectSuccessfulLogin(user_context);

  SetAutoLoginPolicy(kPublicSessionUserEmail, kAutoLoginLongDelay);
  EXPECT_TRUE(auto_login_timer());

  UserProfileLoadedObserver profile_loaded_observer;

  // Log in and check that it stopped the timer.
  existing_user_controller()->Login(user_context, SigninSpecifics());
  EXPECT_TRUE(is_login_in_progress());
  ASSERT_TRUE(auto_login_timer());
  EXPECT_FALSE(auto_login_timer()->IsRunning());

  profile_loaded_observer.Wait();

  // Wait for login tasks to complete.
  content::RunAllPendingInMessageLoop();

  // Timer should still be stopped after login completes.
  ASSERT_TRUE(auto_login_timer());
  EXPECT_FALSE(auto_login_timer()->IsRunning());
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerPublicSessionTest,
                       GuestModeLoginStopsAutoLogin) {
  EXPECT_CALL(*mock_login_display_, SetUIEnabled(false)).Times(2);
  UserContext user_context(
      LoginManagerMixin::CreateDefaultUserContext(new_user_));
  test::UserSessionManagerTestApi session_manager_test_api(
      UserSessionManager::GetInstance());
  session_manager_test_api.InjectStubUserContext(user_context);

  SetAutoLoginPolicy(kPublicSessionUserEmail, kAutoLoginLongDelay);
  EXPECT_TRUE(auto_login_timer());

  // Login and check that it stopped the timer.
  existing_user_controller()->Login(
      UserContext(user_manager::USER_TYPE_GUEST, EmptyAccountId()),
      SigninSpecifics());
  EXPECT_TRUE(is_login_in_progress());
  ASSERT_TRUE(auto_login_timer());
  EXPECT_FALSE(auto_login_timer()->IsRunning());

  // Wait for login tasks to complete.
  content::RunAllPendingInMessageLoop();

  // Timer should still be stopped after login completes.
  ASSERT_TRUE(auto_login_timer());
  EXPECT_FALSE(auto_login_timer()->IsRunning());
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerPublicSessionTest,
                       CompleteLoginStopsAutoLogin) {
  // Set up mocks to check login success.
  UserContext user_context(
      LoginManagerMixin::CreateDefaultUserContext(new_user_));
  ExpectSuccessfulLogin(user_context);

  SetAutoLoginPolicy(kPublicSessionUserEmail, kAutoLoginLongDelay);
  EXPECT_TRUE(auto_login_timer());

  UserProfileLoadedObserver profile_loaded_observer;

  // Check that login completes and stops the timer.
  existing_user_controller()->CompleteLogin(user_context);
  ASSERT_TRUE(auto_login_timer());
  EXPECT_FALSE(auto_login_timer()->IsRunning());

  profile_loaded_observer.Wait();

  // Wait for login tasks to complete.
  content::RunAllPendingInMessageLoop();

  // Timer should still be stopped after login completes.
  ASSERT_TRUE(auto_login_timer());
  EXPECT_FALSE(auto_login_timer()->IsRunning());
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerPublicSessionTest,
                       PublicSessionLoginStopsAutoLogin) {
  // Set up mocks to check login success.
  UserContext user_context(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                           public_session_account_id_);
  user_context.SetUserIDHash(user_context.GetAccountId().GetUserEmail());
  ExpectSuccessfulLogin(user_context);
  SetAutoLoginPolicy(kPublicSessionUserEmail, kAutoLoginLongDelay);
  EXPECT_TRUE(auto_login_timer());

  UserProfileLoadedObserver profile_loaded_observer;

  // Login and check that it stopped the timer.
  existing_user_controller()->Login(
      UserContext(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                  public_session_account_id_),
      SigninSpecifics());

  EXPECT_TRUE(is_login_in_progress());
  ASSERT_TRUE(auto_login_timer());
  EXPECT_FALSE(auto_login_timer()->IsRunning());

  profile_loaded_observer.Wait();

  // Wait for login tasks to complete.
  content::RunAllPendingInMessageLoop();

  // Timer should still be stopped after login completes.
  ASSERT_TRUE(auto_login_timer());
  EXPECT_FALSE(auto_login_timer()->IsRunning());
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerPublicSessionTest,
                       LoginForbiddenWhenUntrusted) {
  // Make cros settings untrusted.
  MakeCrosSettingsPermanentlyUntrusted();

  // Check that the attempt to start a public session fails with an error.
  ExpectLoginFailure();
  UserContext user_context(
      LoginManagerMixin::CreateDefaultUserContext(new_user_));
  existing_user_controller()->Login(user_context, SigninSpecifics());
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerPublicSessionTest,
                       NoAutoLoginWhenUntrusted) {
  // Start the public session timer.
  SetAutoLoginPolicy(kPublicSessionUserEmail, kAutoLoginLongDelay);
  EXPECT_TRUE(auto_login_timer());

  // Make cros settings untrusted.
  MakeCrosSettingsPermanentlyUntrusted();

  // Check that when the timer fires, auto-login fails with an error.
  ExpectLoginFailure();
  FireAutoLogin();
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerPublicSessionTest,
                       ArcDataSnapshotdAutoLogin) {
  arc_data_snapshotd_manager()->set_state_for_testing(
      arc::data_snapshotd::ArcDataSnapshotdManager::State::kBlockedUi);
  EXPECT_FALSE(arc_data_snapshotd_manager()->IsAutoLoginAllowed());
  EXPECT_TRUE(arc_data_snapshotd_manager()->IsAutoLoginConfigured());

  ConfigureAutoLogin();

  // Do not start an auto-login public account session when in blocked UI mode.
  EXPECT_TRUE(auto_login_account_id().is_valid());
  EXPECT_EQ(public_session_account_id_, auto_login_account_id());
  EXPECT_EQ(0, auto_login_delay());
  EXPECT_FALSE(auto_login_timer());
  auto& reset_autologin_callback =
      arc_data_snapshotd_manager()->get_reset_autologin_callback_for_testing();
  EXPECT_FALSE(reset_autologin_callback.is_null());

  // Allow to launch public account session (MGS).
  arc_data_snapshotd_manager()->set_state_for_testing(
      arc::data_snapshotd::ArcDataSnapshotdManager::State::kMgsToLaunch);
  EXPECT_TRUE(arc_data_snapshotd_manager()->IsAutoLoginAllowed());
  EXPECT_TRUE(arc_data_snapshotd_manager()->IsAutoLoginConfigured());

  // Set up mocks to check login success.
  UserContext user_context(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                           public_session_account_id_);
  user_context.SetUserIDHash(user_context.GetAccountId().GetUserEmail());
  ExpectSuccessfulLogin(user_context);

  std::move(reset_autologin_callback).Run();

  // Start auto-login and wait for login tasks to complete.
  content::RunAllPendingInMessageLoop();

  // Setup profile before changing the state.
  EXPECT_TRUE(user_manager::UserManager::Get()->IsLoggedInAsPublicAccount());
  EnableArcForProfile(ProfileManager::GetActiveUserProfile());

  arc_data_snapshotd_manager()->OnSnapshotSessionStarted();

  EXPECT_TRUE(auto_login_account_id().is_valid());
  EXPECT_EQ(0, auto_login_delay());
  EXPECT_TRUE(auto_login_timer());
  EXPECT_EQ(arc::data_snapshotd::ArcDataSnapshotdManager::State::kMgsLaunched,
            arc_data_snapshotd_manager()->state());
}

class ExistingUserControllerSecondPublicSessionTest
    : public ExistingUserControllerPublicSessionTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    ExistingUserControllerPublicSessionTest::SetUpInProcessBrowserTestFixture();
    AddSecondPublicSessionAccount();
  }

 private:
  void AddSecondPublicSessionAccount() {
    // Setup the device policy.
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    em::DeviceLocalAccountInfoProto* account =
        proto.mutable_device_local_accounts()->add_account();
    account->set_account_id(kPublicSessionSecondUserEmail);
    account->set_type(
        em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
    RefreshDevicePolicy();

    // Setup the device local account policy.
    policy::UserPolicyBuilder device_local_account_policy;
    device_local_account_policy.policy_data().set_username(
        kPublicSessionSecondUserEmail);
    device_local_account_policy.policy_data().set_policy_type(
        policy::dm_protocol::kChromePublicAccountPolicyType);
    device_local_account_policy.policy_data().set_settings_entity_id(
        kPublicSessionSecondUserEmail);
    device_local_account_policy.Build();
    session_manager_client()->set_device_local_account_policy(
        kPublicSessionSecondUserEmail, device_local_account_policy.GetBlob());
  }
};
// Test that if two public session accounts are configured for the device, auto
// login is not happening.
IN_PROC_BROWSER_TEST_F(ExistingUserControllerSecondPublicSessionTest,
                       ArcDataSnapshotdTwoAccounts) {
  // Allow to launch public account session (MGS).
  arc_data_snapshotd_manager()->set_state_for_testing(
      arc::data_snapshotd::ArcDataSnapshotdManager::State::kMgsToLaunch);
  EXPECT_TRUE(arc_data_snapshotd_manager()->IsAutoLoginAllowed());
  EXPECT_TRUE(arc_data_snapshotd_manager()->IsAutoLoginConfigured());

  ConfigureAutoLogin();

  // Do not configure auto login if more than one public session is configured.
  EXPECT_FALSE(auto_login_account_id().is_valid());
  EXPECT_EQ(0, auto_login_delay());
  EXPECT_FALSE(auto_login_timer());
}

class ExistingUserControllerActiveDirectoryTest
    : public ExistingUserControllerTest {
 public:
  ExistingUserControllerActiveDirectoryTest() {
    // All tests related to Active Directory login don't make sense when the
    // kChromadAvailable feature is disabled. We also don't need to verify that
    // the device is disabled in that case, because the Chromad disabling
    // feature is already tested in `device_disabling_manager_unittest.cc`.
    scoped_feature_list_.InitAndEnableFeature(features::kChromadAvailable);
  }

  // Overriden from ExistingUserControllerTest:
  void SetUp() override {
    device_state_.SetState(
        DeviceStateMixin::State::OOBE_COMPLETED_ACTIVE_DIRECTORY_ENROLLED);
    ExistingUserControllerTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    ExistingUserControllerTest::SetUpInProcessBrowserTestFixture();

    // This is called before ChromeBrowserMain initializes the fake dbus
    // clients, and DisableOperationDelayForTesting() needs to be called before
    // other ChromeBrowserMain initialization occurs.
    AuthPolicyClient::InitializeFake();
    FakeAuthPolicyClient::Get()->DisableOperationDelayForTesting();
    // Required for tpm_util. Will be destroyed in browser shutdown.
    UserDataAuthClient::InitializeFake();

    RefreshDevicePolicy();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void TearDownOnMainThread() override {
    base::RunLoop().RunUntilIdle();
    ExistingUserControllerTest::TearDownOnMainThread();
  }

 protected:
  // Needs to be a member because this class is a friend of
  // AuthPolicyCredentialsManagerFactory to access GetServiceForBrowserContext.
  KerberosFilesHandler* GetKerberosFilesHandler() {
    auto* authpolicy_credentials_manager =
        static_cast<AuthPolicyCredentialsManager*>(
            AuthPolicyCredentialsManagerFactory::GetInstance()
                ->GetServiceForBrowserContext(
                    ProfileManager::GetLastUsedProfile(), false /* create */));
    EXPECT_TRUE(authpolicy_credentials_manager);
    return authpolicy_credentials_manager->GetKerberosFilesHandlerForTesting();
  }

  void LoginAdOnline() {
    ExpectLoginSuccess();
    UserContext user_context(user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY,
                             ad_account_id_);
    user_context.SetKey(Key(kPassword));
    user_context.SetUserIDHash(ad_account_id_.GetUserEmail());
    user_context.SetAuthFlow(UserContext::AUTH_FLOW_ACTIVE_DIRECTORY);
    ASSERT_EQ(user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY,
              user_context.GetUserType());
    UserProfileLoadedObserver profile_loaded_observer;
    existing_user_controller()->CompleteLogin(user_context);

    profile_loaded_observer.Wait();

    // This only works if no RunLoop::Run() call is made after the Kerberos file
    // writer task has been posted. Ideally, SetFilesChangedForTesting() should
    // be called before the task is posted, but we don't have a profile yet at
    // that point, so we can't get the files handler.
    base::RunLoop run_loop;
    GetKerberosFilesHandler()->SetFilesChangedForTesting(
        run_loop.QuitClosure());
    run_loop.Run();
    CheckKerberosFiles(true /* enable_dns_cname_lookup */);
  }

  void UpdateProviderPolicy(const policy::PolicyMap& policy) {
    policy_provider_.UpdateChromePolicy(policy);
  }

  void ExpectLoginFailure() {
    EXPECT_CALL(*mock_login_display_, SetUIEnabled(false)).Times(2);
    EXPECT_CALL(
        *mock_signin_ui_,
        ShowSigninError(SigninError::kGoogleAccountNotAllowed,
                        "Google accounts are not allowed on this device"))
        .Times(1);
    EXPECT_CALL(*mock_login_display_, SetUIEnabled(true)).Times(1);
  }

  void ExpectLoginAllowlistFailure() {
    EXPECT_CALL(*mock_login_display_, SetUIEnabled(false)).Times(2);
    EXPECT_CALL(*mock_login_display_host_, ShowAllowlistCheckFailedError())
        .Times(1);
    EXPECT_CALL(*mock_login_display_, SetUIEnabled(true)).Times(1);
  }

  void ExpectLoginSuccess() {
    EXPECT_CALL(*mock_login_display_, SetUIEnabled(false)).Times(2);
    EXPECT_CALL(*mock_login_display_, SetUIEnabled(true)).Times(1);
  }

  std::string GetExpectedKerberosConfig(bool enable_dns_cname_lookup) {
    std::string config(base::StringPrintf(
        kKrb5CnameSettings, enable_dns_cname_lookup ? "true" : "false"));
    config += FakeAuthPolicyClient::Get()->user_kerberos_conf();
    return config;
  }

  void CheckKerberosFiles(bool enable_dns_cname_lookup) {
    base::ScopedAllowBlockingForTesting allow_io;
    std::string file_contents;
    EXPECT_TRUE(
        base::ReadFileToString(GetKerberosConfigPath(), &file_contents));
    EXPECT_EQ(GetExpectedKerberosConfig(enable_dns_cname_lookup),
              file_contents);

    EXPECT_TRUE(base::ReadFileToString(GetKerberosCredentialsCachePath(),
                                       &file_contents));
    EXPECT_EQ(file_contents,
              FakeAuthPolicyClient::Get()->user_kerberos_creds());
  }

  // Applies policy and waits until both config and credentials files changed.
  void ApplyPolicyAndWaitFilesChanged(bool enable_dns_cname_lookup) {
    policy::PolicyMap policies;
    policies.Set(policy::key::kDisableAuthNegotiateCnameLookup,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(!enable_dns_cname_lookup), nullptr);
    base::RunLoop run_loop;
    GetKerberosFilesHandler()->SetFilesChangedForTesting(
        run_loop.QuitClosure());
    UpdateProviderPolicy(policies);
    run_loop.Run();
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ExistingUserControllerActiveDirectoryTestCreateProfileDir
    : public ExistingUserControllerActiveDirectoryTest {
 public:
  ExistingUserControllerActiveDirectoryTestCreateProfileDir() {
    cryptohome_mixin_.MarkUserAsExisting(ad_account_id_);
  }

 private:
  CryptohomeMixin cryptohome_mixin_{&mixin_host_};
};

class ExistingUserControllerActiveDirectoryUserAllowlistTest
    : public ExistingUserControllerActiveDirectoryTest {
 public:
  ExistingUserControllerActiveDirectoryUserAllowlistTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    ExistingUserControllerActiveDirectoryTest::
        SetUpInProcessBrowserTestFixture();
    em::ChromeDeviceSettingsProto device_policy;
    device_policy.mutable_user_allowlist()->add_user_allowlist()->assign(
        kUserAllowlist);
    device_policy.mutable_allow_new_users()->set_allow_new_users(false);
    FakeAuthPolicyClient::Get()->set_device_policy(device_policy);
  }

  void SetUpLoginDisplay() override {
    EXPECT_CALL(*mock_login_display_host_, GetLoginDisplay())
        .Times(AnyNumber())
        .WillRepeatedly(Return(mock_login_display_.get()));
    EXPECT_CALL(*mock_login_display_host_, GetSigninUI())
        .Times(AnyNumber())
        .WillRepeatedly(Return(mock_signin_ui_.get()));
    EXPECT_CALL(*mock_login_display_, Init(_, true)).Times(1);
  }
};

// Tests that Active Directory online login succeeds on the Active Directory
// managed device.
IN_PROC_BROWSER_TEST_F(ExistingUserControllerActiveDirectoryTest,
                       ActiveDirectoryOnlineLogin_Success) {
  LoginAdOnline();
}

// Tests if DisabledAuthNegotiateCnameLookup changes trigger updating user
// Kerberos files.
IN_PROC_BROWSER_TEST_F(ExistingUserControllerActiveDirectoryTest,
                       PolicyChangeTriggersFileUpdate) {
  LoginAdOnline();

  ApplyPolicyAndWaitFilesChanged(false /* enable_dns_cname_lookup */);
  CheckKerberosFiles(false /* enable_dns_cname_lookup */);

  ApplyPolicyAndWaitFilesChanged(true /* enable_dns_cname_lookup */);
  CheckKerberosFiles(true /* enable_dns_cname_lookup */);
}

// Tests if user Kerberos files changed D-Bus signal triggers updating user
// Kerberos files.
IN_PROC_BROWSER_TEST_F(ExistingUserControllerActiveDirectoryTest,
                       UserKerberosFilesChangedSignalTriggersFileUpdate) {
  LoginAdOnline();

  // Set authpolicyd's copy of the Kerberos files and wait until Chrome's copy
  // has updated.
  base::RunLoop run_loop;
  GetKerberosFilesHandler()->SetFilesChangedForTesting(run_loop.QuitClosure());
  FakeAuthPolicyClient::Get()->SetUserKerberosFiles("new_kerberos_creds",
                                                    "new_kerberos_config");
  run_loop.Run();
  CheckKerberosFiles(true /* enable_dns_cname_lookup */);
}

// Tests that Active Directory offline login succeeds on the Active Directory
// managed device.
IN_PROC_BROWSER_TEST_F(
    ExistingUserControllerActiveDirectoryTestCreateProfileDir,
    ActiveDirectoryOfflineLogin_Success) {
  ExpectLoginSuccess();
  UserContext user_context(user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY,
                           ad_account_id_);
  user_context.SetKey(Key(kPassword));
  user_context.SetUserIDHash(ad_account_id_.GetUserEmail());
  ASSERT_EQ(user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY,
            user_context.GetUserType());

  UserProfileLoadedObserver profile_loaded_observer;
  existing_user_controller()->Login(user_context, SigninSpecifics());
  profile_loaded_observer.Wait();

  // Note: Can't call SetFilesChangedForTesting() earlier, see LoginAdOnline().
  base::RunLoop run_loop;
  GetKerberosFilesHandler()->SetFilesChangedForTesting(run_loop.QuitClosure());
  run_loop.Run();
  CheckKerberosFiles(true /* enable_dns_cname_lookup */);
}

// Tests that Gaia login fails on the Active Directory managed device.
IN_PROC_BROWSER_TEST_F(ExistingUserControllerActiveDirectoryTest,
                       GAIAAccountLogin_Failure) {
  ExpectLoginFailure();
  UserContext user_context(
      LoginManagerMixin::CreateDefaultUserContext(new_user_));
  existing_user_controller()->CompleteLogin(user_context);
}

// Tests that authentication succeeds if user email matches allowlist.
IN_PROC_BROWSER_TEST_F(ExistingUserControllerActiveDirectoryUserAllowlistTest,
                       Success) {
  ExpectLoginSuccess();
  UserContext user_context(user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY,
                           ad_account_id_);
  user_context.SetKey(Key(kPassword));
  user_context.SetUserIDHash(ad_account_id_.GetUserEmail());
  user_context.SetAuthFlow(UserContext::AUTH_FLOW_ACTIVE_DIRECTORY);
  ASSERT_EQ(user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY,
            user_context.GetUserType());
  UserProfileLoadedObserver profile_loaded_observer;
  existing_user_controller()->CompleteLogin(user_context);

  profile_loaded_observer.Wait();
}

// Tests that authentication fails if user email does not match allowlist.
IN_PROC_BROWSER_TEST_F(ExistingUserControllerActiveDirectoryUserAllowlistTest,
                       Fail) {
  ExpectLoginAllowlistFailure();
  AccountId account_id =
      AccountId::AdFromUserEmailObjGuid(kUserNotMatchingAllowlist, kObjectGuid);
  UserContext user_context(user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY,
                           account_id);
  user_context.SetKey(Key(kPassword));
  user_context.SetUserIDHash(account_id.GetUserEmail());
  user_context.SetAuthFlow(UserContext::AUTH_FLOW_ACTIVE_DIRECTORY);
  ASSERT_EQ(user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY,
            user_context.GetUserType());
  existing_user_controller()->CompleteLogin(user_context);
}

class ExistingUserControllerSavePasswordHashTest
    : public ExistingUserControllerTest {
 public:
  ExistingUserControllerSavePasswordHashTest() = default;
  ~ExistingUserControllerSavePasswordHashTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    ExistingUserControllerTest::SetUpInProcessBrowserTestFixture();
    RefreshDevicePolicy();
  }
};

// Tests that successful GAIA online login saves SyncPasswordData to user
// profile prefs.
IN_PROC_BROWSER_TEST_F(ExistingUserControllerSavePasswordHashTest,
                       GaiaOnlineLoginSavesPasswordHashToPrefs) {
  UserContext user_context(
      LoginManagerMixin::CreateDefaultUserContext(new_user_));
  UserProfileLoadedObserver profile_loaded_observer;
  existing_user_controller()->CompleteLogin(user_context);

  AccountId account_id = profile_loaded_observer.Wait();

  // Verify password hash and salt are saved to prefs.
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  EXPECT_TRUE(profile->GetPrefs()->HasPrefPath(
      password_manager::prefs::kPasswordHashDataList));
}

// Tests that successful offline login saves SyncPasswordData to user profile
// prefs.
IN_PROC_BROWSER_TEST_F(ExistingUserControllerSavePasswordHashTest,
                       OfflineLoginSavesPasswordHashToPrefs) {
  UserContext user_context(
      LoginManagerMixin::CreateDefaultUserContext(existing_user_));
  UserProfileLoadedObserver profile_loaded_observer;
  existing_user_controller()->Login(user_context, SigninSpecifics());

  AccountId account_id = profile_loaded_observer.Wait();

  // Verify password hash and salt are saved to prefs.
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  EXPECT_TRUE(profile->GetPrefs()->HasPrefPath(
      password_manager::prefs::kPasswordHashDataList));
}

// Tests different auth failures for an existing user login attempts.
class ExistingUserControllerAuthFailureTest : public OobeBaseTest {
 public:
  ExistingUserControllerAuthFailureTest() = default;
  ~ExistingUserControllerAuthFailureTest() override = default;

  void SetUpStubAuthenticatorAndAttemptLogin(
      AuthFailure::FailureReason failure_reason) {
    const UserContext user_context =
        LoginManagerMixin::CreateDefaultUserContext(test_user_);

    auto authenticator_builder =
        std::make_unique<StubAuthenticatorBuilder>(user_context);
    if (failure_reason != AuthFailure::NONE)
      authenticator_builder->SetUpAuthFailure(failure_reason);

    test::UserSessionManagerTestApi(UserSessionManager::GetInstance())
        .InjectAuthenticatorBuilder(std::move(authenticator_builder));

    // Some auth failure tests verify that error bubble is shown in the login
    // UI. Given that login UI ignores auth failures, unless it attempted a
    // login, the login attempt cannot be shortcut by login manager mixin API -
    // it has to go through login UI.
    const std::string& password = user_context.GetKey()->GetSecret();
    LoginScreenTestApi::SubmitPassword(test_user_.account_id, password,
                                       true /*check_if_submittable*/);
  }

  void SetUpStubAuthenticatorAndAttemptLoginWithWrongPassword() {
    const UserContext user_context =
        LoginManagerMixin::CreateDefaultUserContext(test_user_);
    auto authenticator_builder =
        std::make_unique<StubAuthenticatorBuilder>(user_context);
    test::UserSessionManagerTestApi(UserSessionManager::GetInstance())
        .InjectAuthenticatorBuilder(std::move(authenticator_builder));

    LoginScreenTestApi::SubmitPassword(test_user_.account_id, "wrong!!!!!",
                                       true /*check_if_submittable*/);
  }

  // Waits for auth error message to be shown in login UI.
  void WaitForAuthErrorMessage() {
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(LoginScreenTestApi::IsAuthErrorBubbleShown());
  }

 protected:
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId("user@gmail.com", "user")};
  LoginManagerMixin login_manager_{&mixin_host_, {test_user_}};
};

IN_PROC_BROWSER_TEST_F(ExistingUserControllerAuthFailureTest,
                       CryptohomeMissing) {
  SetUpStubAuthenticatorAndAttemptLogin(AuthFailure::MISSING_CRYPTOHOME);

  WaitForGaiaPageLoad();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_EQ(fake_gaia_.fake_gaia()->prefilled_email(),
            test_user_.account_id.GetUserEmail());

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(test_user_.account_id);
  ASSERT_TRUE(user);
  EXPECT_TRUE(user->force_online_signin());
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerAuthFailureTest,
                       CryptohomeUnrecoverable) {
  SetUpStubAuthenticatorAndAttemptLogin(AuthFailure::UNRECOVERABLE_CRYPTOHOME);

  WaitForGaiaPageLoad();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_EQ(fake_gaia_.fake_gaia()->prefilled_email(),
            test_user_.account_id.GetUserEmail());

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(test_user_.account_id);
  ASSERT_TRUE(user);
  EXPECT_TRUE(user->force_online_signin());
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerAuthFailureTest, TpmError) {
  SetUpStubAuthenticatorAndAttemptLogin(AuthFailure::TPM_ERROR);

  OobeScreenWaiter(TpmErrorView::kScreenId).Wait();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());

  EXPECT_EQ(
      0, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());

  test::TapOnPathAndWaitForOobeToBeDestroyed(
      {"tpm-error-message", "restartButton"});

  EXPECT_EQ(
      1, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerAuthFailureTest, OwnerRequired) {
  SetUpStubAuthenticatorAndAttemptLogin(AuthFailure::OWNER_REQUIRED);
  WaitForAuthErrorMessage();
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerAuthFailureTest, WrongPassword) {
  SetUpStubAuthenticatorAndAttemptLoginWithWrongPassword();
  WaitForAuthErrorMessage();
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerAuthFailureTest,
                       WrongPasswordWhileOffline) {
  NetworkStateTestHelper network_state_test_helper(
      false /*use_default_devices_and_services*/);
  network_state_test_helper.ClearServices();

  SetUpStubAuthenticatorAndAttemptLoginWithWrongPassword();
  WaitForAuthErrorMessage();
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerAuthFailureTest,
                       CryptohomeUnavailable) {
  FakeUserDataAuthClient::TestApi::Get()->SetServiceIsAvailable(false);
  SetUpStubAuthenticatorAndAttemptLogin(AuthFailure::NONE);

  FakeUserDataAuthClient::TestApi::Get()->ReportServiceIsNotAvailable();
  WaitForAuthErrorMessage();
}

class ExistingUserControllerProfileTest : public LoginManagerTest {
 public:
  ExistingUserControllerProfileTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
    // Login as a managed user would save force-online-signin to true and
    // invalidate the auth token into local state, which would prevent to focus
    // during the second part of the test which happens in the login screen.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSkipForceOnlineSignInForTesting);
  }

  void TearDownInProcessBrowserTestFixture() override {
    LoginManagerTest::TearDownInProcessBrowserTestFixture();
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kSkipForceOnlineSignInForTesting);
  }

 protected:
  void SetManagedBy(std::string managed_by) {
    std::unique_ptr<ScopedUserPolicyUpdate> scoped_user_policy_update =
        user_policy_mixin_.RequestPolicyUpdate();
    if (!managed_by.empty()) {
      scoped_user_policy_update->policy_data()->set_managed_by(managed_by);
    } else {
      scoped_user_policy_update->policy_data()->clear_managed_by();
    }
  }

  void Login(const LoginManagerMixin::TestUserInfo& test_user) {
    login_manager_mixin_.SkipPostLoginScreens();

    auto context = LoginManagerMixin::CreateDefaultUserContext(test_user);
    login_manager_mixin_.LoginAndWaitForActiveSession(context);
  }

  std::u16string ConstructManagedSessionUserWarning(std::string manager) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_MANAGED_SESSION_MONITORING_USER_WARNING,
        base::UTF8ToUTF16(manager));
  }

  const LoginManagerMixin::TestUserInfo not_managed_user_{
      AccountId::FromUserEmailGaiaId(kNewUser, kNewGaiaID)};
  const LoginManagerMixin::TestUserInfo managed_user_{
      AccountId::FromUserEmailGaiaId(kManagedUser, kManagedGaiaID)};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, managed_user_.account_id};
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(ExistingUserControllerProfileTest,
                       ManagedUserManagedBy) {
  SetManagedBy(kManager);
  Login(managed_user_);

  user_manager::KnownUser known_user(g_browser_process->local_state());
  EXPECT_TRUE(known_user.GetIsEnterpriseManaged(managed_user_.account_id));

  // Verify that managed_by has been stored in prefs
  const std::string* manager =
      known_user.GetAccountManager(managed_user_.account_id);
  ASSERT_TRUE(manager);
  EXPECT_EQ(*manager, kManager);

  // Set the lock screen so that the managed warning can be queried.
  ScreenLockerTester().Lock();

  EXPECT_TRUE(LoginScreenTestApi::IsManagedMessageInDialogShown(
      managed_user_.account_id));

  // Verify that the lock screen text uses the prefs value for its construction.
  EXPECT_EQ(
      LoginScreenTestApi::GetManagementDisclosureText(managed_user_.account_id),
      ConstructManagedSessionUserWarning(kManager));
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerProfileTest, ManagedUserDomain) {
  SetManagedBy(std::string());
  Login(managed_user_);
  user_manager::KnownUser known_user(g_browser_process->local_state());
  EXPECT_TRUE(known_user.GetIsEnterpriseManaged(managed_user_.account_id));

  // Verify that managed_by has been stored in prefs
  const std::string* manager =
      known_user.GetAccountManager(managed_user_.account_id);
  ASSERT_TRUE(manager);
  EXPECT_EQ(*manager, kManagedDomain);

  // Set the lock screen so that the managed warning can be queried.
  ScreenLockerTester().Lock();

  EXPECT_TRUE(LoginScreenTestApi::IsManagedMessageInDialogShown(
      managed_user_.account_id));

  // Verify that the lock screen text uses the prefs value for its construction.
  EXPECT_EQ(
      LoginScreenTestApi::GetManagementDisclosureText(managed_user_.account_id),
      ConstructManagedSessionUserWarning(kManagedDomain));
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerProfileTest, NotManagedUserLogin) {
  Login(not_managed_user_);
  user_manager::KnownUser known_user(g_browser_process->local_state());
  EXPECT_FALSE(known_user.GetIsEnterpriseManaged(not_managed_user_.account_id));

  // Verify that no value is stored in prefs for this user.
  EXPECT_FALSE(known_user.GetAccountManager(not_managed_user_.account_id));

  ScreenLockerTester().Lock();

  // Verify that no managed warning is shown for an unmanaged user.
  EXPECT_FALSE(LoginScreenTestApi::IsManagedMessageInDialogShown(
      not_managed_user_.account_id));

  // Verify that the owner user gets saved into local state.
  absl::optional<std::string> owner =
      user_manager::UserManager::Get()->GetOwnerEmail();
  ASSERT_TRUE(owner.has_value());
  EXPECT_EQ(owner.value(), not_managed_user_.account_id.GetUserEmail());
}

}  // namespace ash
