// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_GET_AUTH_TOKEN_FUNCTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_GET_AUTH_TOKEN_FUNCTION_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/identity/gaia_web_auth_flow.h"
#include "chrome/browser/extensions/api/identity/identity_mint_queue.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

namespace extensions {

// identity.getAuthToken fetches an OAuth 2 function for the
// caller. The request has three sub-flows: non-interactive,
// interactive, and sign-in.
//
// In the non-interactive flow, getAuthToken requests a token from
// GAIA. GAIA may respond with a token, an error, or "consent
// required". In the consent required cases, getAuthToken proceeds to
// the second, interactive phase.
//
// The interactive flow presents a scope approval dialog to the
// user. If the user approves the request, a grant will be recorded on
// the server, and an access token will be returned to the caller.
//
// In some cases we need to display a sign-in dialog. Normally the
// profile will be signed in already, but if it turns out we need a
// new login token, there is a sign-in flow. If that flow completes
// successfully, getAuthToken proceeds to the non-interactive flow.
class IdentityGetAuthTokenFunction : public ChromeAsyncExtensionFunction,
                                     public GaiaWebAuthFlow::Delegate,
                                     public IdentityMintRequestQueue::Request,
                                     public signin::IdentityManager::Observer,
#if defined(OS_CHROMEOS)
                                     public OAuth2AccessTokenManager::Consumer,
#endif
                                     public OAuth2MintTokenFlow::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.getAuthToken",
                             EXPERIMENTAL_IDENTITY_GETAUTHTOKEN)

  IdentityGetAuthTokenFunction();

  const ExtensionTokenKey* GetExtensionTokenKeyForTest() { return &token_key_; }

  void OnIdentityAPIShutdown();

 protected:
  ~IdentityGetAuthTokenFunction() override;

  void SigninFailed();

  // GaiaWebAuthFlow::Delegate implementation:
  void OnGaiaFlowFailure(GaiaWebAuthFlow::Failure failure,
                         GoogleServiceAuthError service_error,
                         const std::string& oauth_error) override;
  void OnGaiaFlowCompleted(const std::string& access_token,
                           const std::string& expiration) override;

  // Starts a login access token request.
  virtual void StartTokenKeyAccountAccessTokenRequest();

// TODO(blundell): Investigate feasibility of moving the ChromeOS use case
// to use the Identity Service instead of being an
// OAuth2AccessTokenManager::Consumer.
#if defined(OS_CHROMEOS)
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override;
#endif

  void OnAccessTokenFetchCompleted(GoogleServiceAuthError error,
                                   signin::AccessTokenInfo access_token_info);

  // Invoked on completion of the access token fetcher.
  // Exposed for testing.
  void OnGetAccessTokenComplete(const base::Optional<std::string>& access_token,
                                base::Time expiration_time,
                                const GoogleServiceAuthError& error);

  // Starts a mint token request to GAIA.
  // Exposed for testing.
  virtual void StartGaiaRequest(const std::string& login_access_token);

  // Caller owns the returned instance.
  // Exposed for testing.
  virtual std::unique_ptr<OAuth2MintTokenFlow> CreateMintTokenFlow();

  // Pending request for an access token from the device account (via
  // DeviceOAuth2TokenService).
  std::unique_ptr<OAuth2AccessTokenManager::Request>
      device_access_token_request_;

  // Pending fetcher for an access token for |token_key_.account_id| (via
  // IdentityManager).
  std::unique_ptr<signin::AccessTokenFetcher>
      token_key_account_access_token_fetcher_;

 private:
  FRIEND_TEST_ALL_PREFIXES(GetAuthTokenFunctionTest,
                           ComponentWithChromeClientId);
  FRIEND_TEST_ALL_PREFIXES(GetAuthTokenFunctionTest,
                           ComponentWithNormalClientId);
  FRIEND_TEST_ALL_PREFIXES(GetAuthTokenFunctionTest, InteractiveQueueShutdown);
  FRIEND_TEST_ALL_PREFIXES(GetAuthTokenFunctionTest, NoninteractiveShutdown);

  // Request the primary account info.
  // |extension_gaia_id|: The GAIA ID that was set in the parameters for this
  // instance, or empty if this was not in the parameters.
  void GetAuthTokenForPrimaryAccount(const std::string& extension_gaia_id);

  // Wrapper to FindExtendedAccountInfoForAccountWithRefreshTokenByGaiaId() to
  // avoid a synchronous call to IdentityManager in RunAsync().
  void FetchExtensionAccountInfo(const std::string& gaia_id);

  // Called when the AccountInfo that this instance should use is available.
  void OnReceivedExtensionAccountInfo(const CoreAccountInfo* account_info);

  // signin::IdentityManager::Observer implementation:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnPrimaryAccountSet(
      const CoreAccountInfo& primary_account_info) override;

  // ExtensionFunction:
  bool RunAsync() override;

  // Helpers to report async function results to the caller.
  void StartAsyncRun();
  void CompleteAsyncRun(bool success);
  void CompleteFunctionWithResult(const std::string& access_token);
  void CompleteFunctionWithError(const std::string& error);

  // Whether a signin flow should be initiated in the user's current state.
  bool ShouldStartSigninFlow();

  // Initiate/complete the sub-flows.
  void StartSigninFlow();
  void StartMintTokenFlow(IdentityMintRequestQueue::MintType type);
  void CompleteMintTokenFlow();

  // IdentityMintRequestQueue::Request implementation:
  void StartMintToken(IdentityMintRequestQueue::MintType type) override;

  // OAuth2MintTokenFlow::Delegate implementation:
  void OnMintTokenSuccess(const std::string& access_token,
                          int time_to_live) override;
  void OnMintTokenFailure(const GoogleServiceAuthError& error) override;
  void OnIssueAdviceSuccess(const IssueAdviceInfo& issue_advice) override;

