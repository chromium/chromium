// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/safe_browsing/aw_safe_browsing_subresource_helper.h"

#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_blocking_page.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_ui_manager.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/net_errors.h"

namespace android_webview {

AwSafeBrowsingSubresourceHelper::~AwSafeBrowsingSubresourceHelper() {}

void AwSafeBrowsingSubresourceHelper::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetNetErrorCode() == net::ERR_BLOCKED_BY_CLIENT) {
    AwSafeBrowsingUIManager* manager =
        AwBrowserProcess::GetInstance()->GetSafeBrowsingUIManager();
    if (!manager)
      return;
    security_interstitials::UnsafeResource resource;
    if (manager->PopUnsafeResourceForURL(navigation_handle->GetURL(),
                                         &resource)) {
      AwSafeBrowsingBlockingPage* blocking_page =
          AwSafeBrowsingBlockingPage::CreateBlockingPage(
              manager, navigation_handle->GetWebContents(),
              navigation_handle->GetURL(), resource);
      security_interstitials::SecurityInterstitialTabHelper::
          AssociateBlockingPage(navigation_handle->GetWebContents(),
                                navigation_handle->GetNavigationId(),
                                base::WrapUnique(blocking_page));
    }
  }
}

AwSafeBrowsingSubresourceHelper::AwSafeBrowsingSubresourceHelper(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AwSafeBrowsingSubresourceHelper)

}  // namespace android_webview
