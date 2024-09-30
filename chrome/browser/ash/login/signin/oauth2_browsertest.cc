// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/signin/oauth2_login_manager.h"
#include "chrome/browser/ash/login/signin/oauth2_login_manager_factory.h"
#include "chrome/browser/ash/login/signin_specifics.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_extension_test_notification_observer.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/command_line_switches.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace ash {

namespace {

using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;
using ::net::test_server::HungResponse;

// Email of owner account for test.
const char kTestGaiaId[] = "12345";
const char kTestEmail[] = "username@gmail.com";
const char kTestRawEmail[] = "User.Name@gmail.com";
const char kTestAccountPassword[] = "fake-password";
const char kTestAccountServices[] = "[]";
const char kTestAuthCode[] = "fake-auth-code";
const char kTestAuthLoginAccessToken[] = "fake-access-token";
const char kTestRefreshToken[] = "fake-refresh-token";
const char kTestAuthSIDCookie[] = "fake-auth-SID-cookie";
const char kTestAuthLSIDCookie[] = "fake-auth-LSID-cookie";
const char kTestSessionSIDCookie[] = "fake-session-SID-cookie";
const char kTestSessionLSIDCookie[] = "fake-session-LSID-cookie";
const char kTestSession2SIDCookie[] = "fake-session2-SID-cookie";
const char kTestSession2LSIDCookie[] = "fake-session2-LSID-cookie";
const char kTestIdTokenAdvancedProtectionEnabled[] =
    "dummy-header."
    "eyAic2VydmljZXMiOiBbInRpYSJdIH0="  // payload: { "services": ["tia"] }
    ".dummy-signature";
const char kTestIdTokenAdvancedProtectionDisabled[] =
    "dummy-header."
    "eyAic2VydmljZXMiOiBbXSB9"  // payload: { "services": [] }
    ".dummy-signature";
constexpr char kGooglePageContent[] =
    "<html><title>Hello!</title><script>alert('hello');</script>"
    "<body>Hello Google!</body></html>";
constexpr char kRandomPageContent[] =
    "<html><title>SomthingElse</title><body>I am SomethingElse</body></html>";
constexpr char kHelloPagePath[] = "/hello_google";
constexpr char kRandomPagePath[] = "/non_google_page";
constexpr char kMultiLoginPath[] = "/oauth/multilogin";

CoreAccountId PickAccountId(Profile* profile,
                            const std::string& gaia_id,
                            const std::string& email) {
  return IdentityManagerFactory::GetInstance()
      ->GetForProfile(profile)
      ->PickAccountIdForAccount(gaia_id, email);
}

const char* BoolToString(bool value) {
  return value ? "true" : "false";
}

class OAuth2LoginManagerStateWaiter : public OAuth2LoginManager::Observer {
 public:
  explicit OAuth2LoginManagerStateWaiter(Profile* profile)
      : profile_(profile) {}

  OAuth2LoginManagerStateWaiter(const OAuth2LoginManagerStateWaiter&) = delete;
  OAuth2LoginManagerStateWaiter& operator=(
      const OAuth2LoginManagerStateWaiter&) = delete;

  void WaitForStates(
      const std::set<OAuth2LoginManager::SessionRestoreState>& states) {
    DCHECK(!waiting_for_state_);
    OAuth2LoginManager* login_manager =
        OAuth2LoginManagerFactory::GetInstance()->GetForProfile(profile_);
    states_ = states;
    if (states_.find(login_manager->state()) != states_.end()) {
      final_state_ = login_manager->state();
      return;
    }

    waiting_for_state_ = true;
    login_manager->AddObserver(this);
    signal_ = std::make_unique<base::test::TestFuture<void>>();
    EXPECT_TRUE(signal_->Wait());
    login_manager->RemoveObserver(this);
  }

  OAuth2LoginManager::SessionRestoreState final_state() { return final_state_; }

 private:
  // OAuth2LoginManager::Observer overrides.
  void OnSessionRestoreStateChanged(
      Profile* user_profile,
      OAuth2LoginManager::SessionRestoreState state) override {
    if (!waiting_for_state_)
      return;

    if (states_.find(state) == states_.end())
      return;

    final_state_ = state;
    waiting_for_state_ = false;
    // Acts as a notification for anyone waiting on `signal_`.
    signal_->SetValue();
  }

  const raw_ptr<Profile> profile_;
  std::set<OAuth2LoginManager::SessionRestoreState> states_;
  bool waiting_for_state_ = false;
  OAuth2LoginManager::SessionRestoreState final_state_ =
      OAuth2LoginManager::SESSION_RESTORE_NOT_STARTED;
  std::unique_ptr<base::test::TestFuture<void>> signal_;
};

// Blocks a thread associated with a given `task_runner` on construction and
// unblocks it on destruction.
class ThreadBlocker {
 public:
  explicit ThreadBlocker(base::SingleThreadTaskRunner* task_runner)
      : unblock_event_(new base::WaitableEvent(
            base::WaitableEvent::ResetPolicy::MANUAL,
            base::WaitableEvent::InitialState::NOT_SIGNALED)) {
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(&BlockThreadOnThread,
                                         base::Owned(unblock_event_.get())));
  }

  ThreadBlocker(const ThreadBlocker&) = delete;
  ThreadBlocker& operator=(const ThreadBlocker&) = delete;

