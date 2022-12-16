// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_UI_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_UI_MANAGER_DELEGATE_H_

#include "components/safe_browsing/content/browser/ui_manager.h"

namespace safe_browsing {

// Provides embedder-specific logic for SafeBrowsingUIManager.
class ChromeSafeBrowsingUIManagerDelegate
    : public SafeBrowsingUIManager::Delegate {
 public:
  ChromeSafeBrowsingUIManagerDelegate();
  ~ChromeSafeBrowsingUIManagerDelegate() override;

  ChromeSafeBrowsingUIManagerDelegate(
      const ChromeSafeBrowsingUIManagerDelegate&) = delete;
  ChromeSafeBrowsingUIManagerDelegate& operator=(
      const ChromeSafeBrowsingUIManagerDelegate&) = delete;

  // SafeBrowsingUIManager::Delegate:
  std::string GetApplicationLocale() override;
  void TriggerSecurityInterstitialShownExtensionEventIfDesired(
      content::WebContents* web_contents,
      const GURL& page_url,
      const std::string& reason,
      int net_error_code) override;
  void TriggerSecurityInterstitialProceededExtensionEventIfDesired(
      content::WebContents* web_contents,
      const GURL& page_url,
      const std::string& reason,
      int net_error_code) override;
#if !BUILDFLAG(IS_ANDROID)
  void TriggerUrlFilteringInterstitialExtensionEventIfDesired(
      content::WebContents* web_contents,
      const GURL& page_url,
      const std::string& threat_type,
      safe_browsing::RTLookupResponse rt_lookup_response) override;
#endif
  prerender::NoStatePrefetchContents* GetNoStatePrefetchContentsIfExists(
      content::WebContents* web_contents) override;
  bool IsHostingExtension(content::WebContents* web_contents) override;
  PrefService* GetPrefs(content::BrowserContext* browser_context) override;
  history::HistoryService* GetHistoryService(
      content::BrowserContext* browser_context) override;
  PingManager* GetPingManager(
      content::BrowserContext* browser_context) override;
  bool IsMetricsAndCrashReportingEnabled() override;
  bool IsSendingOfHitReportsEnabled() override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_UI_MANAGER_DELEGATE_H_
