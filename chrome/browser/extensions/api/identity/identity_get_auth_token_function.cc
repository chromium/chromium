// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_get_auth_token_function.h"

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/extensions/api/identity.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension_l10n_util.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/identity/public/cpp/scope_set.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_constants.h"
#endif

namespace extensions {

namespace {

#if defined(OS_CHROMEOS)
// The list of apps that are allowed to use the Identity API to retrieve the
// token from the device robot account in a public session.
const char* const kPublicSessionAllowedOrigins[] = {
    // Chrome Remote Desktop - Chromium branding.
    "chrome-extension://ljacajndfccfgnfohlgkdphmbnpkjflk/",
    // Chrome Remote Desktop - Official branding.
    "chrome-extension://gbchcmhmhahfdphkhkmpfmihenigjmpp/"};
#endif

const char* const kExtensionsIdentityAPIOAuthConsumerName =
    "extensions_identity_api";

bool IsBrowserSigninAllowed(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed);
}

}  // namespace

IdentityGetAuthTokenFunction::IdentityGetAuthTokenFunction()
    :
#if defined(OS_CHROMEOS)
      OAuth2AccessTokenManager::Consumer(
          kExtensionsIdentityAPIOAuthConsumerName),
#endif
      interactive_(false),
      should_prompt_for_scopes_(false),
      should_prompt_for_signin_(false),
      token_key_(/*extension_id=*/std::string(),
                 /*account_id=*/CoreAccountId(),
                 /*scopes=*/std::set<std::string>()),
      scoped_identity_manager_observer_(this) {
}

IdentityGetAuthTokenFunction::~IdentityGetAuthTokenFunction() {
  TRACE_EVENT_ASYNC_END0("identity", "IdentityGetAuthTokenFunction", this);
}

bool IdentityGetAuthTokenFunction::RunAsync() {
  TRACE_EVENT_ASYNC_BEGIN1("identity",
                           "IdentityGetAuthTokenFunction",
                           this,
                           "extension",
                           extension()->id());

  if (GetProfile()->IsOffTheRecord()) {
    error_ = identity_constants::kOffTheRecord;
    return false;
  }

  std::unique_ptr<api::identity::GetAuthToken::Params> params(
      api::identity::GetAuthToken::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  interactive_ = params->details.get() &&
      params->details->interactive.get() &&
      *params->details->interactive;

  should_prompt_for_scopes_ = interactive_;
  should_prompt_for_signin_ =
      interactive_ && IsBrowserSigninAllowed(GetProfile());

  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(extension());

  // Check that the necessary information is present in the manifest.
  oauth2_client_id_ = GetOAuth2ClientId();
  if (oauth2_client_id_.empty()) {
    error_ = identity_constants::kInvalidClientId;
    return false;
  }

  std::set<std::string> scopes(oauth2_info.scopes.begin(),
                               oauth2_info.scopes.end());
  std::string gaia_id;

  if (params->details.get()) {
    if (params->details->account.get())
      gaia_id = params->details->account->id;

    if (params->details->scopes.get()) {
      scopes = std::set<std::string>(params->details->scopes->begin(),
                                     params->details->scopes->end());
    }
  }

  if (scopes.empty()) {
    error_ = identity_constants::kInvalidScopes;
    return false;
  }

  token_key_.scopes = scopes;
  token_key_.extension_id = extension()->id();

  // From here on out, results must be returned asynchronously.
  StartAsyncRun();

  if (gaia_id.empty() || IsPrimaryAccountOnly()) {
    // Try the primary account.
    // TODO(https://crbug.com/932400): collapse the asynchronicity
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            &IdentityGetAuthTokenFunction::GetAuthTokenForPrimaryAccount,
            weak_ptr_factory_.GetWeakPtr(), gaia_id));
  } else {
    // Get the AccountInfo for the account that the extension wishes to use.
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&IdentityGetAuthTokenFunction::FetchExtensionAccountInfo,
                       weak_ptr_factory_.GetWeakPtr(), gaia_id));
  }

  return true;
}

