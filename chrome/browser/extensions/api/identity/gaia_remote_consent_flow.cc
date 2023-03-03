// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/gaia_remote_consent_flow.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/google_accounts_private_api_host.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/multilogin_parameters.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/signin/core/browser/consistency_cookie_manager.h"
#endif

namespace extensions {

namespace {

void RecordResultHistogram(GaiaRemoteConsentFlow::Failure failure) {
  base::UmaHistogramEnumeration("Signin.Extensions.GaiaRemoteConsentFlowResult",
                                failure);
}

}  // namespace

GaiaRemoteConsentFlow::Delegate::~Delegate() = default;

GaiaRemoteConsentFlow::GaiaRemoteConsentFlow(
    Delegate* delegate,
    Profile* profile,
    const ExtensionTokenKey& token_key,
    const RemoteConsentResolutionData& resolution_data)
    : delegate_(delegate),
      profile_(profile),
      account_id_(token_key.account_info.account_id),
      resolution_data_(resolution_data),
      web_flow_started_(false) {}

GaiaRemoteConsentFlow::~GaiaRemoteConsentFlow() {
  DetachWebAuthFlow();
}

void GaiaRemoteConsentFlow::Start() {
  if (!web_flow_) {
    web_flow_ = std::make_unique<WebAuthFlow>(
        this, profile_, resolution_data_.url, WebAuthFlow::INTERACTIVE,
        WebAuthFlow::GET_AUTH_TOKEN);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // `profile_` may be nullptr in tests.
    if (profile_ &&
        !base::FeatureList::IsEnabled(features::kWebAuthFlowInBrowserTab)) {
      AccountReconcilorFactory::GetForProfile(profile_)
          ->GetConsistencyCookieManager()
          ->AddExtraCookieManager(GetCookieManagerForPartition());
    }
#endif
  }

  if (base::FeatureList::IsEnabled(features::kWebAuthFlowInBrowserTab)) {
    StartWebFlow();
    return;
  }

  SetAccountsInCookie();
}

void GaiaRemoteConsentFlow::StartWebFlow() {
  network::mojom::CookieManager* cookie_manager =
      GetCookieManagerForPartition();
  net::CookieOptions options;
  for (const auto& cookie : resolution_data_.cookies) {
    cookie_manager->SetCanonicalCookie(
        cookie,
        net::cookie_util::SimulatedCookieSource(cookie, url::kHttpsScheme),
        options, network::mojom::CookieManager::SetCanonicalCookieCallback());
  }

  web_flow_->Start();
  web_flow_started_ = true;
}

void GaiaRemoteConsentFlow::OnSetAccountsComplete(
    signin::SetAccountsInCookieResult result) {
  // No need to inject account cookies when the flow is displayed in a browser
  // tab.
  DCHECK(!base::FeatureList::IsEnabled(features::kWebAuthFlowInBrowserTab));

  set_accounts_in_cookie_task_.reset();
  if (web_flow_started_) {
    return;
  }

  if (result != signin::SetAccountsInCookieResult::kSuccess) {
    GaiaRemoteConsentFlowFailed(
        GaiaRemoteConsentFlow::Failure::SET_ACCOUNTS_IN_COOKIE_FAILED);
    return;
  }

  identity_api_set_consent_result_subscription_ =
      IdentityAPI::GetFactoryInstance()
          ->Get(profile_)
          ->RegisterOnSetConsentResultCallback(
              base::BindRepeating(&GaiaRemoteConsentFlow::OnConsentResultSet,
                                  base::Unretained(this)));

  scoped_observation_.Observe(IdentityManagerFactory::GetForProfile(profile_));
  StartWebFlow();
}

void GaiaRemoteConsentFlow::ReactToConsentResult(
    const std::string& consent_result) {
  bool consent_approved = false;
  std::string gaia_id;
  if (!gaia::ParseOAuth2MintTokenConsentResult(consent_result,
                                               &consent_approved, &gaia_id)) {
    GaiaRemoteConsentFlowFailed(GaiaRemoteConsentFlow::INVALID_CONSENT_RESULT);
    return;
  }

  if (!consent_approved) {
    GaiaRemoteConsentFlowFailed(GaiaRemoteConsentFlow::NO_GRANT);
    return;
  }

  RecordResultHistogram(GaiaRemoteConsentFlow::NONE);
  delegate_->OnGaiaRemoteConsentFlowApproved(consent_result, gaia_id);
}

void GaiaRemoteConsentFlow::OnConsentResultSet(
    const std::string& consent_result,
    const std::string& window_id) {
  // JS hook in a browser tab calls `ReactToConsentResult()` directly.
  DCHECK(!base::FeatureList::IsEnabled(features::kWebAuthFlowInBrowserTab));

  if (!web_flow_ || window_id != web_flow_->GetAppWindowKey()) {
    return;
  }

  identity_api_set_consent_result_subscription_ = {};

  ReactToConsentResult(consent_result);
}

void GaiaRemoteConsentFlow::OnAuthFlowFailure(WebAuthFlow::Failure failure) {
  GaiaRemoteConsentFlow::Failure gaia_failure;

  switch (failure) {
    case WebAuthFlow::WINDOW_CLOSED:
      gaia_failure = GaiaRemoteConsentFlow::WINDOW_CLOSED;
      break;
    case WebAuthFlow::USER_NAVIGATED_AWAY:
      gaia_failure = GaiaRemoteConsentFlow::USER_NAVIGATED_AWAY;
      break;
    case WebAuthFlow::LOAD_FAILED:
    case WebAuthFlow::TIMED_OUT:
      gaia_failure = GaiaRemoteConsentFlow::LOAD_FAILED;
      break;
    case WebAuthFlow::INTERACTION_REQUIRED:
      NOTREACHED() << "Unexpected error from web auth flow: " << failure;
      gaia_failure = GaiaRemoteConsentFlow::LOAD_FAILED;
      break;
  }

  GaiaRemoteConsentFlowFailed(gaia_failure);
}

content::StoragePartition* GaiaRemoteConsentFlow::GetStoragePartition() {
  content::StoragePartition* storage_partition = web_flow_->GetGuestPartition();
  if (!storage_partition) {
    // `web_flow_` doesn't have a guest partition only when the Auth Through
    // Browser Tab flow is used.
    DCHECK(base::FeatureList::IsEnabled(features::kWebAuthFlowInBrowserTab));
    storage_partition = profile_->GetDefaultStoragePartition();
  }

  return storage_partition;
}

std::unique_ptr<GaiaAuthFetcher>
GaiaRemoteConsentFlow::CreateGaiaAuthFetcherForPartition(
    GaiaAuthConsumer* consumer,
    const gaia::GaiaSource& source) {
  return std::make_unique<GaiaAuthFetcher>(
      consumer, source,
      GetStoragePartition()->GetURLLoaderFactoryForBrowserProcess());
}

network::mojom::CookieManager*
GaiaRemoteConsentFlow::GetCookieManagerForPartition() {
  return GetStoragePartition()->GetCookieManagerForBrowserProcess();
}

void GaiaRemoteConsentFlow::OnEndBatchOfRefreshTokenStateChanges() {
  // No need to copy added accounts when showing the flow in a browser tab.
  DCHECK(!base::FeatureList::IsEnabled(features::kWebAuthFlowInBrowserTab));

// On ChromeOS, new accounts are added through the account manager. They need to
// be pushed to the partition used by this flow explicitly.
// On Desktop, sign-in happens on the Web and a new account is directly added to
// this partition's cookie jar. An extra update triggered from here might change
// cookies order in the middle of the flow. This may lead to a bug like
// https://crbug.com/1112343.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(ash::IsAccountManagerAvailable(profile_));
  SetAccountsInCookie();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  if (AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile_))
    SetAccountsInCookie();
#endif
}

