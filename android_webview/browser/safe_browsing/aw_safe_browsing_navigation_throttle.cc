// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/safe_browsing/aw_safe_browsing_navigation_throttle.h"

#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_blocking_page.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_ui_manager.h"
#include "base/memory/ptr_util.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/browser/navigation_handle.h"

namespace android_webview {

AwSafeBrowsingNavigationThrottle::AwSafeBrowsingNavigationThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {}

const char* AwSafeBrowsingNavigationThrottle::GetNameForLogging() {
  return "AwSafeBrowsingNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
AwSafeBrowsingNavigationThrottle::WillFailRequest() {
  AwSafeBrowsingUIManager* manager =
      AwBrowserProcess::GetInstance()->GetSafeBrowsingUIManager();
  if (manager) {
    security_interstitials::UnsafeResource resource;
    content::NavigationHandle* handle = navigation_handle();
    if (manager->PopUnsafeResourceForURL(handle->GetURL(), &resource)) {
      AwSafeBrowsingBlockingPage* blocking_page =
          AwSafeBrowsingBlockingPage::CreateBlockingPage(
              manager, handle->GetWebContents(), handle->GetURL(), resource);
      std::string error_page_content = blocking_page->GetHTMLContents();
      security_interstitials::SecurityInterstitialTabHelper::
          AssociateBlockingPage(handle->GetWebContents(),
                                handle->GetNavigationId(),
                                base::WrapUnique(blocking_page));
      return content::NavigationThrottle::ThrottleCheckResult(
          CANCEL, net::ERR_BLOCKED_BY_CLIENT, error_page_content);
    }
  }
  return content::NavigationThrottle::PROCEED;
}

}  // namespace android_webview
