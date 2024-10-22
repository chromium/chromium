// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/interstitials/chrome_settings_page_helper.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "chrome/browser/ssl/https_only_mode_controller_client.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/browser/ssl/insecure_form/insecure_form_controller_client.h"
#include "chrome/browser/ssl/ssl_error_controller_client.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/security_interstitials/content/content_metrics_helper.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_WIN)
#include "base/enterprise_util.h"
#elif BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process_platform_part.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "components/security_interstitials/content/captive_portal_helper_android.h"
#include "content/public/common/referrer.h"
#include "net/android/network_library.h"
#include "ui/base/window_open_disposition.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#include "net/base/net_errors.h"
#include "net/dns/public/secure_dns_mode.h"
#endif

namespace {

enum EnterpriseManaged {
  ENTERPRISE_MANAGED_STATUS_NOT_SET,
  ENTERPRISE_MANAGED_STATUS_TRUE,
  ENTERPRISE_MANAGED_STATUS_FALSE
};

EnterpriseManaged g_is_enterprise_managed_for_testing =
    ENTERPRISE_MANAGED_STATUS_NOT_SET;

bool IsEnterpriseManaged() {
  // Return the value of the testing flag if it's set.
  if (g_is_enterprise_managed_for_testing == ENTERPRISE_MANAGED_STATUS_TRUE) {
    return true;
  }

  if (g_is_enterprise_managed_for_testing == ENTERPRISE_MANAGED_STATUS_FALSE) {
    return false;
  }

#if BUILDFLAG(IS_WIN)
  if (base::IsManagedOrEnterpriseDevice()) {
    return true;
  }
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  if (g_browser_process->platform_part()->browser_policy_connector_ash()) {
    return true;
  }
#endif  // BUILDFLAG(IS_WIN)

  return false;
}

// Opens the login page for a captive portal. Passed in to
// CaptivePortalBlockingPage to be invoked when the user has pressed the
// connect button.
void OpenLoginPage(content::WebContents* web_contents) {
#if !BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  // OpenLoginTabForWebContents() is not available on Android (the only
  // platform on which captive portal detection is not enabled). Simply open
  // the platform's portal detection URL in a new tab.
  const std::string url = security_interstitials::GetCaptivePortalServerUrl(
      base::android::AttachCurrentThread());
  content::OpenURLParams params(GURL(url), content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  web_contents->OpenURL(params, /*navigation_handle_callback=*/{});
#else
  ChromeSecurityBlockingPageFactory::OpenLoginTabForWebContents(web_contents,
                                                                true);
#endif  // !BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
}

std::unique_ptr<ContentMetricsHelper> CreateMetricsHelperAndStartRecording(
    content::WebContents* web_contents,
    const GURL& request_url,
    const std::string& metric_prefix,
    bool overridable) {
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix = metric_prefix;
  std::unique_ptr<ContentMetricsHelper> metrics_helper =
      std::make_unique<ContentMetricsHelper>(
          HistoryServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()),
              ServiceAccessType::EXPLICIT_ACCESS),
          request_url, reporting_info);
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  metrics_helper.get()->StartRecordingCaptivePortalMetrics(
      CaptivePortalServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      overridable);
#endif
  return metrics_helper;
}

std::unique_ptr<security_interstitials::SettingsPageHelper>
CreateSettingsPageHelper() {
  return security_interstitials::ChromeSettingsPageHelper::
      CreateChromeSettingsPageHelper();
}

void LogSafeBrowsingSecuritySensitiveAction(
    safe_browsing::SafeBrowsingMetricsCollector* metrics_collector) {
  if (metrics_collector) {
    metrics_collector->AddSafeBrowsingEventToPref(
        safe_browsing::SafeBrowsingMetricsCollector::EventType::
            SECURITY_SENSITIVE_SSL_INTERSTITIAL);
  }
}

}  // namespace

std::unique_ptr<SSLBlockingPage>
ChromeSecurityBlockingPageFactory::CreateSSLPage(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    const base::Time& time_triggered,
    const GURL& support_url) {
  bool overridable = SSLBlockingPage::IsOverridable(options_mask);
  std::unique_ptr<ContentMetricsHelper> metrics_helper(
      CreateMetricsHelperAndStartRecording(
          web_contents, request_url,
          overridable ? "ssl_overridable" : "ssl_nonoverridable", overridable));

  StatefulSSLHostStateDelegate* state =
      StatefulSSLHostStateDelegateFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  state->DidDisplayErrorPage(cert_error);

  LogSafeBrowsingSecuritySensitiveAction(
      safe_browsing::SafeBrowsingMetricsCollectorFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext())));

  auto controller_client = std::make_unique<SSLErrorControllerClient>(
      web_contents, ssl_info, cert_error, request_url,
      std::move(metrics_helper), CreateSettingsPageHelper());

  std::unique_ptr<SSLBlockingPage> page;

  page = std::make_unique<SSLBlockingPage>(
      web_contents, cert_error, ssl_info, request_url, options_mask,
      time_triggered, support_url, overridable,
      /*can_show_enhanced_protection_message=*/true,
      std::move(controller_client));

  return page;
}

