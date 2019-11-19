// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_navigation_throttle.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/browser/navigation_handle.h"

namespace safe_browsing {

SafeBrowsingNavigationThrottle::SafeBrowsingNavigationThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {}

const char* SafeBrowsingNavigationThrottle::GetNameForLogging() {
  return "SafeBrowsingNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
SafeBrowsingNavigationThrottle::WillFailRequest() {
  SafeBrowsingService* service = g_browser_process->safe_browsing_service();
  if (!service) {
    return content::NavigationThrottle::PROCEED;
  }

  security_interstitials::UnsafeResource resource;
  content::NavigationHandle* handle = navigation_handle();
  scoped_refptr<SafeBrowsingUIManager> manager = service->ui_manager();

  if (manager->PopUnsafeResourceForURL(handle->GetURL(), &resource)) {
    SafeBrowsingBlockingPage* blocking_page =
        SafeBrowsingBlockingPage::CreateBlockingPage(
            manager.get(), handle->GetWebContents(), handle->GetURL(),
            resource);
    std::string error_page_content = blocking_page->GetHTMLContents();
    security_interstitials::SecurityInterstitialTabHelper::
        AssociateBlockingPage(handle->GetWebContents(),
                              handle->GetNavigationId(),
                              base::WrapUnique(blocking_page));

    return content::NavigationThrottle::ThrottleCheckResult(
        CANCEL, net::ERR_BLOCKED_BY_CLIENT, error_page_content);
  }

  return content::NavigationThrottle::PROCEED;
}

}  // namespace safe_browsing
