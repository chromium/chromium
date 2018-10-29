// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/mutable_profile_oauth2_token_service_delegate.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/scoped_account_consistency.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/dice_header_helper.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/user_events/user_event_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using signin::AccountConsistencyMethod;

namespace {

constexpr int kAccountReconcilorDelayMs = 10;

enum SignoutType {
  kSignoutTypeFirst = 0,

  kAllAccounts = 0,       // Sign out from all accounts.
  kMainAccount = 1,       // Sign out from main account only.
  kSecondaryAccount = 2,  // Sign out from secondary account only.

  kSignoutTypeLast
};

const char kAuthorizationCode[] = "authorization_code";
const char kDiceResponseHeader[] = "X-Chrome-ID-Consistency-Response";
const char kChromeSyncEndpointURL[] = "/signin/chrome/sync";
const char kEnableSyncURL[] = "/enable_sync";
const char kGoogleSignoutResponseHeader[] = "Google-Accounts-SignOut";
const char kMainEmail[] = "main_email@example.com";
const char kMainGaiaID[] = "main_gaia_id";
const char kNoDiceRequestHeader[] = "NoDiceHeader";
const char kOAuth2TokenExchangeURL[] = "/oauth2/v4/token";
const char kOAuth2TokenRevokeURL[] = "/o/oauth2/revoke";
const char kSecondaryEmail[] = "secondary_email@example.com";
const char kSecondaryGaiaID[] = "secondary_gaia_id";
const char kSigninURL[] = "/signin";
const char kSignoutURL[] = "/signout";

// Test response that does not complete synchronously. It must be unblocked by
// calling the completion closure.
class BlockedHttpResponse : public net::test_server::BasicHttpResponse {
 public:
  explicit BlockedHttpResponse(
      base::OnceCallback<void(base::OnceClosure)> callback)
      : callback_(std::move(callback)) {}

  void SendResponse(
      const net::test_server::SendBytesCallback& send,
      const net::test_server::SendCompleteCallback& done) override {
    // Called on the IO thread to unblock the response.
    base::OnceClosure unblock_io_thread =
        base::BindOnce(send, ToResponseString(), done);
    // Unblock the response from any thread by posting a task to the IO thread.
    base::OnceClosure unblock_any_thread =
        base::BindOnce(base::IgnoreResult(&base::TaskRunner::PostTask),
                       base::ThreadTaskRunnerHandle::Get(), FROM_HERE,
                       std::move(unblock_io_thread));
    // Pass |unblock_any_thread| to the caller on the UI thread.
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(std::move(callback_), std::move(unblock_any_thread)));
  }

 private:
  base::OnceCallback<void(base::OnceClosure)> callback_;
};

}  // namespace

namespace FakeGaia {

// Handler for the signin page on the embedded test server.
// The response has the content of the Dice request header in its body, and has
// the Dice response header.
// Handles both the "Chrome Sync" endpoint and the old endpoint.
std::unique_ptr<HttpResponse> HandleSigninURL(
    const base::RepeatingCallback<void(const std::string&)>& callback,
    const HttpRequest& request) {
  if (!net::test_server::ShouldHandle(request, kSigninURL) &&
      !net::test_server::ShouldHandle(request, kChromeSyncEndpointURL))
    return nullptr;

  // Extract Dice request header.
  std::string header_value = kNoDiceRequestHeader;
  auto it = request.headers.find(signin::kDiceRequestHeader);
  if (it != request.headers.end())
    header_value = it->second;

  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindRepeating(callback, header_value));

  // Add the SIGNIN dice header.
  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  if (header_value != kNoDiceRequestHeader) {
    http_response->AddCustomHeader(
        kDiceResponseHeader,
        base::StringPrintf(
            "action=SIGNIN,authuser=1,id=%s,email=%s,authorization_code=%s",
            kMainGaiaID, kMainEmail, kAuthorizationCode));
  }

  // When hitting the Chrome Sync endpoint, redirect to kEnableSyncURL, which
  // adds the ENABLE_SYNC dice header.
  if (net::test_server::ShouldHandle(request, kChromeSyncEndpointURL)) {
    http_response->set_code(net::HTTP_FOUND);  // 302 redirect.
    http_response->AddCustomHeader("location", kEnableSyncURL);
  }

  http_response->AddCustomHeader("Cache-Control", "no-store");
  return std::move(http_response);
}

// Handler for the Gaia endpoint adding the ENABLE_SYNC dice header.
std::unique_ptr<HttpResponse> HandleEnableSyncURL(
    const base::RepeatingCallback<void(base::OnceClosure)>& callback,
    const HttpRequest& request) {
  if (!net::test_server::ShouldHandle(request, kEnableSyncURL))
    return nullptr;

  std::unique_ptr<BlockedHttpResponse> http_response =
      std::make_unique<BlockedHttpResponse>(callback);
  http_response->AddCustomHeader(
      kDiceResponseHeader,
      base::StringPrintf("action=ENABLE_SYNC,authuser=1,id=%s,email=%s",
                         kMainGaiaID, kMainEmail));
  http_response->AddCustomHeader("Cache-Control", "no-store");
  return std::move(http_response);
}

