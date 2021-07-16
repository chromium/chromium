// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_subresource_tab_helper.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/net_errors.h"

namespace safe_browsing {

SafeBrowsingSubresourceTabHelper::~SafeBrowsingSubresourceTabHelper() {}

// static
void SafeBrowsingSubresourceTabHelper::CreateForWebContents(
    content::WebContents* web_contents,
    SafeBrowsingUIManager* manager) {
  if (FromWebContents(web_contents))
    return;

  web_contents->SetUserData(
      kSafeBrowsingSubresourceTabHelperWebContentsUserDataKey,
      std::make_unique<SafeBrowsingSubresourceTabHelper>(web_contents,
                                                         manager));
}

// static
SafeBrowsingSubresourceTabHelper*
SafeBrowsingSubresourceTabHelper::FromWebContents(
    content::WebContents* web_contents) {
  return static_cast<SafeBrowsingSubresourceTabHelper*>(
      web_contents->GetUserData(
          kSafeBrowsingSubresourceTabHelperWebContentsUserDataKey));
}

void SafeBrowsingSubresourceTabHelper::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetNetErrorCode() == net::ERR_BLOCKED_BY_CLIENT) {
    security_interstitials::UnsafeResource resource;
    if (manager_->PopUnsafeResourceForURL(navigation_handle->GetURL(),
                                          &resource)) {
      safe_browsing::SafeBrowsingBlockingPage* blocking_page =
          manager_->blocking_page_factory()->CreateSafeBrowsingPage(
              manager_, navigation_handle->GetWebContents(),
              navigation_handle->GetURL(), {resource},
              /*should_trigger_reporting=*/true);
      security_interstitials::SecurityInterstitialTabHelper::
          AssociateBlockingPage(navigation_handle->GetWebContents(),
                                navigation_handle->GetNavigationId(),
                                base::WrapUnique(blocking_page));
    }
  }
}

SafeBrowsingSubresourceTabHelper::SafeBrowsingSubresourceTabHelper(
    content::WebContents* web_contents,
    SafeBrowsingUIManager* manager)
    : WebContentsObserver(web_contents), manager_(manager) {}

const char SafeBrowsingSubresourceTabHelper::
    kSafeBrowsingSubresourceTabHelperWebContentsUserDataKey[] =
        "safe_browsing_subresource_tab_helper";

}  // namespace safe_browsing
