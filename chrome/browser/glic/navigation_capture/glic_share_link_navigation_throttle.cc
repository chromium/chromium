// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/navigation_capture/glic_share_link_navigation_throttle.h"

#include "content/public/browser/navigation_throttle_registry.h"

namespace glic {

// static
void GlicShareLinkNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {}

GlicShareLinkNavigationThrottle::GlicShareLinkNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

GlicShareLinkNavigationThrottle::~GlicShareLinkNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
GlicShareLinkNavigationThrottle::WillStartRequest() {
  return PROCEED;
}

const char* GlicShareLinkNavigationThrottle::GetNameForLogging() {
  return "GlicShareLinkNavigationThrottle";
}

}  // namespace glic
