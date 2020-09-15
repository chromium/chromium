// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_get_auth_token_function.h"

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/extensions/api/identity/identity_get_auth_token_error.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/extensions/api/identity.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_l10n_util.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
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

std::string GetOAuth2MintTokenFlowVersion() {
  return version_info::GetVersionNumber();
}

std::string GetOAuth2MintTokenFlowChannel() {
  return version_info::GetChannelString(chrome::GetChannel());
}

void RecordFunctionResult(const IdentityGetAuthTokenError& error,
                          bool remote_consent_approved) {
  base::UmaHistogramEnumeration("Signin.Extensions.GetAuthTokenResult",
                                error.state());
  if (remote_consent_approved) {
    base::UmaHistogramEnumeration(
        "Signin.Extensions.GetAuthTokenResult.RemoteConsentApproved",
        error.state());
  }
}

bool IsReturnScopesInGetAuthTokenEnabled() {
  return base::FeatureList::IsEnabled(
      extensions_features::kReturnScopesInGetAuthToken);
}

bool IsSelectedUserIdInGetAuthTokenEnabled() {
  return base::FeatureList::IsEnabled(
      extensions_features::kSelectedUserIdInGetAuthToken);
}

}  // namespace

IdentityGetAuthTokenFunction::IdentityGetAuthTokenFunction()
#if defined(OS_CHROMEOS)
    : OAuth2AccessTokenManager::Consumer(
          kExtensionsIdentityAPIOAuthConsumerName)
#endif
{
}

IdentityGetAuthTokenFunction::~IdentityGetAuthTokenFunction() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("identity", "IdentityGetAuthTokenFunction",
                                  this);
}

ExtensionFunction::ResponseAction IdentityGetAuthTokenFunction::Run() {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("identity", "IdentityGetAuthTokenFunction",
                                    this, "extension", extension()->id());

  if (GetProfile()->IsOffTheRecord()) {
    IdentityGetAuthTokenError error(
        IdentityGetAuthTokenError::State::kOffTheRecord);
    RecordFunctionResult(error, remote_consent_approved_);
    return RespondNow(Error(error.ToString()));
  }

  std::unique_ptr<api::identity::GetAuthToken::Params> params(
      api::identity::GetAuthToken::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  interactive_ = params->details.get() && params->details->interactive.get() &&
                 *params->details->interactive;

  should_prompt_for_scopes_ = interactive_;
  should_prompt_for_signin_ =
      interactive_ && IsBrowserSigninAllowed(GetProfile());

  enable_granular_permissions_ =
      IsReturnScopesInGetAuthTokenEnabled() && params->details.get() &&
      params->details->enable_granular_permissions.get() &&
      *params->details->enable_granular_permissions;

  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(extension());

  // Check that the necessary information is present in the manifest.
  oauth2_client_id_ = GetOAuth2ClientId();
  if (oauth2_client_id_.empty()) {
    IdentityGetAuthTokenError error(
        IdentityGetAuthTokenError::State::kInvalidClientId);
    RecordFunctionResult(error, remote_consent_approved_);
    return RespondNow(Error(error.ToString()));
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
    IdentityGetAuthTokenError error(
        IdentityGetAuthTokenError::State::kEmptyScopes);
    RecordFunctionResult(error, remote_consent_approved_);
    return RespondNow(Error(error.ToString()));
  }

  token_key_.scopes = scopes;
  token_key_.extension_id = extension()->id();

  if (gaia_id.empty() && !IsPrimaryAccountOnly()) {
    gaia_id = IdentityAPI::GetFactoryInstance()
                  ->Get(GetProfile())
                  ->GetGaiaIdForExtension(token_key_.extension_id)
                  .value_or("");
  }

  selected_gaia_id_ = gaia_id;
  // From here on out, results must be returned asynchronously.
  StartAsyncRun();

  if (gaia_id.empty() || IsPrimaryAccountOnly()) {
    // Try the primary account.
    // TODO(https://crbug.com/932400): collapse the asynchronicity
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &IdentityGetAuthTokenFunction::GetAuthTokenForPrimaryAccount,
            weak_ptr_factory_.GetWeakPtr(), gaia_id));
  } else {
    // Get the AccountInfo for the account that the extension wishes to use.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&IdentityGetAuthTokenFunction::FetchExtensionAccountInfo,
                       weak_ptr_factory_.GetWeakPtr(), gaia_id));
  }

  return RespondLater();
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
    CompleteFunctionWithError(IdentityGetAuthTokenError(
        IdentityGetAuthTokenError::State::kUserNonPrimary));
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
  token_key_.account_info = account_info ? *account_info : CoreAccountInfo();

