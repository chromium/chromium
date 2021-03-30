// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/login_manager_mixin.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/signin_specifics.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/stub_authenticator_builder.h"
#include "chromeos/login/auth/user_context.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"

namespace chromeos {

namespace {

// Ensure LoginManagerMixin is only created once.
bool g_instance_created = false;

constexpr char kGmailDomain[] = "@gmail.com";
constexpr char kManagedDomain[] = "@example.com";

void AppendUsers(LoginManagerMixin::UserList* users,
                 const std::string& domain,
                 int n) {
  int num = users->size();
  for (int i = 0; i < n; ++i, ++num) {
    const std::string email = "test_user_" + base::NumberToString(num) + domain;
    const std::string gaia_id = base::NumberToString(num) + "111111111";
    users->push_back(LoginManagerMixin::TestUserInfo(
        AccountId::FromUserEmailGaiaId(email, gaia_id)));
  }
}

}  // namespace

// static
UserContext LoginManagerMixin::CreateDefaultUserContext(
    const TestUserInfo& user_info) {
  UserContext user_context(user_info.user_type, user_info.account_id);
  user_context.SetKey(Key("password"));
  return user_context;
}

void LoginManagerMixin::AppendRegularUsers(int n) {
  AppendUsers(&initial_users_, kGmailDomain, n);
}

void LoginManagerMixin::AppendManagedUsers(int n) {
  AppendUsers(&initial_users_, kManagedDomain, n);
}

LoginManagerMixin::LoginManagerMixin(InProcessBrowserTestMixinHost* host)
    : LoginManagerMixin(host, UserList()) {}

LoginManagerMixin::LoginManagerMixin(InProcessBrowserTestMixinHost* host,
                                     const UserList& initial_users)
    : LoginManagerMixin(host, initial_users, nullptr) {}

LoginManagerMixin::LoginManagerMixin(InProcessBrowserTestMixinHost* host,
                                     const UserList& initial_users,
                                     FakeGaiaMixin* gaia_mixin)
    : InProcessBrowserTestMixin(host),
      initial_users_(initial_users),
      local_state_mixin_(host, this),
      fake_gaia_mixin_(gaia_mixin) {
  DCHECK(!g_instance_created);
  g_instance_created = true;
}

LoginManagerMixin::~LoginManagerMixin() {
  g_instance_created = false;
}

void LoginManagerMixin::SetDefaultLoginSwitches(
    const std::vector<test::SessionFlagsManager::Switch>& switches) {
  session_flags_manager_.SetDefaultLoginSwitches(switches);
}

bool LoginManagerMixin::SetUpUserDataDirectory() {
  if (session_restore_enabled_)
    session_flags_manager_.SetUpSessionRestore();
  session_flags_manager_.AppendSwitchesToCommandLine(
      base::CommandLine::ForCurrentProcess());
  return true;
}

void LoginManagerMixin::SetUpLocalState() {
  for (const auto& user : initial_users_) {
    ListPrefUpdate users_pref(g_browser_process->local_state(),
                              "LoggedInUsers");
    users_pref->AppendIfNotPresent(
        std::make_unique<base::Value>(user.account_id.GetUserEmail()));

    DictionaryPrefUpdate user_type_update(g_browser_process->local_state(),
                                          "UserType");
    user_type_update->SetKey(user.account_id.GetAccountIdKey(),
                             base::Value(static_cast<int>(user.user_type)));

    DictionaryPrefUpdate user_token_update(g_browser_process->local_state(),
                                           "OAuthTokenStatus");
    user_token_update->SetKey(user.account_id.GetUserEmail(),
                              base::Value(static_cast<int>(user.token_status)));

    user_manager::known_user::UpdateId(user.account_id);

    if (user.user_type == user_manager::USER_TYPE_CHILD) {
      user_manager::known_user::SetProfileRequiresPolicy(
          user.account_id,
          user_manager::known_user::ProfileRequiresPolicy::kPolicyRequired);
    }
  }

  StartupUtils::MarkOobeCompleted();
}

void LoginManagerMixin::SetUpOnMainThread() {
  test::UserSessionManagerTestApi session_manager_test_api(
      UserSessionManager::GetInstance());
  session_manager_test_api.SetShouldLaunchBrowserInTests(
      should_launch_browser_);
  session_manager_test_api.SetShouldObtainTokenHandleInTests(
      should_obtain_handles_);
}

void LoginManagerMixin::TearDownOnMainThread() {
  session_flags_manager_.Finalize();
}

void LoginManagerMixin::AttemptLoginUsingAuthenticator(
    const UserContext& user_context,
    std::unique_ptr<StubAuthenticatorBuilder> authenticator_builder) {
  test::UserSessionManagerTestApi(UserSessionManager::GetInstance())
      .InjectAuthenticatorBuilder(std::move(authenticator_builder));
  ExistingUserController::current_controller()->Login(user_context,
                                                      SigninSpecifics());
}

void LoginManagerMixin::WaitForActiveSession() {
  SessionStateWaiter(session_manager::SessionState::ACTIVE).Wait();
}

bool LoginManagerMixin::LoginAndWaitForActiveSession(
    const UserContext& user_context) {
  AttemptLoginUsingAuthenticator(
      user_context, std::make_unique<StubAuthenticatorBuilder>(user_context));
  WaitForActiveSession();

  user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  return active_user &&
         active_user->GetAccountId() == user_context.GetAccountId();
}

void LoginManagerMixin::LoginWithDefaultContext(const TestUserInfo& user_info) {
  UserContext user_context = CreateDefaultUserContext(user_info);
  AttemptLoginUsingAuthenticator(
      user_context, std::make_unique<StubAuthenticatorBuilder>(user_context));
}

void LoginManagerMixin::LoginAsNewRegularUser() {
  ASSERT_FALSE(session_manager::SessionManager::Get()->IsSessionStarted());
  TestUserInfo test_user(
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId));
  UserContext user_context = CreateDefaultUserContext(test_user);
  AttemptLoginUsingAuthenticator(
      user_context, std::make_unique<StubAuthenticatorBuilder>(user_context));
}

void LoginManagerMixin::LoginAsNewChildUser() {
  ASSERT_FALSE(session_manager::SessionManager::Get()->IsSessionStarted());
  TestUserInfo test_child_user_(
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId),
      user_manager::USER_TYPE_CHILD);
  UserContext user_context = CreateDefaultUserContext(test_child_user_);
  user_context.SetRefreshToken(FakeGaiaMixin::kFakeRefreshToken);
  fake_gaia_mixin_->SetupFakeGaiaForChildUser(
      test_child_user_.account_id.GetUserEmail(),
      test_child_user_.account_id.GetGaiaId(), FakeGaiaMixin::kFakeRefreshToken,
      false /*issue_any_scope_token*/);
  AttemptLoginUsingAuthenticator(
      user_context, std::make_unique<StubAuthenticatorBuilder>(user_context));
}

}  // namespace chromeos
