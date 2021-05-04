// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/navigation_throttle.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/url_util.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace enterprise_connectors {

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
    navigation_handle()->SetRequestHeader("X-Device-Trust", "VerifiedAccess");
    return PROCEED;
  }

  // If a challenge is coming from the Idp.
  if (!navigation_handle()->GetRequestHeaders().HasHeader(
          "x-verified-access-challenge")) {
    navigation_handle()->RemoveRequestHeader("X-Device-Trust");
    return PROCEED;
  }

  return PROCEED;
}

}  // namespace enterprise_connectors
