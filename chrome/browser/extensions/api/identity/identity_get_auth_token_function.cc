// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_get_auth_token_function.h"

#include <set>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/identity/identity_get_auth_token_error.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/extensions/api/identity.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/api/oauth2.h"
#include "extensions/common/manifest_handlers/oauth2_manifest_handler.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/idle/idle.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/account_manager_core/account_manager_util.h"
#include "google_apis/gaia/gaia_constants.h"
#endif

namespace extensions {

namespace {

const char* const kExtensionsIdentityAPIOAuthConsumerName =
    "extensions_identity_api";

bool IsBrowserSigninAllowed(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed);
}

std::string_view GetOAuth2MintTokenFlowVersion() {
  return version_info::GetVersionNumber();
}

std::string_view GetOAuth2MintTokenFlowChannel() {
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

bool IsInteractionAllowed(
    IdentityGetAuthTokenFunction::InteractivityStatus status) {
  switch (status) {
    case IdentityGetAuthTokenFunction::InteractivityStatus::kNotRequested:
    case IdentityGetAuthTokenFunction::InteractivityStatus::kDisallowedIdle:
    case IdentityGetAuthTokenFunction::InteractivityStatus::
        kDisallowedSigninDisallowed:
      return false;
    case IdentityGetAuthTokenFunction::InteractivityStatus::kAllowedWithGesture:
    case IdentityGetAuthTokenFunction::InteractivityStatus::
        kAllowedWithActivity:
      return true;
  }
}

CoreAccountInfo GetSigninPrimaryAccount(Profile* profile) {
  return IdentityManagerFactory::GetForProfile(profile)->GetPrimaryAccountInfo(
      signin::ConsentLevel::kSignin);
}

}  // namespace

IdentityGetAuthTokenFunction::IdentityGetAuthTokenFunction() = default;

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

  std::optional<api::identity::GetAuthToken::Params> params =
      api::identity::GetAuthToken::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  ComputeInteractivityStatus(params->details);

  enable_granular_permissions_ =
      params->details &&
      params->details->enable_granular_permissions.value_or(false);

  DCHECK(extension());
  const auto& oauth2_info = OAuth2ManifestHandler::GetOAuth2Info(*extension());

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

  if (params->details) {
    if (params->details->account) {
      gaia_id = params->details->account->id;
    }

    if (params->details->scopes) {
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

  // TODO(msalama): Remove this block as `IsPrimaryAccountOnly()` always return
  // false.
  // Detect and handle the case where the extension is using an account other
  // than the primary account.
  if (IsPrimaryAccountOnly() && !gaia_id.empty() &&
      gaia_id != GetSigninPrimaryAccount(GetProfile()).gaia) {
    IdentityGetAuthTokenError error(
        IdentityGetAuthTokenError::State::kUserNonPrimary);
    RecordFunctionResult(error, remote_consent_approved_);
    return RespondNow(Error(error.ToString()));
  }

  // From here on out, results must be returned asynchronously.
  StartAsyncRun();

  // TODO(crbug.com/40614113): collapse the asynchronicity
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&IdentityGetAuthTokenFunction::GetAuthTokenForAccount,
                     weak_ptr_factory_.GetWeakPtr(), gaia_id));

  return RespondLater();
}

void IdentityGetAuthTokenFunction::GetAuthTokenForAccount(
    const std::string& gaia_id) {
  selected_gaia_id_ = gaia_id;
  if (gaia_id.empty()) {
    selected_gaia_id_ = IdentityAPI::GetFactoryInstance()
                            ->Get(GetProfile())
                            ->GetGaiaIdForExtension(token_key_.extension_id)
                            .value_or("");
  }

  CoreAccountInfo selected_account;
  if (!selected_gaia_id_.empty()) {
    // TODO(msalama): Check has access to accounts.
    selected_account = IdentityManagerFactory::GetForProfile(GetProfile())
                           ->FindExtendedAccountInfoByGaiaId(selected_gaia_id_);
  } else {
    selected_account = GetSigninPrimaryAccount(GetProfile());
  }

  token_key_.account_info = selected_account;
#if BUILDFLAG(IS_CHROMEOS)
  if (g_browser_process->browser_policy_connector()
          ->IsDeviceEnterpriseManaged()) {
    if (chromeos::IsManagedGuestSession()) {
      CompleteFunctionWithError(IdentityGetAuthTokenError(
          IdentityGetAuthTokenError::State::kNotAllowlistedInPublicSession));
      return;
    }
    if (chromeos::IsKioskSession()) {
      StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE);
      return;
    }
  }
