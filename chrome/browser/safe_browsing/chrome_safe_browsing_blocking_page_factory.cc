// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_safe_browsing_blocking_page_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_block_controller_client.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_block_page.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_warn_controller_client.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_warn_page.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/interstitials/chrome_settings_page_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_controller_client.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/content_metrics_helper.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "content/public/browser/web_contents.h"

namespace safe_browsing {

namespace {
const char kHelpCenterLink[] = "cpn_safe_browsing";

}  // namespace

SafeBrowsingBlockingPage*
ChromeSafeBrowsingBlockingPageFactory::CreateSafeBrowsingPage(
    BaseUIManager* ui_manager,
    content::WebContents* web_contents,
    const GURL& main_frame_url,
    const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources,
    bool should_trigger_reporting) {
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  // Create appropriate display options for this blocking page.
  PrefService* prefs = profile->GetPrefs();
  bool is_extended_reporting_opt_in_allowed =
      IsExtendedReportingOptInAllowed(*prefs);
  bool is_proceed_anyway_disabled =
      prefs->GetBoolean(prefs::kSafeBrowsingProceedAnywayDisabled);

  // Determine if any prefs need to be updated prior to showing the security
  // interstitial. This must happen before querying IsScout to populate the
  // Display Options below.
  safe_browsing::UpdatePrefsBeforeSecurityInterstitial(prefs);

  security_interstitials::BaseSafeBrowsingErrorUI::SBErrorDisplayOptions
      display_options(BaseBlockingPage::IsMainPageLoadBlocked(unsafe_resources),
                      is_extended_reporting_opt_in_allowed,
                      web_contents->GetBrowserContext()->IsOffTheRecord(),
                      IsExtendedReportingEnabled(*prefs),
                      IsExtendedReportingPolicyManaged(*prefs),
                      IsEnhancedProtectionEnabled(*prefs),
                      is_proceed_anyway_disabled,
                      true,  // should_open_links_in_new_tab
                      true,  // always_show_back_to_safety
                      true,  // is_enhanced_protection_message_enabled
                      IsSafeBrowsingPolicyManaged(*prefs), kHelpCenterLink);

  auto* trigger_manager =
      g_browser_process->safe_browsing_service()
          ? g_browser_process->safe_browsing_service()->trigger_manager()
          : nullptr;
  return new SafeBrowsingBlockingPage(
      ui_manager, web_contents, main_frame_url, unsafe_resources,
      CreateControllerClient(web_contents, unsafe_resources, ui_manager),
      display_options, should_trigger_reporting,
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext()),
      SafeBrowsingMetricsCollectorFactory::GetForProfile(profile),
      trigger_manager);
}

#if !BUILDFLAG(IS_ANDROID)
security_interstitials::SecurityInterstitialPage*
ChromeSafeBrowsingBlockingPageFactory::CreateEnterpriseWarnPage(
    BaseUIManager* ui_manager,
    content::WebContents* web_contents,
    const GURL& main_frame_url,
    const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources) {
  return new EnterpriseWarnPage(
      ui_manager, web_contents, main_frame_url, unsafe_resources,
      std::make_unique<EnterpriseWarnControllerClient>(web_contents,
                                                       main_frame_url));
}

security_interstitials::SecurityInterstitialPage*
ChromeSafeBrowsingBlockingPageFactory::CreateEnterpriseBlockPage(
    BaseUIManager* ui_manager,
    content::WebContents* web_contents,
    const GURL& main_frame_url,
    const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources) {
  return new EnterpriseBlockPage(
      web_contents, main_frame_url,
      std::make_unique<EnterpriseBlockControllerClient>(web_contents,
                                                        main_frame_url));
}
#endif

ChromeSafeBrowsingBlockingPageFactory::ChromeSafeBrowsingBlockingPageFactory() =
    default;

// static
std::unique_ptr<security_interstitials::SecurityInterstitialControllerClient>
ChromeSafeBrowsingBlockingPageFactory::CreateControllerClient(
    content::WebContents* web_contents,
    const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources,
    const BaseUIManager* ui_manager) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(profile);

  std::unique_ptr<ContentMetricsHelper> metrics_helper =
      std::make_unique<ContentMetricsHelper>(
          HistoryServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()),
              ServiceAccessType::EXPLICIT_ACCESS),
          unsafe_resources[0].url,
          BaseBlockingPage::GetReportingInfo(unsafe_resources));

  auto chrome_settings_page_helper =
      std::make_unique<security_interstitials::ChromeSettingsPageHelper>();

  return std::make_unique<ChromeControllerClient>(
      web_contents, std::move(metrics_helper), profile->GetPrefs(),
      ui_manager->app_locale(), ui_manager->default_safe_page(),
      std::move(chrome_settings_page_helper));
}

}  // namespace safe_browsing
