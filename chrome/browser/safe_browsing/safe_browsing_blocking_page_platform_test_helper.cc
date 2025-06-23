// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_blocking_page_platform_test_helper.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_block_controller_client.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_block_page.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_warn_controller_client.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_warn_page.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_blocking_page_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/core/browser/db/fake_database_manager.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/test/base/ui_test_utils.h"
#else
#include "chrome/test/base/chrome_test_utils.h"
#endif

using content::BrowserThread;
using content::WebContents;
using security_interstitials::BaseSafeBrowsingErrorUI;

namespace safe_browsing {

// -------- TestSafeBrowsingBlockingPage class implementation -------- //
TestSafeBrowsingBlockingPage::TestSafeBrowsingBlockingPage(
    BaseUIManager* manager,
    WebContents* web_contents,
    const GURL& main_frame_url,
    const UnsafeResourceList& unsafe_resources,
    const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options,
    bool should_trigger_reporting,
    bool is_proceed_anyway_disabled,
    bool is_safe_browsing_surveys_enabled,
    base::OnceCallback<void(bool, SBThreatType)>
        trust_safety_sentiment_service_trigger,
    base::OnceCallback<void(bool, SBThreatType)>
        ignore_auto_revocation_notifications_trigger,
    std::optional<base::TimeTicks> blocked_page_shown_timestamp)
    : SafeBrowsingBlockingPage(
          manager,
          web_contents,
          main_frame_url,
          unsafe_resources,
          ChromeSafeBrowsingBlockingPageFactory::CreateControllerClient(
              web_contents,
              unsafe_resources,
              manager,
              blocked_page_shown_timestamp),
          display_options,
          should_trigger_reporting,
          HistoryServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()),
              ServiceAccessType::EXPLICIT_ACCESS),
          SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
              web_contents->GetBrowserContext()),
          SafeBrowsingMetricsCollectorFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext())),
          g_browser_process->safe_browsing_service()->trigger_manager(),
          is_proceed_anyway_disabled,
          is_safe_browsing_surveys_enabled,
          std::move(trust_safety_sentiment_service_trigger),
          std::move(ignore_auto_revocation_notifications_trigger),
          /*url_loader_for_testing=*/nullptr) {
  // Don't wait the whole 3 seconds for the browser test.
  SetThreatDetailsProceedDelayForTesting(100);
}
// SecurityInterstitialPage methods:
void TestSafeBrowsingBlockingPage::CommandReceived(const std::string& command) {
  SafeBrowsingBlockingPage::CommandReceived(command);
}

// -------- SafeBrowsingBlockingPageTestHelper class implementation -------- //
void SafeBrowsingBlockingPageTestHelper::MaybeWaitForAsyncChecksToComplete(
    content::WebContents* web_contents,
    scoped_refptr<SafeBrowsingUIManager> ui_manager,
    bool wait_for_load_stop) {
  AsyncCheckTracker* tracker =
      safe_browsing::AsyncCheckTracker::GetOrCreateForWebContents(
          web_contents, std::move(ui_manager),
          /*should_sync_checker_check_allowlist=*/false);
  // If all pending async checks are already resolved or were never created,
  // don't wait for the tracker to say the checkers size reached 0, because
  // that will never occur.
  if (tracker->PendingCheckersSizeForTesting() > 0u) {
    base::RunLoop async_checks_completed_run_loop;
    tracker->SetOnAllCheckersCompletedForTesting(
        async_checks_completed_run_loop.QuitClosure());
    async_checks_completed_run_loop.Run();
  }
  if (wait_for_load_stop) {
    // When all pending checks have been resolved, we may still need to wait
    // for the interstitial to load.
    content::WaitForLoadStop(web_contents);
  }
}

// ------- TestSafeBrowsingBlockingPageFactory class implementation ------- //
TestSafeBrowsingBlockingPageFactory::TestSafeBrowsingBlockingPageFactory()
    : always_show_back_to_safety_(true) {}