std::unique_ptr<CaptivePortalBlockingPage>
ChromeSecurityBlockingPageFactory::CreateCaptivePortalBlockingPage(
    content::WebContents* web_contents,
    const GURL& request_url,
    const GURL& login_url,
    const net::SSLInfo& ssl_info,
    int cert_error) {
  auto page = std::make_unique<CaptivePortalBlockingPage>(
      web_contents, request_url, login_url,
      /*can_show_enhanced_protection_message=*/true, ssl_info,
      std::make_unique<SSLErrorControllerClient>(
          web_contents, ssl_info, cert_error, request_url,
          CreateMetricsHelperAndStartRecording(web_contents, request_url,
                                               "captive_portal", false),
          CreateSettingsPageHelper()),
      base::BindRepeating(&OpenLoginPage));

  return page;
}

std::unique_ptr<BadClockBlockingPage>
ChromeSecurityBlockingPageFactory::CreateBadClockBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    const base::Time& time_triggered,
    ssl_errors::ClockState clock_state) {
  auto page = std::make_unique<BadClockBlockingPage>(
      web_contents, cert_error, ssl_info, request_url, time_triggered,
      /*can_show_enhanced_protection_message=*/true, clock_state,
      std::make_unique<SSLErrorControllerClient>(
          web_contents, ssl_info, cert_error, request_url,
          CreateMetricsHelperAndStartRecording(web_contents, request_url,
                                               "bad_clock", false),
          CreateSettingsPageHelper()));

  return page;
}

std::unique_ptr<MITMSoftwareBlockingPage>
ChromeSecurityBlockingPageFactory::CreateMITMSoftwareBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const GURL& request_url,
    const net::SSLInfo& ssl_info,
    const std::string& mitm_software_name) {
  LogSafeBrowsingSecuritySensitiveAction(
      safe_browsing::SafeBrowsingMetricsCollectorFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext())));

  auto page = std::make_unique<MITMSoftwareBlockingPage>(
      web_contents, cert_error, request_url,
      /*can_show_enhanced_protection_message=*/true, ssl_info,
      mitm_software_name, IsEnterpriseManaged(),
      std::make_unique<SSLErrorControllerClient>(
          web_contents, ssl_info, cert_error, request_url,
          CreateMetricsHelperAndStartRecording(web_contents, request_url,
                                               "mitm_software", false),
          CreateSettingsPageHelper()));

  return page;
}

std::unique_ptr<BlockedInterceptionBlockingPage>
ChromeSecurityBlockingPageFactory::CreateBlockedInterceptionBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const GURL& request_url,
    const net::SSLInfo& ssl_info) {
  LogSafeBrowsingSecuritySensitiveAction(
      safe_browsing::SafeBrowsingMetricsCollectorFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext())));

  auto page = std::make_unique<BlockedInterceptionBlockingPage>(
      web_contents, cert_error, request_url,
      /*can_show_enhanced_protection_message=*/true, ssl_info,
      std::make_unique<SSLErrorControllerClient>(
          web_contents, ssl_info, cert_error, request_url,
          CreateMetricsHelperAndStartRecording(web_contents, request_url,
                                               "blocked_interception", false),
          CreateSettingsPageHelper()));

  return page;
}

std::unique_ptr<security_interstitials::InsecureFormBlockingPage>
ChromeSecurityBlockingPageFactory::CreateInsecureFormBlockingPage(
    content::WebContents* web_contents,
    const GURL& request_url) {
  std::unique_ptr<InsecureFormControllerClient> client =
      std::make_unique<InsecureFormControllerClient>(web_contents, request_url);
  auto page =
      std::make_unique<security_interstitials::InsecureFormBlockingPage>(
          web_contents, request_url, std::move(client));
  return page;
}

