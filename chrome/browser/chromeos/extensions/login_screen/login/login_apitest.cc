// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/login_api.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/login_screen/login_screen_apitest_base.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kAccountId[] = "public-session@test";
constexpr char kPassword[] = "password";
constexpr char kWrongPassword[] = "wrong password";
constexpr char kData[] = "some data";
constexpr char kInSessionExtensionId[] = "ofcpkomnogjenhfajfjadjmjppbegnad";
constexpr char kInSessionExtensionCrxPath[] =
    "extensions/api_test/login_screen_apis/in_session_extension.crx";

// launchManagedGuestSession tests.
constexpr char kLaunchManagedGuestSession[] = "LoginLaunchManagedGuestSession";
constexpr char kLaunchManagedGuestSessionWithPassword[] =
    "LoginLaunchManagedGuestSessionWithPassword";
constexpr char kLaunchManagedGuestSessionNoAccounts[] =
    "LoginLaunchManagedGuestSessionNoAccounts";
constexpr char kLaunchManagedGuestSessionAlreadyExistsActiveSession[] =
    "LoginLaunchManagedGuestSessionAlreadyExistsActiveSession";
// exitCurrentSession tests.
constexpr char kExitCurrentSession[] = "LoginExitCurrentSession";
// fetchDataForNextLoginAttempt tests.
constexpr char kFetchDataForNextLoginAttempt[] =
    "LoginFetchDataForNextLoginAttempt";
// lockManagedGuestSession tests.
constexpr char kLockManagedGuestSessionNotActive[] =
    "LoginLockManagedGuestSessionNotActive";
// onExternalLogoutDone tests.
constexpr char kLoginOnExternalLogoutDone[] = "LoginOnExternalLogoutDone";
constexpr char kInSessionLoginNotifyExternalLogoutDone[] =
    "InSessionLoginNotifyExternalLogoutDone";
// onRequestExternalLogout tests.
constexpr char kLoginRequestExternalLogout[] = "LoginRequestExternalLogout";
constexpr char kInSessionLoginOnRequestExternalLogout[] =
    "InSessionLoginOnRequestExternalLogout";
// unlockManagedGuestSession tests.
constexpr char kUnlockManagedGuestSession[] = "LoginUnlockManagedGuestSession";
constexpr char kUnlockManagedGuestSessionWrongPassword[] =
    "LoginUnlockManagedGuestSessionWrongPassword";
constexpr char kUnlockManagedGuestSessionNotLocked[] =
    "LoginUnlockManagedGuestSessionNotLocked";
// In-session extension tests.
constexpr char kInSessionLoginLockManagedGuestSession[] =
    "InSessionLoginLockManagedGuestSession";

// External logout listener set up messages.
constexpr char kOnExternalLogoutDoneLoginScreenMessage[] =
    "onExternalLogoutDoneLoginScreenMessage";
constexpr char kOnRequestExternalLogoutInSessionMessage[] =
    "onRequestExternalLogoutInSessionMessage";

}  // namespace

namespace chromeos {

class LoginApitest : public LoginScreenApitestBase {
 public:
  LoginApitest() : LoginScreenApitestBase(version_info::Channel::CANARY) {}

  LoginApitest(const LoginApitest&) = delete;

  LoginApitest& operator=(const LoginApitest&) = delete;

  ~LoginApitest() override = default;

  void SetUpDeviceLocalAccountPolicy() {
    enterprise_management::ChromeDeviceSettingsProto& proto(
        device_policy()->payload());
    enterprise_management::DeviceLocalAccountsProto* device_local_accounts =
        proto.mutable_device_local_accounts();
    enterprise_management::DeviceLocalAccountInfoProto* const account =
        device_local_accounts->add_account();
    account->set_account_id(kAccountId);
    account->set_type(enterprise_management::DeviceLocalAccountInfoProto::
                          ACCOUNT_TYPE_PUBLIC_SESSION);
    RefreshDevicePolicy();
    policy_test_server_mixin_.UpdateDevicePolicy(proto);
  }

  void SetUpSessionExtensionUserPolicyBuilder() {
    user_policy_builder_ = std::make_unique<policy::UserPolicyBuilder>();
    enterprise_management::PolicyData& policy_data =
        user_policy_builder_->policy_data();
    policy_data.set_public_key_version(1);
    user_policy_builder_->SetDefaultSigningKey();
  }

  void RefreshPolicies() {
    base::RunLoop run_loop;
    g_browser_process->policy_service()->RefreshPolicies(
        run_loop.QuitClosure(), policy::PolicyFetchReason::kTest);
    run_loop.Run();
  }

