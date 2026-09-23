// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_ui_manager_delegate.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/interstitials/delayed_interstitial_reporter.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/interstitials/enterprise_util.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_ping_manager_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/process_manager.h"  // nogncheck
#endif

namespace safe_browsing {

namespace {

// Waits for the destination page to finish loading after an interstitial
// bypass before sampling its title.
void ReportTitleAfterInterstitialBypass(
    content::WebContents* web_contents,
    std::string uma_suffix,
    enterprise_data_protection::DelayedInterstitialReporter::TitleCallback
        callback) {
  if (!base::FeatureList::IsEnabled(
          enterprise_data_protection::kEnterpriseTabTitleReporting)) {
    std::move(callback).Run(std::string());
    return;
  }
  enterprise_data_protection::DelayedInterstitialReporter::Start(
      web_contents, std::move(callback), /*is_bypassing_interstitial=*/true,
      std::move(uma_suffix));
}

}  // namespace

ChromeSafeBrowsingUIManagerDelegate::ChromeSafeBrowsingUIManagerDelegate() =
    default;
ChromeSafeBrowsingUIManagerDelegate::~ChromeSafeBrowsingUIManagerDelegate() =
    default;

std::string ChromeSafeBrowsingUIManagerDelegate::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

void ChromeSafeBrowsingUIManagerDelegate::
    TriggerSecurityInterstitialShownExtensionEventIfDesired(
        content::WebContents* web_contents,
        const GURL& page_url,
        const std::string& reason,
        int net_error_code) {
  if (!web_contents) {
    return;
  }
  MaybeTriggerSecurityInterstitialShownEvent(web_contents, page_url, reason,
                                             net_error_code,
                                             /*tab_title=*/std::string());
}

void ChromeSafeBrowsingUIManagerDelegate::
    TriggerSecurityInterstitialProceededExtensionEventIfDesired(
        content::WebContents* web_contents,
        const GURL& page_url,
        const std::string& reason,
        int net_error_code) {
  if (!web_contents) {
    return;
  }
  ReportTitleAfterInterstitialBypass(
      web_contents, "SafeBrowsing",
      base::BindOnce(
          [](base::WeakPtr<content::WebContents> web_contents, GURL page_url,
             std::string reason, int net_error_code,
             const std::string& tab_title) {
            if (web_contents) {
              MaybeTriggerSecurityInterstitialProceededEvent(
                  web_contents.get(), page_url, reason, net_error_code,
                  tab_title);
            }
          },
          web_contents->GetWeakPtr(), page_url, reason, net_error_code));
}

void ChromeSafeBrowsingUIManagerDelegate::
    TriggerUrlFilteringInterstitialExtensionEventIfDesired(
        content::WebContents* web_contents,
        const GURL& page_url,
        const std::string& threat_type,
        safe_browsing::RTLookupResponse rt_lookup_response,
        bool is_bypassing_interstitial) {
  if (!web_contents) {
    return;
  }
  if (!is_bypassing_interstitial) {
    MaybeTriggerUrlFilteringInterstitialEvent(web_contents, page_url,
                                              threat_type, rt_lookup_response,
                                              /*tab_title=*/std::string());
    return;
  }
  ReportTitleAfterInterstitialBypass(
      web_contents, "UrlFiltering",
      base::BindOnce(
          [](base::WeakPtr<content::WebContents> web_contents, GURL page_url,
             std::string threat_type,
             safe_browsing::RTLookupResponse rt_lookup_response,
             const std::string& tab_title) {
            if (web_contents) {
              MaybeTriggerUrlFilteringInterstitialEvent(
                  web_contents.get(), std::move(page_url), threat_type,
                  std::move(rt_lookup_response), tab_title);
            }
          },
          web_contents->GetWeakPtr(), page_url, threat_type,
          std::move(rt_lookup_response)));
}

prerender::NoStatePrefetchContents*
ChromeSafeBrowsingUIManagerDelegate::GetNoStatePrefetchContentsIfExists(
    content::WebContents* web_contents) {
  return prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
      web_contents);
}

bool ChromeSafeBrowsingUIManagerDelegate::IsHostingExtension(
    content::WebContents* web_contents) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extensions::ProcessManager* extension_manager =
      extensions::ProcessManager::Get(web_contents->GetBrowserContext());
  if (!extension_manager)
    return false;

  extensions::ExtensionHost* extension_host =
      extension_manager->GetBackgroundHostForRenderFrameHost(
          web_contents->GetPrimaryMainFrame());
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

PingManager* ChromeSafeBrowsingUIManagerDelegate::GetPingManager(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return ChromePingManagerFactory::GetForBrowserContext(browser_context);
}

bool ChromeSafeBrowsingUIManagerDelegate::IsMetricsAndCrashReportingEnabled() {
  return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
}

}  // namespace safe_browsing