void IdentityGetAuthTokenFunction::GetAuthTokenForPrimaryAccount(
    const std::string& extension_gaia_id) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(GetProfile());
  CoreAccountInfo primary_account_info =
      identity_manager->GetPrimaryAccountInfo();
  bool primary_account_only = IsPrimaryAccountOnly();

  // Detect and handle the case where the extension is using an account other
  // than the primary account.
  if (primary_account_only && !extension_gaia_id.empty() &&
      extension_gaia_id != primary_account_info.gaia) {
    // TODO(courage): should this be a different error?
    CompleteFunctionWithError(identity_constants::kUserNotSignedIn);
    return;
  }

  if (primary_account_only || !primary_account_info.gaia.empty()) {
    // The extension is using the primary account.
    OnReceivedExtensionAccountInfo(&primary_account_info);
  } else {
    // No primary account, try the first account in cookies.
    DCHECK_EQ(AccountListeningMode::kNotListening, account_listening_mode_);
    account_listening_mode_ = AccountListeningMode::kListeningCookies;
    signin::AccountsInCookieJarInfo accounts_in_cookies =
        identity_manager->GetAccountsInCookieJar();
    if (accounts_in_cookies.accounts_are_fresh) {
      OnAccountsInCookieUpdated(accounts_in_cookies,
                                GoogleServiceAuthError::AuthErrorNone());
    } else {
      scoped_identity_manager_observer_.Add(identity_manager);
    }
  }
}

void IdentityGetAuthTokenFunction::FetchExtensionAccountInfo(
    const std::string& gaia_id) {
  OnReceivedExtensionAccountInfo(base::OptionalOrNullptr(
      IdentityManagerFactory::GetForProfile(GetProfile())
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByGaiaId(
              gaia_id)));
}

void IdentityGetAuthTokenFunction::OnReceivedExtensionAccountInfo(
    const CoreAccountInfo* account_info) {
  token_key_.account_id =
      account_info ? account_info->account_id : CoreAccountId();

#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  bool is_kiosk = user_manager::UserManager::Get()->IsLoggedInAsKioskApp();
  bool is_public_session =
      user_manager::UserManager::Get()->IsLoggedInAsPublicAccount();

  if (connector->IsEnterpriseManaged() && (is_kiosk || is_public_session)) {
    if (is_public_session && !IsOriginWhitelistedInPublicSession()) {
      CompleteFunctionWithError(identity_constants::kUserNotSignedIn);
      return;
    }

    StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE);
    return;
  }
#endif

  if (!account_info ||
      !IdentityManagerFactory::GetForProfile(GetProfile())
           ->HasAccountWithRefreshToken(account_info->account_id)) {
    if (!ShouldStartSigninFlow()) {
      CompleteFunctionWithError(
          IsBrowserSigninAllowed(GetProfile())
              ? identity_constants::kUserNotSignedIn
              : identity_constants::kBrowserSigninNotAllowed);
      return;
    }
    // Display a login prompt.
    StartSigninFlow();
  } else {
    StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE);
  }
}

void IdentityGetAuthTokenFunction::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  if (account_listening_mode_ != AccountListeningMode::kListeningCookies)
    return;

  // Stop listening cookies.
  account_listening_mode_ = AccountListeningMode::kNotListening;
  scoped_identity_manager_observer_.RemoveAll();

  const std::vector<gaia::ListedAccount>& accounts =
      accounts_in_cookie_jar_info.signed_in_accounts;

  if (!accounts.empty()) {
    const gaia::ListedAccount& account = accounts[0];
    // If the account is in auth error, it won't be in the identity manager.
    // Save the email now to use as email hint for the login prompt.
    email_for_default_web_account_ = account.email;
    OnReceivedExtensionAccountInfo(base::OptionalOrNullptr(
        IdentityManagerFactory::GetForProfile(GetProfile())
            ->FindExtendedAccountInfoForAccountWithRefreshTokenByGaiaId(
                account.gaia_id)));
  } else {
    OnReceivedExtensionAccountInfo(nullptr);
  }
}

void IdentityGetAuthTokenFunction::StartAsyncRun() {
  // Balanced in CompleteAsyncRun
  AddRef();

  identity_api_shutdown_subscription_ =
      extensions::IdentityAPI::GetFactoryInstance()
          ->Get(GetProfile())
          ->RegisterOnShutdownCallback(base::Bind(
              &IdentityGetAuthTokenFunction::OnIdentityAPIShutdown, this));
}

void IdentityGetAuthTokenFunction::CompleteAsyncRun(bool success) {
  identity_api_shutdown_subscription_.reset();

  SendResponse(success);
  Release();  // Balanced in StartAsyncRun
}