// Handler for the signout page on the embedded test server.
// Responds with a Google-Accounts-SignOut header for the main account, the
// secondary account, or both (depending on the SignoutType, which is encoded in
// the query string).
std::unique_ptr<HttpResponse> HandleSignoutURL(const HttpRequest& request) {
  if (!net::test_server::ShouldHandle(request, kSignoutURL))
    return nullptr;

  // Build signout header.
  int query_value;
  EXPECT_TRUE(base::StringToInt(request.GetURL().query(), &query_value));
  SignoutType signout_type = static_cast<SignoutType>(query_value);
  EXPECT_GE(signout_type, kSignoutTypeFirst);
  EXPECT_LT(signout_type, kSignoutTypeLast);
  std::string signout_header_value;
  if (signout_type == kAllAccounts || signout_type == kMainAccount) {
    signout_header_value =
        base::StringPrintf("email=\"%s\", obfuscatedid=\"%s\", sessionindex=1",
                           kMainEmail, kMainGaiaID);
  }
  if (signout_type == kAllAccounts || signout_type == kSecondaryAccount) {
    if (!signout_header_value.empty())
      signout_header_value += ", ";
    signout_header_value +=
        base::StringPrintf("email=\"%s\", obfuscatedid=\"%s\", sessionindex=2",
                           kSecondaryEmail, kSecondaryGaiaID);
  }

  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  http_response->AddCustomHeader(kGoogleSignoutResponseHeader,
                                 signout_header_value);
  http_response->AddCustomHeader("Cache-Control", "no-store");
  return std::move(http_response);
}

// Handler for OAuth2 token exchange.
// Checks that the request is well formatted and returns a refresh token in a
// JSON dictionary.
std::unique_ptr<HttpResponse> HandleOAuth2TokenExchangeURL(
    const base::RepeatingCallback<void(base::OnceClosure)>& callback,
    const HttpRequest& request) {
  if (!net::test_server::ShouldHandle(request, kOAuth2TokenExchangeURL))
    return nullptr;

  // Check that the authorization code is somewhere in the request body.
  if (!request.has_content)
    return nullptr;
  if (request.content.find(kAuthorizationCode) == std::string::npos)
    return nullptr;

  std::unique_ptr<BlockedHttpResponse> http_response =
      std::make_unique<BlockedHttpResponse>(callback);

  std::string content =
      "{"
      "  \"access_token\":\"access_token\","
      "  \"refresh_token\":\"new_refresh_token\","
      "  \"expires_in\":9999"
      "}";

  http_response->set_content(content);
  http_response->set_content_type("text/plain");
  http_response->AddCustomHeader("Cache-Control", "no-store");
  return std::move(http_response);
}

// Handler for OAuth2 token revocation.
std::unique_ptr<HttpResponse> HandleOAuth2TokenRevokeURL(
    const base::RepeatingClosure& callback,
    const HttpRequest& request) {
  if (!net::test_server::ShouldHandle(request, kOAuth2TokenRevokeURL))
    return nullptr;

  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI}, callback);

  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  http_response->AddCustomHeader("Cache-Control", "no-store");
  return std::move(http_response);
}

// Handler for ServiceLogin on the embedded test server.
// Calls the callback with the dice request header, or kNoDiceRequestHeader if
// there is no Dice header.
std::unique_ptr<HttpResponse> HandleChromeSigninEmbeddedURL(
    const base::RepeatingCallback<void(const std::string&)>& callback,
    const HttpRequest& request) {
  if (!net::test_server::ShouldHandle(request,
                                      "/embedded/setup/chrome/usermenu"))
    return nullptr;

  std::string dice_request_header(kNoDiceRequestHeader);
  auto it = request.headers.find(signin::kDiceRequestHeader);
  if (it != request.headers.end())
    dice_request_header = it->second;
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindRepeating(callback, dice_request_header));

  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  http_response->AddCustomHeader("Cache-Control", "no-store");
  return std::move(http_response);
}

}  // namespace FakeGaia

