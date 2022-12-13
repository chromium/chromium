// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_SAFE_BROWSING_BLOCKING_PAGE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_SAFE_BROWSING_BLOCKING_PAGE_FACTORY_H_

#include "components/safe_browsing/content/browser/safe_browsing_blocking_page_factory.h"

namespace safe_browsing {

// The default SafeBrowsingBlockingPageFactory for //chrome.
class ChromeSafeBrowsingBlockingPageFactory
    : public SafeBrowsingBlockingPageFactory {
 public:
  SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources,
      bool should_trigger_reporting) override;

#if !BUILDFLAG(IS_ANDROID)
  security_interstitials::SecurityInterstitialPage* CreateEnterpriseWarnPage(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
      override;

  security_interstitials::SecurityInterstitialPage* CreateEnterpriseBlockPage(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
      override;
#endif

  ChromeSafeBrowsingBlockingPageFactory();
  ChromeSafeBrowsingBlockingPageFactory(
      const ChromeSafeBrowsingBlockingPageFactory&) = delete;
  ChromeSafeBrowsingBlockingPageFactory& operator=(
      const ChromeSafeBrowsingBlockingPageFactory&) = delete;

  // Creates a SecurityInterstitialControllerClient configured for //chrome for
  // safe browsing blocking pages.
  static std::unique_ptr<
      security_interstitials::SecurityInterstitialControllerClient>
  CreateControllerClient(
      content::WebContents* web_contents,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources,
      const BaseUIManager* ui_manager);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_SAFE_BROWSING_BLOCKING_PAGE_FACTORY_H_
