// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/login_manager_test.h"

#include <string>

#include "ash/constants/ash_switches.h"
#include "ash/metrics/login_unlock_throughput_recorder.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/profile_prepared_waiter.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host_webui.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

LoginManagerTest::LoginManagerTest() {
  set_exit_when_last_browser_closes(false);
}

LoginManagerTest::~LoginManagerTest() {}

void LoginManagerTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kLoginManager);
  command_line->AppendSwitch(switches::kForceLoginManagerInTests);
  command_line->AppendSwitch(
      switches::kDisableOOBEChromeVoxHintTimerForTesting);

  MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
}

void LoginManagerTest::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");

  test::UserSessionManagerTestApi session_manager_test_api(
      UserSessionManager::GetInstance());
  session_manager_test_api.SetShouldLaunchBrowserInTests(
      should_launch_browser_);
  session_manager_test_api.SetShouldObtainTokenHandleInTests(false);

  MixinBasedInProcessBrowserTest::SetUpOnMainThread();
}

void LoginManagerTest::RegisterUser(const AccountId& account_id) {
  ScopedListPrefUpdate users_pref(g_browser_process->local_state(),
                                  "LoggedInUsers");
  base::Value email_value(account_id.GetUserEmail());
  if (!base::Contains(users_pref.Get(), email_value))
    users_pref->Append(std::move(email_value));
  if (user_manager::UserManager::IsInitialized()) {
    user_manager::KnownUser(g_browser_process->local_state())
        .SaveKnownUser(account_id);
    user_manager::UserManager::Get()->SaveUserOAuthStatus(
        account_id, user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
  }
}

constexpr char LoginManagerTest::kPassword[] = "password";

constexpr char LoginManagerTest::kLocalPassword[] = "local-password";

UserContext LoginManagerTest::CreateUserContext(const AccountId& account_id,
                                                const std::string& password) {
  UserContext user_context(user_manager::UserType::kRegular, account_id);
  user_context.SetKey(Key(password));
  user_context.SetGaiaPassword(GaiaPassword(password));
  user_context.SetPasswordKey(Key(password));
  if (account_id.GetUserEmail() == FakeGaiaMixin::kEnterpriseUser1) {
    user_context.SetRefreshToken(FakeGaiaMixin::kTestRefreshToken1);
  } else if (account_id.GetUserEmail() == FakeGaiaMixin::kEnterpriseUser2) {
    user_context.SetRefreshToken(FakeGaiaMixin::kTestRefreshToken2);
  }
  return user_context;
}

UserContext LoginManagerTest::CreateUserContextWithLocalPassword(
    const AccountId& account_id,
    const std::string& password) {
  UserContext user_context(user_manager::UserType::kRegular, account_id);
  user_context.SetKey(Key(password));
  user_context.SetLocalPasswordInput(LocalPasswordInput(password));
  user_context.SetPasswordKey(Key(password));
  return user_context;
}

void LoginManagerTest::SetExpectedCredentials(const UserContext& user_context) {
  test::UserSessionManagerTestApi session_manager_test_api(
      UserSessionManager::GetInstance());
  session_manager_test_api.InjectStubUserContext(user_context);
}

bool LoginManagerTest::TryToLogin(const UserContext& user_context) {
  if (!AddUserToSession(user_context))
    return false;
  if (const user_manager::User* active_user =
          user_manager::UserManager::Get()->GetActiveUser())
    return active_user->GetAccountId() == user_context.GetAccountId();
  return false;
}

bool LoginManagerTest::AddUserToSession(const UserContext& user_context) {
  ExistingUserController* controller =
      ExistingUserController::current_controller();
  if (!controller) {
    ADD_FAILURE();
    return false;
  }
  test::ProfilePreparedWaiter profile_prepared(user_context.GetAccountId());
  controller->Login(user_context, SigninSpecifics());
  profile_prepared.Wait();
  const user_manager::UserList& logged_users =
      user_manager::UserManager::Get()->GetLoggedInUsers();
  for (user_manager::UserList::const_iterator it = logged_users.begin();
       it != logged_users.end(); ++it) {
    if ((*it)->GetAccountId() == user_context.GetAccountId())
      return true;
  }
  return false;
}

void LoginManagerTest::LoginUser(const AccountId& account_id) {
  const UserContext user_context = CreateUserContext(account_id, kPassword);
  SetExpectedCredentials(user_context);
  EXPECT_TRUE(TryToLogin(user_context));
}

void LoginManagerTest::LoginUserWithLocalPassword(const AccountId& account_id) {
  const UserContext user_context =
      CreateUserContextWithLocalPassword(account_id, kLocalPassword);
  SetExpectedCredentials(user_context);
  EXPECT_TRUE(TryToLogin(user_context));
}

void LoginManagerTest::AddUser(const AccountId& account_id) {
  const UserContext user_context = CreateUserContext(account_id, kPassword);
  SetExpectedCredentials(user_context);
  EXPECT_TRUE(AddUserToSession(user_context));
}

void LoginManagerTest::LoginUserWithDbusClient(const AccountId& account_id,
                                               const std::string& password) {
  const UserContext user_context = CreateUserContext(account_id, password);
  EXPECT_TRUE(TryToLogin(user_context));
}

void LoginManagerTest::AddUserWithDbusClient(const AccountId& account_id,
                                             const std::string& password) {
  const UserContext user_context = CreateUserContext(account_id, password);
  EXPECT_TRUE(AddUserToSession(user_context));
}

void LoginManagerTest::SetExpectedCredentialsWithDbusClient(
    const AccountId& account_id,
    const std::string& password) {
  auto* test_api = FakeUserDataAuthClient::TestApi::Get();
  test_api->set_enable_auth_check(true);

  const auto cryptohome_id =
      cryptohome::CreateAccountIdentifierFromAccountId(account_id);
  ash::Key key{password};
  key.Transform(ash::Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                ash::SystemSaltGetter::ConvertRawSaltToHexString(
                    ash::FakeCryptohomeMiscClient::GetStubSystemSalt()));

  user_data_auth::AuthFactor auth_factor;
  user_data_auth::AuthInput auth_input;

  auth_factor.set_label(ash::kCryptohomeGaiaKeyLabel);
  auth_factor.set_type(user_data_auth::AUTH_FACTOR_TYPE_PASSWORD);

  auth_input.mutable_password_input()->set_secret(key.GetSecret());

  test_api->AddExistingUser(cryptohome_id);
  test_api->AddAuthFactor(cryptohome_id, auth_factor, auth_input);
}

}  // namespace ash