  std::unique_ptr<extensions::TestExtensionRegistryObserver>
  GetTestExtensionRegistryObserver(const std::string& extension_id) {
    const user_manager::User* active_user =
        user_manager::UserManager::Get()->GetActiveUser();
    Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(active_user);
    return std::make_unique<extensions::TestExtensionRegistryObserver>(
        extensions::ExtensionRegistry::Get(profile), extension_id);
  }

  virtual void SetUpInSessionExtension() {
    SetUpSessionExtensionUserPolicyBuilder();

    const user_manager::User* active_user =
        user_manager::UserManager::Get()->GetActiveUser();
    Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(active_user);

    extension_force_install_mixin_.InitWithEmbeddedPolicyMixin(
        profile, &policy_test_server_mixin_, user_policy_builder_.get(),
        kAccountId, policy::dm_protocol::kChromePublicAccountPolicyType);

    extensions::ExtensionId extension_id;
    EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromCrx(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
            .AppendASCII(kInSessionExtensionCrxPath),
        ExtensionForceInstallMixin::WaitMode::kLoad, &extension_id));

    const extensions::Extension* extension =
        extension_force_install_mixin_.GetEnabledExtension(extension_id);
    ASSERT_TRUE(extension);
    ASSERT_EQ(extension_id, kInSessionExtensionId);
  }

  void SetTestCustomArg(const std::string custom_arg) {
    config_.Set("customArg", base::Value(custom_arg));
    extensions::TestGetConfigFunction::set_test_config_state(&config_);
  }

  void LogInWithPassword() {
    ash::SessionStateWaiter waiter(session_manager::SessionState::ACTIVE);
    SetTestCustomArg(kPassword);
    SetUpLoginScreenExtensionAndRunTest(kLaunchManagedGuestSessionWithPassword);
    waiter.Wait();
  }

  void SetSessionState(session_manager::SessionState session_state) {
    session_manager::SessionManager::Get()->SetSessionState(session_state);
  }

  // Also checks that session is locked.
  void LockScreen() { ash::ScreenLockerTester().Lock(); }

 protected:
  std::unique_ptr<policy::UserPolicyBuilder> user_policy_builder_;

 private:
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
  base::Value::Dict config_;
};

IN_PROC_BROWSER_TEST_F(LoginApitest, LaunchManagedGuestSession) {
  SetUpDeviceLocalAccountPolicy();
  ash::SessionStateWaiter waiter(session_manager::SessionState::ACTIVE);
  SetUpLoginScreenExtensionAndRunTest(kLaunchManagedGuestSession);
  waiter.Wait();

  // Check that the active user is of type |USER_TYPE_PUBLIC_ACCOUNT|.
  // We cannot use the email as an identifier as a different email is generated
  // for managed guest sessions.
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  auto* active_user = user_manager->GetActiveUser();
  ASSERT_TRUE(active_user);
  EXPECT_EQ(user_manager::UserType::kPublicAccount, active_user->GetType());
  EXPECT_FALSE(active_user->CanLock());
}

IN_PROC_BROWSER_TEST_F(LoginApitest, LaunchManagedGuestSessionWithPassword) {
  SetUpDeviceLocalAccountPolicy();
  LogInWithPassword();

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->GetActiveUser()->CanLock());
}

IN_PROC_BROWSER_TEST_F(LoginApitest, LaunchManagedGuestSessionNoAccounts) {
  SetUpLoginScreenExtensionAndRunTest(kLaunchManagedGuestSessionNoAccounts);
}

IN_PROC_BROWSER_TEST_F(LoginApitest, ExitCurrentSession) {
  SetUpDeviceLocalAccountPolicy();
  SetTestCustomArg(kData);

  base::RunLoop exit_waiter;
  auto subscription =
      browser_shutdown::AddAppTerminatingCallback(exit_waiter.QuitClosure());
  SetUpLoginScreenExtensionAndRunTest(kExitCurrentSession,
                                      /*assert_test_succeed=*/false);
  exit_waiter.Run();

  PrefService* local_state = g_browser_process->local_state();
  EXPECT_EQ(kData, local_state->GetString(
                       prefs::kLoginExtensionApiDataForNextLoginAttempt));
}

IN_PROC_BROWSER_TEST_F(LoginApitest, FetchDataForNextLoginAttempt) {
  SetTestCustomArg(kData);
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                         kData);
  SetUpLoginScreenExtensionAndRunTest(kFetchDataForNextLoginAttempt);

  EXPECT_EQ("", local_state->GetString(
                    prefs::kLoginExtensionApiDataForNextLoginAttempt));
}