void IdentityGetAuthTokenFunction::CompleteFunctionWithResult(
    const std::string& access_token) {
  SetResult(std::make_unique<base::Value>(access_token));
  CompleteAsyncRun(true);
}

void IdentityGetAuthTokenFunction::CompleteFunctionWithError(
    const std::string& error) {
  TRACE_EVENT_ASYNC_STEP_PAST1("identity",
                               "IdentityGetAuthTokenFunction",
                               this,
                               "CompleteFunctionWithError",
                               "error",
                               error);
  error_ = error;
  CompleteAsyncRun(false);
}

bool IdentityGetAuthTokenFunction::ShouldStartSigninFlow() {
  if (!should_prompt_for_signin_)
    return false;

  auto* identity_manager = IdentityManagerFactory::GetForProfile(GetProfile());
  bool account_needs_reauth =
      !identity_manager->HasAccountWithRefreshToken(token_key_.account_id) ||
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          token_key_.account_id);
  return account_needs_reauth;
}

void IdentityGetAuthTokenFunction::StartSigninFlow() {
  DCHECK(ShouldStartSigninFlow());

  // All cached tokens are invalid because the user is not signed in.
  IdentityAPI* id_api =
      extensions::IdentityAPI::GetFactoryInstance()->Get(GetProfile());
  id_api->EraseAllCachedTokens();

  // If the signin flow fails, don't display the login prompt again.
  should_prompt_for_signin_ = false;

#if defined(OS_CHROMEOS)
  // In normal mode (i.e. non-kiosk mode), the user has to log out to
  // re-establish credentials. Let the global error popup handle everything.
  // In kiosk mode, interactive sign-in is not supported.
  SigninFailed();
  return;
#endif

  if (g_browser_process->IsShuttingDown()) {
    // The login prompt cannot be displayed when the browser process is shutting
    // down.
    SigninFailed();
    return;
  }

  DCHECK_EQ(AccountListeningMode::kNotListening, account_listening_mode_);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(GetProfile());
  account_listening_mode_ = AccountListeningMode::kListeningTokens;
  if (IsPrimaryAccountOnly()) {
    if (!identity_manager->HasPrimaryAccount()) {
      account_listening_mode_ = AccountListeningMode::kListeningPrimaryAccount;
    } else {
      // Fixing an authentication error. Either there is no token, or it is in
      // error.
      DCHECK_EQ(token_key_.account_id, identity_manager->GetPrimaryAccountId());
      DCHECK(!identity_manager->HasAccountWithRefreshToken(
                 token_key_.account_id) ||
             identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
                 token_key_.account_id));
    }
  }
  scoped_identity_manager_observer_.Add(identity_manager);

  ShowExtensionLoginPrompt();
}

void IdentityGetAuthTokenFunction::StartMintTokenFlow(
    IdentityMintRequestQueue::MintType type) {
#if !defined(OS_CHROMEOS)
  // ChromeOS in kiosk mode may start the mint token flow without account.
  DCHECK(!token_key_.account_id.empty());
  DCHECK(IdentityManagerFactory::GetForProfile(GetProfile())
             ->HasAccountWithRefreshToken(token_key_.account_id));
#endif

  mint_token_flow_type_ = type;

  // Flows are serialized to prevent excessive traffic to GAIA, and
  // to consolidate UI pop-ups.
  IdentityAPI* id_api =
      extensions::IdentityAPI::GetFactoryInstance()->Get(GetProfile());

  if (!should_prompt_for_scopes_) {
    // Caller requested no interaction.

    if (type == IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE) {
      // GAIA told us to do a consent UI.
      CompleteFunctionWithError(identity_constants::kNoGrant);
      return;
    }
    if (!id_api->mint_queue()->empty(
            IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE, token_key_)) {
      // Another call is going through a consent UI.
      CompleteFunctionWithError(identity_constants::kNoGrant);
      return;
    }
  }
  id_api->mint_queue()->RequestStart(type, token_key_, this);
}

void IdentityGetAuthTokenFunction::CompleteMintTokenFlow() {
  IdentityMintRequestQueue::MintType type = mint_token_flow_type_;

  extensions::IdentityAPI::GetFactoryInstance()
      ->Get(GetProfile())
      ->mint_queue()
      ->RequestComplete(type, token_key_, this);
}

