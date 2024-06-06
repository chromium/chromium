// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_SAFE_BROWSING_BLOCKING_PAGE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_SAFE_BROWSING_BLOCKING_PAGE_FACTORY_H_

#if defined(UNIT_TEST)
#include "components/content_settings/core/browser/host_content_settings_map.h"
#endif
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page_factory.h"

namespace safe_browsing {

#if defined(UNIT_TEST)
// If the user bypassed a phishing interstitial and the url is valid, set the
// REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS setting value to ignore future
// autorevocation.
void MaybeIgnoreAbusiveNotificationAutoRevocation(
    scoped_refptr<HostContentSettingsMap> hcsm,
    GURL url,
    bool did_proceed,
    SBThreatType threat_type);
#endif

// The default SafeBrowsingBlockingPageFactory for //chrome.
class ChromeSafeBrowsingBlockingPageFactory
    : public SafeBrowsingBlockingPageFactory {
 public:
  SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources,
      bool should_trigger_reporting,
      std::optional<base::TimeTicks> blocked_page_shown_timestamp) override;

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
      const BaseUIManager* ui_manager,
      std::optional<base::TimeTicks> blocked_page_shown_timestamp);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_SAFE_BROWSING_BLOCKING_PAGE_FACTORY_H_