#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  bool is_kiosk = user_manager::UserManager::Get()->IsLoggedInAsKioskApp();
  bool is_public_session =
      user_manager::UserManager::Get()->IsLoggedInAsPublicAccount();

  if (connector->IsEnterpriseManaged() && (is_kiosk || is_public_session)) {
    if (is_public_session && !IsOriginAllowlistedInPublicSession()) {
      CompleteFunctionWithError(IdentityGetAuthTokenError(
          IdentityGetAuthTokenError::State::kNotAllowlistedInPublicSession));
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
      IdentityGetAuthTokenError error(
          IsBrowserSigninAllowed(GetProfile())
              ? IdentityGetAuthTokenError::State::kUserNotSignedIn
              : IdentityGetAuthTokenError::State::kBrowserSigninNotAllowed);
      CompleteFunctionWithError(error);
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
          ->RegisterOnShutdownCallback(base::BindOnce(
              &IdentityGetAuthTokenFunction::OnIdentityAPIShutdown, this));
}

void IdentityGetAuthTokenFunction::CompleteAsyncRun(ResponseValue response) {
  identity_api_shutdown_subscription_.reset();

  Respond(std::move(response));
  Release();  // Balanced in StartAsyncRun
}

void IdentityGetAuthTokenFunction::CompleteFunctionWithResult(
    const std::string& access_token,
    const std::set<std::string>& granted_scopes) {
  RecordFunctionResult(IdentityGetAuthTokenError(), remote_consent_approved_);

  if (IsReturnScopesInGetAuthTokenEnabled()) {
    std::unique_ptr<base::Value> granted_scopes_value =
        std::make_unique<base::Value>(base::Value::Type::LIST);
    for (const auto& scope : granted_scopes)
      granted_scopes_value->Append(scope);

    CompleteAsyncRun(TwoArguments(std::make_unique<base::Value>(access_token),
                                  std::move(granted_scopes_value)));
  } else {
    CompleteAsyncRun(OneArgument(std::make_unique<base::Value>(access_token)));
  }
}

void IdentityGetAuthTokenFunction::CompleteFunctionWithError(
    const IdentityGetAuthTokenError& error) {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1("identity", "CompleteFunctionWithError",
                                      this, "error", error.ToString());
  RecordFunctionResult(error, remote_consent_approved_);
  CompleteAsyncRun(Error(error.ToString()));
}

bool IdentityGetAuthTokenFunction::ShouldStartSigninFlow() {
  if (!should_prompt_for_signin_)
    return false;

  auto* identity_manager = IdentityManagerFactory::GetForProfile(GetProfile());
  bool account_needs_reauth =
      !identity_manager->HasAccountWithRefreshToken(
          token_key_.account_info.account_id) ||
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          token_key_.account_info.account_id);
  return account_needs_reauth;
}

void IdentityGetAuthTokenFunction::StartSigninFlow() {
  DCHECK(ShouldStartSigninFlow());

  // All cached tokens are invalid because the user is not signed in.
  IdentityAPI* id_api =
      extensions::IdentityAPI::GetFactoryInstance()->Get(GetProfile());
  id_api->token_cache()->EraseAllTokens();

  // If the signin flow fails, don't display the login prompt again.
  should_prompt_for_signin_ = false;

#if defined(OS_CHROMEOS)
  // In normal mode (i.e. non-kiosk mode), the user has to log out to
  // re-establish credentials. Let the global error popup handle everything.
  // In kiosk mode, interactive sign-in is not supported.
  SigninFailed();
#else
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
      DCHECK_EQ(token_key_.account_info.account_id,
                identity_manager->GetPrimaryAccountId());
      DCHECK(!identity_manager->HasAccountWithRefreshToken(
                 token_key_.account_info.account_id) ||
             identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
                 token_key_.account_info.account_id));
    }
  }
  scoped_identity_manager_observer_.Add(identity_manager);

  ShowExtensionLoginPrompt();
