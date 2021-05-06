// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/navigation_throttle.h"

#include "base/memory/ptr_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/interface.pb.h"
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
  PrefService* prefs_ =
      Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext())
          ->GetPrefs();
  // TODO(b/183690432): Check if the browser or device is being managed
  // to create the throttle.
  if (!prefs_->HasPrefPath(kContextAwareAccessSignalsAllowlistPref) ||
      prefs_->GetList(kContextAwareAccessSignalsAllowlistPref)->empty())
    return nullptr;

  return std::make_unique<DeviceTrustNavigationThrottle>(navigation_handle);
}

DeviceTrustNavigationThrottle::DeviceTrustNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {
  device_trust_service_ =
      DeviceTrustFactory::GetForProfile(Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext()));
  matcher_ = std::make_unique<url_matcher::URLMatcher>();

  // Start listening for pref changes.
  pref_change_registrar_.Init(user_prefs::UserPrefs::Get(
      navigation_handle->GetWebContents()->GetBrowserContext()));
  pref_change_registrar_.Add(
      kContextAwareAccessSignalsAllowlistPref,
      base::BindRepeating(&DeviceTrustNavigationThrottle::OnPolicyUpdate,
                          base::Unretained(this)));
  OnPolicyUpdate();
}

DeviceTrustNavigationThrottle::~DeviceTrustNavigationThrottle() = default;

void DeviceTrustNavigationThrottle::OnPolicyUpdate() {
  url_matcher::URLMatcherConditionSet::ID id(0);
  if (!matcher_->IsEmpty()) {
    // Clear old conditions in case they exist.
    matcher_->RemoveConditionSets({id});
  }

  PrefService* prefs = pref_change_registrar_.prefs();
  if (device_trust_service_->IsEnabled()) {
    // Add the new endpoints to the conditions.
    const base::Value* origins =
        prefs->GetList(kContextAwareAccessSignalsAllowlistPref);
    policy::url_util::AddFilters(matcher_.get(), true /* allowed */, &id,
                                 &base::Value::AsListValue(*origins));
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

  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return PROCEED;

  DCHECK(device_trust_service_);
  if (!device_trust_service_->IsEnabled())
    return PROCEED;

  DCHECK(matcher_);
  auto matches = matcher_->MatchURL(url);
  if (matches.empty())
    return PROCEED;

  // If we are starting an attestation flow.
  if (navigation_handle()->GetResponseHeaders() == nullptr) {
    navigation_handle()->SetRequestHeader(kDeviceTrustHeader,
                                          kDeviceTrustHeaderValue);
    return PROCEED;
  }

  // If a challenge is coming from the Idp.
  if (navigation_handle()->GetResponseHeaders()->HasHeader(
          kVerifiedAccessChallengeHeader)) {
    // Remove request header since is not needed for challenge response.
    navigation_handle()->RemoveRequestHeader(kDeviceTrustHeader);

    // Get challenge.
    const net::HttpResponseHeaders* headers =
        navigation_handle()->GetResponseHeaders();
    std::string challenge;
    if (headers->GetNormalizedHeader(kVerifiedAccessChallengeHeader,
                                     &challenge)) {
      // Post a task to get the challenge response. It will defer the navigation
      // and it will be resumed after it's built.
      // `StartSignChallengeAndReplyWithResponse` won't run in the main thread,
      // and then reply to `ReplyChallengeResponseAndResume` which makes use of
      // `weak_ptr_factory_.GetWeakPtr()` so it won't run in case the object is
      // destroyed.
      AttestationCallback reply = base::BindOnce(
          &DeviceTrustNavigationThrottle::ReplyChallengeResponseAndResume,
          weak_ptr_factory_.GetWeakPtr());

      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
          base::BindOnce(&DeviceTrustNavigationThrottle::
                             StartSignChallengeAndReplyWithResponse,
                         base::Unretained(this), challenge),
          std::move(reply));

      return DEFER;
    }
  }
  return PROCEED;
}

std::string
DeviceTrustNavigationThrottle::StartSignChallengeAndReplyWithResponse(
    const std::string& challenge) {
  return device_trust_service_->BuildChallengeResponse(challenge);
}

void DeviceTrustNavigationThrottle::ReplyChallengeResponseAndResume(
    std::string challenge_response) {
  navigation_handle()->SetRequestHeader(kVerifiedAccessResponseHeader,
                                        challenge_response);
  Resume();
}

}  // namespace enterprise_connectors