class DiceBrowserTestBase : public InProcessBrowserTest,
                            public AccountReconcilor::Observer,
                            public identity::IdentityManager::Observer {
 protected:
  ~DiceBrowserTestBase() override {}

  explicit DiceBrowserTestBase(
      AccountConsistencyMethod account_consistency_method)
      : scoped_account_consistency_(account_consistency_method),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        enable_sync_requested_(false),
        token_requested_(false),
        refresh_token_available_(false),
        token_revoked_notification_count_(0),
        token_revoked_count_(0),
        reconcilor_blocked_count_(0),
        reconcilor_unblocked_count_(0),
        reconcilor_started_count_(0) {
    https_server_.RegisterDefaultHandler(base::BindRepeating(
        &FakeGaia::HandleSigninURL,
        base::BindRepeating(&DiceBrowserTestBase::OnSigninRequest,
                            base::Unretained(this))));
    https_server_.RegisterDefaultHandler(base::BindRepeating(
        &FakeGaia::HandleEnableSyncURL,
        base::BindRepeating(&DiceBrowserTestBase::OnEnableSyncRequest,
                            base::Unretained(this))));
    https_server_.RegisterDefaultHandler(
        base::BindRepeating(&FakeGaia::HandleSignoutURL));
    https_server_.RegisterDefaultHandler(base::BindRepeating(
        &FakeGaia::HandleOAuth2TokenExchangeURL,
        base::BindRepeating(&DiceBrowserTestBase::OnTokenExchangeRequest,
                            base::Unretained(this))));
    https_server_.RegisterDefaultHandler(base::BindRepeating(
        &FakeGaia::HandleOAuth2TokenRevokeURL,
        base::BindRepeating(&DiceBrowserTestBase::OnTokenRevocationRequest,
                            base::Unretained(this))));
    https_server_.RegisterDefaultHandler(base::BindRepeating(
        &FakeGaia::HandleChromeSigninEmbeddedURL,
        base::BindRepeating(&DiceBrowserTestBase::OnChromeSigninEmbeddedRequest,
                            base::Unretained(this))));
    signin::SetDiceAccountReconcilorBlockDelayForTesting(
        kAccountReconcilorDelayMs);
  }

  // Navigates to the given path on the test server.
  void NavigateToURL(const std::string& path) {
    ui_test_utils::NavigateToURL(browser(), https_server_.GetURL(path));
  }

  // Returns the token service.
  ProfileOAuth2TokenService* GetTokenService() {
    return ProfileOAuth2TokenServiceFactory::GetForProfile(
        browser()->profile());
  }

  // Returns the account tracker service.
  AccountTrackerService* GetAccountTrackerService() {
    return AccountTrackerServiceFactory::GetForProfile(browser()->profile());
  }

  // Returns the identity manager.
  identity::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

  // Returns the signin manager.
  SigninManager* GetSigninManager() {
    return SigninManagerFactory::GetForProfile(browser()->profile());
  }

  // Returns the account ID associated with kMainEmail, kMainGaiaID.
  std::string GetMainAccountID() {
    return GetAccountTrackerService()->PickAccountIdForAccount(kMainGaiaID,
                                                               kMainEmail);
  }

  // Returns the account ID associated with kSecondaryEmail, kSecondaryGaiaID.
  std::string GetSecondaryAccountID() {
    return GetAccountTrackerService()->PickAccountIdForAccount(kSecondaryGaiaID,
                                                               kSecondaryEmail);
  }

  std::string GetDeviceId() {
    return GetSigninScopedDeviceIdForProfile(browser()->profile());
  }

  // Signin with a main account and add token for a secondary account.
  void SetupSignedInAccounts() {
    // Signin main account.
    SigninManager* signin_manager = GetSigninManager();
    signin_manager->StartSignInWithRefreshToken(
        "existing_refresh_token", kMainGaiaID, kMainEmail, "password",
        SigninManager::OAuthTokenFetchedCallback());
    ASSERT_TRUE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));
    ASSERT_FALSE(GetTokenService()->RefreshTokenHasError(GetMainAccountID()));
    ASSERT_EQ(GetMainAccountID(), signin_manager->GetAuthenticatedAccountId());

    // Add a token for a secondary account.
    std::string secondary_account_id =
        GetAccountTrackerService()->SeedAccountInfo(kSecondaryGaiaID,
                                                    kSecondaryEmail);
    GetTokenService()->UpdateCredentials(secondary_account_id, "other_token");
    ASSERT_TRUE(
        GetTokenService()->RefreshTokenIsAvailable(secondary_account_id));
    ASSERT_FALSE(GetTokenService()->RefreshTokenHasError(secondary_account_id));
  }

  // Navigate to a Gaia URL setting the Google-Accounts-SignOut header.
  void SignOutWithDice(SignoutType signout_type) {
    NavigateToURL(base::StringPrintf("%s?%i", kSignoutURL, signout_type));
    signin::AccountConsistencyMethod account_consistency =
        AccountConsistencyModeManager::GetMethodForProfile(
            browser()->profile());
    if (signin::DiceMethodGreaterOrEqual(
            account_consistency,
            signin::AccountConsistencyMethod::kDiceMigration)) {
      EXPECT_EQ(1, reconcilor_blocked_count_);
      WaitForReconcilorUnblockedCount(1);
    } else {
      EXPECT_EQ(0, reconcilor_blocked_count_);
      WaitForReconcilorUnblockedCount(0);
    }
    base::RunLoop().RunUntilIdle();
  }

  // InProcessBrowserTest:
  void SetUp() override {
    ASSERT_TRUE(https_server_.InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    const GURL& base_url = https_server_.base_url();
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url.spec());
    command_line->AppendSwitchASCII(switches::kGoogleApisUrl, base_url.spec());
    command_line->AppendSwitchASCII(switches::kLsoUrl, base_url.spec());
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    https_server_.StartAcceptingConnections();

    GetIdentityManager()->AddObserver(this);
    // Wait for the token service to be ready.
    if (!GetTokenService()->AreAllCredentialsLoaded())
      WaitForClosure(&tokens_loaded_quit_closure_);
    ASSERT_TRUE(GetTokenService()->AreAllCredentialsLoaded());

    AccountReconcilor* reconcilor =
        AccountReconcilorFactory::GetForProfile(browser()->profile());

    // Reconcilor starts as soon as the token service finishes loading its
    // credentials. Abort the reconcilor here to make sure tests start in a
    // stable state.
    reconcilor->AbortReconcile();
    reconcilor->AddObserver(this);
  }

  void TearDownOnMainThread() override {
    GetIdentityManager()->RemoveObserver(this);
    AccountReconcilorFactory::GetForProfile(browser()->profile())
        ->RemoveObserver(this);
  }

  // Calls |closure| if it is not null and resets it after.
  void RunClosureIfValid(base::OnceClosure closure) {
    if (closure)
      std::move(closure).Run();
  }

  // Creates and runs a RunLoop until |closure| is called.
  void WaitForClosure(base::OnceClosure* closure) {
    base::RunLoop run_loop;
    *closure = run_loop.QuitClosure();
    run_loop.Run();
  }

  // FakeGaia callbacks:
  void OnSigninRequest(const std::string& dice_request_header) {
    EXPECT_EQ(dice_request_header != kNoDiceRequestHeader,
              IsReconcilorBlocked());
    dice_request_header_ = dice_request_header;
  }

  void OnChromeSigninEmbeddedRequest(const std::string& dice_request_header) {
    dice_request_header_ = dice_request_header;
    RunClosureIfValid(std::move(chrome_signin_embedded_quit_closure_));
  }

  void OnEnableSyncRequest(base::OnceClosure unblock_response_closure) {
    EXPECT_TRUE(IsReconcilorBlocked());
    enable_sync_requested_ = true;
    RunClosureIfValid(std::move(enable_sync_requested_quit_closure_));
    unblock_enable_sync_response_closure_ = std::move(unblock_response_closure);
  }

  void OnTokenExchangeRequest(base::OnceClosure unblock_response_closure) {
    // The token must be exchanged only once.
    EXPECT_FALSE(token_requested_);
    EXPECT_TRUE(IsReconcilorBlocked());
    token_requested_ = true;
    RunClosureIfValid(std::move(token_requested_quit_closure_));
    unblock_token_exchange_response_closure_ =
        std::move(unblock_response_closure);
  }

  void OnTokenRevocationRequest() {
    ++token_revoked_count_;
    RunClosureIfValid(std::move(token_revoked_quit_closure_));
  }

  // AccountReconcilor::Observer:
  void OnBlockReconcile() override { ++reconcilor_blocked_count_; }
  void OnUnblockReconcile() override {
    ++reconcilor_unblocked_count_;
    RunClosureIfValid(std::move(unblock_count_quit_closure_));
  }
  void OnStartReconcile() override { ++reconcilor_started_count_; }

  // identity::IdentityManager::Observer
  void OnPrimaryAccountSet(const AccountInfo& primary_account_info) override {
    RunClosureIfValid(std::move(on_primary_account_set_quit_closure_));
  }

  void OnRefreshTokenUpdatedForAccount(const AccountInfo& account_info,
                                       bool is_valid) override {
    if (is_valid && account_info.account_id == GetMainAccountID()) {
      refresh_token_available_ = true;
      RunClosureIfValid(std::move(refresh_token_available_quit_closure_));
    }
  }

  void OnRefreshTokenRemovedForAccount(const std::string& account_id) override {
    ++token_revoked_notification_count_;
  }

  void OnRefreshTokensLoaded() override {
    RunClosureIfValid(std::move(tokens_loaded_quit_closure_));
  }

  // Returns true if the account reconcilor is currently blocked.
  bool IsReconcilorBlocked() {
    EXPECT_GE(reconcilor_blocked_count_, reconcilor_unblocked_count_);
    EXPECT_LE(reconcilor_blocked_count_, reconcilor_unblocked_count_ + 1);
    return (reconcilor_unblocked_count_ + 1) == reconcilor_blocked_count_;
  }

  // Waits until |reconcilor_unblocked_count_| reaches |count|.
  void WaitForReconcilorUnblockedCount(int count) {
    if (reconcilor_unblocked_count_ == count)
      return;

    ASSERT_EQ(count - 1, reconcilor_unblocked_count_);
    // Wait for the timeout after the request is complete.
    WaitForClosure(&unblock_count_quit_closure_);
    EXPECT_EQ(count, reconcilor_unblocked_count_);
  }

  // Waits until the user is authenticated.
  void WaitForSigninSucceeded() {
    if (GetIdentityManager()->GetPrimaryAccountId().empty())
      WaitForClosure(&on_primary_account_set_quit_closure_);
  }

  // Waits for the ENABLE_SYNC request to hit the server, and unblocks the
  // response. If this is not called, ENABLE_SYNC will not be sent by the
  // server.
  // Note: this does not wait for the response to reach Chrome.
  void SendEnableSyncResponse() {
    if (!enable_sync_requested_)
      WaitForClosure(&enable_sync_requested_quit_closure_);
    DCHECK(unblock_enable_sync_response_closure_);
    std::move(unblock_enable_sync_response_closure_).Run();
  }

  // Waits until the token request is sent to the server, the response is
  // received and the refresh token is available. If this is not called, the
  // refresh token will not be sent by the server.
  void SendRefreshTokenResponse() {
    // Wait for the request hitting the server.
    if (!token_requested_)
      WaitForClosure(&token_requested_quit_closure_);
    EXPECT_TRUE(token_requested_);
    // Unblock the server response.
    DCHECK(unblock_token_exchange_response_closure_);
    std::move(unblock_token_exchange_response_closure_).Run();
    // Wait for the response coming back.
    if (!refresh_token_available_)
      WaitForClosure(&refresh_token_available_quit_closure_);
    EXPECT_TRUE(refresh_token_available_);
  }

  void WaitForTokenRevokedCount(int count) {
    EXPECT_LE(token_revoked_count_, count);
    while (token_revoked_count_ < count)
      WaitForClosure(&token_revoked_quit_closure_);
    EXPECT_EQ(count, token_revoked_count_);
  }

  ScopedAccountConsistency scoped_account_consistency_;
  net::EmbeddedTestServer https_server_;
  bool enable_sync_requested_;
  bool token_requested_;
  bool refresh_token_available_;
  int token_revoked_notification_count_;
  int token_revoked_count_;
  int reconcilor_blocked_count_;
  int reconcilor_unblocked_count_;
  int reconcilor_started_count_;
  std::string dice_request_header_;

  // Unblocks the server responses.
  base::OnceClosure unblock_token_exchange_response_closure_;
  base::OnceClosure unblock_enable_sync_response_closure_;

  // Used for waiting asynchronous events.
  base::OnceClosure enable_sync_requested_quit_closure_;
  base::OnceClosure token_requested_quit_closure_;
  base::OnceClosure token_revoked_quit_closure_;
  base::OnceClosure refresh_token_available_quit_closure_;
  base::OnceClosure chrome_signin_embedded_quit_closure_;
  base::OnceClosure unblock_count_quit_closure_;
  base::OnceClosure tokens_loaded_quit_closure_;
  base::OnceClosure on_primary_account_set_quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(DiceBrowserTestBase);
};

