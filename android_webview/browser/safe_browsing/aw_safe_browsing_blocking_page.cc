// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/safe_browsing/aw_safe_browsing_blocking_page.h"

#include <memory>
#include <utility>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_contents_client_bridge.h"
#include "android_webview/browser/network_service/aw_web_resource_request.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_ui_manager.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/threat_details.h"
#include "components/safe_browsing/content/browser/triggers/trigger_manager.h"
#include "components/safe_browsing/content/browser/unsafe_resource_util.h"
#include "components/safe_browsing/content/browser/web_contents_key.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "components/security_interstitials/core/safe_browsing_quiet_error_ui.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"

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
    ErrorUiType errorUiType,
    std::unique_ptr<AwWebResourceRequest> resource_request)
    : BaseBlockingPage(ui_manager,
                       web_contents,
                       main_frame_url,
                       unsafe_resources,
                       std::move(controller_client),
                       display_options),
      threat_details_in_progress_(false),
      resource_request_(std::move(resource_request)) {
  if (errorUiType == ErrorUiType::QUIET_SMALL ||
      errorUiType == ErrorUiType::QUIET_GIANT) {
    set_sb_error_ui(std::make_unique<SafeBrowsingQuietErrorUI>(
        unsafe_resources[0].url, GetInterstitialReason(unsafe_resources),
        display_options, ui_manager->app_locale(),
        base::Time::NowFromSystemTime(), controller(),
        errorUiType == ErrorUiType::QUIET_GIANT));
  }

  if (unsafe_resources.size() == 1 &&
      ShouldReportThreatDetails(unsafe_resources[0].threat_type)) {
    AwBrowserContext* aw_browser_context =
        AwBrowserContext::FromWebContents(web_contents);
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
        aw_browser_context->GetDefaultStoragePartition()
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
                /*referrer_chain_provider*/ nullptr,
                sb_error_ui()->get_error_display_options());
  }
  warning_shown_ts_ = base::Time::Now().InMillisecondsSinceUnixEpoch();
}

AwSafeBrowsingBlockingPage* AwSafeBrowsingBlockingPage::CreateBlockingPage(
    AwSafeBrowsingUIManager* ui_manager,
    content::WebContents* web_contents,
    const GURL& main_frame_url,
    const UnsafeResource& unsafe_resource,
    std::unique_ptr<AwWebResourceRequest> resource_request,
    std::optional<base::TimeTicks> blocked_page_shown_timestamp) {
  // Log the threat type that triggers the safe browsing blocking page.
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.BlockingPage.ThreatType",
                            unsafe_resource.threat_type);
  const UnsafeResourceList unsafe_resources{unsafe_resource};
  AwBrowserContext* browser_context =
      AwBrowserContext::FromWebContents(web_contents);
  PrefService* pref_service = browser_context->GetPrefService();
  // TODO(crbug.com/40723201): Set is_enhanced_protection_message_enabled once
  // enhanced protection is supported on aw.
  BaseSafeBrowsingErrorUI::SBErrorDisplayOptions display_options =
      BaseSafeBrowsingErrorUI::SBErrorDisplayOptions(
          IsMainPageLoadPending(unsafe_resources),
          safe_browsing::IsExtendedReportingOptInAllowed(*pref_service),
          browser_context->IsOffTheRecord(),
          safe_browsing::IsExtendedReportingEnabledBypassDeprecationFlag(
              *pref_service),
          safe_browsing::IsExtendedReportingPolicyManaged(*pref_service),
          safe_browsing::IsEnhancedProtectionEnabled(*pref_service),
          pref_service->GetBoolean(::prefs::kSafeBrowsingProceedAnywayDisabled),
          false,  // should_open_links_in_new_tab
          false,  // always_show_back_to_safety
          false,  // is_enhanced_protection_message_enabled
          safe_browsing::IsSafeBrowsingPolicyManaged(*pref_service),
          "cpn_safe_browsing_wv");  // help_center_article_link

  ErrorUiType errorType =
      static_cast<ErrorUiType>(ui_manager->GetErrorUiType(web_contents));

  // TODO(carlosil): This logic is necessary to support committed and non
  // committed interstitials, it can be cleaned up when removing non-committed
  // interstitials.
  content::NavigationEntry* entry =
      safe_browsing::unsafe_resource_util::GetNavigationEntryForResource(
          unsafe_resource);
  GURL url =
      (main_frame_url.is_empty() && entry) ? entry->GetURL() : main_frame_url;

  // TODO(crbug.com/40723201): Set settings_page_helper once enhanced protection
  // is supported on aw.
  return new AwSafeBrowsingBlockingPage(
      ui_manager, web_contents, url, unsafe_resources,
      CreateControllerClient(web_contents, unsafe_resources, ui_manager,
                             pref_service, /*settings_page_helper*/ nullptr,
                             blocked_page_shown_timestamp),
      display_options, errorType, std::move(resource_request));
}

AwSafeBrowsingBlockingPage::~AwSafeBrowsingBlockingPage() {}

void AwSafeBrowsingBlockingPage::CreatedPostCommitErrorPageNavigation(
    content::NavigationHandle* error_page_navigation_handle) {
  DCHECK(!resource_request_);
  resource_request_ = std::make_unique<AwWebResourceRequest>(
      error_page_navigation_handle->GetURL().spec(),
      error_page_navigation_handle->IsPost() ? "POST" : "GET",
      error_page_navigation_handle->IsInPrimaryMainFrame(),
      error_page_navigation_handle->HasUserGesture(),
      error_page_navigation_handle->GetRequestHeaders());
  resource_request_->is_renderer_initiated =
      error_page_navigation_handle->IsRendererInitiated();
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
  auto result =
      AwBrowserProcess::GetInstance()
          ->GetSafeBrowsingTriggerManager()
          ->FinishCollectingThreatDetails(
              safe_browsing::TriggerType::SECURITY_INTERSTITIAL,
              safe_browsing::GetWebContentsKey(web_contents()), delay,
              did_proceed, num_visits,
              sb_error_ui()->get_error_display_options(), warning_shown_ts_);
  bool report_sent = result.IsReportSent();

  if (report_sent) {
    controller()->metrics_helper()->RecordUserInteraction(
        security_interstitials::MetricsHelper::EXTENDED_REPORTING_IS_ENABLED);
  }
}

void AwSafeBrowsingBlockingPage::OnInterstitialClosing() {
  if (resource_request_ && !proceeded()) {
    AwContentsClientBridge* client =
        AwContentsClientBridge::FromWebContents(web_contents());
    // With committed interstitials, the navigation to the site is failed before
    // showing the interstitial so we omit notifications to embedders at that
    // time, and manually trigger them here.
    if (client) {
      client->OnReceivedError(*resource_request_,
                              safe_browsing::kNetErrorCodeForSafeBrowsing, true,
                              false);
    }
  }
  safe_browsing::BaseBlockingPage::OnInterstitialClosing();
}

}  // namespace android_webview