#endif

  if (selected_account.IsEmpty()) {
    if (!ShouldStartSigninFlow()) {
      CompleteFunctionWithError(
          GetErrorFromInteractivityStatus(InteractionType::kSignin));
      return;
    }
    // Display a login prompt.
    StartSigninFlow();
  } else {
    StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE);
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
  identity_api_shutdown_subscription_ = {};

  Respond(std::move(response));
  Release();  // Balanced in StartAsyncRun
}

void IdentityGetAuthTokenFunction::CompleteFunctionWithResult(
    const std::string& access_token,
    const std::set<std::string>& granted_scopes) {
  RecordFunctionResult(IdentityGetAuthTokenError(), remote_consent_approved_);

  api::identity::GetAuthTokenResult result;
  result.token = access_token;
  result.granted_scopes.emplace(granted_scopes.begin(), granted_scopes.end());

  CompleteAsyncRun(WithArguments(result.ToValue()));
}

void IdentityGetAuthTokenFunction::CompleteFunctionWithError(
    const IdentityGetAuthTokenError& error) {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1("identity", "CompleteFunctionWithError",
                                      this, "error", error.ToString());
  RecordFunctionResult(error, remote_consent_approved_);
  CompleteAsyncRun(Error(error.ToString()));
}

bool IdentityGetAuthTokenFunction::ShouldStartSigninFlow() {
  if (!IsInteractionAllowed(interactivity_status_for_signin_)) {
    return false;
  }

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
  interactivity_status_for_signin_ = InteractivityStatus::kNotRequested;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // In normal mode (i.e. non-kiosk mode), the user has to log out to
  // re-establish credentials. Let the global error popup handle everything.
  // In kiosk mode, interactive sign-in is not supported.
  SigninFailed();
#else
  if (ExtensionsBrowserClient::Get()->IsShuttingDown()) {
    // The login prompt cannot be displayed when the browser process is shutting
    // down.
    SigninFailed();
    return;
  }

  CHECK(!waiting_on_account_);
  waiting_on_account_ = true;
  auto* identity_manager = IdentityManagerFactory::GetForProfile(GetProfile());
  scoped_identity_manager_observation_.Observe(identity_manager);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin) &&
      !identity_manager->GetAccountsWithRefreshTokens().empty() &&
      switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    // The user is signed in on the web but not to Chrome.
    MaybeShowChromeSigninDialog();
    return;
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  ShowExtensionLoginPrompt();
#endif
}