class DiceBrowserTest : public DiceBrowserTestBase {
 public:
  DiceBrowserTest() : DiceBrowserTestBase(AccountConsistencyMethod::kDice) {}
};

class DiceFixAuthErrorsBrowserTest : public DiceBrowserTestBase {
 public:
  DiceFixAuthErrorsBrowserTest()
      : DiceBrowserTestBase(AccountConsistencyMethod::kDiceFixAuthErrors) {}
};

// Checks that signin on Gaia triggers the fetch for a refresh token.
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, Signin) {
  EXPECT_EQ(0, reconcilor_started_count_);

  // Navigate to Gaia and sign in.
  NavigateToURL(kSigninURL);

  // Check that the Dice request header was sent.
  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  EXPECT_EQ(base::StringPrintf("version=%s,client_id=%s,device_id=%s,"
                               "signin_mode=all_accounts,"
                               "signout_mode=show_confirmation",
                               signin::kDiceProtocolVersion, client_id.c_str(),
                               GetDeviceId().c_str()),
            dice_request_header_);

  // Check that the token was requested and added to the token service.
  SendRefreshTokenResponse();
  EXPECT_TRUE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));
  // Sync should not be enabled.
  EXPECT_TRUE(GetSigninManager()->GetAuthenticatedAccountId().empty());
  EXPECT_TRUE(GetSigninManager()->GetAccountIdForAuthInProgress().empty());

  EXPECT_EQ(1, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(1);
  EXPECT_EQ(1, reconcilor_started_count_);
}

