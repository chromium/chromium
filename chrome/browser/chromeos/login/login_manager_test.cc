// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_manager_test.h"

#include <string>

#include "ash/public/cpp/ash_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager_test_api.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_webui.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr char kGAIAHost[] = "accounts.google.com";
constexpr char kTestUserinfoToken1[] = "fake-userinfo-token-1";
constexpr char kTestRefreshToken1[] = "fake-refresh-token-1";
constexpr char kTestUserinfoToken2[] = "fake-userinfo-token-2";
constexpr char kTestRefreshToken2[] = "fake-refresh-token-2";

UserContext CreateUserContext(const AccountId& account_id) {
  UserContext user_context(user_manager::UserType::USER_TYPE_REGULAR,
                           account_id);
  user_context.SetKey(Key("password"));
  if (account_id.GetUserEmail() == LoginManagerTest::kEnterpriseUser1) {
    user_context.SetRefreshToken(kTestRefreshToken1);
  } else if (account_id.GetUserEmail() == LoginManagerTest::kEnterpriseUser2) {
    user_context.SetRefreshToken(kTestRefreshToken2);
  }
  return user_context;
}

}  // namespace

constexpr char LoginManagerTest::kEnterpriseUser1[] = "user-1@example.com";
constexpr char LoginManagerTest::kEnterpriseUser1GaiaId[] = "0000111111";
constexpr char LoginManagerTest::kEnterpriseUser2[] = "user-2@example.com";
constexpr char LoginManagerTest::kEnterpriseUser2GaiaId[] = "0000222222";

LoginManagerTest::LoginManagerTest(bool should_launch_browser,
                                   bool should_initialize_webui)
    : should_launch_browser_(should_launch_browser),
      should_initialize_webui_(should_initialize_webui),
      web_contents_(NULL) {
  set_exit_when_last_browser_closes(false);
}

LoginManagerTest::~LoginManagerTest() {}

void LoginManagerTest::SetUp() {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir);

  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));

  // Don't spin up the IO thread yet since no threads are allowed while
  // spawning sandbox host process. See crbug.com/322732.
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

  // Start https wrapper here so that the URLs can be pointed at it in
  // SetUpCommandLine().
  ASSERT_TRUE(gaia_https_forwarder_.Initialize(
      kGAIAHost, embedded_test_server()->base_url()));

  MixinBasedBrowserTest::SetUp();
}

void LoginManagerTest::TearDownOnMainThread() {
  MixinBasedBrowserTest::TearDownOnMainThread();

  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
}

void LoginManagerTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitch(ash::switches::kShowWebUiLogin);
  command_line->AppendSwitch(chromeos::switches::kLoginManager);
  command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);

  const GURL gaia_url = gaia_https_forwarder_.GetURLForSSLHost(std::string());
  command_line->AppendSwitchASCII(::switches::kGaiaUrl, gaia_url.spec());
  command_line->AppendSwitchASCII(::switches::kLsoUrl, gaia_url.spec());
  command_line->AppendSwitchASCII(::switches::kGoogleApisUrl, gaia_url.spec());

  fake_gaia_.Initialize();
  fake_gaia_.set_issue_oauth_code_cookie(true);

  MixinBasedBrowserTest::SetUpCommandLine(command_line);
}

void LoginManagerTest::SetUpOnMainThread() {
  LoginDisplayHostWebUI::DisableRestrictiveProxyCheckForTest();

  // Start the accept thread as the sandbox host process has already been
  // spawned.
  host_resolver()->AddRule("*", "127.0.0.1");
  embedded_test_server()->StartAcceptingConnections();

  FakeGaia::AccessTokenInfo token_info;
  token_info.scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  token_info.scopes.insert(GaiaConstants::kOAuthWrapBridgeUserInfoScope);
  token_info.audience = GaiaUrls::GetInstance()->oauth2_chrome_client_id();

  token_info.token = kTestUserinfoToken1;
  token_info.email = kEnterpriseUser1;
  fake_gaia_.IssueOAuthToken(kTestRefreshToken1, token_info);

  token_info.token = kTestUserinfoToken2;
  token_info.email = kEnterpriseUser2;
  fake_gaia_.IssueOAuthToken(kTestRefreshToken2, token_info);

  if (should_initialize_webui_) {
    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources())
        .Wait();
    InitializeWebContents();
  }
  test::UserSessionManagerTestApi session_manager_test_api(
      UserSessionManager::GetInstance());
  session_manager_test_api.SetShouldLaunchBrowserInTests(
      should_launch_browser_);
  session_manager_test_api.SetShouldObtainTokenHandleInTests(false);

  MixinBasedBrowserTest::SetUpOnMainThread();
}

void LoginManagerTest::RegisterUser(const AccountId& account_id) {
  ListPrefUpdate users_pref(g_browser_process->local_state(), "LoggedInUsers");
  users_pref->AppendIfNotPresent(
      std::make_unique<base::Value>(account_id.GetUserEmail()));
  if (user_manager::UserManager::IsInitialized())
    user_manager::known_user::SetProfileEverInitialized(account_id, false);
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
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  controller->Login(user_context, SigninSpecifics());
  observer.Wait();
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

// static
std::string LoginManagerTest::GetGaiaIDForUserID(const std::string& user_id) {
  if (user_id == LoginManagerTest::kEnterpriseUser1)
    return LoginManagerTest::kEnterpriseUser1GaiaId;
  if (user_id == LoginManagerTest::kEnterpriseUser2)
    return LoginManagerTest::kEnterpriseUser2GaiaId;
  return "gaia-id-" + user_id;
}

void LoginManagerTest::JSExpect(const std::string& expression) {
  js_checker_.ExpectTrue(expression);
}

void LoginManagerTest::InitializeWebContents() {
  LoginDisplayHost* host = LoginDisplayHost::default_host();
  EXPECT_TRUE(host != NULL);

  content::WebContents* web_contents = host->GetOobeWebContents();
  EXPECT_TRUE(web_contents != NULL);
  set_web_contents(web_contents);
  js_checker_.set_web_contents(web_contents);
}

}  // namespace chromeos