  ~ThreadBlocker() { unblock_event_->Signal(); }

 private:
  // Blocks the target thread until `event` is signaled.
  static void BlockThreadOnThread(base::WaitableEvent* event) { event->Wait(); }

  // `unblock_event_` is deleted after BlockThreadOnThread returns.
  const raw_ptr<base::WaitableEvent> unblock_event_;
};

// Helper class that is added as a RequestMonitor of embedded test server to
// wait for a request to happen and defer it until Unblock is called.
class RequestDeferrer {
 public:
  RequestDeferrer()
      : blocking_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED),
        start_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                     base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  RequestDeferrer(const RequestDeferrer&) = delete;
  RequestDeferrer& operator=(const RequestDeferrer&) = delete;

  void UnblockRequest() { blocking_event_.Signal(); }

  void WaitForRequestToStart() {
    // If we have already served the request, bail out.
    if (start_event_.IsSignaled())
      return;

    signal_ = std::make_unique<base::test::TestFuture<void>>();
    EXPECT_TRUE(signal_->Wait());
  }

  void InterceptRequest(const HttpRequest& request) {
    start_event_.Signal();
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&RequestDeferrer::NotifyOnUIThread,
                                  base::Unretained(this)));
    blocking_event_.Wait();
  }

 private:
  void NotifyOnUIThread() {
    if (signal_) {
      signal_->SetValue();
    }
  }

  base::WaitableEvent blocking_event_;
  base::WaitableEvent start_event_;
  std::unique_ptr<base::test::TestFuture<void>> signal_;
};

}  // namespace

class OAuth2Test : public OobeBaseTest {
 public:
  OAuth2Test(const OAuth2Test&) = delete;
  OAuth2Test& operator=(const OAuth2Test&) = delete;

 protected:
  OAuth2Test() = default;
  ~OAuth2Test() override = default;

