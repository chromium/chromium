// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the SafeBrowsingBlockingPage class.

#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"

#include <memory>

#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/interstitials/chrome_settings_page_helper.h"
#include "chrome/browser/interstitials/enterprise_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/safe_browsing/chrome_controller_client.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/threat_details.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/triggers/trigger_manager.h"
#include "components/security_interstitials/content/content_metrics_helper.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using content::WebContents;
using security_interstitials::BaseSafeBrowsingErrorUI;
using security_interstitials::SecurityInterstitialControllerClient;

namespace safe_browsing {

namespace {
const char kHelpCenterLink[] = "cpn_safe_browsing";
}  // namespace

// static
SafeBrowsingBlockingPageFactory* SafeBrowsingBlockingPage::factory_ = NULL;

// The default SafeBrowsingBlockingPageFactory.  Global, made a singleton so we
// don't leak it.
class SafeBrowsingBlockingPageFactoryImpl
    : public SafeBrowsingBlockingPageFactory {
 public:
  SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      BaseUIManager* ui_manager,
      WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources,
      bool should_trigger_reporting) override {
    // Create appropriate display options for this blocking page.
    PrefService* prefs =
        Profile::FromBrowserContext(web_contents->GetBrowserContext())
            ->GetPrefs();
    bool is_extended_reporting_opt_in_allowed =
        IsExtendedReportingOptInAllowed(*prefs);
    bool is_proceed_anyway_disabled =
        prefs->GetBoolean(prefs::kSafeBrowsingProceedAnywayDisabled);

    // Determine if any prefs need to be updated prior to showing the security
    // interstitial. This must happen before querying IsScout to populate the
    // Display Options below.
    safe_browsing::UpdatePrefsBeforeSecurityInterstitial(prefs);

    BaseSafeBrowsingErrorUI::SBErrorDisplayOptions display_options(
        BaseBlockingPage::IsMainPageLoadBlocked(unsafe_resources),
        is_extended_reporting_opt_in_allowed,
        web_contents->GetBrowserContext()->IsOffTheRecord(),
        IsExtendedReportingEnabled(*prefs),
        IsExtendedReportingPolicyManaged(*prefs),
        IsEnhancedProtectionEnabled(*prefs), is_proceed_anyway_disabled,
        true,  // should_open_links_in_new_tab
        true,  // always_show_back_to_safety
        IsEnhancedProtectionMessageInInterstitialsEnabled(),
        IsSafeBrowsingPolicyManaged(*prefs), kHelpCenterLink);

    return new SafeBrowsingBlockingPage(
        ui_manager, web_contents, main_frame_url, unsafe_resources,
        display_options, should_trigger_reporting);
  }

 private:
  friend struct base::LazyInstanceTraitsBase<
      SafeBrowsingBlockingPageFactoryImpl>;

  SafeBrowsingBlockingPageFactoryImpl() { }

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPageFactoryImpl);
};

static base::LazyInstance<SafeBrowsingBlockingPageFactoryImpl>::DestructorAtExit
    g_safe_browsing_blocking_page_factory_impl = LAZY_INSTANCE_INITIALIZER;

// static
const security_interstitials::SecurityInterstitialPage::TypeID
    SafeBrowsingBlockingPage::kTypeForTesting =
        &SafeBrowsingBlockingPage::kTypeForTesting;

SafeBrowsingBlockingPage::SafeBrowsingBlockingPage(
    BaseUIManager* ui_manager,
    WebContents* web_contents,
    const GURL& main_frame_url,
    const UnsafeResourceList& unsafe_resources,
    const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options,
    bool should_trigger_reporting,
    network::SharedURLLoaderFactory* url_loader_for_testing)
    : BaseBlockingPage(
          ui_manager,
          web_contents,
          main_frame_url,
          unsafe_resources,
          CreateControllerClient(web_contents, unsafe_resources, ui_manager),
          display_options),
      threat_details_in_progress_(false) {
  // Make sure the safe browsing service is available - it may not be when
  // shutting down.
  if (!g_browser_process->safe_browsing_service())
    return;

  // Start computing threat details. Trigger Manager will decide if it's safe to
  // begin collecting data at this time. The report will be sent only if the
  // user opts-in on the blocking page later.
  // If there's more than one malicious resources, it means the user clicked
  // through the first warning, so we don't prepare additional reports.
  if (unsafe_resources.size() == 1 &&
      ShouldReportThreatDetails(unsafe_resources[0].threat_type)) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
        url_loader_for_testing
            ? url_loader_for_testing
            : content::BrowserContext::GetDefaultStoragePartition(profile)
                  ->GetURLLoaderFactoryForBrowserProcess();
    if (should_trigger_reporting) {
      threat_details_in_progress_ =
          g_browser_process->safe_browsing_service()
              ->trigger_manager()
              ->StartCollectingThreatDetails(
                  TriggerType::SECURITY_INTERSTITIAL, web_contents,
                  unsafe_resources[0], url_loader_factory,
                  HistoryServiceFactory::GetForProfile(
                      profile, ServiceAccessType::EXPLICIT_ACCESS),
                  sb_error_ui()->get_error_display_options());
    }
  }
}