// Checks that re-auth on Gaia triggers the fetch for a refresh token.
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, Reauth) {
  EXPECT_EQ(0, reconcilor_started_count_);

  // Start from a signed-in state.
  SetupSignedInAccounts();
  EXPECT_EQ(1, reconcilor_started_count_);

  // Navigate to Gaia and sign in again with the main account.
  NavigateToURL(kSigninURL);

  // Check that the Dice request header was sent.
  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  EXPECT_EQ(base::StringPrintf("version=%s,client_id=%s,device_id=%s,"
                               "signin_mode=all_accounts,"
                               "signout_mode=show_confirmation",
                               signin::kDiceProtocolVersion, client_id.c_str(),
                               GetDeviceId().c_str()),
            dice_request_header_);

  // Check that the token was requested and added to the token service.
  SendRefreshTokenResponse();
  EXPECT_EQ(GetMainAccountID(),
            GetSigninManager()->GetAuthenticatedAccountId());

  // Old token must not be revoked (see http://crbug.com/865189).
  EXPECT_EQ(0, token_revoked_notification_count_);

  EXPECT_EQ(1, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(1);
  EXPECT_EQ(2, reconcilor_started_count_);
}

// Checks that the Dice signout flow works and deletes all tokens.
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, SignoutMainAccount) {
  // Start from a signed-in state.
  SetupSignedInAccounts();

  // Signout from main account.
  SignOutWithDice(kMainAccount);

  // Check that the user is in error state.
  EXPECT_EQ(GetMainAccountID(),
            GetSigninManager()->GetAuthenticatedAccountId());
  MutableProfileOAuth2TokenServiceDelegate* delegate =
      static_cast<MutableProfileOAuth2TokenServiceDelegate*>(
          GetTokenService()->GetDelegate());
  EXPECT_TRUE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));
  EXPECT_TRUE(GetTokenService()->RefreshTokenHasError(GetMainAccountID()));
  EXPECT_EQ(MutableProfileOAuth2TokenServiceDelegate::kInvalidRefreshToken,
            delegate->GetRefreshTokenForTest(GetMainAccountID()));
  EXPECT_TRUE(
      GetTokenService()->RefreshTokenIsAvailable(GetSecondaryAccountID()));
  EXPECT_EQ("other_token",
            delegate->GetRefreshTokenForTest(GetSecondaryAccountID()));

  // Token for main account is revoked on server but not notified in the client.
  EXPECT_EQ(0, token_revoked_notification_count_);
  WaitForTokenRevokedCount(1);

  EXPECT_EQ(1, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(1);
}