void GaiaRemoteConsentFlow::SetWebAuthFlowForTesting(
    std::unique_ptr<WebAuthFlow> web_auth_flow) {
  DetachWebAuthFlow();
  web_flow_ = std::move(web_auth_flow);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // `profile_` may be nullptr in tests.
  if (profile_) {
    AccountReconcilorFactory::GetForProfile(profile_)
        ->GetConsistencyCookieManager()
        ->AddExtraCookieManager(GetCookieManagerForPartition());
  }
#endif
}

WebAuthFlow* GaiaRemoteConsentFlow::GetWebAuthFlowForTesting() const {
  return web_flow_.get();
}

void GaiaRemoteConsentFlow::SetAccountsInCookie() {
  // No need to inject account cookies when the flow is displayed in a browser
  // tab.
  DCHECK(!base::FeatureList::IsEnabled(features::kWebAuthFlowInBrowserTab));

  // Reset a task that is already in flight because it contains stale
  // information.
  if (set_accounts_in_cookie_task_)
    set_accounts_in_cookie_task_.reset();

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  std::vector<CoreAccountId> accounts;
  if (IdentityAPI::GetFactoryInstance()
          ->Get(profile_)
          ->AreExtensionsRestrictedToPrimaryAccount()) {
    CoreAccountId primary_account_id =
        identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync);
    accounts.push_back(primary_account_id);
  } else {
    auto chrome_accounts_with_refresh_tokens =
        identity_manager->GetAccountsWithRefreshTokens();
    for (const auto& chrome_account : chrome_accounts_with_refresh_tokens) {
      // An account in persistent error state would make multilogin fail.
      // Showing only a subset of accounts seems to be a better alternative than
      // failing with an error.
      if (identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
              chrome_account.account_id)) {
        continue;
      }
      accounts.push_back(chrome_account.account_id);
    }
  }

  // base::Unretained() is safe here because this class owns
  // |set_accounts_in_cookie_task_| that will eventually invoke this callback.
  set_accounts_in_cookie_task_ =
      identity_manager->GetAccountsCookieMutator()
          ->SetAccountsInCookieForPartition(
              this,
              {gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
               accounts},
              gaia::GaiaSource::kChrome,
              base::BindOnce(&GaiaRemoteConsentFlow::OnSetAccountsComplete,
                             base::Unretained(this)));
}

void GaiaRemoteConsentFlow::GaiaRemoteConsentFlowFailed(Failure failure) {
  RecordResultHistogram(failure);
  delegate_->OnGaiaRemoteConsentFlowFailed(failure);
}

void GaiaRemoteConsentFlow::DetachWebAuthFlow() {
  if (!web_flow_)
    return;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // `profile_` may be nullptr in tests.
  if (profile_ &&
      !base::FeatureList::IsEnabled(features::kWebAuthFlowInBrowserTab)) {
    AccountReconcilorFactory::GetForProfile(profile_)
        ->GetConsistencyCookieManager()
        ->RemoveExtraCookieManager(GetCookieManagerForPartition());
  }
#endif
  web_flow_.release()->DetachDelegateAndDelete();
}

void GaiaRemoteConsentFlow::OnNavigationFinished(
    content::NavigationHandle* navigation_handle) {
  // No need to create the receiver if we are not displaying the auth page
  // through a Browser Tgab.
  if (!base::FeatureList::IsEnabled(features::kWebAuthFlowInBrowserTab)) {
    return;
  }

  GoogleAccountsPrivateApiHost::CreateReceiver(
      base::BindRepeating(&GaiaRemoteConsentFlow::ReactToConsentResult,
                          weak_factory.GetWeakPtr()),
      navigation_handle);
}

}  // namespace extensions