void TestSafeBrowsingBlockingPageFactory::SetAlwaysShowBackToSafety(
    bool value) {
  always_show_back_to_safety_ = value;
}
#if BUILDFLAG(FULL_SAFE_BROWSING)
MockTrustSafetySentimentService*
TestSafeBrowsingBlockingPageFactory::GetMockSentimentService() {
  return mock_sentiment_service_;
}
#endif
SafeBrowsingBlockingPage*
TestSafeBrowsingBlockingPageFactory::CreateSafeBrowsingPage(
    BaseUIManager* delegate,
    WebContents* web_contents,
    const GURL& main_frame_url,
    const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources,
    bool should_trigger_reporting,
    std::optional<base::TimeTicks> blocked_page_shown_timestamp) {
  shown_interstitial_ = InterstitialShown::kConsumer;
  PrefService* prefs =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetPrefs();
  bool is_extended_reporting_opt_in_allowed =
      prefs->GetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed);
  bool is_proceed_anyway_disabled =
      prefs->GetBoolean(prefs::kSafeBrowsingProceedAnywayDisabled);
  bool is_safe_browsing_surveys_enabled = IsSafeBrowsingSurveysEnabled(*prefs);

  BaseSafeBrowsingErrorUI::SBErrorDisplayOptions display_options(
      BaseBlockingPage::IsMainPageResourceLoadPending(unsafe_resources),
      is_extended_reporting_opt_in_allowed,
      web_contents->GetBrowserContext()->IsOffTheRecord(),
      IsExtendedReportingEnabled(*prefs),
      IsExtendedReportingPolicyManaged(*prefs),
      IsEnhancedProtectionEnabled(*prefs), is_proceed_anyway_disabled,
      false,  // should_open_links_in_new_tab
      always_show_back_to_safety_,
      /*is_enhanced_protection_message_enabled=*/true,
      IsSafeBrowsingPolicyManaged(*prefs),
      "cpn_safe_browsing" /* help_center_article_link */);

#if BUILDFLAG(FULL_SAFE_BROWSING)
  mock_sentiment_service_ = static_cast<MockTrustSafetySentimentService*>(
      TrustSafetySentimentServiceFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()),
              base::BindRepeating(&BuildMockTrustSafetySentimentService)));
#endif

  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));

  base::OnceCallback<void(bool, SBThreatType)>
      trust_safety_sentiment_service_trigger = base::NullCallback();
#if BUILDFLAG(FULL_SAFE_BROWSING)
  if (is_safe_browsing_surveys_enabled) {
    trust_safety_sentiment_service_trigger =
        base::BindOnce(&MockTrustSafetySentimentService::
                           InteractedWithSafeBrowsingInterstitial,
                       base::Unretained(mock_sentiment_service_));
  }
#endif

  return new TestSafeBrowsingBlockingPage(
      delegate, web_contents, main_frame_url, unsafe_resources, display_options,
      should_trigger_reporting, is_proceed_anyway_disabled,
      is_safe_browsing_surveys_enabled,
      std::move(trust_safety_sentiment_service_trigger),
      base::BindOnce(
          &safe_browsing::MaybeIgnoreAbusiveNotificationAutoRevocation,
          base::WrapRefCounted(hcsm), main_frame_url),
      blocked_page_shown_timestamp);
}

security_interstitials::SecurityInterstitialPage*
TestSafeBrowsingBlockingPageFactory::CreateEnterpriseWarnPage(
    BaseUIManager* ui_manager,
    content::WebContents* web_contents,
    const GURL& main_frame_url,
    const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources) {
  shown_interstitial_ = InterstitialShown::kEnterpriseWarn;
  return new EnterpriseWarnPage(
      ui_manager, web_contents, main_frame_url, unsafe_resources,
      std::make_unique<EnterpriseWarnControllerClient>(web_contents,
                                                       main_frame_url));
}

security_interstitials::SecurityInterstitialPage*
TestSafeBrowsingBlockingPageFactory::CreateEnterpriseBlockPage(
    BaseUIManager* ui_manager,
    content::WebContents* web_contents,
    const GURL& main_frame_url,
    const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources) {
  shown_interstitial_ = InterstitialShown::kEnterpriseBlock;
  return new EnterpriseBlockPage(
      web_contents, main_frame_url, unsafe_resources,
      std::make_unique<EnterpriseBlockControllerClient>(web_contents,
                                                        main_frame_url));
}

TestSafeBrowsingBlockingPageFactory::InterstitialShown
TestSafeBrowsingBlockingPageFactory::GetShownInterstitial() {
  return shown_interstitial_;
}

// -------- FakeSafeBrowsingUIManager class implementation -------- //
FakeSafeBrowsingUIManager::FakeSafeBrowsingUIManager(
    std::unique_ptr<SafeBrowsingBlockingPageFactory> blocking_page_factory)
    : TestSafeBrowsingUIManager(std::move(blocking_page_factory)) {}
