// Copyright 2019 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/extensions/login_screen/login_screen_apitest_base.h"
#include "chrome/common/pref_names.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/userdataauth/fake_userdataauth_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kAccountId[] = "public-session@test";
constexpr char kPassword[] = "password";
constexpr char kWrongPassword[] = "wrong password";
constexpr char kData[] = "some data";
constexpr char kInSessionExtensionId[] = "ofcpkomnogjenhfajfjadjmjppbegnad";
const char kInSessionExtensionUpdateManifestPath[] =
    "/extensions/api_test/login_screen_apis/update_manifest.xml";
constexpr char kWrongExtensionId[] = "abcdefghijklmnopqrstuvwxyzabcdef";

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
// unlockManagedGuestSession tests.
constexpr char kUnlockManagedGuestSession[] = "LoginUnlockManagedGuestSession";
constexpr char kUnlockManagedGuestSessionWrongPassword[] =
    "LoginUnlockManagedGuestSessionWrongPassword";
constexpr char kUnlockManagedGuestSessionNotLocked[] =
    "LoginUnlockManagedGuestSessionNotLocked";
constexpr char kUnlockManagedGuestSessionNotManagedGuestSession[] =
    "LoginUnlockManagedGuestSessionNotManagedGuestSession";
constexpr char kUnlockManagedGuestSessionWrongExtensionId[] =
    "LoginUnlockManagedGuestSessionWrongExtensionId";
// In-session extension tests.
constexpr char kInSessionLoginLockManagedGuestSession[] =
    "InSessionLoginLockManagedGuestSession";
constexpr char kInSessionLoginLockManagedGuestSessionNoPermission[] =
    "InSessionLoginLockManagedGuestSessionNoPermission";
constexpr char kInSessionUnlockManagedGuestSessionNoPermission[] =
    "InSessionLoginUnlockManagedGuestSessionNoPermission";

}  // namespace

namespace chromeos {

class LoginApitest : public LoginScreenApitestBase {
 public:
  LoginApitest() : LoginScreenApitestBase(version_info::Channel::STABLE) {}

  LoginApitest(const LoginApitest&) = delete;

  LoginApitest& operator=(const LoginApitest&) = delete;

  ~LoginApitest() override = default;

  void SetUpDeviceLocalAccountPolicy() {
    enterprise_management::DeviceLocalAccountsProto* const
        device_local_accounts =
            device_policy()->payload().mutable_device_local_accounts();
    enterprise_management::DeviceLocalAccountInfoProto* const account =
        device_local_accounts->add_account();
    account->set_account_id(kAccountId);
    account->set_type(enterprise_management::DeviceLocalAccountInfoProto::
                          ACCOUNT_TYPE_PUBLIC_SESSION);
    RefreshDevicePolicy();
  }

  std::unique_ptr<policy::UserPolicyBuilder>
  MakeInSessionExtensionUserPolicyBuilder() {
    std::unique_ptr<policy::UserPolicyBuilder> user_policy_builder =
        std::make_unique<policy::UserPolicyBuilder>();
    enterprise_management::PolicyData& policy_data =
        user_policy_builder->policy_data();
    policy_data.set_public_key_version(1);
    user_policy_builder->payload()
        .mutable_extensioninstallforcelist()
        ->mutable_value()
        ->add_entries(base::ReplaceStringPlaceholders(
            "$1;$2",
            {kInSessionExtensionId,
             embedded_test_server()
                 ->GetURL(kInSessionExtensionUpdateManifestPath)
                 .spec()},
            nullptr));
    user_policy_builder->SetDefaultSigningKey();

    return user_policy_builder;
  }

  void RefreshPolicies() {
    base::RunLoop run_loop;
    g_browser_process->policy_service()->RefreshPolicies(
        run_loop.QuitClosure());
    run_loop.Run();
  }

  std::unique_ptr<extensions::TestExtensionRegistryObserver>
  GetTestExtensionRegistryObserver(const std::string& extension_id) {
    const user_manager::User* active_user =
        user_manager::UserManager::Get()->GetActiveUser();
    Profile* profile =
        chromeos::ProfileHelper::Get()->GetProfileByUser(active_user);
    return std::make_unique<extensions::TestExtensionRegistryObserver>(
        extensions::ExtensionRegistry::Get(profile), extension_id);
  }

  virtual void SetUpInSessionExtension() {
    std::unique_ptr<policy::UserPolicyBuilder> user_policy_builder =
        MakeInSessionExtensionUserPolicyBuilder();
    enterprise_management::PolicyData& policy_data =
        user_policy_builder->policy_data();
    policy_data.set_policy_type(
        policy::dm_protocol::kChromePublicAccountPolicyType);
    policy_data.set_username(kAccountId);
    policy_data.set_settings_entity_id(kAccountId);
    user_policy_builder->Build();

    auto registry_observer =
        GetTestExtensionRegistryObserver(kInSessionExtensionId);

    ASSERT_TRUE(local_policy_mixin_.server()->UpdatePolicy(
        policy::dm_protocol::kChromePublicAccountPolicyType, kAccountId,
        user_policy_builder->payload().SerializeAsString()));
    session_manager_client()->set_device_local_account_policy(
        kAccountId, user_policy_builder->GetBlob());
    RefreshPolicies();

    registry_observer->WaitForExtensionReady();
  }