  // OobeBaseTest overrides.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);

    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);

    // Disable sync since we don't really need this for these tests and it also
    // makes OAuth2Test.MergeSession test flaky http://crbug.com/408867.
    command_line->AppendSwitch(syncer::kDisableSync);
    // Skip post login screens.
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
  }

  void RegisterAdditionalRequestHandlers() override {
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &OAuth2Test::InterceptRequest, base::Unretained(this)));
    fake_gaia_.gaia_server()->RegisterRequestMonitor(base::BindRepeating(
        &OAuth2Test::InterceptRequest, base::Unretained(this)));
  }

  void SetupGaiaServerForNewAccount(bool is_under_advanced_protection) {
    FakeGaia::Configuration params;
    params.auth_sid_cookie = kTestAuthSIDCookie;
    params.auth_lsid_cookie = kTestAuthLSIDCookie;
    params.auth_code = kTestAuthCode;
    params.refresh_token = kTestRefreshToken;
    params.access_token = kTestAuthLoginAccessToken;
    params.session_sid_cookie = kTestSessionSIDCookie;
    params.session_lsid_cookie = kTestSessionLSIDCookie;
    params.id_token = is_under_advanced_protection
                          ? kTestIdTokenAdvancedProtectionEnabled
                          : kTestIdTokenAdvancedProtectionDisabled;
    fake_gaia_.fake_gaia()->SetConfiguration(params);
    fake_gaia_.SetupFakeGaiaForLogin(kTestEmail, kTestGaiaId,
                                     kTestRefreshToken);
  }

  const extensions::Extension* LoadMergeSessionExtension() {
    extensions::ChromeTestExtensionLoader loader(GetProfile());
    scoped_refptr<const extensions::Extension> extension =
        loader.LoadExtension(test_data_dir_.AppendASCII("extensions")
                                 .AppendASCII("api_test")
                                 .AppendASCII("merge_session"));
    return extension.get();
  }

  void SetupGaiaServerForUnexpiredAccount() {
    FakeGaia::Configuration params;
    params.email = kTestEmail;
    fake_gaia_.fake_gaia()->SetConfiguration(params);
    fake_gaia_.SetupFakeGaiaForLogin(kTestEmail, kTestGaiaId,
                                     kTestRefreshToken);
  }

  void SetupGaiaServerForExpiredAccount() {
    FakeGaia::Configuration params;
    params.session_sid_cookie = kTestSession2SIDCookie;
    params.session_lsid_cookie = kTestSession2LSIDCookie;
    fake_gaia_.fake_gaia()->SetConfiguration(params);
    fake_gaia_.SetupFakeGaiaForLogin(kTestEmail, kTestGaiaId,
                                     kTestRefreshToken);
  }

  void LoginAsExistingUser() {
    // PickAccountId does not work at this point as the primary user profile has
    // not yet been created.
    EXPECT_EQ(GetOAuthStatusFromLocalState(kTestEmail),
              user_manager::User::OAUTH2_TOKEN_STATUS_VALID);

    // Try login.  Primary profile has changed.
    AccountId account_id =
        AccountId::FromUserEmailGaiaId(kTestEmail, kTestGaiaId);
    LoginScreenTestApi::SubmitPassword(account_id, kTestAccountPassword,
                                       true /*check_if_submittable */);
    test::WaitForPrimaryUserSessionStart();
    Profile* profile = ProfileManager::GetPrimaryUserProfile();

    // Wait for the session merge to finish.
    WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_DONE);
    EXPECT_EQ(GetOAuthStatusFromLocalState(kTestEmail),
              user_manager::User::OAUTH2_TOKEN_STATUS_VALID);

    // Check for existence of the primary account and its refresh token.
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    CoreAccountInfo primary_account =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
    EXPECT_TRUE(gaia::AreEmailsSame(kTestEmail, primary_account.email));
    EXPECT_EQ(kTestGaiaId, primary_account.gaia);
    EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
        primary_account.account_id));
  }

  bool TryToLogin(const AccountId& account_id, const std::string& password) {
    if (!AddUserToSession(account_id, password))
      return false;

    if (const user_manager::User* active_user =
            user_manager::UserManager::Get()->GetActiveUser()) {
      return active_user->GetAccountId() == account_id;
    }

    return false;
  }

  user_manager::User::OAuthTokenStatus GetOAuthStatusFromLocalState(
      const std::string& email) const {
    PrefService* local_state = g_browser_process->local_state();
    const base::Value::Dict& prefs_oauth_status =
        local_state->GetDict("OAuthTokenStatus");

    std::optional<int> oauth_token_status = prefs_oauth_status.FindInt(email);
    if (!oauth_token_status.has_value())
      return user_manager::User::OAUTH_TOKEN_STATUS_UNKNOWN;

    user_manager::User::OAuthTokenStatus result =
        static_cast<user_manager::User::OAuthTokenStatus>(
            oauth_token_status.value());
    return result;
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(GetProfile());
  }

 protected:
  // OobeBaseTest overrides.
  Profile* GetProfile() {
    if (user_manager::UserManager::Get()->GetActiveUser())
      return ProfileManager::GetPrimaryUserProfile();

    return ProfileManager::GetActiveUserProfile();
  }

  bool AddUserToSession(const AccountId& account_id,
                        const std::string& password) {
    ExistingUserController* controller =
        ExistingUserController::current_controller();
    if (!controller) {
      ADD_FAILURE();
      return false;
    }

    UserContext user_context(user_manager::UserType::kRegular, account_id);
    user_context.SetKey(Key(password));
    controller->Login(user_context, SigninSpecifics());
    test::WaitForPrimaryUserSessionStart();
    const user_manager::UserList& logged_users =
        user_manager::UserManager::Get()->GetLoggedInUsers();
    for (user_manager::UserList::const_iterator it = logged_users.begin();
         it != logged_users.end(); ++it) {
      if ((*it)->GetAccountId() == account_id)
        return true;
    }
    return false;
  }

  void CheckSessionState(OAuth2LoginManager::SessionRestoreState state) {
    OAuth2LoginManager* login_manager =
        OAuth2LoginManagerFactory::GetInstance()->GetForProfile(GetProfile());
    ASSERT_EQ(state, login_manager->state());
  }

  void SetSessionRestoreState(OAuth2LoginManager::SessionRestoreState state) {
    OAuth2LoginManager* login_manager =
        OAuth2LoginManagerFactory::GetInstance()->GetForProfile(GetProfile());
    login_manager->SetSessionRestoreState(state);
  }

  void WaitForMergeSessionCompletion(
      OAuth2LoginManager::SessionRestoreState final_state) {
    // Wait for the session merge to finish.
    std::set<OAuth2LoginManager::SessionRestoreState> states;
    states.insert(OAuth2LoginManager::SESSION_RESTORE_DONE);
    states.insert(OAuth2LoginManager::SESSION_RESTORE_FAILED);
    states.insert(OAuth2LoginManager::SESSION_RESTORE_CONNECTION_FAILED);
    OAuth2LoginManagerStateWaiter merge_session_waiter(GetProfile());
    merge_session_waiter.WaitForStates(states);
    EXPECT_EQ(merge_session_waiter.final_state(), final_state);
  }

  void StartNewUserSession(bool wait_for_merge,
                           bool is_under_advanced_protection) {
    SetupGaiaServerForNewAccount(is_under_advanced_protection);
    SimulateNetworkOnline();
    WaitForGaiaPageLoad();

    // Use capitalized and dotted user name on purpose to make sure
    // our email normalization kicks in.
    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->ShowSigninScreenForTest(kTestRawEmail, kTestAccountPassword,
                                  kTestAccountServices);
    test::WaitForPrimaryUserSessionStart();

    if (wait_for_merge) {
      // Wait for the session merge to finish.
      WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_DONE);
    }
  }

  void InterceptRequest(const HttpRequest& request) {
    const GURL request_url =
        GURL("http://localhost").Resolve(request.relative_url);
    auto it = request_deferers_.find(request_url.path());
    if (it == request_deferers_.end())
      return;

    it->second->InterceptRequest(request);
  }

  void AddRequestDeferer(const std::string& path,
                         RequestDeferrer* request_deferer) {
    DCHECK(request_deferers_.find(path) == request_deferers_.end());
    request_deferers_[path] = request_deferer;
  }

  void SimulateNetworkOnline() {
    network_portal_detector_.SimulateDefaultNetworkState(
        NetworkPortalDetectorMixin::NetworkStatus::kOnline);
  }

  FakeGaiaMixin fake_gaia_{&mixin_host_};
  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};

 private:
  base::FilePath test_data_dir_;
  std::map<std::string, raw_ptr<RequestDeferrer, CtnExperimental>>
      request_deferers_;
};