// Checks that signing out from a secondary account does not delete the main
// token.
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, SignoutSecondaryAccount) {
  // Start from a signed-in state.
  SetupSignedInAccounts();

  // Signout from secondary account.
  SignOutWithDice(kSecondaryAccount);

  // Check that the user is still signed in from main account, but secondary
  // token is deleted.
  EXPECT_EQ(GetMainAccountID(),
            GetSigninManager()->GetAuthenticatedAccountId());
  EXPECT_TRUE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));
  EXPECT_FALSE(
      GetTokenService()->RefreshTokenIsAvailable(GetSecondaryAccountID()));
  EXPECT_EQ(1, token_revoked_notification_count_);
  WaitForTokenRevokedCount(1);
  EXPECT_EQ(1, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(1);
}

// Checks that the Dice signout flow works and deletes all tokens.
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, SignoutAllAccounts) {
  // Start from a signed-in state.
  SetupSignedInAccounts();

  // Signout from all accounts.
  SignOutWithDice(kAllAccounts);

  // Check that the user is in error state.
  EXPECT_EQ(GetMainAccountID(),
            GetSigninManager()->GetAuthenticatedAccountId());
  EXPECT_TRUE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));
  EXPECT_TRUE(GetTokenService()->RefreshTokenHasError(GetMainAccountID()));
  MutableProfileOAuth2TokenServiceDelegate* delegate =
      static_cast<MutableProfileOAuth2TokenServiceDelegate*>(
          GetTokenService()->GetDelegate());
  EXPECT_EQ(MutableProfileOAuth2TokenServiceDelegate::kInvalidRefreshToken,
            delegate->GetRefreshTokenForTest(GetMainAccountID()));
  EXPECT_FALSE(
      GetTokenService()->RefreshTokenIsAvailable(GetSecondaryAccountID()));

  // Token for main account is revoked on server but not notified in the client.
  EXPECT_EQ(1, token_revoked_notification_count_);
  WaitForTokenRevokedCount(2);

  EXPECT_EQ(1, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(1);
}