  void SetTestCustomArg(const std::string custom_arg) {
    config_.SetKey("customArg", base::Value(custom_arg));
    extensions::TestGetConfigFunction::set_test_config_state(&config_);
  }

  void LogInWithPassword() {
    SessionStateWaiter waiter(session_manager::SessionState::ACTIVE);
    SetTestCustomArg(kPassword);
    SetUpLoginScreenExtensionAndRunTest(kLaunchManagedGuestSessionWithPassword);
    waiter.Wait();
  }

  void SetSessionState(session_manager::SessionState session_state) {
    session_manager::SessionManager::Get()->SetSessionState(session_state);
  }

  // Also checks that session is locked.
  void LockScreen() { screen_locker_tester_.Lock(); }

 private:
  chromeos::LocalPolicyTestServerMixin local_policy_mixin_{&mixin_host_};
  base::DictionaryValue config_;
  ScreenLockerTester screen_locker_tester_;
};

IN_PROC_BROWSER_TEST_F(LoginApitest, LaunchManagedGuestSession) {
  SetUpDeviceLocalAccountPolicy();
  SessionStateWaiter waiter(session_manager::SessionState::ACTIVE);
  SetUpLoginScreenExtensionAndRunTest(kLaunchManagedGuestSession);
  waiter.Wait();

  // Check that the active user is of type |USER_TYPE_PUBLIC_ACCOUNT|.
  // We cannot use the email as an identifier as a different email is generated
  // for managed guest sessions.
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->GetActiveUser()->GetType() ==
              user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  EXPECT_FALSE(user_manager->CanCurrentUserLock());
}

IN_PROC_BROWSER_TEST_F(LoginApitest, LaunchManagedGuestSessionWithPassword) {
  SetUpDeviceLocalAccountPolicy();
  LogInWithPassword();

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->CanCurrentUserLock());
}

IN_PROC_BROWSER_TEST_F(LoginApitest, LaunchManagedGuestSessionNoAccounts) {
  SetUpLoginScreenExtensionAndRunTest(kLaunchManagedGuestSessionNoAccounts);
}