std::unique_ptr<security_interstitials::HttpsOnlyModeBlockingPage>
ChromeSecurityBlockingPageFactory::CreateHttpsOnlyModeBlockingPage(
    content::WebContents* web_contents,
    const GURL& request_url,
    security_interstitials::https_only_mode::HttpInterstitialState
        interstitial_state) {
  std::unique_ptr<HttpsOnlyModeControllerClient> client =
      std::make_unique<HttpsOnlyModeControllerClient>(web_contents,
                                                      request_url);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  interstitial_state.enabled_by_advanced_protection =
      profile &&
      safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
          profile)
          ->IsUnderAdvancedProtection();
  // HFM interstitial with Site Engagement heuristic is only shown if the
  // feature flag is enabled, so update the relevant flag here.
  interstitial_state.enabled_by_engagement_heuristic =
      interstitial_state.enabled_by_engagement_heuristic &&
      base::FeatureList::IsEnabled(features::kHttpsFirstModeV2ForEngagedSites);
  auto page =
      std::make_unique<security_interstitials::HttpsOnlyModeBlockingPage>(
          web_contents, request_url, std::move(client), interstitial_state,
          /*use_new_interstitial=*/IsNewHttpsFirstModeInterstitialEnabled());
  return page;
}

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

// Open a login tab or popup for the captive portal login page.
void OpenLoginTab(Browser* browser,
                  captive_portal::CaptivePortalWindowType portal_type) {
  // We only end up here when a captive portal result was received, so it's safe
  // to assume profile has a captive_portal::CaptivePortalService.
  NavigateParams params(
      browser,
      CaptivePortalServiceFactory::GetForProfile(browser->profile())
          ->test_url(),
      ui::PAGE_TRANSITION_TYPED);
  WindowOpenDisposition disposition;
  switch (portal_type) {
    case captive_portal::CaptivePortalWindowType::kPopup:
      disposition = WindowOpenDisposition::NEW_POPUP;
      break;
    case captive_portal::CaptivePortalWindowType::kTab:
      disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
      break;
    default:
      NOTREACHED() << "Invalid captive portal window type";
  }
  params.captive_portal_window_type = portal_type;
  params.disposition = disposition;
  Navigate(&params);

  content::WebContents* new_contents = params.navigated_or_inserted_contents;
  captive_portal::CaptivePortalTabHelper* captive_portal_tab_helper =
      captive_portal::CaptivePortalTabHelper::FromWebContents(new_contents);
  captive_portal_tab_helper->SetIsLoginTab();
}

// static
void ChromeSecurityBlockingPageFactory::OpenLoginTabForWebContents(
    content::WebContents* web_contents,
    bool focus) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);

  // If the Profile doesn't have a tabbed browser window open, do nothing.
  if (!browser)
    return;

  SecureDnsConfig secure_dns_config =
      SystemNetworkContextManager::GetStubResolverConfigReader()
          ->GetSecureDnsConfiguration(
              false /* force_check_parental_controls */);

  // If the DNS mode is SECURE, captive portal login tabs should be opened in
  // new popup windows where secure DNS will be disabled.
  if (secure_dns_config.mode() == net::SecureDnsMode::kSecure) {
    // If there is already a captive portal popup window, do not create another.
    for (auto* contents : AllTabContentses()) {
      captive_portal::CaptivePortalTabHelper* captive_portal_tab_helper =
          captive_portal::CaptivePortalTabHelper::FromWebContents(contents);
      if (captive_portal_tab_helper->IsLoginTab()) {
        Browser* browser_with_login_tab = chrome::FindBrowserWithTab(contents);
        browser_with_login_tab->window()->Show();
        browser_with_login_tab->tab_strip_model()->ActivateTabAt(
            browser_with_login_tab->tab_strip_model()->GetIndexOfWebContents(
                contents));
        return;
      }
    }

    // Otherwise, create a captive portal popup window.
    OpenLoginTab(browser, captive_portal::CaptivePortalWindowType::kPopup);
    return;
  }

  // Check if the Profile's topmost browser window already has a login tab.
  // If so, do nothing.
  // TODO(mmenke):  Consider focusing that tab, at least if this is the tab
  //                helper for the currently active tab for the profile.
  for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
    content::WebContents* contents =
        browser->tab_strip_model()->GetWebContentsAt(i);
    captive_portal::CaptivePortalTabHelper* captive_portal_tab_helper =
        captive_portal::CaptivePortalTabHelper::FromWebContents(contents);
    if (captive_portal_tab_helper->IsLoginTab()) {
      if (focus)
        browser->tab_strip_model()->ActivateTabAt(i);
      return;
    }
  }

  // Otherwise, open a login tab.
  OpenLoginTab(browser, captive_portal::CaptivePortalWindowType::kTab);
}

#endif  // BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

void ChromeSecurityBlockingPageFactory::SetEnterpriseManagedForTesting(
    bool enterprise_managed) {
  if (enterprise_managed) {
    g_is_enterprise_managed_for_testing = ENTERPRISE_MANAGED_STATUS_TRUE;
  } else {
    g_is_enterprise_managed_for_testing = ENTERPRISE_MANAGED_STATUS_FALSE;
  }
}
