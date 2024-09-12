// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/existing_user_controller.h"

#include <string>
#include <vector>

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
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/mock_login_display_host.h"
#include "chrome/browser/ui/ash/login/mock_signin_ui.h"
#include "chrome/browser/ui/ash/login/signin_ui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/locale_switch_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/tpm_error_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/stub_authenticator_builder.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/settings/cros_settings.h"
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
#include "components/policy/core/common/device_local_account_type.h"
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
 public:
  ExistingUserControllerTest(const ExistingUserControllerTest&) = delete;
  ExistingUserControllerTest& operator=(const ExistingUserControllerTest&) =
      delete;

 protected:
  ExistingUserControllerTest() = default;

  ExistingUserController* existing_user_controller() {
    return ExistingUserController::current_controller();
  }

  const ExistingUserController* existing_user_controller() const {
    return ExistingUserController::current_controller();
  }

  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    mock_login_display_host_ = std::make_unique<MockLoginDisplayHost>();
    mock_signin_ui_ = std::make_unique<MockSigninUI>();
    SetUpLoginDisplay();
  }

  virtual void SetUpLoginDisplay() {
    ON_CALL(*mock_login_display_host_, GetSigninUI())
        .WillByDefault(Return(mock_signin_ui_.get()));

    ON_CALL(*mock_login_display_host_, GetWizardContext())
        .WillByDefault(Return(&dummy_wizard_context_));
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

    ash::AuthEventsRecorder::Get()->OnAuthenticationSurfaceChange(
        AuthEventsRecorder::AuthenticationSurface::kLogin);
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
    EXPECT_CALL(*mock_signin_ui_,
                ShowSigninError(SigninError::kOwnerKeyLost, std::string()))
        .Times(1);
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

  WizardContext dummy_wizard_context_;
};

IN_PROC_BROWSER_TEST_F(ExistingUserControllerTest, ExistingUserLogin) {
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
      UserContext(user_manager::UserType::kGuest, EmptyAccountId()),
      SigninSpecifics());
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
  ExistingUserControllerPublicSessionTest() = default;

  void SetUpOnMainThread() override {
    ExistingUserControllerTest::SetUpOnMainThread();

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
          policy::DeviceLocalAccountType::kPublicSession));

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
  UserContext user_context(user_manager::UserType::kPublicAccount,
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
  UserContext user_context(user_manager::UserType::kPublicAccount,
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
  UserContext user_context(
      LoginManagerMixin::CreateDefaultUserContext(new_user_));
  test::UserSessionManagerTestApi session_manager_test_api(
      UserSessionManager::GetInstance());
  session_manager_test_api.InjectStubUserContext(user_context);

  SetAutoLoginPolicy(kPublicSessionUserEmail, kAutoLoginLongDelay);
  EXPECT_TRUE(auto_login_timer());

  // Login and check that it stopped the timer.
  existing_user_controller()->Login(
      UserContext(user_manager::UserType::kGuest, EmptyAccountId()),
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
  UserContext user_context(user_manager::UserType::kPublicAccount,
                           public_session_account_id_);
  user_context.SetUserIDHash(user_context.GetAccountId().GetUserEmail());
  ExpectSuccessfulLogin(user_context);
  SetAutoLoginPolicy(kPublicSessionUserEmail, kAutoLoginLongDelay);
  EXPECT_TRUE(auto_login_timer());

  UserProfileLoadedObserver profile_loaded_observer;

  // Login and check that it stopped the timer.
  existing_user_controller()->Login(
      UserContext(user_manager::UserType::kPublicAccount,
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
      /*use_default_devices_and_services=*/false);
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
    login_manager_mixin_.LoginAsNewRegularUser(context);
    login_manager_mixin_.WaitForActiveSession();
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
  EXPECT_TRUE(
      LoginScreenTestApi::ShowRemoveAccountDialog(managed_user_.account_id));

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
  EXPECT_TRUE(
      LoginScreenTestApi::ShowRemoveAccountDialog(managed_user_.account_id));

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
  EXPECT_TRUE(LoginScreenTestApi::ShowRemoveAccountDialog(
      not_managed_user_.account_id));

  // Verify that no managed warning is shown for an unmanaged user.
  EXPECT_FALSE(LoginScreenTestApi::IsManagedMessageInDialogShown(
      not_managed_user_.account_id));
}

}  // namespace ash
