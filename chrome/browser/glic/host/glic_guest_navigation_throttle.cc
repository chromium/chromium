// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_guest_navigation_throttle.h"

#include "base/feature_list.h"
#include "chrome/browser/glic/host/glic_web_client_manager.h"
#include "chrome/browser/glic/host/glic_web_contents_manager.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/host/guest_util_internal.h"
#include "chrome/browser/glic/public/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"

namespace glic {

// static
void GlicGuestNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  if (!base::FeatureList::IsEnabled(features::kGlicDisconnectedWebview)) {
    return;
  }
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!handle.IsInPrimaryMainFrame() && !handle.IsInPrerenderedMainFrame()) {
    return;
  }
  if (!IsGlicGuest(handle.GetWebContents())) {
    return;
  }
  registry.AddThrottle(std::make_unique<GlicGuestNavigationThrottle>(registry));
}

GlicGuestNavigationThrottle::GlicGuestNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

GlicGuestNavigationThrottle::~GlicGuestNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
GlicGuestNavigationThrottle::WillStartRequest() {
  return CheckUrl();
}

content::NavigationThrottle::ThrottleCheckResult
GlicGuestNavigationThrottle::WillRedirectRequest() {
  return CheckUrl();
}

content::NavigationThrottle::ThrottleCheckResult
GlicGuestNavigationThrottle::WillCommitWithoutUrlLoader() {
  return CheckUrl();
}

const char* GlicGuestNavigationThrottle::GetNameForLogging() {
  return "GlicGuestNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
GlicGuestNavigationThrottle::CheckUrl() {
  const GURL& url = navigation_handle()->GetURL();

  GlicWebClientManager* manager =
      GetWebClientManagerForWebContents(navigation_handle()->GetWebContents());

  if (IsAdminBlockedUrl(url)) {
    if (manager) {
      manager->OnGuestNavigationBlocked(mojom::GuestPageType::kDisabledByAdmin);
    }
    return CANCEL_AND_IGNORE;
  }

  if (!IsGuestOriginAllowed(
          url::Origin::Create(url),
          navigation_handle()->GetWebContents()->GetBrowserContext())) {
    if (manager) {
      manager->OnGuestNavigationBlocked(mojom::GuestPageType::kLoadError);
    }
    return CANCEL_AND_IGNORE;
  }

  return PROCEED;
}

}  // namespace glic
