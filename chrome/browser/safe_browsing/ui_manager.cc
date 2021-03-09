// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/ui_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/interstitials/enterprise_util.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_subresource_tab_helper.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/threat_details.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/ping_manager.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/process_manager.h"
#endif
#include "ipc/ipc_message.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::WebContents;
using safe_browsing::HitReport;
using safe_browsing::SBThreatType;

namespace safe_browsing {

SafeBrowsingUIManager::SafeBrowsingUIManager(
    const scoped_refptr<SafeBrowsingService>& service)
    : BaseUIManager(), sb_service_(service) {}

SafeBrowsingUIManager::~SafeBrowsingUIManager() {}

void SafeBrowsingUIManager::Stop(bool shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (shutdown)
    sb_service_ = nullptr;
}

void SafeBrowsingUIManager::CreateAndSendHitReport(
    const UnsafeResource& resource) {
  WebContents* web_contents = resource.web_contents_getter.Run();
  DCHECK(web_contents);
  HitReport hit_report;
  hit_report.malicious_url = resource.url;
  hit_report.is_subresource = resource.is_subresource;
  hit_report.threat_type = resource.threat_type;
  hit_report.threat_source = resource.threat_source;
  hit_report.population_id = resource.threat_metadata.population_id;

  NavigationEntry* entry = GetNavigationEntryForResource(resource);
  if (entry) {
    hit_report.page_url = entry->GetURL();
    hit_report.referrer_url = entry->GetReferrer().url;
  }

  // When the malicious url is on the main frame, and resource.original_url
  // is not the same as the resource.url, that means we have a redirect from
  // resource.original_url to resource.url.
  // Also, at this point, page_url points to the _previous_ page that we
  // were on. We replace page_url with resource.original_url and referrer
  // with page_url.
  if (!resource.is_subresource && !resource.original_url.is_empty() &&
      resource.original_url != resource.url) {
    hit_report.referrer_url = hit_report.page_url;
    hit_report.page_url = resource.original_url;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  hit_report.extended_reporting_level =
      profile ? GetExtendedReportingLevel(*profile->GetPrefs())
              : SBER_LEVEL_OFF;
  hit_report.is_enhanced_protection =
      IsEnhancedProtectionEnabled(*profile->GetPrefs());
  hit_report.is_metrics_reporting_active =
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();

  MaybeReportSafeBrowsingHit(hit_report, web_contents);

  for (Observer& observer : observer_list_)
    observer.OnSafeBrowsingHit(resource);
}

// static
void SafeBrowsingUIManager::StartDisplayingBlockingPage(
    scoped_refptr<SafeBrowsingUIManager> ui_manager,
    const security_interstitials::UnsafeResource& resource) {
  content::WebContents* web_contents = resource.web_contents_getter.Run();
  prerender::NoStatePrefetchContents* no_state_prefetch_contents =
      web_contents
          ? prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
                web_contents)
          : nullptr;
  if (!web_contents || no_state_prefetch_contents) {
    if (no_state_prefetch_contents) {
      no_state_prefetch_contents->Destroy(
          prerender::FINAL_STATUS_SAFE_BROWSING);
    }
    // Tab is gone or it's being prerendered.
    resource.DispatchCallback(FROM_HERE, false /*proceed*/,
                              false /*showed_interstitial*/);
    return;
  }

// We don't show interstitials for extension triggered SB errors, since they
// might not be visible, and cause the extension to hang. The request is just
// cancelled instead.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ProcessManager* extension_manager =
      extensions::ProcessManager::Get(web_contents->GetBrowserContext());
  if (extension_manager) {
    extensions::ExtensionHost* extension_host =
        extension_manager->GetExtensionHostForRenderFrameHost(
            web_contents->GetMainFrame());
    if (extension_host) {
      resource.DispatchCallback(FROM_HERE, false /* proceed */,
                                false /* showed_interstitial */);
      return;
    }
  }
#endif

  // With committed interstitials, if this is a main frame load, we need to
  // get the navigation URL and referrer URL from the navigation entry now,
  // since they are required for threat reporting, and the entry will be
  // destroyed once the request is failed.
  if (resource.IsMainPageLoadBlocked()) {
    content::NavigationEntry* entry =
        web_contents->GetController().GetPendingEntry();
    if (entry) {
      security_interstitials::UnsafeResource resource_copy(resource);
      resource_copy.navigation_url = entry->GetURL();
      resource_copy.referrer_url = entry->GetReferrer().url;
      ui_manager->DisplayBlockingPage(resource_copy);
      return;
    }
  }
  ui_manager->DisplayBlockingPage(resource);
}

// static
bool SafeBrowsingUIManager::ShouldSendHitReport(const HitReport& hit_report,
                                                WebContents* web_contents) {
  return web_contents &&
         hit_report.extended_reporting_level != SBER_LEVEL_OFF &&
         !web_contents->GetBrowserContext()->IsOffTheRecord();
}

// A SafeBrowsing hit is sent after a blocking page for malware/phishing
// or after the warning dialog for download urls, only for
// extended-reporting users.
void SafeBrowsingUIManager::MaybeReportSafeBrowsingHit(
    const HitReport& hit_report,
    WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Send report if user opted-in to extended reporting and is not in
  //  incognito mode.
  if (!ShouldSendHitReport(hit_report, web_contents))
    return;

  // The service may delete the ping manager (i.e. when user disabling service,
  // etc). This happens on the IO thread.
  if (!sb_service_ || !sb_service_->ping_manager())
    return;

  DVLOG(1) << "ReportSafeBrowsingHit: " << hit_report.malicious_url << " "
           << hit_report.page_url << " " << hit_report.referrer_url << " "
           << hit_report.is_subresource << " " << hit_report.threat_type;
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  sb_service_->ping_manager()->ReportSafeBrowsingHit(
      sb_service_->GetURLLoaderFactory(profile), hit_report);
}

// Static.
void SafeBrowsingUIManager::CreateAllowlistForTesting(
    content::WebContents* web_contents) {
  EnsureAllowlistCreated(web_contents);
}

void SafeBrowsingUIManager::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_list_.AddObserver(observer);
}