IN_PROC_BROWSER_TEST_F(LoginApitest, LockManagedGuestSession) {
  SetUpDeviceLocalAccountPolicy();
  LogInWithPassword();

  SetUpTestListeners();
  SetUpInSessionExtension();
  ash::SessionStateWaiter waiter(session_manager::SessionState::LOCKED);
  RunTest(kInSessionLoginLockManagedGuestSession);
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(LoginApitest, LockManagedGuestSessionNotActive) {
  SetUpDeviceLocalAccountPolicy();
  LogInWithPassword();

  // Login screen extensions stop when the session becomes active and start
  // again when the session is locked. The test extension will be waiting for a
  // new test after the session is locked.
  SetUpTestListeners();
  LockScreen();
  RunTest(kLockManagedGuestSessionNotActive);
}

IN_PROC_BROWSER_TEST_F(LoginApitest, UnlockManagedGuestSession) {
  SetUpDeviceLocalAccountPolicy();
  LogInWithPassword();
  ASSERT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  ash::SessionStateWaiter locked_waiter(session_manager::SessionState::LOCKED);
  SetUpTestListeners();
  LockScreen();
  locked_waiter.Wait();
  ASSERT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOCKED);

  // The test extension running on the login screen does not call test.succeed()
  // since the extension itself will be disabled and stopped as a result of the
  // login.unlockManagedGuestSession() API call. Instead, verify the session
  // state here.
  ash::SessionStateWaiter active_waiter(session_manager::SessionState::ACTIVE);
  RunTest(kUnlockManagedGuestSession, /*assert_test_succeed=*/false);
  active_waiter.Wait();
  ASSERT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);
}