void IdentityGetAuthTokenFunction::StartMintTokenFlow(
    IdentityMintRequestQueue::MintType type) {
#if !BUILDFLAG(IS_CHROMEOS)
  // ChromeOS in kiosk mode may start the mint token flow without account.
  DCHECK(!token_key_.account_info.IsEmpty())
      << "token_key_.account_info is empty!";
  DCHECK(IdentityManagerFactory::GetForProfile(GetProfile())
             ->HasAccountWithRefreshToken(token_key_.account_info.account_id))
      << "No Refresh token!";
#endif
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("identity", "MintTokenFlow", this, "type",
                                    type);

  mint_token_flow_type_ = type;

  // Flows are serialized to prevent excessive traffic to GAIA, and
  // to consolidate UI pop-ups.
  IdentityAPI* id_api =
      extensions::IdentityAPI::GetFactoryInstance()->Get(GetProfile());

  if (!IsInteractionAllowed(interactivity_status_for_consent_)) {
    if (type == IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE) {
      // GAIA told us to do a consent UI.
      CompleteFunctionWithError(
          GetErrorFromInteractivityStatus(InteractionType::kConsent));
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

  DCHECK(extension());
  const auto& oauth2_info = OAuth2ManifestHandler::GetOAuth2Info(*extension());
  IdentityAPI* id_api = IdentityAPI::GetFactoryInstance()->Get(GetProfile());
  IdentityTokenCacheValue cache_entry =
      id_api->token_cache()->GetToken(token_key_);
  IdentityTokenCacheValue::CacheValueStatus cache_status = cache_entry.status();

  if (type == IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE) {
    switch (cache_status) {
      case IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND:
#if BUILDFLAG(IS_CHROMEOS)
        // Always force minting token for ChromeOS kiosk app and managed guest
        // session.
        if (chromeos::IsManagedGuestSession()) {
          CompleteFunctionWithError(
              IdentityGetAuthTokenError(IdentityGetAuthTokenError::State::
                                            kNotAllowlistedInPublicSession));
          return;
        }
        if (chromeos::IsKioskSession()) {
          gaia_mint_token_mode_ = OAuth2MintTokenFlow::MODE_MINT_TOKEN_FORCE;
          if (g_browser_process->browser_policy_connector()
                  ->IsDeviceEnterpriseManaged()) {
            StartDeviceAccessTokenRequest();
          } else {
            StartTokenKeyAccountAccessTokenRequest();
          }
          return;
        }
#endif

        if (oauth2_info.auto_approve && *oauth2_info.auto_approve) {
          // oauth2_info.auto_approve is protected by an allowlist in
          // _manifest_features.json hence only selected extensions take
          // advantage of forcefully minting the token.
          gaia_mint_token_mode_ = OAuth2MintTokenFlow::MODE_MINT_TOKEN_FORCE;
        } else {
          gaia_mint_token_mode_ = OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE;
        }

        StartTokenKeyAccountAccessTokenRequest();
        break;

      case IdentityTokenCacheValue::CACHE_STATUS_TOKEN:
        CompleteMintTokenFlow();
        CompleteFunctionWithResult(cache_entry.token(),
                                   cache_entry.granted_scopes());
        break;

      case IdentityTokenCacheValue::CACHE_STATUS_REMOTE_CONSENT:
        CompleteMintTokenFlow();
        interactivity_status_for_signin_ = InteractivityStatus::kNotRequested;
        resolution_data_ = cache_entry.resolution_data();
        StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE);
        break;

      case IdentityTokenCacheValue::CACHE_STATUS_REMOTE_CONSENT_APPROVED:
        consent_result_ = cache_entry.consent_result();
        interactivity_status_for_signin_ = InteractivityStatus::kNotRequested;
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
      case IdentityTokenCacheValue::CACHE_STATUS_REMOTE_CONSENT:
        ShowRemoteConsentDialog(resolution_data_);
        break;
      case IdentityTokenCacheValue::CACHE_STATUS_REMOTE_CONSENT_APPROVED:
        consent_result_ = cache_entry.consent_result();
        interactivity_status_for_signin_ = InteractivityStatus::kNotRequested;
        gaia_mint_token_mode_ = OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE;
        StartTokenKeyAccountAccessTokenRequest();
        break;
    }
  }
}

void IdentityGetAuthTokenFunction::OnMintTokenSuccess(
    const OAuth2MintTokenFlow::MintTokenResult& result) {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("identity", "OnMintTokenSuccess", this);

  IdentityTokenCacheValue token = IdentityTokenCacheValue::CreateToken(
      result.access_token, result.granted_scopes, result.time_to_live);
  IdentityAPI::GetFactoryInstance()
      ->Get(GetProfile())
      ->token_cache()
      ->SetToken(token_key_, token);

  CompleteMintTokenFlow();
  CompleteFunctionWithResult(result.access_token, result.granted_scopes);
}

void IdentityGetAuthTokenFunction::OnMintTokenFailure(
    const GoogleServiceAuthError& error) {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1("identity", "OnMintTokenFailure", this,
                                      "error", error.ToString());
  CompleteMintTokenFlow();
  switch (error.state()) {
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
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

void IdentityGetAuthTokenFunction::OnRemoteConsentSuccess(
    const RemoteConsentResolutionData& resolution_data) {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("identity", "OnRemoteConsentSuccess",
                                      this);

  IdentityAPI::GetFactoryInstance()
      ->Get(GetProfile())
      ->token_cache()
      ->SetToken(token_key_,
                 IdentityTokenCacheValue::CreateRemoteConsent(resolution_data));
  interactivity_status_for_signin_ = InteractivityStatus::kNotRequested;
  resolution_data_ = resolution_data;
  CompleteMintTokenFlow();
  StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE);
}

void IdentityGetAuthTokenFunction::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (!waiting_on_account_) {
    return;
  }

  if (GetSigninPrimaryAccount(GetProfile()).IsEmpty()) {
    // Wait on primary account set.
    return;
  }

  // TODO(msalama): Clean-up, this shouldn't be possible.
  // No specific account id was requested, use the first one we find.
  if (token_key_.account_info.IsEmpty()) {
    token_key_.account_info = account_info;
  }

  if (token_key_.account_info == account_info) {
    // Stop listening tokens.
    waiting_on_account_ = false;
    scoped_identity_manager_observation_.Reset();

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

void IdentityGetAuthTokenFunction::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    return;
  }

  if (!waiting_on_account_) {
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("identity",
                                      "OnPrimaryAccountChanged (set)", this);

  const CoreAccountInfo& primary_account_info =
      event_details.GetCurrentState().primary_account;
  DCHECK(token_key_.account_info.IsEmpty());
  token_key_.account_info = primary_account_info;

  DCHECK(!GetSigninPrimaryAccount(GetProfile()).IsEmpty());
  waiting_on_account_ = false;
  scoped_identity_manager_observation_.Reset();

  StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE);
}

void IdentityGetAuthTokenFunction::SigninFailed() {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("identity", "SigninFailed", this);
  CompleteFunctionWithError(IdentityGetAuthTokenError(
      IdentityGetAuthTokenError::State::kSignInFailed));
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
      NOTREACHED_IN_MIGRATION();
      break;

    case GaiaRemoteConsentFlow::CANNOT_CREATE_WINDOW:
      error = IdentityGetAuthTokenError(
          IdentityGetAuthTokenError::State::kCannotCreateWindow);
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

  AccountInfo account = IdentityManagerFactory::GetForProfile(GetProfile())
                            ->FindExtendedAccountInfoByGaiaId(gaia_id);
  if (account.IsEmpty()) {
    CompleteMintTokenFlow();
    CompleteFunctionWithError(IdentityGetAuthTokenError(
        IdentityGetAuthTokenError::State::kRemoteConsentUserNotSignedIn));
    return;
  }

  if (IsPrimaryAccountOnly()) {
    CoreAccountId primary_account_id =
        IdentityManagerFactory::GetForProfile(GetProfile())
            ->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
    if (primary_account_id != account.account_id) {
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
  new_token_key.account_info = account;
  id_api->token_cache()->SetToken(
      new_token_key,
      IdentityTokenCacheValue::CreateRemoteConsentApproved(consent_result));
  CompleteMintTokenFlow();
  token_key_ = new_token_key;
  consent_result_ = consent_result;
  interactivity_status_for_signin_ = InteractivityStatus::kNotRequested;
  StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE);
}

void IdentityGetAuthTokenFunction::OnGetAccessTokenComplete(
    const std::optional<std::string>& access_token,
    base::Time expiration_time,
    const GoogleServiceAuthError& error) {
  // By the time we get here we should no longer have an outstanding access
  // token request.
#if BUILDFLAG(IS_CHROMEOS)
  DCHECK(!device_oauth2_token_fetcher_);
#endif
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

#if BUILDFLAG(IS_CHROMEOS)
void IdentityGetAuthTokenFunction::OnAccessTokenForDeviceAccountFetchCompleted(
    crosapi::mojom::AccessTokenResultPtr result) {
  std::optional<std::string> access_token;
  base::Time expiration_time;
  GoogleServiceAuthError error = GoogleServiceAuthError::AuthErrorNone();
  if (result->is_access_token_info()) {
    access_token = result->get_access_token_info()->access_token;
    expiration_time = result->get_access_token_info()->expiration_time;
  } else {
    DCHECK(result->is_error());
    error = account_manager::FromMojoGoogleServiceAuthError(result->get_error())
                .value_or(GoogleServiceAuthError(
                    GoogleServiceAuthError::SERVICE_ERROR));
  }
  device_oauth2_token_fetcher_.reset();
  OnGetAccessTokenComplete(access_token, expiration_time, error);
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
    OnGetAccessTokenComplete(std::nullopt, base::Time(), error);
  }
}

void IdentityGetAuthTokenFunction::OnIdentityAPIShutdown() {
#if BUILDFLAG(IS_CHROMEOS)
  device_oauth2_token_fetcher_.reset();
#endif
  if (gaia_remote_consent_flow_) {
    gaia_remote_consent_flow_->Stop();
  }
  token_key_account_access_token_fetcher_.reset();
  scoped_identity_manager_observation_.Reset();
  extensions::IdentityAPI::GetFactoryInstance()
      ->Get(GetProfile())
      ->mint_queue()
      ->RequestCancel(token_key_, this);

  CompleteFunctionWithError(IdentityGetAuthTokenError(
      IdentityGetAuthTokenError::State::kBrowserContextShutDown));
}

#if BUILDFLAG(IS_CHROMEOS)
void IdentityGetAuthTokenFunction::StartDeviceAccessTokenRequest() {
  device_oauth2_token_fetcher_ = std::make_unique<DeviceOAuth2TokenFetcher>();
  // Since robot account refresh tokens are scoped down to [any-api] only,
  // request access token for [any-api] instead of login.
  // `Unretained()` is safe because this outlives
  // `device_oauth2_token_fetcher_`.
  device_oauth2_token_fetcher_->FetchAccessTokenForDeviceAccount(
      {GaiaConstants::kAnyApiOAuth2Scope},
      base::BindOnce(&IdentityGetAuthTokenFunction::
                         OnAccessTokenForDeviceAccountFetchCompleted,
                     base::Unretained(this)));
}
#endif

void IdentityGetAuthTokenFunction::StartTokenKeyAccountAccessTokenRequest() {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("identity", "GetAccessToken", this);

  auto* identity_manager = IdentityManagerFactory::GetForProfile(GetProfile());
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

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void IdentityGetAuthTokenFunction::MaybeShowChromeSigninDialog() {
  IdentityAPI* identity_api =
      IdentityAPI::GetFactoryInstance()->Get(GetProfile());
  identity_api->MaybeShowChromeSigninDialog(
      extension()->name(),
      base::BindOnce(
          &IdentityGetAuthTokenFunction::OnChromeSigninDialogDestroyed,
          weak_ptr_factory_.GetWeakPtr()));
}

void IdentityGetAuthTokenFunction::OnChromeSigninDialogDestroyed() {
  if (!IdentityManagerFactory::GetForProfile(GetProfile())
           ->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // The user, cancelled or dismissed the Chrome sign in dialog for
    // extensions.
    waiting_on_account_ = false;
    scoped_identity_manager_observation_.Reset();
    CompleteFunctionWithError(IdentityGetAuthTokenError(
        IdentityGetAuthTokenError::State::kSignInFailed));
    return;
  }
  // Note: We rely on `OnPrimaryAccountChanged()` to detect that Chrome is
  // signed in.
}
#endif

void IdentityGetAuthTokenFunction::ShowExtensionLoginPrompt() {
  const CoreAccountInfo& account = token_key_.account_info;
  std::string email_hint = account.IsEmpty()
                               ? GetSigninPrimaryAccount(GetProfile()).email
                               : account.email;

  signin_ui_util::ShowExtensionSigninPrompt(GetProfile(),
                                            IsPrimaryAccountOnly(), email_hint);
}

void IdentityGetAuthTokenFunction::ShowRemoteConsentDialog(
    const RemoteConsentResolutionData& resolution_data) {
  gaia_remote_consent_flow_ = std::make_unique<GaiaRemoteConsentFlow>(
      this, GetProfile(), token_key_, resolution_data, user_gesture());
  gaia_remote_consent_flow_->Start();
}

std::unique_ptr<OAuth2MintTokenFlow>
IdentityGetAuthTokenFunction::CreateMintTokenFlow() {
  std::string signin_scoped_device_id =
      GetSigninScopedDeviceIdForProfile(GetProfile());
  auto mint_token_flow = std::make_unique<OAuth2MintTokenFlow>(
      this,
      OAuth2MintTokenFlow::Parameters::CreateForExtensionFlow(
          extension()->id(), oauth2_client_id_,
          std::vector<std::string_view>(token_key_.scopes.begin(),
                                        token_key_.scopes.end()),
          gaia_mint_token_mode_, enable_granular_permissions_,
          GetOAuth2MintTokenFlowVersion(), GetOAuth2MintTokenFlowChannel(),
          signin_scoped_device_id, GetSelectedUserId(), consent_result_));
  return mint_token_flow;
}

bool IdentityGetAuthTokenFunction::HasRefreshTokenForTokenKeyAccount() const {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(GetProfile());
  return identity_manager->HasAccountWithRefreshToken(
      token_key_.account_info.account_id);
}

std::string IdentityGetAuthTokenFunction::GetOAuth2ClientId() const {
  DCHECK(extension());
  const auto& oauth2_info = OAuth2ManifestHandler::GetOAuth2Info(*extension());

  std::string client_id;
  if (oauth2_info.client_id) {
    client_id = *oauth2_info.client_id;
  }

  // Component apps using auto_approve may use Chrome's client ID by
  // omitting the field.
  if (client_id.empty() &&
      extension()->location() == mojom::ManifestLocation::kComponent &&
      oauth2_info.auto_approve && *oauth2_info.auto_approve) {
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
  if (selected_gaia_id_ == token_key_.account_info.gaia) {
    return selected_gaia_id_;
  }

  return "";
}

void IdentityGetAuthTokenFunction::ComputeInteractivityStatus(
    const std::optional<api::identity::TokenDetails>& details) {
  bool interactive = details && details->interactive.value_or(false);
  if (!interactive) {
    interactivity_status_for_consent_ = InteractivityStatus::kNotRequested;
    interactivity_status_for_signin_ = InteractivityStatus::kNotRequested;
    return;
  }

  InteractivityStatus status = InteractivityStatus::kDisallowedIdle;
  // Interactive mode requires user action, to prevent unwanted signin tabs.
  // See b/259072565.
  idle_time_ = base::Seconds(ui::CalculateIdleTime());
  if (user_gesture()) {
    status = InteractivityStatus::kAllowedWithGesture;
  } else if (ui::CalculateIdleState(kGetAuthTokenInactivityTime.InSeconds()) ==
             ui::IDLE_STATE_ACTIVE) {
    status = InteractivityStatus::kAllowedWithActivity;
  }

  interactivity_status_for_consent_ = status;
  interactivity_status_for_signin_ = status;

  if (IsInteractionAllowed(interactivity_status_for_signin_) &&
      !IsBrowserSigninAllowed(GetProfile())) {
    interactivity_status_for_signin_ =
        InteractivityStatus::kDisallowedSigninDisallowed;
  }
}

IdentityGetAuthTokenError
IdentityGetAuthTokenFunction::GetErrorFromInteractivityStatus(
    InteractionType interaction_type) const {
  InteractivityStatus status = InteractivityStatus::kNotRequested;
  switch (interaction_type) {
    case InteractionType::kSignin:
      status = interactivity_status_for_signin_;
      break;
    case InteractionType::kConsent:
      status = interactivity_status_for_consent_;
      break;
  }
  DCHECK(!IsInteractionAllowed(status));

  IdentityGetAuthTokenError::State state =
      IdentityGetAuthTokenError::State::kNone;
  switch (status) {
    case InteractivityStatus::kNotRequested:
      state = interaction_type == InteractionType::kConsent
                  ? IdentityGetAuthTokenError::State::
                        kGaiaConsentInteractionRequired
                  : IdentityGetAuthTokenError::State::kUserNotSignedIn;
      break;
    case InteractivityStatus::kDisallowedIdle:
      state = IdentityGetAuthTokenError::State::kInteractivityDenied;
      break;
    case InteractivityStatus::kDisallowedSigninDisallowed:
      state = IdentityGetAuthTokenError::State::kBrowserSigninNotAllowed;
      break;
    case InteractivityStatus::kAllowedWithGesture:
    case InteractivityStatus::kAllowedWithActivity:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  DCHECK_NE(state, IdentityGetAuthTokenError::State::kNone);
  return IdentityGetAuthTokenError(state);
}

}  // namespace extensions