class CookieReader {
 public:
  CookieReader() = default;

  CookieReader(const CookieReader&) = delete;
  CookieReader& operator=(const CookieReader&) = delete;

  ~CookieReader() = default;

  void ReadCookies(Profile* profile) {
    base::test::TestFuture<void> signal;
    profile->GetDefaultStoragePartition()
        ->GetCookieManagerForBrowserProcess()
        ->GetAllCookies(base::BindOnce(&CookieReader::OnGotAllCookies,
                                       base::Unretained(this),
                                       signal.GetCallback()));
    EXPECT_TRUE(signal.Wait());
  }

  std::string GetCookieValue(const std::string& name) {
    for (const auto& item : cookie_list_) {
      if (item.Name() == name) {
        return item.Value();
      }
    }
    return std::string();
  }

 private:
  void OnGotAllCookies(base::OnceClosure callback,
                       const net::CookieList& cookies) {
    cookie_list_ = cookies;
    std::move(callback).Run();
  }

  net::CookieList cookie_list_;
};

// PRE_MergeSession is testing merge session for a new profile.
IN_PROC_BROWSER_TEST_F(OAuth2Test, PRE_PRE_PRE_MergeSession) {
  StartNewUserSession(/*wait_for_merge=*/true,
                      /*is_under_advanced_protection=*/false);
  // Check for existence of refresh token.
  CoreAccountId account_id =
      PickAccountId(GetProfile(), kTestGaiaId, kTestEmail);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(account_id));

  EXPECT_EQ(GetOAuthStatusFromLocalState(kTestEmail),
            user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
  CookieReader cookie_reader;
  cookie_reader.ReadCookies(GetProfile());
  EXPECT_EQ(cookie_reader.GetCookieValue("SID"), kTestSessionSIDCookie);
  EXPECT_EQ(cookie_reader.GetCookieValue("LSID"), kTestSessionLSIDCookie);
}

// MergeSession test is running merge session process for an existing profile
// that was generated in PRE_PRE_PRE_MergeSession test. In this test, we
// are not running cookie minting process since the /ListAccounts call confirms
// that the session is not stale.
IN_PROC_BROWSER_TEST_F(OAuth2Test, PRE_PRE_MergeSession) {
  SetupGaiaServerForUnexpiredAccount();
  SimulateNetworkOnline();
  LoginAsExistingUser();
  CookieReader cookie_reader;
  cookie_reader.ReadCookies(GetProfile());
  // These are still cookie values from the initial session since
  // /ListAccounts
  EXPECT_EQ(cookie_reader.GetCookieValue("SID"), kTestSessionSIDCookie);
  EXPECT_EQ(cookie_reader.GetCookieValue("LSID"), kTestSessionLSIDCookie);
}

// MergeSession test is running merge session process for an existing profile
// that was generated in PRE_PRE_MergeSession test.
IN_PROC_BROWSER_TEST_F(OAuth2Test, PRE_MergeSession) {
  SetupGaiaServerForExpiredAccount();
  SimulateNetworkOnline();
  LoginAsExistingUser();
  CookieReader cookie_reader;
  cookie_reader.ReadCookies(GetProfile());
  // These should be cookie values that we generated by calling the cookie
  // minting endpoint, since /ListAccounts should have tell us that the initial
  // session cookies are stale.
  EXPECT_EQ(cookie_reader.GetCookieValue("SID"), kTestSession2SIDCookie);
  EXPECT_EQ(cookie_reader.GetCookieValue("LSID"), kTestSession2LSIDCookie);
}

// MergeSession test is attempting to merge session for an existing profile
// that was generated in PRE_PRE_MergeSession test. This attempt should fail
// since FakeGaia instance isn't configured to return relevant tokens/cookies.
// TODO(crbug.com/40791508): Test is flaky on chromeos
IN_PROC_BROWSER_TEST_F(OAuth2Test, DISABLED_MergeSession) {
  SimulateNetworkOnline();

  EXPECT_EQ(1, LoginScreenTestApi::GetUsersCount());

  // PickAccountId does not work at this point as the primary user profile has
  // not yet been created.
  EXPECT_EQ(GetOAuthStatusFromLocalState(kTestEmail),
            user_manager::User::OAUTH2_TOKEN_STATUS_VALID);

  EXPECT_TRUE(
      TryToLogin(AccountId::FromUserEmailGaiaId(kTestEmail, kTestGaiaId),
                 kTestAccountPassword));

  CoreAccountId account_id =
      PickAccountId(GetProfile(), kTestGaiaId, kTestEmail);
  ASSERT_EQ(kTestGaiaId, account_id.ToString());

  // Wait for the session merge to finish.
  WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_DONE);

  base::RepeatingCallback<bool(const GoogleServiceAuthError&)> predicate =
      base::BindRepeating([](const GoogleServiceAuthError& error) {
        return error.state() == GoogleServiceAuthError::SERVICE_ERROR;
      });
  signin::WaitForErrorStateOfRefreshTokenUpdatedForAccount(
      identity_manager(), account_id, predicate);

  EXPECT_EQ(GetOAuthStatusFromLocalState(kTestEmail),
            user_manager::User::OAUTH2_TOKEN_STATUS_INVALID);
}