FakeSafeBrowsingUIManager::~FakeSafeBrowsingUIManager() = default;

// Overrides SafeBrowsingUIManager.
void FakeSafeBrowsingUIManager::AttachThreatDetailsAndLaunchSurvey(
    content::BrowserContext* browser_context,
    std::unique_ptr<ClientSafeBrowsingReportRequest> report) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ValidateReportForHats(report->SerializeAsString());
  OnAttachThreatDetailsAndLaunchSurvey();
}

void FakeSafeBrowsingUIManager::ValidateReportForHats(
    std::string report_string) {
  if (expect_empty_report_for_hats_) {
    EXPECT_TRUE(report_string.empty());
  } else {
    EXPECT_FALSE(report_string.empty());
  }
  ClientSafeBrowsingReportRequest report;
  report.ParseFromString(report_string);
  if (expect_report_url_for_hats_) {
    EXPECT_TRUE(base::EndsWith(report.url(), "empty.html"));
  } else {
    EXPECT_TRUE(report.url().empty());
  }
  if (expect_interstitial_interactions_) {
    EXPECT_EQ(report.interstitial_interactions_size(), 2);
  } else {
    EXPECT_EQ(report.interstitial_interactions_size(), 0);
  }
}

// Overrides SafeBrowsingUIManager
void FakeSafeBrowsingUIManager::SendThreatDetails(
    content::BrowserContext* browser_context,
    std::unique_ptr<ClientSafeBrowsingReportRequest> report) {
  std::string serialized;
  report->SerializeToString(&serialized);

  // Notify the UI thread that we got a report.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&FakeSafeBrowsingUIManager::OnThreatDetailsDone,
                                this, serialized));
}

void FakeSafeBrowsingUIManager::OnThreatDetailsDone(
    const std::string& serialized) {
  if (threat_details_done_) {
    return;
  }
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
  report_ = serialized;

  ASSERT_TRUE(threat_details_done_callback_);
  std::move(threat_details_done_callback_).Run();
  threat_details_done_ = true;
}

void FakeSafeBrowsingUIManager::MaybeReportSafeBrowsingHit(
    std::unique_ptr<HitReport> hit_report,
    WebContents* web_contents) {
  if (SafeBrowsingUIManager::ShouldSendHitReport(hit_report.get(),
                                                 web_contents)) {
    hit_report_count_++;
    hit_report_sent_threat_source_ = hit_report.get()->threat_source;
  }
}

void FakeSafeBrowsingUIManager::MaybeSendClientSafeBrowsingWarningShownReport(
    std::unique_ptr<ClientSafeBrowsingReportRequest> report,
    WebContents* web_contents) {
  if (SafeBrowsingUIManager::ShouldSendClientSafeBrowsingWarningShownReport(
          web_contents)) {
    report_sent_ = true;
    if (report->has_client_properties() &&
        report->client_properties().has_is_async_check()) {
      report_sent_is_async_check_ =
          report->client_properties().is_async_check();
    }
  }
}

bool FakeSafeBrowsingUIManager::hit_report_sent() {
  return hit_report_count_ > 0;
}
int FakeSafeBrowsingUIManager::hit_report_count() {
  return hit_report_count_;
}
bool FakeSafeBrowsingUIManager::report_sent() {
  return report_sent_;
}
std::optional<ThreatSource>
FakeSafeBrowsingUIManager::hit_report_sent_threat_source() {
  return hit_report_sent_threat_source_;
}
std::optional<bool> FakeSafeBrowsingUIManager::report_sent_is_async_check() {
  return report_sent_is_async_check_;
}

void FakeSafeBrowsingUIManager::set_threat_details_done_callback(
    base::OnceClosure callback) {
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
  EXPECT_FALSE(threat_details_done_callback_);
  threat_details_done_callback_ = std::move(callback);
}

std::string FakeSafeBrowsingUIManager::GetReport() {
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return report_;
}

void FakeSafeBrowsingUIManager::SetExpectEmptyReportForHats(
    bool expect_empty_report_for_hats) {
  expect_empty_report_for_hats_ = expect_empty_report_for_hats;
}

void FakeSafeBrowsingUIManager::SetExpectReportUrlForHats(
    bool expect_report_url_for_hats) {
  expect_report_url_for_hats_ = expect_report_url_for_hats;
}