#endif
}

void IdentityGetAuthTokenFunction::StartMintTokenFlow(
    IdentityMintRequestQueue::MintType type) {
#if !defined(OS_CHROMEOS)
  // ChromeOS in kiosk mode may start the mint token flow without account.
  DCHECK(!token_key_.account_info.IsEmpty());
  DCHECK(IdentityManagerFactory::GetForProfile(GetProfile())
             ->HasAccountWithRefreshToken(token_key_.account_info.account_id));
#endif
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("identity", "MintTokenFlow", this, "type",
                                    type);

  mint_token_flow_type_ = type;

  // Flows are serialized to prevent excessive traffic to GAIA, and
  // to consolidate UI pop-ups.
  IdentityAPI* id_api =
      extensions::IdentityAPI::GetFactoryInstance()->Get(GetProfile());

  if (!should_prompt_for_scopes_) {
    // Caller requested no interaction.

    if (type == IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE) {
      // GAIA told us to do a consent UI.
      CompleteFunctionWithError(IdentityGetAuthTokenError(
          IdentityGetAuthTokenError::State::kGaiaConsentInteractionRequired));
      return;
    }

    if (!id_api->mint_queue()->empty(
            IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE, token_key_)) {
      // Another call is going through a consent UI.
      CompleteFunctionWithError(
          IdentityGetAuthTokenError(IdentityGetAuthTokenError::State::
                                        kGaiaConsentInteractionAlreadyRunning));
      return;
    }
  }

  id_api->mint_queue()->RequestStart(type, token_key_, this);
}

void IdentityGetAuthTokenFunction::CompleteMintTokenFlow() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("identity", "MintTokenFlow", this);

  IdentityMintRequestQueue::MintType type = mint_token_flow_type_;

  extensions::IdentityAPI::GetFactoryInstance()
      ->Get(GetProfile())
      ->mint_queue()
      ->RequestComplete(type, token_key_, this);
}

void IdentityGetAuthTokenFunction::StartMintToken(
    IdentityMintRequestQueue::MintType type) {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1("identity", "StartMintToken", this,
                                      "type", type);

  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(extension());
  IdentityAPI* id_api = IdentityAPI::GetFactoryInstance()->Get(GetProfile());
  IdentityTokenCacheValue cache_entry =
      id_api->token_cache()->GetToken(token_key_);
  IdentityTokenCacheValue::CacheValueStatus cache_status = cache_entry.status();

  if (type == IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE) {
    switch (cache_status) {
      case IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND:
#if defined(OS_CHROMEOS)
        // Always force minting token for ChromeOS kiosk app and public session.
        if (user_manager::UserManager::Get()->IsLoggedInAsPublicAccount() &&
            !IsOriginAllowlistedInPublicSession()) {
          CompleteFunctionWithError(
              IdentityGetAuthTokenError(IdentityGetAuthTokenError::State::
                                            kNotAllowlistedInPublicSession));
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
          // oauth2_info.auto_approve is protected by an allowlist in
          // _manifest_features.json hence only selected extensions take
          // advantage of forcefully minting the token.
          gaia_mint_token_mode_ = OAuth2MintTokenFlow::MODE_MINT_TOKEN_FORCE;
        else
          gaia_mint_token_mode_ = OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE;
        StartTokenKeyAccountAccessTokenRequest();
        break;

      case IdentityTokenCacheValue::CACHE_STATUS_TOKEN:
        CompleteMintTokenFlow();
        CompleteFunctionWithResult(cache_entry.token(),
                                   cache_entry.granted_scopes());
        break;

      case IdentityTokenCacheValue::CACHE_STATUS_ADVICE:
        CompleteMintTokenFlow();
        should_prompt_for_signin_ = false;
        issue_advice_ = cache_entry.issue_advice();
        StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE);
        break;

      case IdentityTokenCacheValue::CACHE_STATUS_REMOTE_CONSENT:
        CompleteMintTokenFlow();
        should_prompt_for_signin_ = false;
        resolution_data_ = cache_entry.resolution_data();
        StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE);
        break;

      case IdentityTokenCacheValue::CACHE_STATUS_REMOTE_CONSENT_APPROVED:
        consent_result_ = cache_entry.consent_result();
        should_prompt_for_signin_ = false;
        gaia_mint_token_mode_ = OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE;
        StartTokenKeyAccountAccessTokenRequest();
        break;
    }
  } else {
    DCHECK(type == IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE);

    switch (cache_status) {
      case IdentityTokenCacheValue::CACHE_STATUS_TOKEN:
        CompleteMintTokenFlow();
        CompleteFunctionWithResult(cache_entry.token(),
                                   cache_entry.granted_scopes());
        break;
      case IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND:
      case IdentityTokenCacheValue::CACHE_STATUS_ADVICE:
        ShowOAuthApprovalDialog(issue_advice_);
        break;
      case IdentityTokenCacheValue::CACHE_STATUS_REMOTE_CONSENT:
        ShowRemoteConsentDialog(resolution_data_);
        break;
      case IdentityTokenCacheValue::CACHE_STATUS_REMOTE_CONSENT_APPROVED:
        consent_result_ = cache_entry.consent_result();
        should_prompt_for_signin_ = false;
        gaia_mint_token_mode_ = OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE;
        StartTokenKeyAccountAccessTokenRequest();
        break;
    }
  }
}