// Sets up a new user with stored refresh token.
IN_PROC_BROWSER_TEST_F(OAuth2Test, PRE_OverlappingContinueSessionRestore) {
  StartNewUserSession(/*wait_for_merge=*/true,
                      /*is_under_advanced_protection=*/false);
}

// Tests that ContinueSessionRestore could be called multiple times.
IN_PROC_BROWSER_TEST_F(OAuth2Test, DISABLED_OverlappingContinueSessionRestore) {
  SetupGaiaServerForUnexpiredAccount();
  SimulateNetworkOnline();

  // Blocks database thread to control TokenService::LoadCredentials timing.
  // TODO(achuith): Fix this. crbug.com/753615.
  auto thread_blocker = std::make_unique<ThreadBlocker>(nullptr);

  // Signs in as the existing user created in pre test.
  EXPECT_TRUE(
      TryToLogin(AccountId::FromUserEmailGaiaId(kTestEmail, kTestGaiaId),
                 kTestAccountPassword));

  // Checks that refresh token is not yet loaded.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  const CoreAccountId account_id =
      PickAccountId(GetProfile(), kTestGaiaId, kTestEmail);
  EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(account_id));

  // Invokes ContinueSessionRestore multiple times and there should be
  // no DCHECK failures.
  OAuth2LoginManager* login_manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(GetProfile());
  login_manager->ContinueSessionRestore();
  login_manager->ContinueSessionRestore();

  // Let go DB thread to finish TokenService::LoadCredentials.
  thread_blocker.reset();

  // Session restore can finish normally and token is loaded.
  WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_DONE);
  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(account_id));
}

// Tests that user session is terminated if merge session fails for an online
// sign-in. This is necessary to prevent policy exploit.
// See http://crbug.com/677312
IN_PROC_BROWSER_TEST_F(OAuth2Test, TerminateOnBadMergeSessionAfterOnlineAuth) {
  SimulateNetworkOnline();
  WaitForGaiaPageLoad();

  base::test::TestFuture<void> signal;
  auto subscription =
      browser_shutdown::AddAppTerminatingCallback(signal.GetCallback());

  // Configure FakeGaia so that online auth succeeds but merge session fails.
  FakeGaia::Configuration params;
  params.auth_sid_cookie = kTestAuthSIDCookie;
  params.auth_lsid_cookie = kTestAuthLSIDCookie;
  params.auth_code = kTestAuthCode;
  params.refresh_token = kTestRefreshToken;
  params.access_token = kTestAuthLoginAccessToken;
  fake_gaia_.fake_gaia()->SetConfiguration(params);

  // Simulate an online sign-in.
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(kTestRawEmail, kTestAccountPassword,
                                kTestAccountServices);

  // User session should be terminated.
  EXPECT_TRUE(signal.Wait());

  // Merge session should fail. Check after `termination_waiter` to ensure
  // user profile is initialized and there is an OAuth2LoginManage.
  WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_FAILED);
}

IN_PROC_BROWSER_TEST_F(OAuth2Test, VerifyInAdvancedProtectionAfterOnlineAuth) {
  StartNewUserSession(/*wait_for_merge=*/true,
                      /*is_under_advanced_protection=*/true);

  // Verify that AccountInfo is properly updated.
  auto* identity_manager =
      IdentityManagerFactory::GetInstance()->GetForProfile(GetProfile());
  EXPECT_TRUE(
      identity_manager->FindExtendedAccountInfoByEmailAddress(kTestEmail)
          .is_under_advanced_protection);
}

IN_PROC_BROWSER_TEST_F(OAuth2Test,
                       VerifyNotInAdvancedProtectionAfterOnlineAuth) {
  StartNewUserSession(/*wait_for_merge=*/true,
                      /*is_under_advanced_protection=*/false);

  // Verify that AccountInfo is properly updated.
  auto* identity_manager =
      IdentityManagerFactory::GetInstance()->GetForProfile(GetProfile());
  EXPECT_FALSE(
      identity_manager->FindExtendedAccountInfoByEmailAddress(kTestEmail)
          .is_under_advanced_protection);
}

