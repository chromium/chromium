// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/gaia_remote_consent_flow.h"

#include <algorithm>

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/google_accounts_private_api_host.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_id.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace extensions {

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

  if (resolution_data_.cookies.empty()) {
    OnResolutionDataCookiesSet({});
    return;
  }

  network::mojom::CookieManager* cookie_manager =
      GetCookieManagerForPartition();
  net::CookieOptions options;
  base::RepeatingCallback<void(net::CookieAccessResult)> cookie_set_callback =
      base::BarrierCallback<net::CookieAccessResult>(
          resolution_data_.cookies.size(),
          base::BindOnce(&GaiaRemoteConsentFlow::OnResolutionDataCookiesSet,
                         weak_factory.GetWeakPtr()));
  for (const auto& cookie : resolution_data_.cookies) {
    cookie_manager->SetCanonicalCookie(
        cookie,
        net::cookie_util::SimulatedCookieSource(cookie, url::kHttpsScheme),
        options, cookie_set_callback);
  }
}

void GaiaRemoteConsentFlow::ReactToConsentResult(
    const std::string& consent_result) {
  bool consent_approved = false;
  GaiaId gaia_id;
  if (!gaia::ParseOAuth2MintTokenConsentResult(consent_result,
                                               &consent_approved, &gaia_id)) {
    delegate_->OnGaiaRemoteConsentFlowFailed(
        GaiaRemoteConsentFlow::INVALID_CONSENT_RESULT);
    return;
  }

  if (!consent_approved) {
    delegate_->OnGaiaRemoteConsentFlowFailed(GaiaRemoteConsentFlow::NO_GRANT);
    return;
  }

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
      NOTREACHED() << "Unexpected error from web auth flow: " << failure;
    case WebAuthFlow::CANNOT_CREATE_WINDOW:
      gaia_failure = GaiaRemoteConsentFlow::CANNOT_CREATE_WINDOW;
      break;
  }

  delegate_->OnGaiaRemoteConsentFlowFailed(gaia_failure);
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

void GaiaRemoteConsentFlow::OnResolutionDataCookiesSet(
    const std::vector<net::CookieAccessResult>& cookie_set_result) {
  bool cookies_set_failed = std::ranges::any_of(
      cookie_set_result, [](const net::CookieAccessResult& result) {
        return !result.status.IsInclude();
      });

  if (cookies_set_failed) {
    delegate_->OnGaiaRemoteConsentFlowFailed(SET_RESOLUTION_COOKIES_FAILED);
    return;
  }

  web_flow_->Start();
  web_flow_started_ = true;
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