void IdentityGetAuthTokenFunction::StartMintToken(
    IdentityMintRequestQueue::MintType type) {
  TRACE_EVENT_ASYNC_STEP_PAST1("identity",
                               "IdentityGetAuthTokenFunction",
                               this,
                               "StartMintToken",
                               "type",
                               type);

  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(extension());
  IdentityAPI* id_api = IdentityAPI::GetFactoryInstance()->Get(GetProfile());
  IdentityTokenCacheValue cache_entry = id_api->GetCachedToken(token_key_);
  IdentityTokenCacheValue::CacheValueStatus cache_status =
      cache_entry.status();

  if (type == IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE) {
    switch (cache_status) {
      case IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND:
#if defined(OS_CHROMEOS)
        // Always force minting token for ChromeOS kiosk app and public session.
        if (user_manager::UserManager::Get()->IsLoggedInAsPublicAccount() &&
            !IsOriginWhitelistedInPublicSession()) {
          CompleteFunctionWithError(identity_constants::kUserNotSignedIn);
          return;
        }

        if (user_manager::UserManager::Get()->IsLoggedInAsKioskApp() ||
            user_manager::UserManager::Get()->IsLoggedInAsPublicAccount()) {
          gaia_mint_token_mode_ = OAuth2MintTokenFlow::MODE_MINT_TOKEN_FORCE;
          policy::BrowserPolicyConnectorChromeOS* connector =
              g_browser_process->platform_part()
                  ->browser_policy_connector_chromeos();
          if (connector->IsEnterpriseManaged()) {
            StartDeviceAccessTokenRequest();
          } else {
            StartTokenKeyAccountAccessTokenRequest();
          }
          return;
        }
#endif

        if (oauth2_info.auto_approve)
          // oauth2_info.auto_approve is protected by a whitelist in
          // _manifest_features.json hence only selected extensions take
          // advantage of forcefully minting the token.
          gaia_mint_token_mode_ = OAuth2MintTokenFlow::MODE_MINT_TOKEN_FORCE;
        else
          gaia_mint_token_mode_ = OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE;
        StartTokenKeyAccountAccessTokenRequest();
        break;

      case IdentityTokenCacheValue::CACHE_STATUS_TOKEN:
        CompleteMintTokenFlow();
        CompleteFunctionWithResult(cache_entry.token());
        break;

      case IdentityTokenCacheValue::CACHE_STATUS_ADVICE:
        CompleteMintTokenFlow();
        should_prompt_for_signin_ = false;
        issue_advice_ = cache_entry.issue_advice();
        StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE);
        break;
    }
  } else {
    DCHECK(type == IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE);

    if (cache_status == IdentityTokenCacheValue::CACHE_STATUS_TOKEN) {
      CompleteMintTokenFlow();
      CompleteFunctionWithResult(cache_entry.token());
    } else {
      ShowOAuthApprovalDialog(issue_advice_);
    }
  }
}

void IdentityGetAuthTokenFunction::OnMintTokenSuccess(
    const std::string& access_token, int time_to_live) {
  TRACE_EVENT_ASYNC_STEP_PAST0("identity",
                               "IdentityGetAuthTokenFunction",
                               this,
                               "OnMintTokenSuccess");

  IdentityTokenCacheValue token(access_token,
                                base::TimeDelta::FromSeconds(time_to_live));
  IdentityAPI::GetFactoryInstance()
      ->Get(GetProfile())
      ->SetCachedToken(token_key_, token);

  CompleteMintTokenFlow();
  CompleteFunctionWithResult(access_token);
}

void IdentityGetAuthTokenFunction::OnMintTokenFailure(
    const GoogleServiceAuthError& error) {
  TRACE_EVENT_ASYNC_STEP_PAST1("identity",
                               "IdentityGetAuthTokenFunction",
                               this,
                               "OnMintTokenFailure",
                               "error",
                               error.ToString());
  CompleteMintTokenFlow();
  switch (error.state()) {
    case GoogleServiceAuthError::SERVICE_ERROR:
      if (ShouldStartSigninFlow()) {
        StartSigninFlow();
        return;
      }
      break;
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
      // TODO(courage): flush ticket and retry once
      if (ShouldStartSigninFlow()) {
        StartSigninFlow();
        return;
      }
      break;
    default:
      // Return error to caller.
      break;
  }

  CompleteFunctionWithError(
      std::string(identity_constants::kAuthFailure) + error.ToString());
}