void SafeBrowsingUIManager::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_list_.RemoveObserver(observer);
}

const std::string SafeBrowsingUIManager::app_locale() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_browser_process->GetApplicationLocale();
}

history::HistoryService* SafeBrowsingUIManager::history_service(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return HistoryServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()),
      ServiceAccessType::EXPLICIT_ACCESS);
}

const GURL SafeBrowsingUIManager::default_safe_page() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GURL(chrome::kChromeUINewTabURL);
}

// If the user had opted-in to send ThreatDetails, this gets called
// when the report is ready.
void SafeBrowsingUIManager::SendSerializedThreatDetails(
    content::BrowserContext* browser_context,
    const std::string& serialized) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The service may delete the ping manager (i.e. when user disabling service,
  // etc). This happens on the IO thread.
  if (sb_service_.get() == NULL || sb_service_->ping_manager() == NULL)
    return;

  if (!serialized.empty()) {
    DVLOG(1) << "Sending serialized threat details.";
    sb_service_->ping_manager()->ReportThreatDetails(
        sb_service_->GetURLLoaderFactory(
            Profile::FromBrowserContext(browser_context)),
        serialized);
  }
}

void SafeBrowsingUIManager::OnBlockingPageDone(
    const std::vector<UnsafeResource>& resources,
    bool proceed,
    content::WebContents* web_contents,
    const GURL& main_frame_url,
    bool showed_interstitial) {
  BaseUIManager::OnBlockingPageDone(resources, proceed, web_contents,
                                    main_frame_url, showed_interstitial);
  if (proceed && !resources.empty()) {
    MaybeTriggerSecurityInterstitialProceededEvent(
        web_contents, main_frame_url,
        GetThreatTypeStringForInterstitial(resources[0].threat_type),
        /*net_error_code=*/0);
  }
}
// Static.
GURL SafeBrowsingUIManager::GetMainFrameAllowlistUrlForResourceForTesting(
    const security_interstitials::UnsafeResource& resource) {
  return GetMainFrameAllowlistUrlForResource(resource);
}

BaseBlockingPage* SafeBrowsingUIManager::CreateBlockingPageForSubresource(
    content::WebContents* contents,
    const GURL& blocked_url,
    const UnsafeResource& unsafe_resource) {
  SafeBrowsingSubresourceTabHelper::CreateForWebContents(contents);
  // This blocking page is only used to retrieve the HTML for the page, so we
  // set |should_trigger_reporting| to false. Reports for subresources are
  // triggered when creating the blocking page that gets associated in
  // SafeBrowsingSubresourceTabHelper.
  SafeBrowsingBlockingPage* blocking_page =
      SafeBrowsingBlockingPage::CreateBlockingPage(
          this, contents, blocked_url, unsafe_resource,
          /*should_trigger_reporting=*/false);

  // Report that we showed an interstitial.
  MaybeTriggerSecurityInterstitialShownEvent(
      contents, blocked_url,
      GetThreatTypeStringForInterstitial(unsafe_resource.threat_type),
      /*net_error_code=*/0);

  return blocking_page;
}

}  // namespace safe_browsing