IN_PROC_BROWSER_TEST_F(LoginApitest, ExitCurrentSession) {
  SetUpDeviceLocalAccountPolicy();
  SetTestCustomArg(kData);
  content::WindowedNotificationObserver termination_waiter(
      chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources());

  SetUpLoginScreenExtensionAndRunTest(kExitCurrentSession,
                                      /*assert_test_succeed=*/false);
  termination_waiter.Wait();

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
  SessionStateWaiter waiter(session_manager::SessionState::LOCKED);
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

IN_PROC_BROWSER_TEST_F(LoginApitest, LockManagedGuestSessionNoPermission) {
  SetUpDeviceLocalAccountPolicy();
  SessionStateWaiter waiter(session_manager::SessionState::ACTIVE);
  SetUpLoginScreenExtensionAndRunTest(kLaunchManagedGuestSession);
  waiter.Wait();

  SetUpTestListeners();
  SetUpInSessionExtension();
  RunTest(kInSessionLoginLockManagedGuestSessionNoPermission);
}

IN_PROC_BROWSER_TEST_F(LoginApitest, UnlockManagedGuestSession) {
  SetUpDeviceLocalAccountPolicy();
  LogInWithPassword();

  SetUpTestListeners();
  LockScreen();
  SessionStateWaiter waiter(session_manager::SessionState::ACTIVE);
  RunTest(kUnlockManagedGuestSession);
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(LoginApitest, UnlockManagedGuestSessionLockedWithApi) {
  SetUpDeviceLocalAccountPolicy();
  LogInWithPassword();

  // |RunTest()| has to be handled by the test as it requires multiple
  // listeners. Using one listener at a time would result in race conditions.
  ClearTestListeners();
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener login_screen_listener(listener_message(),
                                                     /*will_reply=*/true);
  login_screen_listener.set_extension_id(extension_id());
  ExtensionTestMessageListener in_session_listener(listener_message(),
                                                   /*will_reply=*/true);
  in_session_listener.set_extension_id(kInSessionExtensionId);

  SetUpInSessionExtension();
  SessionStateWaiter locked_waiter(session_manager::SessionState::LOCKED);
  ASSERT_TRUE(in_session_listener.WaitUntilSatisfied());
  in_session_listener.Reply(kInSessionLoginLockManagedGuestSession);
  ASSERT_TRUE(catcher.GetNextResult());
  locked_waiter.Wait();

  SessionStateWaiter active_waiter(session_manager::SessionState::ACTIVE);
  ASSERT_TRUE(login_screen_listener.WaitUntilSatisfied());
  login_screen_listener.Reply(kUnlockManagedGuestSession);
  ASSERT_TRUE(catcher.GetNextResult());
  active_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(LoginApitest, UnlockManagedGuestSessionWrongPassword) {
  // Note: the password check will fail even if the correct password is used as
  // |FakeUserDataAuthClient::CheckKeyEx()| does not check the user session's
  // cipher blob.
  FakeUserDataAuthClient::Get()->set_enable_auth_check(true);
  SetUpDeviceLocalAccountPolicy();
  LogInWithPassword();

  SetTestCustomArg(kWrongPassword);
  SetUpTestListeners();
  LockScreen();
  RunTest(kUnlockManagedGuestSessionWrongPassword);
}

IN_PROC_BROWSER_TEST_F(LoginApitest, UnlockManagedGuestSessionNoPermission) {
  SetUpDeviceLocalAccountPolicy();
  LogInWithPassword();

  SetUpTestListeners();
  SetUpInSessionExtension();
  RunTest(kInSessionUnlockManagedGuestSessionNoPermission);
}

// This test checks that the case where the profile has been created (which
// sets the |kLoginExtensionApiLaunchExtensionId| pref, but the session is not
// yet active.
IN_PROC_BROWSER_TEST_F(LoginApitest, UnlockManagedGuestSessionNotLocked) {
  SetUpDeviceLocalAccountPolicy();
  LogInWithPassword();

  SetUpTestListeners();
  // Manually setting |LOGGED_IN_NOT_ACTIVE| state here as it is difficult to
  // remain in this state during the login process.
  SetSessionState(session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  RunTest(kUnlockManagedGuestSessionNotLocked);
}

IN_PROC_BROWSER_TEST_F(LoginApitest,
                       UnlockManagedGuestSessionWrongExtensionId) {
  SetUpDeviceLocalAccountPolicy();
  LogInWithPassword();

  const user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(active_user);
  PrefService* prefs = profile->GetPrefs();
  prefs->SetString(prefs::kLoginExtensionApiLaunchExtensionId,
                   kWrongExtensionId);

  SetUpTestListeners();
  LockScreen();
  RunTest(kUnlockManagedGuestSessionWrongExtensionId);
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
    std::unique_ptr<policy::UserPolicyBuilder> user_policy_builder =
        MakeInSessionExtensionUserPolicyBuilder();
    enterprise_management::PolicyData& policy_data =
        user_policy_builder->policy_data();
    policy_data.set_policy_type(policy::dm_protocol::kChromeUserPolicyType);
    policy_data.set_username(account_id.GetUserEmail());
    policy_data.set_gaia_id(account_id.GetGaiaId());
    user_policy_builder->Build();

    auto registry_observer =
        GetTestExtensionRegistryObserver(kInSessionExtensionId);

    ASSERT_TRUE(
        logged_in_user_mixin_.GetLocalPolicyTestServerMixin()->UpdateUserPolicy(
            user_policy_builder->payload(), account_id.GetUserEmail()));
    session_manager_client()->set_user_policy(
        cryptohome::CreateAccountIdentifierFromAccountId(account_id),
        user_policy_builder->GetBlob());
    RefreshPolicies();

    registry_observer->WaitForExtensionReady();
  }

 private:
  // Use a different test server as |LoginApitest| uses the one from
  // |embedded_test_server()|.
  net::EmbeddedTestServer test_server_;
  LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_,
      LoggedInUserMixin::LogInType::kRegular,
      &test_server_,
      this,
      /*should_launch_browser=*/true,
      AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kEnterpriseUser1,
                                     FakeGaiaMixin::kEnterpriseUser1GaiaId)};
};

IN_PROC_BROWSER_TEST_F(LoginApitestWithEnterpriseUser,
                       LaunchManagedGuestSessionAlreadyExistsActiveSession) {
  LoginUser();
  LockScreen();
  SetUpLoginScreenExtensionAndRunTest(
      kLaunchManagedGuestSessionAlreadyExistsActiveSession);
}

IN_PROC_BROWSER_TEST_F(LoginApitestWithEnterpriseUser,
                       UnlockManagedGuestSessionNotManagedGuestSession) {
  LoginUser();
  LockScreen();
  SetUpLoginScreenExtensionAndRunTest(
      kUnlockManagedGuestSessionNotManagedGuestSession);
}

// TODO(https://crbug.com/1075511) Flaky test.
IN_PROC_BROWSER_TEST_F(LoginApitestWithEnterpriseUser,
                       DISABLED_LockManagedGuestSessionNotManagedGuestSession) {
  LoginUser();
  SetUpTestListeners();
  SetUpInSessionExtension();
  RunTest(kInSessionLoginLockManagedGuestSessionNoPermission);
}

}  // namespace chromeos