SafeBrowsingBlockingPage::~SafeBrowsingBlockingPage() {
}

security_interstitials::SecurityInterstitialPage::TypeID
SafeBrowsingBlockingPage::GetTypeForTesting() {
  return SafeBrowsingBlockingPage::kTypeForTesting;
}

void SafeBrowsingBlockingPage::OnInterstitialClosing() {
  // With committed interstitials OnProceed and OnDontProceed don't get
  // called, so call FinishThreatDetails from here.
  FinishThreatDetails((proceeded() ? base::TimeDelta::FromMilliseconds(
                                         threat_details_proceed_delay())
                                   : base::TimeDelta()),
                      proceeded(), controller()->metrics_helper()->NumVisits());
  if (!proceeded()) {
    OnDontProceedDone();
  }
  BaseBlockingPage::OnInterstitialClosing();
}

void SafeBrowsingBlockingPage::FinishThreatDetails(const base::TimeDelta& delay,
                                                   bool did_proceed,
                                                   int num_visits) {
  // Not all interstitials collect threat details (eg., incognito mode).
  if (!threat_details_in_progress_)
    return;

  // Make sure the safe browsing service is available - it may not be when
  // shutting down.
  if (!g_browser_process->safe_browsing_service())
    return;

  // Finish computing threat details. TriggerManager will decide if its safe to
  // send the report.
  bool report_sent = g_browser_process->safe_browsing_service()
                         ->trigger_manager()
                         ->FinishCollectingThreatDetails(
                             TriggerType::SECURITY_INTERSTITIAL, web_contents(),
                             delay, did_proceed, num_visits,
                             sb_error_ui()->get_error_display_options());

  if (report_sent) {
    controller()->metrics_helper()->RecordUserInteraction(
        security_interstitials::MetricsHelper::EXTENDED_REPORTING_IS_ENABLED);
  }
}

// static
SafeBrowsingBlockingPage* SafeBrowsingBlockingPage::CreateBlockingPage(
    BaseUIManager* ui_manager,
    WebContents* web_contents,
    const GURL& main_frame_url,
    const UnsafeResource& unsafe_resource,
    bool should_trigger_reporting) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.BlockingPage.ResourceType",
                            unsafe_resource.resource_type);
  const UnsafeResourceList resources{unsafe_resource};
  // Set up the factory if this has not been done already (tests do that
  // before this method is called).
  if (!factory_)
    factory_ = g_safe_browsing_blocking_page_factory_impl.Pointer();
  return factory_->CreateSafeBrowsingPage(ui_manager, web_contents,
                                          main_frame_url, resources,
                                          should_trigger_reporting);
}

// static
std::unique_ptr<SecurityInterstitialControllerClient>
SafeBrowsingBlockingPage::CreateControllerClient(
    WebContents* web_contents,
    const UnsafeResourceList& unsafe_resources,
    const BaseUIManager* ui_manager) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents->GetBrowserContext());
  DCHECK(profile);

  std::unique_ptr<ContentMetricsHelper> metrics_helper =
      std::make_unique<ContentMetricsHelper>(
          HistoryServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()),
              ServiceAccessType::EXPLICIT_ACCESS),
          unsafe_resources[0].url, GetReportingInfo(unsafe_resources));

  auto chrome_settings_page_helper =
      std::make_unique<security_interstitials::ChromeSettingsPageHelper>();

  return std::make_unique<ChromeControllerClient>(
      web_contents, std::move(metrics_helper), profile->GetPrefs(),
      ui_manager->app_locale(), ui_manager->default_safe_page(),
      std::move(chrome_settings_page_helper));
}

}  // namespace safe_browsing