void IdentityGetAuthTokenFunction::OnIssueAdviceSuccess(
    const IssueAdviceInfo& issue_advice) {
  TRACE_EVENT_ASYNC_STEP_PAST0("identity",
                               "IdentityGetAuthTokenFunction",
                               this,
                               "OnIssueAdviceSuccess");

  IdentityAPI::GetFactoryInstance()
      ->Get(GetProfile())
      ->SetCachedToken(token_key_, IdentityTokenCacheValue(issue_advice));
  CompleteMintTokenFlow();

  should_prompt_for_signin_ = false;
  // Existing grant was revoked and we used NO_FORCE, so we got info back
  // instead. Start a consent UI if we can.
  issue_advice_ = issue_advice;
  StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE);
}

void IdentityGetAuthTokenFunction::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (account_listening_mode_ != AccountListeningMode::kListeningTokens)
    return;

  // No specific account id was requested, use the first one we find.
  if (token_key_.account_id.empty())
    token_key_.account_id = account_info.account_id;

  if (token_key_.account_id == account_info.account_id) {
    // Stop listening tokens.
    account_listening_mode_ = AccountListeningMode::kNotListening;
    scoped_identity_manager_observer_.RemoveAll();

    StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE);
  }
}

void IdentityGetAuthTokenFunction::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  if (account_listening_mode_ != AccountListeningMode::kListeningPrimaryAccount)
    return;

  TRACE_EVENT_ASYNC_STEP_PAST0("identity", "IdentityGetAuthTokenFunction", this,
                               "OnPrimaryAccountSet");

  DCHECK(token_key_.account_id.empty());
  token_key_.account_id = primary_account_info.account_id;

  // Stop listening primary account.
  DCHECK(IdentityManagerFactory::GetForProfile(GetProfile())
             ->HasAccountWithRefreshToken(primary_account_info.account_id));
  account_listening_mode_ = AccountListeningMode::kNotListening;
  scoped_identity_manager_observer_.RemoveAll();

  StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE);
}

void IdentityGetAuthTokenFunction::SigninFailed() {
  TRACE_EVENT_ASYNC_STEP_PAST0("identity",
                               "IdentityGetAuthTokenFunction",
                               this,
                               "SigninFailed");
  CompleteFunctionWithError(identity_constants::kUserNotSignedIn);
}

void IdentityGetAuthTokenFunction::OnGaiaFlowFailure(
    GaiaWebAuthFlow::Failure failure,
    GoogleServiceAuthError service_error,
    const std::string& oauth_error) {
  CompleteMintTokenFlow();
  std::string error;

  switch (failure) {
    case GaiaWebAuthFlow::WINDOW_CLOSED:
      error = identity_constants::kUserRejected;
      break;

    case GaiaWebAuthFlow::INVALID_REDIRECT:
      error = identity_constants::kInvalidRedirect;
      break;

    case GaiaWebAuthFlow::SERVICE_AUTH_ERROR:
      // If this is really an authentication error and not just a transient
      // network error, then we show signin UI if appropriate.
      if (service_error.state() != GoogleServiceAuthError::CONNECTION_FAILED &&
          service_error.state() !=
              GoogleServiceAuthError::SERVICE_UNAVAILABLE) {
        if (ShouldStartSigninFlow()) {
          StartSigninFlow();
          return;
        }
      }
      error = std::string(identity_constants::kAuthFailure) +
          service_error.ToString();
      break;

    case GaiaWebAuthFlow::OAUTH_ERROR:
      error = MapOAuth2ErrorToDescription(oauth_error);
      break;

    case GaiaWebAuthFlow::LOAD_FAILED:
      error = identity_constants::kPageLoadFailure;
      break;

    default:
      NOTREACHED() << "Unexpected error from gaia web auth flow: " << failure;
      error = identity_constants::kInvalidRedirect;
      break;
  }

  CompleteFunctionWithError(error);
}

void IdentityGetAuthTokenFunction::OnGaiaFlowCompleted(
    const std::string& access_token,
    const std::string& expiration) {
  TRACE_EVENT_ASYNC_STEP_PAST0("identity",
                               "IdentityGetAuthTokenFunction",
                               this,
                               "OnGaiaFlowCompleted");
  int time_to_live;
  if (!expiration.empty() && base::StringToInt(expiration, &time_to_live)) {
    IdentityTokenCacheValue token_value(
        access_token, base::TimeDelta::FromSeconds(time_to_live));
    IdentityAPI::GetFactoryInstance()
        ->Get(GetProfile())
        ->SetCachedToken(token_key_, token_value);
  }

  CompleteMintTokenFlow();
  CompleteFunctionWithResult(access_token);
}