void IdentityGetAuthTokenFunction::OnMintTokenSuccess(
    const std::string& access_token,
    const std::set<std::string>& granted_scopes,
    int time_to_live) {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("identity", "OnMintTokenSuccess", this);

  IdentityTokenCacheValue token = IdentityTokenCacheValue::CreateToken(
      access_token, granted_scopes, base::TimeDelta::FromSeconds(time_to_live));
  IdentityAPI::GetFactoryInstance()
      ->Get(GetProfile())
      ->token_cache()
      ->SetToken(token_key_, token);

  CompleteMintTokenFlow();
  CompleteFunctionWithResult(access_token, granted_scopes);
}

void IdentityGetAuthTokenFunction::OnMintTokenFailure(
    const GoogleServiceAuthError& error) {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1("identity", "OnMintTokenFailure", this,
                                      "error", error.ToString());
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
      IdentityGetAuthTokenError::FromMintTokenAuthError(error.ToString()));
}

void IdentityGetAuthTokenFunction::OnIssueAdviceSuccess(
    const IssueAdviceInfo& issue_advice) {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("identity", "OnIssueAdviceSuccess", this);

  IdentityAPI* identity_api =
      IdentityAPI::GetFactoryInstance()->Get(GetProfile());
  identity_api->token_cache()->SetToken(
      token_key_, IdentityTokenCacheValue::CreateIssueAdvice(issue_advice));
  // IssueAdvice doesn't communicate back to Chrome which account has been
  // chosen by the user. Cached gaia id may contain incorrect information so
  // it's better to remove it.
  identity_api->EraseGaiaIdForExtension(token_key_.extension_id);
  CompleteMintTokenFlow();

  should_prompt_for_signin_ = false;
  // Existing grant was revoked and we used NO_FORCE, so we got info back
  // instead. Start a consent UI if we can.
  issue_advice_ = issue_advice;
  StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE);
}

void IdentityGetAuthTokenFunction::OnRemoteConsentSuccess(
    const RemoteConsentResolutionData& resolution_data) {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("identity", "OnRemoteConsentSuccess",
                                      this);

  IdentityAPI::GetFactoryInstance()
      ->Get(GetProfile())
      ->token_cache()
      ->SetToken(token_key_,
                 IdentityTokenCacheValue::CreateRemoteConsent(resolution_data));
  should_prompt_for_signin_ = false;
  resolution_data_ = resolution_data;
  CompleteMintTokenFlow();
  StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE);
}