// Checks that Dice request header is not set from request from WebUI.
// See https://crbug.com/428396
#if defined(OS_WIN)
#define MAYBE_NoDiceFromWebUI DISABLED_NoDiceFromWebUI
#else
#define MAYBE_NoDiceFromWebUI NoDiceFromWebUI
#endif
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, MAYBE_NoDiceFromWebUI) {
  // Navigate to Gaia and from the native tab, which uses an extension.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome:chrome-signin"));

  // Check that the request had no Dice request header.
  if (dice_request_header_.empty())
    WaitForClosure(&chrome_signin_embedded_quit_closure_);
  EXPECT_EQ(kNoDiceRequestHeader, dice_request_header_);
  EXPECT_EQ(0, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(0);
}

// Checks that signin on Gaia does not trigger the fetch of refresh token when
// there is no authentication error.
IN_PROC_BROWSER_TEST_F(DiceFixAuthErrorsBrowserTest, SigninNoAuthError) {
  // Start from a signed-in state.
  SetupSignedInAccounts();
  syncer::SyncPrefs(browser()->profile()->GetPrefs()).SetFirstSetupComplete();

  // Navigate to Gaia and sign in.
  NavigateToURL(kSigninURL);

  // Check that the Dice request header was not sent.
  EXPECT_EQ(kNoDiceRequestHeader, dice_request_header_);
  EXPECT_EQ(0, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(0);
}

// Checks that signin on Gaia does not triggers the fetch for a refresh token
// when the user is not signed into Chrome.
IN_PROC_BROWSER_TEST_F(DiceFixAuthErrorsBrowserTest, NotSignedInChrome) {
  // Setup authentication error.
  syncer::SyncPrefs(browser()->profile()->GetPrefs()).SetSyncAuthError(true);
  syncer::SyncPrefs(browser()->profile()->GetPrefs()).SetFirstSetupComplete();

  // Navigate to Gaia and sign in.
  NavigateToURL(kSigninURL);

  // Check that the Dice request header was not sent.
  EXPECT_EQ(kNoDiceRequestHeader, dice_request_header_);
  EXPECT_EQ(0, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(0);
}

// Checks that a refresh token is not requested when accounts don't match.
IN_PROC_BROWSER_TEST_F(DiceFixAuthErrorsBrowserTest, SigninAccountMismatch) {
  // Sign in to Chrome with secondary account, with authentication error.
  SigninManager* signin_manager = GetSigninManager();
  signin_manager->StartSignInWithRefreshToken(
      "existing_refresh_token", kSecondaryGaiaID, kSecondaryEmail, "password",
      SigninManager::OAuthTokenFetchedCallback());
  ASSERT_TRUE(
      GetTokenService()->RefreshTokenIsAvailable(GetSecondaryAccountID()));
  ASSERT_EQ(GetSecondaryAccountID(),
            signin_manager->GetAuthenticatedAccountId());
  syncer::SyncPrefs(browser()->profile()->GetPrefs()).SetSyncAuthError(true);
  syncer::SyncPrefs(browser()->profile()->GetPrefs()).SetFirstSetupComplete();

  // Navigate to Gaia and sign in with the main account (account mismatch).
  NavigateToURL(kSigninURL);

  // Check that the Dice request header was sent.
  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  EXPECT_EQ(base::StringPrintf("version=%s,client_id=%s,device_id=%s,"
                               "sync_account_id=%s,signin_mode=sync_account,"
                               "signout_mode=no_confirmation",
                               signin::kDiceProtocolVersion, client_id.c_str(),
                               GetDeviceId().c_str(),
                               GetSecondaryAccountID().c_str()),
            dice_request_header_);

  // Check that the token was not requested and the authenticated account did
  // not change.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(token_requested_);
  EXPECT_FALSE(refresh_token_available_);
  EXPECT_EQ(GetSecondaryAccountID(),
            GetSigninManager()->GetAuthenticatedAccountId());
  EXPECT_EQ(1, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(1);
}

// Checks that signin on Gaia triggers the fetch for a refresh token when there
// is an authentication error and the user is re-authenticating on the web.
// This test is similar to DiceBrowserTest.Reauth.
IN_PROC_BROWSER_TEST_F(DiceFixAuthErrorsBrowserTest, ReauthFixAuthError) {
  // Start from a signed-in state with authentication error.
  SetupSignedInAccounts();
  syncer::SyncPrefs(browser()->profile()->GetPrefs()).SetSyncAuthError(true);
  syncer::SyncPrefs(browser()->profile()->GetPrefs()).SetFirstSetupComplete();

  // Navigate to Gaia and sign in again with the main account.
  NavigateToURL(kSigninURL);

  // Check that the Dice request header was sent.
  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  EXPECT_EQ(
      base::StringPrintf("version=%s,client_id=%s,device_id=%s,"
                         "sync_account_id=%s,signin_mode=sync_account,"
                         "signout_mode=no_confirmation",
                         signin::kDiceProtocolVersion, client_id.c_str(),
                         GetDeviceId().c_str(), GetMainAccountID().c_str()),
      dice_request_header_);

  // Check that the token was requested and added to the token service.
  SendRefreshTokenResponse();
  EXPECT_EQ(GetMainAccountID(),
            GetSigninManager()->GetAuthenticatedAccountId());

  // Old token must not be revoked (see http://crbug.com/865189).
  EXPECT_EQ(0, token_revoked_notification_count_);

  EXPECT_EQ(1, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(1);
}

// Checks that the Dice signout flow is disabled.
IN_PROC_BROWSER_TEST_F(DiceFixAuthErrorsBrowserTest, Signout) {
  // Start from a signed-in state.
  SetupSignedInAccounts();

  // Signout from main account on the web.
  SignOutWithDice(kMainAccount);

  // Check that the user is still signed in Chrome.
  EXPECT_EQ(GetMainAccountID(),
            GetSigninManager()->GetAuthenticatedAccountId());
  EXPECT_TRUE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));
  EXPECT_TRUE(
      GetTokenService()->RefreshTokenIsAvailable(GetSecondaryAccountID()));
  EXPECT_EQ(0, token_revoked_notification_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(0, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(0);
}

// Tests that Sync is enabled if the ENABLE_SYNC response is received after the
// refresh token.
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, EnableSyncAfterToken) {
  EXPECT_EQ(0, reconcilor_started_count_);

  // Signin using the Chrome Sync endpoint.
  browser()->signin_view_controller()->ShowSignin(
      profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN, browser(),
      signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);

  // Receive token.
  EXPECT_FALSE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));
  SendRefreshTokenResponse();
  EXPECT_TRUE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));

  // Receive ENABLE_SYNC.
  SendEnableSyncResponse();

  // Check that the Dice request header was sent, with signout confirmation.
  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  EXPECT_EQ(base::StringPrintf("version=%s,client_id=%s,device_id=%s,"
                               "signin_mode=all_accounts,"
                               "signout_mode=show_confirmation",
                               signin::kDiceProtocolVersion, client_id.c_str(),
                               GetDeviceId().c_str()),
            dice_request_header_);

  ui_test_utils::UrlLoadObserver ntp_url_observer(
      GURL(chrome::kChromeSearchLocalNtpUrl),
      content::NotificationService::AllSources());

  WaitForSigninSucceeded();
  EXPECT_EQ(GetMainAccountID(),
            GetSigninManager()->GetAuthenticatedAccountId());

  EXPECT_EQ(1, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(1);
  EXPECT_EQ(1, reconcilor_started_count_);

  // Check that the tab was navigated to the NTP.
  ntp_url_observer.Wait();

  // Dismiss the Sync confirmation UI.
  EXPECT_TRUE(login_ui_test_utils::DismissSyncConfirmationDialog(
      browser(), base::TimeDelta::FromSeconds(30)));
}

// Tests that Sync is enabled if the ENABLE_SYNC response is received before the
// refresh token.
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, EnableSyncBeforeToken) {
  EXPECT_EQ(0, reconcilor_started_count_);

  ui_test_utils::UrlLoadObserver enable_sync_url_observer(
      https_server_.GetURL(kEnableSyncURL),
      content::NotificationService::AllSources());

  // Signin using the Chrome Sync endpoint.
  browser()->signin_view_controller()->ShowSignin(
      profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN, browser(),
      signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);

  // Receive ENABLE_SYNC.
  SendEnableSyncResponse();
  // Wait for the page to be fully loaded.
  enable_sync_url_observer.Wait();

  // Receive token.
  EXPECT_FALSE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));
  SendRefreshTokenResponse();
  EXPECT_TRUE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));

  // Check that the Dice request header was sent, with signout confirmation.
  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  EXPECT_EQ(base::StringPrintf("version=%s,client_id=%s,device_id=%s,"
                               "signin_mode=all_accounts,"
                               "signout_mode=show_confirmation",
                               signin::kDiceProtocolVersion, client_id.c_str(),
                               GetDeviceId().c_str()),
            dice_request_header_);

  ui_test_utils::UrlLoadObserver ntp_url_observer(
      GURL(chrome::kChromeSearchLocalNtpUrl),
      content::NotificationService::AllSources());

  WaitForSigninSucceeded();
  EXPECT_EQ(GetMainAccountID(),
            GetSigninManager()->GetAuthenticatedAccountId());

  EXPECT_EQ(1, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(1);
  EXPECT_EQ(1, reconcilor_started_count_);

  // Check that the tab was navigated to the NTP.
  ntp_url_observer.Wait();

  // Dismiss the Sync confirmation UI.
  EXPECT_TRUE(login_ui_test_utils::DismissSyncConfirmationDialog(
      browser(), base::TimeDelta::FromSeconds(30)));
}

