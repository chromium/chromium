// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/content_restriction/aw_content_restriction_navigation_throttle.h"

#include <memory>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/content_restriction/aw_content_restriction_blocked_navigation_tracker.h"
#include "android_webview/browser/content_restriction/aw_content_restriction_blocking_page.h"
#include "android_webview/browser/content_restriction/aw_content_restriction_manager_client.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"

namespace android_webview {

AwContentRestrictionNavigationThrottle::AwContentRestrictionNavigationThrottle(
    content::NavigationThrottleRegistry& registry,
    AwContentRestrictionBlockedNavigationTracker* tracker,
    AwContentRestrictionManagerClient* content_restriction_manager_client)
    : content::NavigationThrottle(registry),
      tracker_(tracker),
      content_restriction_manager_client_(content_restriction_manager_client) {}

AwContentRestrictionNavigationThrottle::
    ~AwContentRestrictionNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
AwContentRestrictionNavigationThrottle::WillFailRequest() {
  DCHECK(tracker_);
  DCHECK(content_restriction_manager_client_);
  int64_t nav_id = navigation_handle()->GetNavigationId();
  if (!tracker_->IsNavigationBlocked(nav_id)) {
    return PROCEED;
  }

  // The request was blocked by content restriction. Show the interstitial.
  std::unique_ptr<security_interstitials::SecurityInterstitialPage>
      blocking_page = AwContentRestrictionBlockingPage::CreateBlockingPage(
          navigation_handle()->GetWebContents(), navigation_handle()->GetURL(),
          content_restriction_manager_client_);
  std::string error_page_content = blocking_page->GetHTMLContents();
  security_interstitials::SecurityInterstitialTabHelper::AssociateBlockingPage(
      navigation_handle(), std::move(blocking_page));
  tracker_->ClearNavigationBlocked(nav_id);

  return content::NavigationThrottle::ThrottleCheckResult(
      CANCEL, net::ERR_BLOCKED_BY_CLIENT, error_page_content);
}

const char* AwContentRestrictionNavigationThrottle::GetNameForLogging() {
  return "AwContentRestrictionNavigationThrottle";
}

}  // namespace android_webview