void IdentityGetAuthTokenFunction::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (account_listening_mode_ != AccountListeningMode::kListeningTokens)
    return;

  // No specific account id was requested, use the first one we find.
  if (token_key_.account_info.IsEmpty())
    token_key_.account_info = account_info;

  if (token_key_.account_info == account_info) {
    // Stop listening tokens.
    account_listening_mode_ = AccountListeningMode::kNotListening;
    scoped_identity_manager_observer_.RemoveAll();

    StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE);
  }
}

bool IdentityGetAuthTokenFunction::TryRecoverFromServiceAuthError(
    const GoogleServiceAuthError& error) {
  // If this is really an authentication error and not just a transient
  // network error, then we show signin UI if appropriate.
  if (error.state() != GoogleServiceAuthError::CONNECTION_FAILED &&
      error.state() != GoogleServiceAuthError::SERVICE_UNAVAILABLE) {
    if (ShouldStartSigninFlow()) {
      StartSigninFlow();
      return true;
    }
  }
  return false;
}

void IdentityGetAuthTokenFunction::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  if (account_listening_mode_ != AccountListeningMode::kListeningPrimaryAccount)
    return;

  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("identity", "OnPrimaryAccountSet", this);

  DCHECK(token_key_.account_info.IsEmpty());
  token_key_.account_info = primary_account_info;

  // Stop listening primary account.
  DCHECK(IdentityManagerFactory::GetForProfile(GetProfile())
             ->HasAccountWithRefreshToken(primary_account_info.account_id));
  account_listening_mode_ = AccountListeningMode::kNotListening;
  scoped_identity_manager_observer_.RemoveAll();

  StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE);
}

void IdentityGetAuthTokenFunction::SigninFailed() {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("identity", "SigninFailed", this);
  CompleteFunctionWithError(IdentityGetAuthTokenError(
      IdentityGetAuthTokenError::State::kSignInFailed));
}

void IdentityGetAuthTokenFunction::OnGaiaFlowFailure(
    GaiaWebAuthFlow::Failure failure,
    GoogleServiceAuthError service_error,
    const std::string& oauth_error) {
  CompleteMintTokenFlow();
  IdentityGetAuthTokenError error;

  switch (failure) {
    case GaiaWebAuthFlow::WINDOW_CLOSED:
      error = IdentityGetAuthTokenError(
          IdentityGetAuthTokenError::State::kGaiaFlowRejected);
      break;

    case GaiaWebAuthFlow::INVALID_REDIRECT:
      error = IdentityGetAuthTokenError(
          IdentityGetAuthTokenError::State::kInvalidRedirect);
      break;

    case GaiaWebAuthFlow::SERVICE_AUTH_ERROR:
      if (TryRecoverFromServiceAuthError(service_error)) {
        return;
      }
      error = IdentityGetAuthTokenError::FromGaiaFlowAuthError(
          service_error.ToString());
      break;

    case GaiaWebAuthFlow::OAUTH_ERROR:
      error = IdentityGetAuthTokenError::FromOAuth2Error(oauth_error);
      break;

    case GaiaWebAuthFlow::LOAD_FAILED:
      error = IdentityGetAuthTokenError(
          IdentityGetAuthTokenError::State::kPageLoadFailure);
      break;

    default:
      NOTREACHED() << "Unexpected error from gaia web auth flow: " << failure;
      error = IdentityGetAuthTokenError(
          IdentityGetAuthTokenError::State::kInvalidRedirect);
      break;
  }

  CompleteFunctionWithError(error);
}

void IdentityGetAuthTokenFunction::OnGaiaFlowCompleted(
    const std::string& access_token,
    const std::string& expiration) {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("identity", "OnGaiaFlowCompleted", this);
  int time_to_live;
  if (!expiration.empty() && base::StringToInt(expiration, &time_to_live)) {
    IdentityTokenCacheValue token_value = IdentityTokenCacheValue::CreateToken(
        access_token, token_key_.scopes,
        base::TimeDelta::FromSeconds(time_to_live));
    IdentityAPI::GetFactoryInstance()
        ->Get(GetProfile())
        ->token_cache()
        ->SetToken(token_key_, token_value);
  }

  CompleteMintTokenFlow();
  CompleteFunctionWithResult(access_token, token_key_.scopes);
}

