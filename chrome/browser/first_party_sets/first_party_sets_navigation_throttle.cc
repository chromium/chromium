// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_navigation_throttle.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"

namespace first_party_sets {

namespace {

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

}  // namespace

FirstPartySetsNavigationThrottle::FirstPartySetsNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle),
      profile_(*Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext())) {}

FirstPartySetsNavigationThrottle::~FirstPartySetsNavigationThrottle() = default;

ThrottleCheckResult FirstPartySetsNavigationThrottle::WillStartRequest() {
  FirstPartySetsPolicyService* service =
      FirstPartySetsPolicyServiceFactory::GetForBrowserContext(&profile_);
  // FirstPartySetsPolicyService is always created.
  DCHECK(service);
  if (!service->is_ready()) {
    service->RegisterThrottleResumeCallback(base::BindOnce(
        &FirstPartySetsNavigationThrottle::Resume, weak_factory_.GetWeakPtr()));
    return content::NavigationThrottle::DEFER;
  }
  return content::NavigationThrottle::PROCEED;
}

const char* FirstPartySetsNavigationThrottle::GetNameForLogging() {
  return "FirstPartySetsNavigationThrottle";
}

std::unique_ptr<FirstPartySetsNavigationThrottle>
FirstPartySetsNavigationThrottle::MaybeCreateNavigationThrottle(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!features::kFirstPartySetsClearSiteDataOnChangedSets.Get() ||
      navigation_handle->GetParentFrameOrOuterDocument()) {
    return nullptr;
  }
  return std::make_unique<FirstPartySetsNavigationThrottle>(navigation_handle);
}

}  // namespace first_party_sets