IN_PROC_BROWSER_TEST_F(LoginApitest, UnlockManagedGuestSessionLockedWithApi) {
  SetUpDeviceLocalAccountPolicy();
  LogInWithPassword();

  // |RunTest()| has to be handled by the test as it requires multiple
  // listeners. Using one listener at a time would result in race conditions.
  ClearTestListeners();
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener login_screen_listener(listener_message(),
                                                     ReplyBehavior::kWillReply);
  login_screen_listener.set_extension_id(extension_id());
  ExtensionTestMessageListener in_session_listener(listener_message(),
                                                   ReplyBehavior::kWillReply);
  in_session_listener.set_extension_id(kInSessionExtensionId);

  SetUpInSessionExtension();
  ash::SessionStateWaiter locked_waiter(session_manager::SessionState::LOCKED);
  ASSERT_TRUE(in_session_listener.WaitUntilSatisfied());
  in_session_listener.Reply(kInSessionLoginLockManagedGuestSession);
  ASSERT_TRUE(catcher.GetNextResult());
  locked_waiter.Wait();

  ash::SessionStateWaiter active_waiter(session_manager::SessionState::ACTIVE);
  ASSERT_TRUE(login_screen_listener.WaitUntilSatisfied());
  login_screen_listener.Reply(kUnlockManagedGuestSession);
  active_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(LoginApitest, UnlockManagedGuestSessionWrongPassword) {
  // Note: the password check will fail even if the correct password is used as
  // |FakeUserDataAuthClient::CheckKeyEx()| does not check the user session's
  // cipher blob.
  ash::FakeUserDataAuthClient::TestApi::Get()->set_enable_auth_check(true);
  SetUpDeviceLocalAccountPolicy();
  LogInWithPassword();

  SetTestCustomArg(kWrongPassword);
  SetUpTestListeners();
  LockScreen();
  RunTest(kUnlockManagedGuestSessionWrongPassword);
}

// This test checks that the case where the profile has been created (which
// sets the |kLoginExtensionApiCanLockManagedGuestSession| pref), but the
// session is not yet active.
IN_PROC_BROWSER_TEST_F(LoginApitest, UnlockManagedGuestSessionNotLocked) {
  SetUpDeviceLocalAccountPolicy();
  LogInWithPassword();

  SetUpTestListeners();
  // Manually setting |LOGGED_IN_NOT_ACTIVE| state here as it is difficult to
  // remain in this state during the login process.
  SetSessionState(session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  RunTest(kUnlockManagedGuestSessionNotLocked);
}

IN_PROC_BROWSER_TEST_F(LoginApitest, ExternalLogoutRequestExternalLogout) {
  SetUpDeviceLocalAccountPolicy();
  LogInWithPassword();
  ClearTestListeners();

  ExtensionTestMessageListener in_session_listener(listener_message(),
                                                   ReplyBehavior::kWillReply);
  in_session_listener.set_extension_id(kInSessionExtensionId);

  SetUpInSessionExtension();

  ExtensionTestMessageListener login_screen_listener(listener_message(),
                                                     ReplyBehavior::kWillReply);
  login_screen_listener.set_extension_id(extension_id());

  LockScreen();

  extensions::ResultCatcher catcher;
  // Set up a `login.onRequestExternalLogout` listener on the in-session
  // extension.
  ExtensionTestMessageListener in_session_message_listener(
      kOnRequestExternalLogoutInSessionMessage);
  in_session_message_listener.set_extension_id(kInSessionExtensionId);
  ASSERT_TRUE(in_session_listener.WaitUntilSatisfied());
  in_session_listener.Reply(kInSessionLoginOnRequestExternalLogout);
  // Confirm the in-session listener was set up.
  ASSERT_TRUE(in_session_message_listener.WaitUntilSatisfied());

  // Request external logout from the login screen extension.
  ASSERT_TRUE(login_screen_listener.WaitUntilSatisfied());
  login_screen_listener.Reply(kLoginRequestExternalLogout);
  // Request and listener trigger succeeded.
  ASSERT_TRUE(catcher.GetNextResult());
  ASSERT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(LoginApitest, ExternalLogoutNotifyExternalLogoutDone) {
  SetUpDeviceLocalAccountPolicy();
  LogInWithPassword();
  ClearTestListeners();

  ExtensionTestMessageListener in_session_listener(listener_message(),
                                                   ReplyBehavior::kWillReply);
  in_session_listener.set_extension_id(kInSessionExtensionId);

  SetUpInSessionExtension();

  ExtensionTestMessageListener login_screen_listener(listener_message(),
                                                     ReplyBehavior::kWillReply);
  login_screen_listener.set_extension_id(extension_id());

  LockScreen();

  extensions::ResultCatcher catcher;
  // Set up a `login.onExternalLogoutDone` listener on the login screen
  // extension.
  ExtensionTestMessageListener login_screen_message_listener(
      kOnExternalLogoutDoneLoginScreenMessage);
  login_screen_message_listener.set_extension_id(extension_id());
  ASSERT_TRUE(login_screen_listener.WaitUntilSatisfied());
  login_screen_listener.Reply(kLoginOnExternalLogoutDone);
  // Confirm the login screen listener was set up.
  ASSERT_TRUE(login_screen_message_listener.WaitUntilSatisfied());

  // Notify the external logout is done from the in-session extension.
  ASSERT_TRUE(in_session_listener.WaitUntilSatisfied());
  in_session_listener.Reply(kInSessionLoginNotifyExternalLogoutDone);
  // Notify and listener trigger succeeded.
  ASSERT_TRUE(catcher.GetNextResult());
  ASSERT_TRUE(catcher.GetNextResult());
}

class LoginApitestWithEnterpriseUser : public LoginApitest {
 public:
  LoginApitestWithEnterpriseUser() = default;

  LoginApitestWithEnterpriseUser(const LoginApitestWithEnterpriseUser&) =
      delete;

  LoginApitestWithEnterpriseUser& operator=(
      const LoginApitestWithEnterpriseUser&) = delete;

  ~LoginApitestWithEnterpriseUser() override = default;

  void LoginUser() { logged_in_user_mixin_.LogInUser(); }

  void SetUpInSessionExtension() override {
    AccountId account_id = logged_in_user_mixin_.GetAccountId();
    SetUpSessionExtensionUserPolicyBuilder();
    enterprise_management::PolicyData& policy_data =
        user_policy_builder_->policy_data();
    policy_data.set_policy_type(policy::dm_protocol::kChromeUserPolicyType);
    policy_data.set_username(account_id.GetUserEmail());
    policy_data.set_gaia_id(account_id.GetGaiaId());
    user_policy_builder_->Build();

    auto registry_observer =
        GetTestExtensionRegistryObserver(kInSessionExtensionId);

    logged_in_user_mixin_.GetEmbeddedPolicyTestServerMixin()->UpdateUserPolicy(
        user_policy_builder_->payload(), account_id.GetUserEmail());
    session_manager_client()->set_user_policy(
        cryptohome::CreateAccountIdentifierFromAccountId(account_id),
        user_policy_builder_->GetBlob());
    RefreshPolicies();

    registry_observer->WaitForExtensionReady();
  }

 private:
  // Use a different test server as |LoginApitest| uses the one from
  // |embedded_test_server()|.
  net::EmbeddedTestServer test_server_;
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      ash::LoggedInUserMixin::LogInType::kManaged};
};

IN_PROC_BROWSER_TEST_F(LoginApitestWithEnterpriseUser,
                       LaunchManagedGuestSessionAlreadyExistsActiveSession) {
  LoginUser();
  LockScreen();
  SetUpLoginScreenExtensionAndRunTest(
      kLaunchManagedGuestSessionAlreadyExistsActiveSession);
}

// TODO(b/214555030): Re-add
// LoginUnlockManagedGuestSessionNotManagedGuestSession API test with the
// correct error message when crrev.com/c/3284871 is landed.

}  // namespace chromeos
