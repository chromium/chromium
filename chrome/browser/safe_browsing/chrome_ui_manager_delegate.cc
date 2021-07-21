// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_ui_manager_delegate.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/interstitials/enterprise_util.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/process_manager.h"
#endif

namespace safe_browsing {

ChromeSafeBrowsingUIManagerDelegate::ChromeSafeBrowsingUIManagerDelegate() =
    default;
ChromeSafeBrowsingUIManagerDelegate::~ChromeSafeBrowsingUIManagerDelegate() =
    default;

const std::string& ChromeSafeBrowsingUIManagerDelegate::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

void ChromeSafeBrowsingUIManagerDelegate::
    TriggerSecurityInterstitialShownExtensionEventIfDesired(
        content::WebContents* web_contents,
        const GURL& page_url,
        const std::string& reason,
        int net_error_code) {
  MaybeTriggerSecurityInterstitialShownEvent(web_contents, page_url, reason,
                                             net_error_code);
}

void ChromeSafeBrowsingUIManagerDelegate::
    TriggerSecurityInterstitialProceededExtensionEventIfDesired(
        content::WebContents* web_contents,
        const GURL& page_url,
        const std::string& reason,
        int net_error_code) {
  MaybeTriggerSecurityInterstitialProceededEvent(web_contents, page_url, reason,
                                                 net_error_code);
}

prerender::NoStatePrefetchContents*
ChromeSafeBrowsingUIManagerDelegate::GetNoStatePrefetchContentsIfExists(
    content::WebContents* web_contents) {
  return prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
      web_contents);
}

bool ChromeSafeBrowsingUIManagerDelegate::IsHostingExtension(
    content::WebContents* web_contents) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ProcessManager* extension_manager =
      extensions::ProcessManager::Get(web_contents->GetBrowserContext());
  if (!extension_manager)
    return false;

  extensions::ExtensionHost* extension_host =
      extension_manager->GetExtensionHostForRenderFrameHost(
          web_contents->GetMainFrame());
  return extension_host != nullptr;
#else
  return false;
#endif
}

PrefService* ChromeSafeBrowsingUIManagerDelegate::GetPrefs(
    content::BrowserContext* browser_context) {
  return Profile::FromBrowserContext(browser_context)->GetPrefs();
}

history::HistoryService* ChromeSafeBrowsingUIManagerDelegate::GetHistoryService(
    content::BrowserContext* browser_context) {
  return HistoryServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context),
      ServiceAccessType::EXPLICIT_ACCESS);
}

bool ChromeSafeBrowsingUIManagerDelegate::IsMetricsAndCrashReportingEnabled() {
  return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
}

}  // namespace safe_browsing