void IdentityGetAuthTokenFunction::OnGetAccessTokenComplete(
    const base::Optional<std::string>& access_token,
    base::Time expiration_time,
    const GoogleServiceAuthError& error) {
  // By the time we get here we should no longer have an outstanding access
  // token request.
  DCHECK(!device_access_token_request_);
  DCHECK(!token_key_account_access_token_fetcher_);
  if (access_token) {
    TRACE_EVENT_ASYNC_STEP_PAST1("identity", "IdentityGetAuthTokenFunction",
                                 this, "OnGetAccessTokenComplete", "account",
                                 token_key_.account_id.id);

    StartGaiaRequest(access_token.value());
  } else {
    TRACE_EVENT_ASYNC_STEP_PAST1("identity", "IdentityGetAuthTokenFunction",
                                 this, "OnGetAccessTokenComplete", "error",
                                 error.ToString());

    OnGaiaFlowFailure(GaiaWebAuthFlow::SERVICE_AUTH_ERROR, error,
                      std::string());
  }
}

#if defined(OS_CHROMEOS)
void IdentityGetAuthTokenFunction::OnGetTokenSuccess(
    const OAuth2AccessTokenManager::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  device_access_token_request_.reset();
  OnGetAccessTokenComplete(token_response.access_token,
                           token_response.expiration_time,
                           GoogleServiceAuthError::AuthErrorNone());
}

void IdentityGetAuthTokenFunction::OnGetTokenFailure(
    const OAuth2AccessTokenManager::Request* request,
    const GoogleServiceAuthError& error) {
  device_access_token_request_.reset();
  OnGetAccessTokenComplete(base::nullopt, base::Time(), error);
}
#endif

void IdentityGetAuthTokenFunction::OnAccessTokenFetchCompleted(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  token_key_account_access_token_fetcher_.reset();
  if (error.state() == GoogleServiceAuthError::NONE) {
    OnGetAccessTokenComplete(access_token_info.token,
                             access_token_info.expiration_time,
                             GoogleServiceAuthError::AuthErrorNone());
  } else {
    OnGetAccessTokenComplete(base::nullopt, base::Time(), error);
  }
}

void IdentityGetAuthTokenFunction::OnIdentityAPIShutdown() {
  gaia_web_auth_flow_.reset();
  device_access_token_request_.reset();
  token_key_account_access_token_fetcher_.reset();
  scoped_identity_manager_observer_.RemoveAll();
  extensions::IdentityAPI::GetFactoryInstance()
      ->Get(GetProfile())
      ->mint_queue()
      ->RequestCancel(token_key_, this);

  CompleteFunctionWithError(identity_constants::kCanceled);
}

#if defined(OS_CHROMEOS)
void IdentityGetAuthTokenFunction::StartDeviceAccessTokenRequest() {
  chromeos::DeviceOAuth2TokenService* service =
      chromeos::DeviceOAuth2TokenServiceFactory::Get();
  // Since robot account refresh tokens are scoped down to [any-api] only,
  // request access token for [any-api] instead of login.
  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert(GaiaConstants::kAnyApiOAuth2Scope);
  device_access_token_request_ = service->StartAccessTokenRequest(
      service->GetRobotAccountId(), scopes, this);
}

bool IdentityGetAuthTokenFunction::IsOriginWhitelistedInPublicSession() {
  DCHECK(extension());
  GURL extension_url = extension()->url();
  for (size_t i = 0; i < base::size(kPublicSessionAllowedOrigins); i++) {
    URLPattern allowed_origin(URLPattern::SCHEME_ALL,
                              kPublicSessionAllowedOrigins[i]);
    if (allowed_origin.MatchesSecurityOrigin(extension_url)) {
      return true;
    }
  }
  return false;
}
#endif

