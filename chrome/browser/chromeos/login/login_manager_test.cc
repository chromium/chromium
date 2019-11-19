// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_manager_test.h"

#include <string>

#include "ash/public/cpp/ash_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager_test_api.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_webui.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

UserContext CreateUserContext(const AccountId& account_id) {
  UserContext user_context(user_manager::UserType::USER_TYPE_REGULAR,
                           account_id);
  user_context.SetKey(Key("password"));
  if (account_id.GetUserEmail() == FakeGaiaMixin::kEnterpriseUser1) {
    user_context.SetRefreshToken(FakeGaiaMixin::kTestRefreshToken1);
  } else if (account_id.GetUserEmail() == FakeGaiaMixin::kEnterpriseUser2) {
    user_context.SetRefreshToken(FakeGaiaMixin::kTestRefreshToken2);
  }
  return user_context;
}

}  // namespace

LoginManagerTest::LoginManagerTest(bool should_launch_browser,
                                   bool should_initialize_webui)
    : should_launch_browser_(should_launch_browser),
      should_initialize_webui_(should_initialize_webui) {
  set_exit_when_last_browser_closes(false);
}

LoginManagerTest::~LoginManagerTest() {}

void LoginManagerTest::SetUpCommandLine(base::CommandLine* command_line) {
  if (force_webui_login_) {
    command_line->AppendSwitch(ash::switches::kShowWebUiLogin);
  }
  command_line->AppendSwitch(chromeos::switches::kLoginManager);
  command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);

  MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
}

void LoginManagerTest::SetUpOnMainThread() {
  LoginDisplayHostWebUI::DisableRestrictiveProxyCheckForTest();

  host_resolver()->AddRule("*", "127.0.0.1");

  if (should_initialize_webui_) {
    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources())
        .Wait();
  }
  test::UserSessionManagerTestApi session_manager_test_api(
      UserSessionManager::GetInstance());
  session_manager_test_api.SetShouldLaunchBrowserInTests(
      should_launch_browser_);
  session_manager_test_api.SetShouldObtainTokenHandleInTests(false);

  MixinBasedInProcessBrowserTest::SetUpOnMainThread();
}

void LoginManagerTest::RegisterUser(const AccountId& account_id) {
  ListPrefUpdate users_pref(g_browser_process->local_state(), "LoggedInUsers");
  users_pref->AppendIfNotPresent(
      std::make_unique<base::Value>(account_id.GetUserEmail()));
  if (user_manager::UserManager::IsInitialized())
    user_manager::known_user::SaveKnownUser(account_id);
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
  SessionStateWaiter waiter;
  controller->Login(user_context, SigninSpecifics());
  waiter.Wait();
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
  const UserContext user_context = CreateUserContext(account_id);
  SetExpectedCredentials(user_context);
  EXPECT_TRUE(TryToLogin(user_context));
}

void LoginManagerTest::AddUser(const AccountId& account_id) {
  const UserContext user_context = CreateUserContext(account_id);
  SetExpectedCredentials(user_context);
  EXPECT_TRUE(AddUserToSession(user_context));
}

}  // namespace chromeos
