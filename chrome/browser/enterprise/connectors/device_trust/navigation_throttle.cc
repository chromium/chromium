// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/navigation_throttle.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/url_util.h"
#include "components/prefs/pref_service.h"
#include "components/url_matcher/url_matcher.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace enterprise_connectors {

// Const headers used in the handshake flow.
constexpr char kDeviceTrustHeader[] = "X-Device-Trust";
constexpr char kDeviceTrustHeaderValue[] = "VerifiedAccess";
constexpr char kVerifiedAccessChallengeHeader[] = "X-Verified-Access-Challenge";
constexpr char kVerifiedAccessResponseHeader[] =
    "X-Verified-Access-Challenge-Response";

// static
std::unique_ptr<DeviceTrustNavigationThrottle>
DeviceTrustNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  PrefService* prefs =
      Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext())
          ->GetPrefs();
  // TODO(b/183690432): Check if the browser or device is being managed
  // to create the throttle.
  if (!DeviceTrustService::IsEnabled(prefs))
    return nullptr;

  DVLOG(1) << "DeviceTrustNavigationThrottle::MaybeCreateThrottleFor";
  return std::make_unique<DeviceTrustNavigationThrottle>(navigation_handle);
}

DeviceTrustNavigationThrottle::DeviceTrustNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : DeviceTrustNavigationThrottle(
          DeviceTrustServiceFactory::GetForProfile(Profile::FromBrowserContext(
              navigation_handle->GetWebContents()->GetBrowserContext())),
          navigation_handle) {}

DeviceTrustNavigationThrottle::DeviceTrustNavigationThrottle(
    DeviceTrustService* device_trust_service,
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle),
      device_trust_service_(device_trust_service) {
  matcher_ = std::make_unique<url_matcher::URLMatcher>();

  // Start listening for pref changes.
  subscription_ =
      device_trust_service_->RegisterTrustedUrlPatternsChangedCallback(
          base::BindRepeating(
              &DeviceTrustNavigationThrottle::OnTrustedUrlPatternsChanged,
              base::Unretained(this)));
}

DeviceTrustNavigationThrottle::~DeviceTrustNavigationThrottle() = default;

void DeviceTrustNavigationThrottle::OnTrustedUrlPatternsChanged(
    const base::ListValue& origins) {
  DVLOG(1)
      << "DeviceTrustNavigationThrottle::OnTrustedUrlPatternsChanged count="
      << origins.GetList().size();

  url_matcher::URLMatcherConditionSet::ID id(0);
  if (!matcher_->IsEmpty()) {
    // Clear old conditions in case they exist.
    matcher_->RemoveConditionSets({id});
  }

  if (device_trust_service_ && device_trust_service_->IsEnabled()) {
    // Add the new endpoints to the conditions.
    policy::url_util::AddFilters(matcher_.get(), true /* allowed */, &id,
                                 &origins);
  }
}

content::NavigationThrottle::ThrottleCheckResult
DeviceTrustNavigationThrottle::WillStartRequest() {
  return AddHeadersIfNeeded();
}

content::NavigationThrottle::ThrottleCheckResult
DeviceTrustNavigationThrottle::WillRedirectRequest() {
  return AddHeadersIfNeeded();
}

const char* DeviceTrustNavigationThrottle::GetNameForLogging() {
  return "DeviceTrustNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
DeviceTrustNavigationThrottle::AddHeadersIfNeeded() {
  const GURL& url = navigation_handle()->GetURL();
  DVLOG(1) << "DeviceTrustNavigationThrottle::AddHeadersIfNeeded url="
           << url.spec().c_str();

  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return PROCEED;

  if (!device_trust_service_ || !device_trust_service_->IsEnabled())
    return PROCEED;

  DCHECK(matcher_);
  auto matches = matcher_->MatchURL(url);
  if (matches.empty())
    return PROCEED;

  DVLOG(1) << "   DeviceTrustNavigationThrottle::AddHeadersIfNeeded matched";

  // If we are starting an attestation flow.
  if (navigation_handle()->GetResponseHeaders() == nullptr) {
    DVLOG(1) << "   DeviceTrustNavigationThrottle::AddHeadersIfNeeded adding "
                "x-device-trust";
    navigation_handle()->SetRequestHeader(kDeviceTrustHeader,
                                          kDeviceTrustHeaderValue);
    return PROCEED;
  }

  // If a challenge is coming from the Idp.
  if (navigation_handle()->GetResponseHeaders()->HasHeader(
          kVerifiedAccessChallengeHeader)) {
    DVLOG(1) << "   DeviceTrustNavigationThrottle::HasHeader url="
             << url.spec().c_str();

    // Remove request header since is not needed for challenge response.
    navigation_handle()->RemoveRequestHeader(kDeviceTrustHeader);

    // Get challenge.
    const net::HttpResponseHeaders* headers =
        navigation_handle()->GetResponseHeaders();
    std::string challenge;
    if (headers->GetNormalizedHeader(kVerifiedAccessChallengeHeader,
                                     &challenge)) {
      // Create callback for `ReplyChallengeResponseAndResume` which will
      // be called after the challenge response is created. With this
      // we can defer the navigation to unblock the main thread.
      AttestationCallback resume_navigation_callback = base::BindOnce(
          &DeviceTrustNavigationThrottle::ReplyChallengeResponseAndResume,
          weak_ptr_factory_.GetWeakPtr());

      // Call `DeviceTrustService::BuildChallengeResponse` which is one step on
      // the chain that builds the challenge response. In this chain we post a
      // task that won't run in the main thread.
      device_trust_service_->BuildChallengeResponse(
          challenge, std::move(resume_navigation_callback));

      return DEFER;
    }
  } else {
    LOG(ERROR) << "No challenge in the response.";
  }
  return PROCEED;
}

void DeviceTrustNavigationThrottle::ReplyChallengeResponseAndResume(
    const std::string& challenge_response) {
  DVLOG(1) << "DeviceTrustNavigationThrottle::ReplyChallengeResponseAndResume "
              "challenge_response="
           << challenge_response;
  if (challenge_response == std::string()) {
    // Cancel the navigation if challenge signature is invalid.
    CancelDeferredNavigation(content::NavigationThrottle::CANCEL_AND_IGNORE);
  } else {
    navigation_handle()->SetRequestHeader(kVerifiedAccessResponseHeader,
                                          challenge_response);
    Resume();
  }
}

}  // namespace enterprise_connectors