// Tests that turning off Dice via preferences works.
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, PRE_TurnOffDice) {
  // Sign the profile in.
  SetupSignedInAccounts();
  syncer::SyncPrefs(browser()->profile()->GetPrefs()).SetFirstSetupComplete();

  EXPECT_TRUE(AccountConsistencyModeManager::IsDiceEnabledForProfile(
      browser()->profile()));

  EXPECT_FALSE(GetSigninManager()->GetAuthenticatedAccountId().empty());
  EXPECT_TRUE(GetSigninManager()->GetAccountIdForAuthInProgress().empty());
  EXPECT_TRUE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));
  EXPECT_FALSE(GetAccountTrackerService()->GetAccounts().empty());

  // Turn off Dice for this profile.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSigninAllowedOnNextStartup, false);
}

IN_PROC_BROWSER_TEST_F(DiceBrowserTest, TurnOffDice) {
  // Check that Dice is disabled.
  EXPECT_FALSE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kSigninAllowedOnNextStartup));
  EXPECT_FALSE(AccountConsistencyModeManager::IsDiceEnabledForProfile(
      browser()->profile()));

  EXPECT_TRUE(GetSigninManager()->GetAuthenticatedAccountId().empty());
  EXPECT_TRUE(GetSigninManager()->GetAccountIdForAuthInProgress().empty());
  EXPECT_FALSE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));
  EXPECT_TRUE(GetAccountTrackerService()->GetAccounts().empty());
}

// Checks that Dice is disabled in incognito mode.
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, Incognito) {
  Browser* incognito_browser = new Browser(Browser::CreateParams(
      browser()->profile()->GetOffTheRecordProfile(), true));

  // Check that Dice is disabled.
  EXPECT_FALSE(AccountConsistencyModeManager::IsDiceEnabledForProfile(
      incognito_browser->profile()));
}