void FakeSafeBrowsingUIManager::SetExpectInterstitialInteractions(
    bool expect_interstitial_interactions) {
  expect_interstitial_interactions_ = expect_interstitial_interactions;
}

// --- SafeBrowsingBlockingPagePlatformBrowserTest class implementation --- //
content::WebContents*
SafeBrowsingBlockingPagePlatformBrowserTest::web_contents() {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  return browser()->tab_strip_model()->GetActiveWebContents();
#else
  return chrome_test_utils::GetActiveWebContents(this);
#endif
}
bool SafeBrowsingBlockingPagePlatformBrowserTest::NavigateToURL(
    const GURL& url) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  return ui_test_utils::NavigateToURL(browser(), url);
#else
  return content::NavigateToURL(web_contents(), url);
#endif
}
Profile* SafeBrowsingBlockingPagePlatformBrowserTest::profile() {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

// --- SafeBrowsingBlockingPageRealTimeUrlCheckTest class implementation --- //
SafeBrowsingBlockingPageRealTimeUrlCheckTest::
    SafeBrowsingBlockingPageRealTimeUrlCheckTest() = default;

void SafeBrowsingBlockingPageRealTimeUrlCheckTest::SetUp() {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kDelayedWarnings});
  SafeBrowsingBlockingPagePlatformBrowserTest::SetUp();
}

void SafeBrowsingBlockingPageRealTimeUrlCheckTest::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
}

void SafeBrowsingBlockingPageRealTimeUrlCheckTest::CreatedBrowserMainParts(
    content::BrowserMainParts* browser_main_parts) {
  SafeBrowsingBlockingPagePlatformBrowserTest::CreatedBrowserMainParts(
      browser_main_parts);
  // Test UI manager and test database manager should be set before
  // the browser is started but after threads are created.
  auto blocking_page_factory =
      std::make_unique<TestSafeBrowsingBlockingPageFactory>();
  blocking_page_factory_ptr_ = blocking_page_factory.get();
  factory_.SetTestUIManager(
      new FakeSafeBrowsingUIManager(std::move(blocking_page_factory)));
  factory_.SetTestDatabaseManager(
      new FakeSafeBrowsingDatabaseManager(content::GetUIThreadTaskRunner({})));
  SafeBrowsingService::RegisterFactory(&factory_);
}

void SafeBrowsingBlockingPageRealTimeUrlCheckTest::
    SetupUrlRealTimeVerdictInCacheManager(
        GURL url,
        Profile* profile,
        RTLookupResponse::ThreatInfo::VerdictType verdict_type,
        std::optional<RTLookupResponse::ThreatInfo::ThreatType> threat_type) {
  safe_browsing::VerdictCacheManagerFactory::GetForProfile(profile)
      ->CacheArtificialRealTimeUrlVerdict(url.spec(), verdict_type,
                                          threat_type);
}
void SafeBrowsingBlockingPageRealTimeUrlCheckTest::SetupUnsafeVerdict(
    GURL url,
    Profile* profile) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(
      "mark_as_real_time_phishing",
      embedded_test_server()->GetURL("/empty.html").spec());
  safe_browsing::VerdictCacheManagerFactory::GetForProfile(profile)
      ->CacheArtificialUnsafeRealTimeUrlVerdictFromSwitch();
}
void SafeBrowsingBlockingPageRealTimeUrlCheckTest::NavigateToURL(
    GURL url,
    bool expect_success) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  ASSERT_TRUE(SafeBrowsingBlockingPagePlatformBrowserTest::NavigateToURL(url));
#else
  // `NavigateToURL` returns false if there is an interstitial shown.
  ASSERT_EQ(SafeBrowsingBlockingPagePlatformBrowserTest::NavigateToURL(url),
            expect_success);
#endif
  SafeBrowsingBlockingPageTestHelper::MaybeWaitForAsyncChecksToComplete(
      web_contents(), factory_.test_safe_browsing_service()->ui_manager().get(),
      /*wait_for_load_stop=*/true);
}
void SafeBrowsingBlockingPageRealTimeUrlCheckTest::SetReportSentCallback(
    base::OnceClosure callback) {
  static_cast<FakeSafeBrowsingUIManager*>(
      factory_.test_safe_browsing_service()->ui_manager().get())
      ->set_threat_details_done_callback(std::move(callback));
}

}  // namespace safe_browsing
