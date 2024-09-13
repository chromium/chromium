// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/gaia_remote_consent_flow.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/google_accounts_private_api_host.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
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
    const RemoteConsentResolutionData& resolution_data,
    bool user_gesture)
    : delegate_(delegate),
      profile_(profile),
      resolution_data_(resolution_data),
      user_gesture_(user_gesture),
      web_flow_started_(false) {}

GaiaRemoteConsentFlow::~GaiaRemoteConsentFlow() {
  DetachWebAuthFlow();
}

void GaiaRemoteConsentFlow::Start() {
  if (!web_flow_) {
    web_flow_ =
        std::make_unique<WebAuthFlow>(this, profile_, resolution_data_.url,
                                      WebAuthFlow::INTERACTIVE, user_gesture_);
  }

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

void GaiaRemoteConsentFlow::Stop() {
  if (web_flow_) {
    web_flow_->Stop();
  }
  profile_ = nullptr;
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

void GaiaRemoteConsentFlow::OnAuthFlowFailure(WebAuthFlow::Failure failure) {
  GaiaRemoteConsentFlow::Failure gaia_failure;

  switch (failure) {
    case WebAuthFlow::WINDOW_CLOSED:
      gaia_failure = GaiaRemoteConsentFlow::WINDOW_CLOSED;
      break;
    case WebAuthFlow::LOAD_FAILED:
    case WebAuthFlow::TIMED_OUT:
      gaia_failure = GaiaRemoteConsentFlow::LOAD_FAILED;
      break;
    case WebAuthFlow::INTERACTION_REQUIRED:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected error from web auth flow: " << failure;
      gaia_failure = GaiaRemoteConsentFlow::LOAD_FAILED;
      break;
    case WebAuthFlow::CANNOT_CREATE_WINDOW:
      gaia_failure = GaiaRemoteConsentFlow::CANNOT_CREATE_WINDOW;
      break;
  }

  GaiaRemoteConsentFlowFailed(gaia_failure);
}

network::mojom::CookieManager*
GaiaRemoteConsentFlow::GetCookieManagerForPartition() {
  return profile_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess();
}

void GaiaRemoteConsentFlow::SetWebAuthFlowForTesting(
    std::unique_ptr<WebAuthFlow> web_auth_flow) {
  DetachWebAuthFlow();
  web_flow_ = std::move(web_auth_flow);
}

WebAuthFlow* GaiaRemoteConsentFlow::GetWebAuthFlowForTesting() const {
  return web_flow_.get();
}

void GaiaRemoteConsentFlow::GaiaRemoteConsentFlowFailed(Failure failure) {
  RecordResultHistogram(failure);
  delegate_->OnGaiaRemoteConsentFlowFailed(failure);
}

void GaiaRemoteConsentFlow::DetachWebAuthFlow() {
  if (!web_flow_) {
    return;
  }

  web_flow_.release()->DetachDelegateAndDelete();
}

void GaiaRemoteConsentFlow::OnNavigationFinished(
    content::NavigationHandle* navigation_handle) {
  GoogleAccountsPrivateApiHost::CreateReceiver(
      base::BindRepeating(&GaiaRemoteConsentFlow::ReactToConsentResult,
                          weak_factory.GetWeakPtr()),
      navigation_handle);
}

}  // namespace extensions