void IdentityGetAuthTokenFunction::OnGaiaRemoteConsentFlowFailed(
    GaiaRemoteConsentFlow::Failure failure) {
  CompleteMintTokenFlow();
  IdentityGetAuthTokenError error;

  switch (failure) {
    case GaiaRemoteConsentFlow::WINDOW_CLOSED:
      error = IdentityGetAuthTokenError(
          IdentityGetAuthTokenError::State::kRemoteConsentFlowRejected);
      break;

    case GaiaRemoteConsentFlow::SET_ACCOUNTS_IN_COOKIE_FAILED:
      error = IdentityGetAuthTokenError(
          IdentityGetAuthTokenError::State::kSetAccountsInCookieFailure);
      break;

    case GaiaRemoteConsentFlow::LOAD_FAILED:
      error = IdentityGetAuthTokenError(
          IdentityGetAuthTokenError::State::kRemoteConsentPageLoadFailure);
      break;

    case GaiaRemoteConsentFlow::INVALID_CONSENT_RESULT:
      error = IdentityGetAuthTokenError(
          IdentityGetAuthTokenError::State::kInvalidConsentResult);
      break;

    case GaiaRemoteConsentFlow::NO_GRANT:
      error =
          IdentityGetAuthTokenError(IdentityGetAuthTokenError::State::kNoGrant);
      break;

    case GaiaRemoteConsentFlow::NONE:
      NOTREACHED();
      break;
  }

  CompleteFunctionWithError(error);
}

void IdentityGetAuthTokenFunction::OnGaiaRemoteConsentFlowApproved(
    const std::string& consent_result,
    const std::string& gaia_id) {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1(
      "identity", "OnGaiaRemoteConsentFlowApproved", this, "gaia_id", gaia_id);
  DCHECK(!consent_result.empty());
  remote_consent_approved_ = true;

  base::Optional<AccountInfo> account =
      IdentityManagerFactory::GetForProfile(GetProfile())
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByGaiaId(gaia_id);
  if (!account) {
    CompleteMintTokenFlow();
    CompleteFunctionWithError(IdentityGetAuthTokenError(
        IdentityGetAuthTokenError::State::kRemoteConsentUserNotSignedIn));
    return;
  }

  if (IsPrimaryAccountOnly()) {
    CoreAccountId primary_account_id =
        IdentityManagerFactory::GetForProfile(GetProfile())
            ->GetPrimaryAccountId();
    if (primary_account_id != account->account_id) {
      CompleteMintTokenFlow();
      CompleteFunctionWithError(IdentityGetAuthTokenError(
          IdentityGetAuthTokenError::State::kRemoteConsentUserNonPrimary));
      return;
    }
  }

  IdentityAPI* id_api = IdentityAPI::GetFactoryInstance()->Get(GetProfile());
  id_api->SetGaiaIdForExtension(token_key_.extension_id, gaia_id);

  // It's important to update the cache before calling CompleteMintTokenFlow()
  // as this call may start a new request synchronously and query the cache.
  ExtensionTokenKey new_token_key(token_key_);
  new_token_key.account_info = account.value();
  id_api->token_cache()->SetToken(
      new_token_key,
      IdentityTokenCacheValue::CreateRemoteConsentApproved(consent_result));
  CompleteMintTokenFlow();
  token_key_ = new_token_key;
  consent_result_ = consent_result;
  should_prompt_for_signin_ = false;
  StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE);
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
    TRACE_EVENT_NESTABLE_ASYNC_END1(
        "identity", "GetAccessToken", this, "account",
        token_key_.account_info.account_id.ToString());

    StartGaiaRequest(access_token.value());
  } else {
    TRACE_EVENT_NESTABLE_ASYNC_END1("identity", "GetAccessToken", this, "error",
                                    error.ToString());

    CompleteMintTokenFlow();
    if (TryRecoverFromServiceAuthError(error)) {
      return;
    }
    CompleteFunctionWithError(
        IdentityGetAuthTokenError::FromGetAccessTokenAuthError(
            error.ToString()));
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

  CompleteFunctionWithError(
      IdentityGetAuthTokenError(IdentityGetAuthTokenError::State::kCanceled));
}

