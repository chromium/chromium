// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/safe_browsing/aw_safe_browsing_navigation_throttle.h"

#include <memory>

#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/network_service/aw_web_resource_request.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_blocking_page.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_ui_manager.h"
#include "base/memory/ptr_util.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/navigation_handle.h"

namespace android_webview {

using safe_browsing::ThreatSeverity;

// static
std::unique_ptr<AwSafeBrowsingNavigationThrottle>
AwSafeBrowsingNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  // Only outer-most main frames show the interstitial through the navigation
  // throttle. In other cases, the interstitial is shown via
  // BaseUIManager::DisplayBlockingPage.
  if (!handle->IsInPrimaryMainFrame() && !handle->IsInPrerenderedMainFrame())
    return nullptr;

  return base::WrapUnique(new AwSafeBrowsingNavigationThrottle(handle));
}

AwSafeBrowsingNavigationThrottle::AwSafeBrowsingNavigationThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {}

const char* AwSafeBrowsingNavigationThrottle::GetNameForLogging() {
  return "AwSafeBrowsingNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
AwSafeBrowsingNavigationThrottle::WillFailRequest() {
  // Subframes and nested frame trees will show an interstitial directly from
  // BaseUIManager::DisplayBlockingPage.
  DCHECK(navigation_handle()->IsInPrimaryMainFrame() ||
         navigation_handle()->IsInPrerenderedMainFrame());
  AwSafeBrowsingUIManager* manager =
      AwBrowserProcess::GetInstance()->GetSafeBrowsingUIManager();
  if (manager) {
    // Goes over |RedirectChain| to get the severest threat information
    security_interstitials::UnsafeResource resource;
    content::NavigationHandle* handle = navigation_handle();
    ThreatSeverity severity =
        manager->GetSeverestThreatForNavigation(handle, resource);

    // Unsafe resource will show a blocking page
    if (severity != std::numeric_limits<ThreatSeverity>::max() &&
        resource.threat_type !=
            safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE) {
      std::unique_ptr<AwWebResourceRequest> request =
          std::make_unique<AwWebResourceRequest>(
              handle->GetURL().spec(), handle->IsPost() ? "POST" : "GET",
              /*is_in_outermost_main_frame=*/true, handle->HasUserGesture(),
              handle->GetRequestHeaders());
      request->is_renderer_initiated = handle->IsRendererInitiated();
      // blocked_page_shown_timestamp is set to nullopt because this blocking
      // page is triggered through navigation throttle, so the blocked page is
      // never shown.
      AwSafeBrowsingBlockingPage* blocking_page =
          AwSafeBrowsingBlockingPage::CreateBlockingPage(
              manager, handle->GetWebContents(), handle->GetURL(), resource,
              std::move(request),
              /*blocked_page_shown_timestamp=*/std::nullopt);
      std::string error_page_content = blocking_page->GetHTMLContents();
      security_interstitials::SecurityInterstitialTabHelper::
          AssociateBlockingPage(handle, base::WrapUnique(blocking_page));
      return content::NavigationThrottle::ThrottleCheckResult(
          CANCEL, net::ERR_BLOCKED_BY_CLIENT, error_page_content);
    }
  }
  return content::NavigationThrottle::PROCEED;
}

}  // namespace android_webview