void IdentityGetAuthTokenFunction::StartTokenKeyAccountAccessTokenRequest() {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(GetProfile());
#if defined(OS_CHROMEOS)
  if (chrome::IsRunningInForcedAppMode()) {
    std::string app_client_id;
    std::string app_client_secret;
    if (chromeos::UserSessionManager::GetInstance()->
            GetAppModeChromeClientOAuthInfo(&app_client_id,
                                            &app_client_secret)) {
      token_key_account_access_token_fetcher_ =
          identity_manager->CreateAccessTokenFetcherForClient(
              token_key_.account_id, app_client_id, app_client_secret,
              kExtensionsIdentityAPIOAuthConsumerName, ::identity::ScopeSet(),
              base::BindOnce(
                  &IdentityGetAuthTokenFunction::OnAccessTokenFetchCompleted,
                  base::Unretained(this)),
              signin::AccessTokenFetcher::Mode::kImmediate);
      return;
    }
  }
#endif

  token_key_account_access_token_fetcher_ =
      identity_manager->CreateAccessTokenFetcherForAccount(
          token_key_.account_id, kExtensionsIdentityAPIOAuthConsumerName,
          ::identity::ScopeSet(),
          base::BindOnce(
              &IdentityGetAuthTokenFunction::OnAccessTokenFetchCompleted,
              base::Unretained(this)),
          signin::AccessTokenFetcher::Mode::kImmediate);
}

void IdentityGetAuthTokenFunction::StartGaiaRequest(
    const std::string& login_access_token) {
  DCHECK(!login_access_token.empty());
  mint_token_flow_ = CreateMintTokenFlow();
  mint_token_flow_->Start(GetProfile()->GetURLLoaderFactory(),
                          login_access_token);
}

void IdentityGetAuthTokenFunction::ShowExtensionLoginPrompt() {
  base::Optional<AccountInfo> account =
      IdentityManagerFactory::GetForProfile(GetProfile())
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              token_key_.account_id);
  std::string email_hint =
      account ? account->email : email_for_default_web_account_;

  LoginUIService* login_ui_service =
      LoginUIServiceFactory::GetForProfile(GetProfile());
  login_ui_service->ShowExtensionLoginPrompt(IsPrimaryAccountOnly(),
                                             email_hint);
}

void IdentityGetAuthTokenFunction::ShowOAuthApprovalDialog(
    const IssueAdviceInfo& issue_advice) {
  const std::string locale = extension_l10n_util::CurrentLocaleOrDefault();

  gaia_web_auth_flow_.reset(new GaiaWebAuthFlow(this, GetProfile(), &token_key_,
                                                oauth2_client_id_, locale));
  gaia_web_auth_flow_->Start();
}

std::unique_ptr<OAuth2MintTokenFlow>
IdentityGetAuthTokenFunction::CreateMintTokenFlow() {
  std::string signin_scoped_device_id =
      GetSigninScopedDeviceIdForProfile(GetProfile());
  auto mint_token_flow = std::make_unique<OAuth2MintTokenFlow>(
      this, OAuth2MintTokenFlow::Parameters(
                extension()->id(), oauth2_client_id_,
                std::vector<std::string>(token_key_.scopes.begin(),
                                         token_key_.scopes.end()),
                signin_scoped_device_id, gaia_mint_token_mode_));
  return mint_token_flow;
}

bool IdentityGetAuthTokenFunction::HasRefreshTokenForTokenKeyAccount() const {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(GetProfile());
  return identity_manager->HasAccountWithRefreshToken(token_key_.account_id);
}

std::string IdentityGetAuthTokenFunction::MapOAuth2ErrorToDescription(
    const std::string& error) {
  const char kOAuth2ErrorAccessDenied[] = "access_denied";
  const char kOAuth2ErrorInvalidScope[] = "invalid_scope";

  if (error == kOAuth2ErrorAccessDenied)
    return std::string(identity_constants::kUserRejected);
  else if (error == kOAuth2ErrorInvalidScope)
    return std::string(identity_constants::kInvalidScopes);
  else
    return std::string(identity_constants::kAuthFailure) + error;
}

std::string IdentityGetAuthTokenFunction::GetOAuth2ClientId() const {
  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(extension());
  std::string client_id = oauth2_info.client_id;

  // Component apps using auto_approve may use Chrome's client ID by
  // omitting the field.
  if (client_id.empty() && extension()->location() == Manifest::COMPONENT &&
      oauth2_info.auto_approve) {
    client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  }
  return client_id;
}

bool IdentityGetAuthTokenFunction::IsPrimaryAccountOnly() const {
  return IdentityAPI::GetFactoryInstance()
      ->Get(GetProfile())
      ->AreExtensionsRestrictedToPrimaryAccount();
}

}  // namespace extensions