#if defined(OS_CHROMEOS)
// Even though the DeviceOAuth2TokenService may be available on non-ChromeOS
// platforms, its robot account is not made available because it should only be
// used for very specific policy-related things. In fact, the device account on
// desktop isn't scoped for anything other than policy invalidations.
void IdentityGetAuthTokenFunction::StartDeviceAccessTokenRequest() {
  DeviceOAuth2TokenService* service = DeviceOAuth2TokenServiceFactory::Get();
  // Since robot account refresh tokens are scoped down to [any-api] only,
  // request access token for [any-api] instead of login.
  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert(GaiaConstants::kAnyApiOAuth2Scope);
  device_access_token_request_ = service->StartAccessTokenRequest(scopes, this);
}

bool IdentityGetAuthTokenFunction::IsOriginAllowlistedInPublicSession() {
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
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("identity", "GetAccessToken", this);

  auto* identity_manager = IdentityManagerFactory::GetForProfile(GetProfile());
#if defined(OS_CHROMEOS)
  if (chrome::IsRunningInForcedAppMode()) {
    std::string app_client_id;
    std::string app_client_secret;
    if (chromeos::UserSessionManager::GetInstance()
            ->GetAppModeChromeClientOAuthInfo(&app_client_id,
                                              &app_client_secret)) {
      token_key_account_access_token_fetcher_ =
          identity_manager->CreateAccessTokenFetcherForClient(
              token_key_.account_info.account_id, app_client_id,
              app_client_secret, kExtensionsIdentityAPIOAuthConsumerName,
              signin::ScopeSet(),
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
          token_key_.account_info.account_id,
          kExtensionsIdentityAPIOAuthConsumerName, signin::ScopeSet(),
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
              token_key_.account_info.account_id);
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

void IdentityGetAuthTokenFunction::ShowRemoteConsentDialog(
    const RemoteConsentResolutionData& resolution_data) {
  gaia_remote_consent_flow_ = std::make_unique<GaiaRemoteConsentFlow>(
      this, GetProfile(), token_key_, resolution_data);
  gaia_remote_consent_flow_->Start();
}

std::unique_ptr<OAuth2MintTokenFlow>
IdentityGetAuthTokenFunction::CreateMintTokenFlow() {
  std::string signin_scoped_device_id =
      GetSigninScopedDeviceIdForProfile(GetProfile());
  auto mint_token_flow = std::make_unique<OAuth2MintTokenFlow>(
      this,
      OAuth2MintTokenFlow::Parameters(
          extension()->id(), oauth2_client_id_,
          std::vector<std::string>(token_key_.scopes.begin(),
                                   token_key_.scopes.end()),
          enable_granular_permissions_, signin_scoped_device_id,
          GetSelectedUserId(), consent_result_, GetOAuth2MintTokenFlowVersion(),
          GetOAuth2MintTokenFlowChannel(), gaia_mint_token_mode_));
  return mint_token_flow;
}

bool IdentityGetAuthTokenFunction::HasRefreshTokenForTokenKeyAccount() const {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(GetProfile());
  return identity_manager->HasAccountWithRefreshToken(
      token_key_.account_info.account_id);
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

Profile* IdentityGetAuthTokenFunction::GetProfile() const {
  return Profile::FromBrowserContext(browser_context());
}

bool IdentityGetAuthTokenFunction::enable_granular_permissions() const {
  return enable_granular_permissions_;
}

std::string IdentityGetAuthTokenFunction::GetSelectedUserId() const {
  if (IsSelectedUserIdInGetAuthTokenEnabled() &&
      selected_gaia_id_ == token_key_.account_info.gaia)
    return selected_gaia_id_;

  return "";
}

}  // namespace extensions