// FakeGoogle serves content of http://www.google.com/hello_google page for
// merge session tests.
class FakeGoogle {
 public:
  FakeGoogle()
      : start_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                     base::WaitableEvent::InitialState::NOT_SIGNALED),
        merge_session_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  FakeGoogle(const FakeGoogle&) = delete;
  FakeGoogle& operator=(const FakeGoogle&) = delete;

  ~FakeGoogle() = default;

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    // The scheme and host of the URL is actually not important but required to
    // get a valid GURL in order to parse `request.relative_url`.
    GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
    std::string request_path = request_url.path();
    std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
    if (request_path == kHelloPagePath) {  // Serving "google" page.
      start_event_.Signal();
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&FakeGoogle::NotifyPageRequestSignal,
                                    base::Unretained(this)));

      http_response->set_code(net::HTTP_OK);
      http_response->set_content_type("text/html");
      http_response->set_content(kGooglePageContent);
    } else if (request_path == kRandomPagePath) {  // Serving "non-google" page.
      http_response->set_code(net::HTTP_OK);
      http_response->set_content_type("text/html");
      http_response->set_content(kRandomPageContent);
    } else if (hang_multilogin_ && request_path == kMultiLoginPath) {
      merge_session_event_.Signal();
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&FakeGoogle::NotifyMergeSessionSignal,
                                    base::Unretained(this)));
      return std::make_unique<HungResponse>();
    } else {
      return nullptr;  // Request not understood.
    }

    return std::move(http_response);
  }

  // True if we have already served the test page.
  bool IsPageRequested() { return start_event_.IsSignaled(); }

  // Waits until we receive a request to serve the test page.
  void WaitForPageRequest() {
    // If we have already served the request, bail out.
    if (start_event_.IsSignaled())
      return;

    page_request_signal_ = std::make_unique<base::test::TestFuture<void>>();
    EXPECT_TRUE(page_request_signal_->Wait());
  }

  // Waits until we receive a request to serve the cookie minting endpoint.
  void WaitForMergeSessionPageRequest() {
    // If we have already served the request, bail out.
    if (merge_session_event_.IsSignaled())
      return;

    merge_session_signal_ = std::make_unique<base::test::TestFuture<void>>();
    EXPECT_TRUE(merge_session_signal_->Wait());
  }

  void set_hang_multilogin() { hang_multilogin_ = true; }

 private:
  void NotifyPageRequestSignal() {
    if (page_request_signal_) {
      page_request_signal_->SetValue();
    }
  }

  void NotifyMergeSessionSignal() {
    if (merge_session_signal_) {
      merge_session_signal_->SetValue();
    }
  }

  // This event will tell us when we actually see HTTP request on the server
  // side. It should be signalled only after the page/XHR throttle had been
  // removed (after merge session completes).
  base::WaitableEvent start_event_;
  base::WaitableEvent merge_session_event_;
  std::unique_ptr<base::test::TestFuture<void>> page_request_signal_;
  std::unique_ptr<base::test::TestFuture<void>> merge_session_signal_;
  bool hang_multilogin_ = false;
};

class MergeSessionTest : public OAuth2Test,
                         public testing::WithParamInterface<bool> {
 public:
  MergeSessionTest(const MergeSessionTest&) = delete;
  MergeSessionTest& operator=(const MergeSessionTest&) = delete;

 protected:
  MergeSessionTest() = default;

  // OAuth2Test overrides.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OAuth2Test::SetUpCommandLine(command_line);

    // Get fake URL for fake google.com.
    const GURL& server_url = embedded_test_server()->base_url();
    GURL::Replacements replace_google_host;
    replace_google_host.SetHostStr("www.google.com");
    GURL google_url = server_url.ReplaceComponents(replace_google_host);
    fake_google_page_url_ = google_url.Resolve(kHelloPagePath);

    GURL::Replacements replace_non_google_host;
    replace_non_google_host.SetHostStr("www.somethingelse.org");
    GURL non_google_url = server_url.ReplaceComponents(replace_non_google_host);
    non_google_page_url_ = non_google_url.Resolve(kRandomPagePath);
  }

  void RegisterAdditionalRequestHandlers() override {
    OAuth2Test::RegisterAdditionalRequestHandlers();
    AddRequestDeferer(kMultiLoginPath, &multilogin_deferer_);

    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &FakeGoogle::HandleRequest, base::Unretained(&fake_google_)));
  }

 protected:
  void UnblockMergeSession() { multilogin_deferer_.UnblockRequest(); }

  virtual void WaitForMergeSessionToStart() {
    multilogin_deferer_.WaitForRequestToStart();
  }

  bool do_async_xhr() const { return GetParam(); }

  void JsExpectAsync(content::WebContents* web_contents,
                     const std::string& expression) {
    content::ExecuteScriptAsync(web_contents, "!!(" + expression + ");");
  }

  void JsExpectOnBackgroundPageAsync(const std::string& extension_id,
                                     const std::string& expression) {
    extensions::ProcessManager* manager =
        extensions::ProcessManager::Get(GetProfile());
    extensions::ExtensionHost* host =
        manager->GetBackgroundHostForExtension(extension_id);
    if (host == nullptr) {
      ADD_FAILURE() << "Extension " << extension_id
                    << " has no background page.";
      return;
    }

    JsExpectAsync(host->host_contents(), expression);
  }

  void JsExpect(content::WebContents* contents, const std::string& expression) {
    ASSERT_EQ(true, content::EvalJs(contents, "!!(" + expression + ");"))
        << expression;
  }

  const GURL& GetBackGroundPageUrl(const std::string& extension_id) {
    extensions::ProcessManager* manager =
        extensions::ProcessManager::Get(GetProfile());
    extensions::ExtensionHost* host =
        manager->GetBackgroundHostForExtension(extension_id);
    return host->host_contents()->GetURL();
  }

  void JsExpectOnBackgroundPage(const std::string& extension_id,
                                const std::string& expression) {
    extensions::ProcessManager* manager =
        extensions::ProcessManager::Get(GetProfile());
    extensions::ExtensionHost* host =
        manager->GetBackgroundHostForExtension(extension_id);
    if (host == nullptr) {
      ADD_FAILURE() << "Extension " << extension_id
                    << " has no background page.";
      return;
    }

    JsExpect(host->host_contents(), expression);
  }

  FakeGoogle fake_google_;
  RequestDeferrer multilogin_deferer_;
  GURL fake_google_page_url_;
  GURL non_google_page_url_;
};