#if defined(OS_CHROMEOS)
  // Starts a login access token request for device robot account. This method
  // will be called only in Chrome OS for:
  // 1. Enterprise kiosk mode.
  // 2. Whitelisted first party apps in public session.
  virtual void StartDeviceAccessTokenRequest();

  bool IsOriginWhitelistedInPublicSession();
#endif

  // Methods for invoking UI. Overridable for testing.
  virtual void ShowExtensionLoginPrompt();
  virtual void ShowOAuthApprovalDialog(const IssueAdviceInfo& issue_advice);

  // Checks if there is a master login token to mint tokens for the extension.
  bool HasRefreshTokenForTokenKeyAccount() const;

  // Maps OAuth2 protocol errors to an error message returned to the
  // developer in chrome.runtime.lastError.
  std::string MapOAuth2ErrorToDescription(const std::string& error);

  std::string GetOAuth2ClientId() const;

  // Returns true if extensions are restricted to the primary account.
  bool IsPrimaryAccountOnly() const;

  bool interactive_;
  bool should_prompt_for_scopes_;
  IdentityMintRequestQueue::MintType mint_token_flow_type_;
  std::unique_ptr<OAuth2MintTokenFlow> mint_token_flow_;
  OAuth2MintTokenFlow::Mode gaia_mint_token_mode_;
  bool should_prompt_for_signin_;

  // Shown in the extension login prompt.
  std::string email_for_default_web_account_;

  ExtensionTokenKey token_key_;
  std::string oauth2_client_id_;
  // When launched in interactive mode, and if there is no existing grant,
  // a permissions prompt will be popped up to the user.
  IssueAdviceInfo issue_advice_;
  std::unique_ptr<GaiaWebAuthFlow> gaia_web_auth_flow_;

  // Invoked when IdentityAPI is shut down.
  std::unique_ptr<base::CallbackList<void()>::Subscription>
      identity_api_shutdown_subscription_;

  ScopedObserver<signin::IdentityManager, signin::IdentityManager::Observer>
      scoped_identity_manager_observer_;

  // This class can be listening to account changes, but only for one type of
  // events at a time.
  enum class AccountListeningMode {
    kNotListening,            // Not listening account changes
    kListeningCookies,        // Listening cookie changes
    kListeningTokens,         // Listening token changes
    kListeningPrimaryAccount  // Listening primary account changes
  };
  AccountListeningMode account_listening_mode_ =
      AccountListeningMode::kNotListening;

  base::WeakPtrFactory<IdentityGetAuthTokenFunction> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_GET_AUTH_TOKEN_FUNCTION_H_
