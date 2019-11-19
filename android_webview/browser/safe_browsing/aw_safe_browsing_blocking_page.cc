// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/safe_browsing/aw_safe_browsing_blocking_page.h"

#include <memory>
#include <utility>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_ui_manager.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/browser/threat_details.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/triggers/trigger_manager.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "components/security_interstitials/core/safe_browsing_quiet_error_ui.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
using content::InterstitialPage;
using content::WebContents;
using security_interstitials::BaseSafeBrowsingErrorUI;
using security_interstitials::SafeBrowsingQuietErrorUI;
using security_interstitials::SecurityInterstitialControllerClient;

namespace android_webview {

AwSafeBrowsingBlockingPage::AwSafeBrowsingBlockingPage(
    AwSafeBrowsingUIManager* ui_manager,
    WebContents* web_contents,
    const GURL& main_frame_url,
    const UnsafeResourceList& unsafe_resources,
    std::unique_ptr<SecurityInterstitialControllerClient> controller_client,
    const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options,
    ErrorUiType errorUiType)
    : BaseBlockingPage(ui_manager,
                       web_contents,
                       main_frame_url,
                       unsafe_resources,
                       std::move(controller_client),
                       display_options),
      threat_details_in_progress_(false) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.Interstitial.Type", errorUiType,
                            ErrorUiType::COUNT);
  if (errorUiType == ErrorUiType::QUIET_SMALL ||
      errorUiType == ErrorUiType::QUIET_GIANT) {
    set_sb_error_ui(std::make_unique<SafeBrowsingQuietErrorUI>(
        unsafe_resources[0].url, main_frame_url,
        GetInterstitialReason(unsafe_resources), display_options,
        ui_manager->app_locale(), base::Time::NowFromSystemTime(), controller(),
        errorUiType == ErrorUiType::QUIET_GIANT));
  }

  if (unsafe_resources.size() == 1 &&
      ShouldReportThreatDetails(unsafe_resources[0].threat_type)) {
    AwBrowserContext* aw_browser_context =
        AwBrowserContext::FromWebContents(web_contents);
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
        content::BrowserContext::GetDefaultStoragePartition(aw_browser_context)
            ->GetURLLoaderFactoryForBrowserProcess();
    // TODO(timvolodine): create a proper history service; currently the
    // HistoryServiceFactory lives in the chrome/ layer and relies on Profile
    // which we don't have in Android WebView (crbug.com/731744).
    threat_details_in_progress_ =
        AwBrowserProcess::GetInstance()
            ->GetSafeBrowsingTriggerManager()
            ->StartCollectingThreatDetails(
                safe_browsing::TriggerType::SECURITY_INTERSTITIAL, web_contents,
                unsafe_resources[0], url_loader_factory,
                /*history_service*/ nullptr,
                sb_error_ui()->get_error_display_options());
  }
}

// static
void AwSafeBrowsingBlockingPage::ShowBlockingPage(
    AwSafeBrowsingUIManager* ui_manager,
    const UnsafeResource& unsafe_resource) {
  DVLOG(1) << __func__ << " " << unsafe_resource.url.spec();
  WebContents* web_contents = unsafe_resource.web_contents_getter.Run();

  if (InterstitialPage::GetInterstitialPage(web_contents) &&
      unsafe_resource.is_subresource) {
    // This is an interstitial for a page's resource, let's queue it.
    UnsafeResourceMap* unsafe_resource_map = GetUnsafeResourcesMap();
    (*unsafe_resource_map)[web_contents].push_back(unsafe_resource);
  } else {
    // There is no interstitial currently showing, or we are about to display a
    // new one for the main frame. If there is already an interstitial, showing
    // the new one will automatically hide the old one.
    AwSafeBrowsingBlockingPage* blocking_page = CreateBlockingPage(
        ui_manager, unsafe_resource.web_contents_getter.Run(), GURL(),
        unsafe_resource);
    blocking_page->Show();
  }
}

AwSafeBrowsingBlockingPage* AwSafeBrowsingBlockingPage::CreateBlockingPage(
    AwSafeBrowsingUIManager* ui_manager,
    content::WebContents* web_contents,
    const GURL& main_frame_url,
    const UnsafeResource& unsafe_resource) {
  const UnsafeResourceList unsafe_resources{unsafe_resource};
  AwBrowserContext* browser_context =
      AwBrowserContext::FromWebContents(web_contents);
  PrefService* pref_service = browser_context->GetPrefService();
  BaseSafeBrowsingErrorUI::SBErrorDisplayOptions display_options =
      BaseSafeBrowsingErrorUI::SBErrorDisplayOptions(
          IsMainPageLoadBlocked(unsafe_resources),
          safe_browsing::IsExtendedReportingOptInAllowed(*pref_service),
          browser_context->IsOffTheRecord(),
          safe_browsing::IsExtendedReportingEnabled(*pref_service),
          safe_browsing::IsExtendedReportingPolicyManaged(*pref_service),
          pref_service->GetBoolean(::prefs::kSafeBrowsingProceedAnywayDisabled),
          false,                    // should_open_links_in_new_tab
          false,                    // always_show_back_to_safety
          "cpn_safe_browsing_wv");  // help_center_article_link

  ErrorUiType errorType =
      static_cast<ErrorUiType>(ui_manager->GetErrorUiType(web_contents));

  // TODO(carlosil): This logic is necessary to support committed and non
  // committed interstitials, it can be cleaned up when removing non-committed
  // interstitials.
  content::NavigationEntry* entry =
      unsafe_resource.GetNavigationEntryForResource();
  GURL url =
      (main_frame_url.is_empty() && entry) ? entry->GetURL() : main_frame_url;

  return new AwSafeBrowsingBlockingPage(
      ui_manager, web_contents, url, unsafe_resources,
      CreateControllerClient(web_contents, unsafe_resources, ui_manager,
                             pref_service),
      display_options, errorType);
}

void AwSafeBrowsingBlockingPage::FinishThreatDetails(
    const base::TimeDelta& delay,
    bool did_proceed,
    int num_visits) {
  // Not all interstitials collect threat details, e.g. when not opted in.
  if (!threat_details_in_progress_)
    return;

  // Finish computing threat details. TriggerManager will decide if it is safe
  // to send the report.
  bool report_sent = AwBrowserProcess::GetInstance()
                         ->GetSafeBrowsingTriggerManager()
                         ->FinishCollectingThreatDetails(
                             safe_browsing::TriggerType::SECURITY_INTERSTITIAL,
                             web_contents(), delay, did_proceed, num_visits,
                             sb_error_ui()->get_error_display_options());

  if (report_sent) {
    controller()->metrics_helper()->RecordUserInteraction(
        security_interstitials::MetricsHelper::EXTENDED_REPORTING_IS_ENABLED);
  }
}

}  // namespace android_webview
