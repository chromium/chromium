// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/content_restriction/aw_content_restriction_navigation_throttle.h"

#include <memory>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/content_restriction/aw_content_restriction_blocked_navigation_tracker.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"

namespace android_webview {

AwContentRestrictionNavigationThrottle::AwContentRestrictionNavigationThrottle(
    content::NavigationThrottleRegistry& registry,
    AwContentRestrictionBlockedNavigationTracker* tracker)
    : content::NavigationThrottle(registry), tracker_(tracker) {}

AwContentRestrictionNavigationThrottle::
    ~AwContentRestrictionNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
AwContentRestrictionNavigationThrottle::WillFailRequest() {
  DCHECK(tracker_);
  int64_t nav_id = navigation_handle()->GetNavigationId();
  if (!tracker_->IsNavigationBlocked(nav_id)) {
    return PROCEED;
  }

  // The request was blocked by content restriction. Show the interstitial.
  // TODO(crbug.com/499090696): Replace placeholder content with the
  // interstitial implementation.
  tracker_->ClearNavigationBlocked(nav_id);
  std::string error_page_content =
      "<html><body><h1>Blocked</h1><p>This content is "
      "restricted.</p></body></html>";

  return content::NavigationThrottle::ThrottleCheckResult(
      CANCEL, net::ERR_BLOCKED_BY_CLIENT, error_page_content);
}

const char* AwContentRestrictionNavigationThrottle::GetNameForLogging() {
  return "AwContentRestrictionNavigationThrottle";
}

}  // namespace android_webview
