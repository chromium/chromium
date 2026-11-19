// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_guest_navigation_throttle.h"

#include "base/feature_list.h"
#include "chrome/browser/glic/gemini_enterprise/geic_url_allowlist.h"
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
GlicGuestNavigationThrottle::WillProcessResponse() {
  const GURL& url = navigation_handle()->GetURL();
  content::BrowserContext* browser_context =
      navigation_handle()->GetWebContents()->GetBrowserContext();

  if (url.SchemeIsHTTPOrHTTPS() &&
      IsPairedWorkforceAuthOrigin(url::Origin::Create(url), browser_context)) {
    if (navigation_handle()->IsInPrimaryMainFrame()) {
      if (GlicWebClientManager* manager = GetWebClientManagerForWebContents(
              navigation_handle()->GetWebContents())) {
        manager->OnGuestNavigationBlocked(
            navigation_handle()->GetNavigationId(),
            mojom::GuestPageType::kSignInRequired, url);
      }
    }
    return CANCEL_AND_IGNORE;
  }

  return PROCEED;
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
  content::BrowserContext* browser_context =
      navigation_handle()->GetWebContents()->GetBrowserContext();
  const url::Origin origin = url::Origin::Create(url);

  // `MaybeCreateAndAdd` attaches this throttle to both the primary main frame
  // and speculative prerendered main frames (`IsInPrerenderedMainFrame()`). If
  // a background prerender navigation is blocked, cancel the prerender silently
  // without notifying `GlicWebClientManager` (which would otherwise transition
  // the active primary page to an error state).
  GlicWebClientManager* manager =
      navigation_handle()->IsInPrimaryMainFrame()
          ? GetWebClientManagerForWebContents(
                navigation_handle()->GetWebContents())
          : nullptr;

  if (IsAdminBlockedUrl(url)) {
    if (manager) {
      manager->OnGuestNavigationBlocked(navigation_handle()->GetNavigationId(),
                                        mojom::GuestPageType::kDisabledByAdmin);
    }
    return CANCEL_AND_IGNORE;
  }

  if (url.SchemeIsHTTPOrHTTPS() &&
      IsPairedWorkforceAuthOrigin(origin, browser_context)) {
    if (geic::IsWorkforceAuthPortalInteractiveSignInPath(url.path())) {
      if (manager) {
        manager->OnGuestNavigationBlocked(
            navigation_handle()->GetNavigationId(),
            mojom::GuestPageType::kSignInRequired, url);
      }
      return CANCEL_AND_IGNORE;
    }
    return PROCEED;
  }

  // Once a navigation has visited the paired Workforce Auth Portal, only allow
  // redirects back to an API-allowed guest origin (e.g.
  // `/side-panel?...&csesidx=<N>`); any redirect to a third-party or 1P login
  // origin (such as `accounts.google.com` or `login.corp.google.com`) requires
  // interactive sign-in in a pop-out browser tab.
  if (!IsGuestOriginAllowed(origin, browser_context) ||
      (!IsOriginAllowedGlicApi(origin, browser_context) &&
       GetLastWorkforceAuthPortalUrl(*navigation_handle()).is_valid())) {
    if (manager) {
      const BlockedGuestNavigation blocked =
          ClassifyBlockedGuestNavigation(*navigation_handle());
      manager->OnGuestNavigationBlocked(navigation_handle()->GetNavigationId(),
                                        blocked.page_type, blocked.blocked_url);
    }
    return CANCEL_AND_IGNORE;
  }

  return PROCEED;
}

}  // namespace glic