Browser* FindOrCreateVisibleBrowser(Profile* profile) {
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  Browser* browser = displayer.browser();
  if (browser->tab_strip_model()->count() == 0)
    chrome::AddTabAt(browser, GURL(), -1, true);
  return browser;
}

IN_PROC_BROWSER_TEST_P(MergeSessionTest, PageThrottle) {
  StartNewUserSession(/*wait_for_merge=*/false,
                      /*is_under_advanced_protection=*/false);

  // Try to open a page from google.com.
  Browser* browser = FindOrCreateVisibleBrowser(GetProfile());
  ui_test_utils::NavigateToURLWithDisposition(
      browser, fake_google_page_url_, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  // JavaScript dialog wait setup.
  content::WebContents* tab =
      browser->tab_strip_model()->GetActiveWebContents();
  auto* js_dialog_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(tab);
  base::test::TestFuture<void> dialog_wait;
  js_dialog_manager->SetDialogShownCallbackForTesting(
      dialog_wait.GetCallback());

  // Wait until we get send merge session request.
  WaitForMergeSessionToStart();

  // Make sure the page is blocked by the throttle.
  EXPECT_FALSE(fake_google_.IsPageRequested());

  // Check that throttle page is displayed instead.
  std::u16string title;
  ui_test_utils::GetCurrentTabTitle(browser, &title);
  DVLOG(1) << "Loaded page at the start : " << title;

  // Unblock GAIA request.
  UnblockMergeSession();

  // Wait for the session merge to finish.
  WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_DONE);

  // Make sure the test page is served.
  fake_google_.WaitForPageRequest();

  // Check that real page is no longer blocked by the throttle and that the
  // real page pops up JS dialog.
  EXPECT_TRUE(dialog_wait.Wait());
  js_dialog_manager->HandleJavaScriptDialog(tab, true, nullptr);

  ui_test_utils::GetCurrentTabTitle(browser, &title);
  DVLOG(1) << "Loaded page at the end : " << title;
}

IN_PROC_BROWSER_TEST_P(MergeSessionTest, Throttle) {
  StartNewUserSession(/*wait_for_merge=*/false,
                      /*is_under_advanced_protection=*/false);

  // Wait until we get send merge session request.
  WaitForMergeSessionToStart();

  // Run background page tests. The tests will just wait for XHR request
  // to complete.
  extensions::ResultCatcher catcher;

  std::unique_ptr<ExtensionTestMessageListener> non_google_xhr_listener(
      new ExtensionTestMessageListener("non-google-xhr-received"));

  // Load extension with a background page. The background page will
  // attempt to load `fake_google_page_url_` via XHR.
  const extensions::Extension* ext = LoadMergeSessionExtension();
  ASSERT_TRUE(ext);

  // Kick off XHR request from the extension.
  JsExpectOnBackgroundPageAsync(
      ext->id(), base::StringPrintf("startThrottledTests('%s', '%s', %s)",
                                    fake_google_page_url_.spec().c_str(),
                                    non_google_page_url_.spec().c_str(),
                                    BoolToString(do_async_xhr())));
  ExtensionTestMessageListener listener("Both XHR's Opened");
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Verify that we've sent XHR request from the extension side (async)...
  // The XMLHttpRequest.send() call is blocked when running synchronously
  // so cannot eval JavaScript.
  if (do_async_xhr()) {
    JsExpectOnBackgroundPage(ext->id(),
                             "googleRequestSent && !googleResponseReceived");
  }

  // ...but didn't see it on the server side yet.
  EXPECT_FALSE(fake_google_.IsPageRequested());

  // Unblock GAIA request.
  UnblockMergeSession();

  WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_DONE);

  // Verify that we've sent XHR request from the extension side...
  // Wait until non-google XHR content to load first.
  ASSERT_TRUE(non_google_xhr_listener->WaitUntilSatisfied());

  if (!catcher.GetNextResult()) {
    std::string message = catcher.message();
    ADD_FAILURE() << "Tests failed: " << message;
  }

  EXPECT_TRUE(fake_google_.IsPageRequested());
}

// The test is too slow for the MSan configuration.
#if defined(MEMORY_SANITIZER)
#define MAYBE_XHRNotThrottled DISABLED_XHRNotThrottled
#else
#define MAYBE_XHRNotThrottled XHRNotThrottled
#endif
IN_PROC_BROWSER_TEST_P(MergeSessionTest, MAYBE_XHRNotThrottled) {
  StartNewUserSession(/*wait_for_merge=*/false,
                      /*is_under_advanced_protection=*/false);

  // Wait until we get send merge session request.
  WaitForMergeSessionToStart();

  // Unblock GAIA request.
  UnblockMergeSession();

  // Wait for the session merge to finish.
  WaitForMergeSessionCompletion(OAuth2LoginManager::SESSION_RESTORE_DONE);

  // Run background page tests. The tests will just wait for XHR request
  // to complete.
  extensions::ResultCatcher catcher;

  std::unique_ptr<ExtensionTestMessageListener> non_google_xhr_listener(
      new ExtensionTestMessageListener("non-google-xhr-received"));

  // Load extension with a background page. The background page will
  // attempt to load `fake_google_page_url_` via XHR.
  const extensions::Extension* ext = LoadMergeSessionExtension();
  ASSERT_TRUE(ext);

  // Kick off XHR request from the extension.
  JsExpectOnBackgroundPage(
      ext->id(), base::StringPrintf("startThrottledTests('%s', '%s', %s)",
                                    fake_google_page_url_.spec().c_str(),
                                    non_google_page_url_.spec().c_str(),
                                    BoolToString(do_async_xhr())));

  if (do_async_xhr()) {
    // Verify that we've sent XHR request from the extension side...
    JsExpectOnBackgroundPage(ext->id(), "googleRequestSent");

    // Wait until non-google XHR content to load.
    ASSERT_TRUE(non_google_xhr_listener->WaitUntilSatisfied());
  } else {
    content::RunAllTasksUntilIdle();
  }

  if (!catcher.GetNextResult()) {
    std::string message = catcher.message();
    ADD_FAILURE() << "Tests failed: " << message;
  }

  if (do_async_xhr()) {
    EXPECT_TRUE(fake_google_.IsPageRequested());
  }
}

