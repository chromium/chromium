// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pwc/pwc_navigation_throttle.h"

#include <memory>

#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace pwc {

// static
void PwcNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  // Lock the main frame of a PrivilegedWebContents to the policy's allowlist.
  // This covers both the primary main frame and a prerendered main frame.
  // Prerender coverage is defense in depth: prerender activation does not run
  // navigation throttles, so if an off-allowlist page (e.g.
  // https://blocked.com) were prerendered and later activated into the primary
  // main frame it would bypass this throttle entirely. Cancelling the
  // off-allowlist navigation while it is still prerendering closes that gap.
  // (Ideally prerendering is also disabled outright for a
  // PrivilegedWebContents; this throttle does not rely on that.) Subframes and
  // other frame trees (e.g. fenced frames) are left alone.
  if (!handle.IsInPrimaryMainFrame() && !handle.IsInPrerenderedMainFrame()) {
    return;
  }
  if (!PrivilegedWebContents::FromWebContents(handle.GetWebContents())) {
    return;
  }
  registry.AddThrottle(std::make_unique<PwcNavigationThrottle>(registry));
}

PwcNavigationThrottle::PwcNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

PwcNavigationThrottle::~PwcNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
PwcNavigationThrottle::WillStartRequest() {
  return CheckUrl();
}

content::NavigationThrottle::ThrottleCheckResult
PwcNavigationThrottle::WillRedirectRequest() {
  return CheckUrl();
}

content::NavigationThrottle::ThrottleCheckResult
PwcNavigationThrottle::WillCommitWithoutUrlLoader() {
  // Navigations that commit without a URL loader (about:blank, about:srcdoc,
  // other empty-document schemes, same-document navigations) never reach
  // WillStartRequest(). None of the empty-document URLs are on the HTTPS-only
  // allowlist, so CheckUrl() cancels them; a same-document navigation stays on
  // the already-committed allowlisted URL and proceeds. This closes the gap
  // where a subframe navigates the main frame to about:blank to move an
  // off-allowlist origin into the main frame.
  return CheckUrl();
}

content::NavigationThrottle::ThrottleCheckResult
PwcNavigationThrottle::CheckUrl() {
  content::NavigationHandle& handle = *navigation_handle();
  PrivilegedWebContents* privileged =
      PrivilegedWebContents::FromWebContents(handle.GetWebContents());
  // The throttle is only added for the primary main frame of a
  // PrivilegedWebContents (see MaybeCreateAndAdd).
  CHECK(privileged);
  // IsNavigationAllowed structurally enforces HTTPS in addition to the
  // component's navigation allowlist.
  if (!privileged->policy().IsNavigationAllowed(
          url::Origin::Create(handle.GetURL()))) {
    return CANCEL_AND_IGNORE;
  }
  return PROCEED;
}

const char* PwcNavigationThrottle::GetNameForLogging() {
  return "PwcNavigationThrottle";
}

}  // namespace pwc
