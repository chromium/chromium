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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/browser/threat_details.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/ping_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::WebContents;
using safe_browsing::HitReport;
using safe_browsing::SBThreatType;

namespace safe_browsing {

SafeBrowsingUIManager::SafeBrowsingUIManager(
    const scoped_refptr<SafeBrowsingService>& service)
    : sb_service_(service) {}

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

  NavigationEntry* entry = resource.GetNavigationEntryForResource();
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
  hit_report.is_metrics_reporting_active =
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();

  MaybeReportSafeBrowsingHit(hit_report, web_contents);

  for (Observer& observer : observer_list_)
    observer.OnSafeBrowsingHit(resource);
}

void SafeBrowsingUIManager::ShowBlockingPageForResource(
    const UnsafeResource& resource) {
  SafeBrowsingBlockingPage::ShowBlockingPage(this, resource);
}

// static
bool SafeBrowsingUIManager::ShouldSendHitReport(
    const HitReport& hit_report,
    const WebContents* web_contents) {
  return web_contents &&
         hit_report.extended_reporting_level != SBER_LEVEL_OFF &&
         !web_contents->GetBrowserContext()->IsOffTheRecord();
}

// A SafeBrowsing hit is sent after a blocking page for malware/phishing
// or after the warning dialog for download urls, only for
// extended-reporting users.
void SafeBrowsingUIManager::MaybeReportSafeBrowsingHit(
    const HitReport& hit_report,
    const WebContents* web_contents) {
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
  sb_service_->ping_manager()->ReportSafeBrowsingHit(hit_report);
}

// Static.
void SafeBrowsingUIManager::CreateWhitelistForTesting(
    content::WebContents* web_contents) {
  EnsureWhitelistCreated(web_contents);
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
    const std::string& serialized) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The service may delete the ping manager (i.e. when user disabling service,
  // etc). This happens on the IO thread.
  if (sb_service_.get() == NULL || sb_service_->ping_manager() == NULL)
    return;

  if (!serialized.empty()) {
    DVLOG(1) << "Sending serialized threat details.";
    sb_service_->ping_manager()->ReportThreatDetails(serialized);
  }
}

void SafeBrowsingUIManager::OnBlockingPageDone(
    const std::vector<UnsafeResource>& resources,
    bool proceed,
    content::WebContents* web_contents,
    const GURL& main_frame_url) {
  BaseUIManager::OnBlockingPageDone(resources, proceed, web_contents,
                                    main_frame_url);
  if (proceed && resources.size() > 0) {
    MaybeTriggerSecurityInterstitialProceededEvent(
        web_contents, main_frame_url,
        GetThreatTypeStringForInterstitial(resources[0].threat_type),
        /*net_error_code=*/0);
  }
}
// Static.
GURL SafeBrowsingUIManager::GetMainFrameWhitelistUrlForResourceForTesting(
    const security_interstitials::UnsafeResource& resource) {
  return GetMainFrameWhitelistUrlForResource(resource);
}

}  // namespace safe_browsing