class MergeSessionTimeoutTest : public MergeSessionTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MergeSessionTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kShortMergeSessionTimeoutForTest);
  }

  void RegisterAdditionalRequestHandlers() override {
    OAuth2Test::RegisterAdditionalRequestHandlers();

    // Do not defer cookie minting requests (like the base class does) because
    // this test will intentionally hang that request to force a timeout.
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &FakeGoogle::HandleRequest, base::Unretained(&fake_google_)));
    // Hanging cookie minting requests is implemented in `fake_google_`, so
    // register it with the GAIA test server as well.
    fake_gaia_.gaia_server()->RegisterRequestHandler(base::BindRepeating(
        &FakeGoogle::HandleRequest, base::Unretained(&fake_google_)));
  }

  void WaitForMergeSessionToStart() override {
    fake_google_.WaitForMergeSessionPageRequest();
  }
};

// TODO(b/320482170) - Consider splitting this test into 2 - one for Google web
// properties, and one for non-Google web properties. It is not possible right
// now because chrome/test/data/extensions/api_test/merge_session/background.js
// has a bunch of hidden assumptions about the ordering of Google and non-Google
// web requests and doesn't really allow firing Google requests without
// non-Google requests (and vice versa).
IN_PROC_BROWSER_TEST_P(MergeSessionTimeoutTest, XHRMergeTimeout) {
  fake_google_.set_hang_multilogin();

  StartNewUserSession(/*wait_for_merge=*/false,
                      /*is_under_advanced_protection=*/false);

  WaitForMergeSessionToStart();

  // Run background page tests. The tests will just wait for XHR request
  // to complete.
  extensions::ResultCatcher catcher;

  std::unique_ptr<ExtensionTestMessageListener> non_google_xhr_listener(
      new ExtensionTestMessageListener("non-google-xhr-received"));
  std::unique_ptr<ExtensionTestMessageListener> google_xhr_listener(
      new ExtensionTestMessageListener("google-xhr-received"));

  // Load extension with a background page. The background page will
  // attempt to load `fake_google_page_url_` via XHR.
  const extensions::Extension* ext = LoadMergeSessionExtension();
  ASSERT_TRUE(ext);

  const base::Time start_time = base::Time::Now();

  // Kick off XHR request from the extension.
  JsExpectOnBackgroundPageAsync(
      ext->id(), base::StringPrintf("startThrottledTests('%s', '%s', %s)",
                                    fake_google_page_url_.spec().c_str(),
                                    non_google_page_url_.spec().c_str(),
                                    BoolToString(do_async_xhr())));

  if (do_async_xhr()) {
    // Verify that we've sent XHR requests from the extension side...
    JsExpectOnBackgroundPage(ext->id(),
                             "googleRequestSent && !googleResponseReceived");
    JsExpectOnBackgroundPage(ext->id(), "nonGoogleRequestSent");

    // ...but didn't see it on the server side yet.
    EXPECT_FALSE(fake_google_.IsPageRequested());

    // Wait until all the XHR loads complete.
    ASSERT_TRUE(google_xhr_listener->WaitUntilSatisfied());
    ASSERT_TRUE(non_google_xhr_listener->WaitUntilSatisfied());

    // If the test runs in less than the test timeout (1 second) then we know
    // that there was no delay. However a slowly running test can still take
    // longer than the timeout.
    base::TimeDelta test_duration = base::Time::Now() - start_time;
    EXPECT_GE(test_duration, base::Seconds(1));
  } else {
    content::RunAllTasksUntilIdle();
  }

  if (!catcher.GetNextResult()) {
    std::string message = catcher.message();
    ADD_FAILURE() << "Tests failed: " << message;
  }

  if (do_async_xhr()) {
    EXPECT_TRUE(fake_google_.IsPageRequested());
  }

  // Because this test has hung the cookie minting response,
  // `UserSessionManager` is still observing `OAuth2LoginManager` - which fails
  // a DCHECK in `~OAuth2LoginManager()`. Manually change the state to avoid
  // this.
  SetSessionRestoreState(
      OAuth2LoginManager::SessionRestoreState::SESSION_RESTORE_FAILED);
}

INSTANTIATE_TEST_SUITE_P(All, MergeSessionTest, testing::Bool());

INSTANTIATE_TEST_SUITE_P(All, MergeSessionTimeoutTest, testing::Bool());

}  // namespace ash
