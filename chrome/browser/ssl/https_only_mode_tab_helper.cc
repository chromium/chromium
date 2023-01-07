// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_only_mode_tab_helper.h"

#include "components/security_interstitials/content/https_only_mode_blocking_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_handle.h"

HttpsOnlyModeTabHelper::~HttpsOnlyModeTabHelper() = default;

void HttpsOnlyModeTabHelper::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (is_timer_interstitial()) {
    set_is_timer_interstitial(false);
    std::unique_ptr<security_interstitials::HttpsOnlyModeBlockingPage>
        blocking_page = factory_->CreateHttpsOnlyModeBlockingPage(
            navigation_handle->GetWebContents(), fallback_url());
    security_interstitials::SecurityInterstitialTabHelper::
        AssociateBlockingPage(navigation_handle, std::move(blocking_page));
  }
}

HttpsOnlyModeTabHelper::HttpsOnlyModeTabHelper(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<HttpsOnlyModeTabHelper>(*web_contents) {
  factory_ = std::make_unique<ChromeSecurityBlockingPageFactory>();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HttpsOnlyModeTabHelper);
