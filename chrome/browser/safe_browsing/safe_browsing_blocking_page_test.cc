// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test creates a fake safebrowsing service, where we can inject known-
// threat urls.  It then uses a real browser to go to these urls, and sends
// "goback" or "proceed" commands and verifies they work.

#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/interstitials/security_interstitial_idn_test.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/passwords_navigation_observer.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_blocking_page_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/safe_browsing/url_lookup_service_factory.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/google/core/common/google_util.h"
#include "components/grit/components_resources.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/page_info/core/features.h"
#include "components/permissions/permission_util.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page_factory.h"
#include "components/safe_browsing/content/browser/threat_details.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/content/browser/unsafe_resource_util.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/fake_database_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/web_ui_constants.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "components/security_interstitials/core/urls.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/test/test_event.h"
#include "ui/views/controls/styled_label.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#endif

using chrome_browser_interstitials::SecurityInterstitialIDNTest;
using content::BrowserThread;
using content::NavigationController;
using content::RenderFrameHost;
using content::WebContents;
using security_interstitials::BaseSafeBrowsingErrorUI;

namespace safe_browsing {

namespace {
const char kEmptyPage[] = "/empty.html";
const char kHTTPSPage[] = "/ssl/google.html";
const char kMaliciousPage[] = "/safe_browsing/malware.html";
const char kCrossSiteMaliciousPage[] = "/safe_browsing/malware2.html";
const char kMaliciousIframe[] = "/safe_browsing/malware_iframe.html";
const char kRedirectToMalware[] = "/safe_browsing/redirect_to_malware.html";
const char kUnrelatedUrl[] = "https://www.google.com";
const char kEnhancedProtectionUrl[] = "chrome://settings/security?q=enhanced";
const char kMaliciousJsPage[] = "/safe_browsing/malware_js.html";
const char kMaliciousJs[] = "/safe_browsing/script.js";

const char kInterstitialCloseHistogram[] = "interstitial.CloseReason";
const char kInterstitialPreCommitPageHistogramSuffix[] = ".before_page_shown";

std::string GetHistogramPrefix(const SBThreatType& threat_type) {
  if (threat_type == SBThreatType::SB_THREAT_TYPE_URL_MALWARE) {
    return "malware";
  } else if (threat_type == SBThreatType::SB_THREAT_TYPE_URL_PHISHING) {
    return "phishing";
  } else if (threat_type == SBThreatType::SB_THREAT_TYPE_URL_UNWANTED) {
    return "harmful";
  } else {
    NOTREACHED_IN_MIGRATION();
    return "";
  }
}

}  // namespace

enum Visibility { VISIBILITY_ERROR = -1, HIDDEN = 0, VISIBLE = 1 };

bool IsShowingInterstitial(WebContents* contents) {
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          contents);
  return helper &&
         (helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting() !=
          nullptr);
}

content::RenderFrameHost* GetRenderFrameHost(Browser* browser) {
  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetPrimaryMainFrame();
}

views::BubbleDialogDelegateView* OpenPageInfo(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  LocationIconView* location_icon_view =
      browser_view->toolbar()->location_bar()->location_icon_view();
  ui::test::TestEvent event;
  location_icon_view->ShowBubble(event);
  views::BubbleDialogDelegateView* page_info =
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
  page_info->set_close_on_deactivate(false);
  return page_info;
}

bool WaitForReady(Browser* browser) {
  WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
  if (!content::WaitForRenderFrameReady(contents->GetPrimaryMainFrame())) {
    return false;
  }
  return chrome_browser_interstitials::IsShowingInterstitial(contents);
}

Visibility GetVisibility(Browser* browser, const std::string& node_id) {
  content::RenderFrameHost* rfh = GetRenderFrameHost(browser);
  if (!rfh) {
    return VISIBILITY_ERROR;
  }

  // clang-format off
  std::string jsFindVisibility = R"(
    (function isNodeVisible(node) {
      if (!node) return 'node not found';
      if (node.offsetWidth === 0 || node.offsetHeight === 0) return false;
      // Do not check opacity, since the css transition may actually leave
      // opacity at 0 after it's been unhidden
      if (node.classList.contains('hidden')) return false;
      // Otherwise, we must check all parent nodes
      var parentVisibility = isNodeVisible(node.parentElement);
      if (parentVisibility === 'node not found') {
        return true; // none of the parents are set invisible
      }
      return parentVisibility;
    }(document.getElementById(')" + node_id + R"(')));)";
  // clang-format on

  content::EvalJsResult result = content::EvalJs(rfh, jsFindVisibility);

  if (result != true && result != false) {
    return VISIBILITY_ERROR;
  }

  return result == true ? VISIBLE : HIDDEN;
}

bool Click(Browser* browser, const std::string& node_id) {
  DCHECK(node_id == "primary-button" || node_id == "proceed-link" ||
         node_id == "whitepaper-link" || node_id == "details-button" ||
         node_id == "opt-in-checkbox" || node_id == "enhanced-protection-link")
      << "Unexpected node_id: " << node_id;
  content::RenderFrameHost* rfh = GetRenderFrameHost(browser);
  if (!rfh) {
    return false;
  }
  // We don't use EvalJs for this one, since clicking
  // the button/link may navigate away before the injected javascript can
  // reply, hanging the test.
  rfh->ExecuteJavaScriptForTests(
      u"document.getElementById('" + base::ASCIIToUTF16(node_id) +
          u"').click();\n",
      base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
  return true;
}

bool ClickAndWaitForDetach(Browser* browser, const std::string& node_id) {
  // We wait for interstitial_detached rather than nav_entry_committed, as
  // going back from a main-frame safe browsing interstitial page will not
  // cause a nav entry committed event.
  content::TestNavigationObserver observer(
      browser->tab_strip_model()->GetActiveWebContents());
  if (!Click(browser, node_id)) {
    return false;
  }
  observer.WaitForNavigationFinished();
  return true;
}

void ExpectSecurityIndicatorDowngrade(content::WebContents* tab,
                                      net::CertStatus cert_status) {
  SecurityStateTabHelper* helper = SecurityStateTabHelper::FromWebContents(tab);
  ASSERT_TRUE(helper);
  EXPECT_EQ(security_state::DANGEROUS, helper->GetSecurityLevel());
  EXPECT_NE(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            helper->GetVisibleSecurityState()->malicious_content_status);
  // TODO(felt): Restore this check when https://crbug.com/641187 is fixed.
  // EXPECT_EQ(cert_status, helper->GetSecurityInfo().cert_status);
}

void ExpectNoSecurityIndicatorDowngrade(content::WebContents* tab) {
  SecurityStateTabHelper* helper = SecurityStateTabHelper::FromWebContents(tab);
  ASSERT_TRUE(helper);
  EXPECT_EQ(security_state::NONE, helper->GetSecurityLevel());
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            helper->GetVisibleSecurityState()->malicious_content_status);
}

// A SafeBrowsingUIManager class that allows intercepting malware details.
class FakeSafeBrowsingUIManager : public TestSafeBrowsingUIManager {
 public:
  explicit FakeSafeBrowsingUIManager(
      std::unique_ptr<SafeBrowsingBlockingPageFactory> blocking_page_factory)
      : TestSafeBrowsingUIManager(std::move(blocking_page_factory)) {}

  FakeSafeBrowsingUIManager(const FakeSafeBrowsingUIManager&) = delete;
  FakeSafeBrowsingUIManager& operator=(const FakeSafeBrowsingUIManager&) =
      delete;

  MOCK_METHOD0(OnAttachThreatDetailsAndLaunchSurvey, void());

  // Overrides SafeBrowsingUIManager.
  void AttachThreatDetailsAndLaunchSurvey(
      content::BrowserContext* browser_context,
      std::unique_ptr<ClientSafeBrowsingReportRequest> report) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    ValidateReportForHats(report->SerializeAsString());
    OnAttachThreatDetailsAndLaunchSurvey();
  }

  void ValidateReportForHats(std::string report_string) {
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
  void SendThreatDetails(
      content::BrowserContext* browser_context,
      std::unique_ptr<ClientSafeBrowsingReportRequest> report) override {
    std::string serialized;
    report->SerializeToString(&serialized);

    // Notify the UI thread that we got a report.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeSafeBrowsingUIManager::OnThreatDetailsDone, this,
                       serialized));
  }

  void OnThreatDetailsDone(const std::string& serialized) {
    if (threat_details_done_) {
      return;
    }
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    report_ = serialized;

    ASSERT_TRUE(threat_details_done_callback_);
    std::move(threat_details_done_callback_).Run();
    threat_details_done_ = true;
  }

  void MaybeReportSafeBrowsingHit(std::unique_ptr<HitReport> hit_report,
                                  WebContents* web_contents) override {
    if (SafeBrowsingUIManager::ShouldSendHitReport(hit_report.get(),
                                                   web_contents)) {
      hit_report_count_++;
      hit_report_sent_threat_source_ = hit_report.get()->threat_source;
    }
  }

  void MaybeSendClientSafeBrowsingWarningShownReport(
      std::unique_ptr<ClientSafeBrowsingReportRequest> report,
      WebContents* web_contents) override {
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

  bool hit_report_sent() { return hit_report_count_ > 0; }
  int hit_report_count() { return hit_report_count_; }
  bool report_sent() { return report_sent_; }
  std::optional<ThreatSource> hit_report_sent_threat_source() {
    return hit_report_sent_threat_source_;
  }
  std::optional<bool> report_sent_is_async_check() {
    return report_sent_is_async_check_;
  }

  void set_threat_details_done_callback(base::OnceClosure callback) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    EXPECT_FALSE(threat_details_done_callback_);
    threat_details_done_callback_ = std::move(callback);
  }

  std::string GetReport() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    return report_;
  }

  void SetExpectEmptyReportForHats(bool expect_empty_report_for_hats) {
    expect_empty_report_for_hats_ = expect_empty_report_for_hats;
  }

  void SetExpectReportUrlForHats(bool expect_report_url_for_hats) {
    expect_report_url_for_hats_ = expect_report_url_for_hats;
  }

  void SetExpectInterstitialInteractions(
      bool expect_interstitial_interactions) {
    expect_interstitial_interactions_ = expect_interstitial_interactions;
  }

 protected:
  ~FakeSafeBrowsingUIManager() override {}

 private:
  std::string report_;
  base::OnceClosure threat_details_done_callback_;
  bool threat_details_done_ = false;
  int hit_report_count_ = 0;
  bool report_sent_ = false;
  bool expect_empty_report_for_hats_ = true;
  bool expect_report_url_for_hats_ = false;
  bool expect_interstitial_interactions_ = false;
  std::optional<ThreatSource> hit_report_sent_threat_source_;
  std::optional<bool> report_sent_is_async_check_;
};

class TestThreatDetailsFactory : public ThreatDetailsFactory {
 public:
  TestThreatDetailsFactory() : details_() {}
  ~TestThreatDetailsFactory() override {}

  std::unique_ptr<ThreatDetails> CreateThreatDetails(
      BaseUIManager* delegate,
      WebContents* web_contents,
      const security_interstitials::UnsafeResource& unsafe_resource,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service,
      ReferrerChainProvider* referrer_chain_provider,
      bool trim_to_ad_tags,
      ThreatDetailsDoneCallback done_callback) override {
    auto details = base::WrapUnique(new ThreatDetails(
        delegate, web_contents, unsafe_resource, url_loader_factory,
        history_service, referrer_chain_provider, trim_to_ad_tags,
        std::move(done_callback)));
    details_ = details.get();
    details->StartCollection();
    return details;
  }

  ThreatDetails* get_details() { return details_; }

 private:
  raw_ptr<ThreatDetails, AcrossTasksDanglingUntriaged> details_;
};

// A SafeBrowingBlockingPage class that lets us wait until it's hidden.
class TestSafeBrowsingBlockingPage : public SafeBrowsingBlockingPage {
 public:
  TestSafeBrowsingBlockingPage(
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
  void CommandReceived(const std::string& command) override {
    SafeBrowsingBlockingPage::CommandReceived(command);
  }
};

void AssertNoInterstitial(Browser* browser) {
  WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));
  return;
}

class TestSafeBrowsingBlockingPageFactory
    : public SafeBrowsingBlockingPageFactory {
 public:
  TestSafeBrowsingBlockingPageFactory() : always_show_back_to_safety_(true) {}
  ~TestSafeBrowsingBlockingPageFactory() override {}

  void SetAlwaysShowBackToSafety(bool value) {
    always_show_back_to_safety_ = value;
  }

  MockTrustSafetySentimentService* GetMockSentimentService() {
    return mock_sentiment_service_;
  }

  SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      BaseUIManager* delegate,
      WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources,
      bool should_trigger_reporting,
      std::optional<base::TimeTicks> blocked_page_shown_timestamp) override {
    PrefService* prefs =
        Profile::FromBrowserContext(web_contents->GetBrowserContext())
            ->GetPrefs();
    bool is_extended_reporting_opt_in_allowed =
        prefs->GetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed);
    bool is_proceed_anyway_disabled =
        prefs->GetBoolean(prefs::kSafeBrowsingProceedAnywayDisabled);
    bool is_safe_browsing_surveys_enabled =
        IsSafeBrowsingSurveysEnabled(*prefs);
    bool is_abusive_notification_revocation_enabled =
        base::FeatureList::IsEnabled(
            safe_browsing::kSafetyHubAbusiveNotificationRevocation);

    BaseSafeBrowsingErrorUI::SBErrorDisplayOptions display_options(
        BaseBlockingPage::IsMainPageLoadPending(unsafe_resources),
        is_extended_reporting_opt_in_allowed,
        web_contents->GetBrowserContext()->IsOffTheRecord(),
        IsExtendedReportingEnabled(*prefs),
        IsExtendedReportingPolicyManaged(*prefs),
        IsEnhancedProtectionEnabled(*prefs), is_proceed_anyway_disabled,
        true,  // should_open_links_in_new_tab
        always_show_back_to_safety_,
        /*is_enhanced_protection_message_enabled=*/true,
        IsSafeBrowsingPolicyManaged(*prefs),
        "cpn_safe_browsing" /* help_center_article_link */);

    mock_sentiment_service_ = static_cast<MockTrustSafetySentimentService*>(
        TrustSafetySentimentServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                Profile::FromBrowserContext(web_contents->GetBrowserContext()),
                base::BindRepeating(&BuildMockTrustSafetySentimentService)));

    auto* hcsm = HostContentSettingsMapFactory::GetForProfile(
        Profile::FromBrowserContext(web_contents->GetBrowserContext()));

    return new TestSafeBrowsingBlockingPage(
        delegate, web_contents, main_frame_url, unsafe_resources,
        display_options, should_trigger_reporting, is_proceed_anyway_disabled,
        is_safe_browsing_surveys_enabled,
        is_safe_browsing_surveys_enabled
            ? base::BindOnce(&MockTrustSafetySentimentService::
                                 InteractedWithSafeBrowsingInterstitial,
                             base::Unretained(mock_sentiment_service_))
            : base::NullCallback(),
        is_abusive_notification_revocation_enabled
            ? base::BindOnce(
                  &safe_browsing::MaybeIgnoreAbusiveNotificationAutoRevocation,
                  base::WrapRefCounted(hcsm), main_frame_url)
            : base::NullCallback(),
        blocked_page_shown_timestamp);
  }

  security_interstitials::SecurityInterstitialPage* CreateEnterpriseWarnPage(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
      override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  security_interstitials::SecurityInterstitialPage* CreateEnterpriseBlockPage(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
      override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

 private:
  raw_ptr<MockTrustSafetySentimentService, DanglingUntriaged>
      mock_sentiment_service_;
  bool always_show_back_to_safety_;
};

class SafeBrowsingBlockingPageTestHelper {
 public:
  SafeBrowsingBlockingPageTestHelper() {}
  SafeBrowsingBlockingPageTestHelper(
      const SafeBrowsingBlockingPageTestHelper&) = delete;
  SafeBrowsingBlockingPageTestHelper& operator=(
      const SafeBrowsingBlockingPageTestHelper&) = delete;

  static void MaybeWaitForAsyncChecksToComplete(
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
};

// Tests the safe browsing blocking page in a browser.
class SafeBrowsingBlockingPageBrowserTest
    : public CertVerifierBrowserTest,
      public testing::WithParamInterface<testing::tuple<SBThreatType, bool>> {
 public:
  SafeBrowsingBlockingPageBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // NOTE: Value copied from the renderer-side threat_dom_details.cc, as
    // threat_dom_details.h can't be depended on from this browser-side code.
    const char kTagAndAttributeParamName[] = "tag_attribute_csv";
    std::map<std::string, std::string> parameters = {
        {kTagAndAttributeParamName, "div,foo,div,baz"}};
    base::test::FeatureRefAndParams tag_and_attribute(
        safe_browsing::kThreatDomDetailsTagAndAttributeFeature, parameters);
    base::test::FeatureRefAndParams add_warning_shown_timestamp_csbrrs(
        safe_browsing::kAddWarningShownTSToClientSafeBrowsingReport, {});
    base::test::FeatureRefAndParams create_warning_shown_csbrrs(
        safe_browsing::kCreateWarningShownClientSafeBrowsingReports, {});
    base::test::FeatureRefAndParams abusive_notification_revocation(
        safe_browsing::kSafetyHubAbusiveNotificationRevocation, {});
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {tag_and_attribute, add_warning_shown_timestamp_csbrrs,
         create_warning_shown_csbrrs, abusive_notification_revocation},
        {});
  }

  SafeBrowsingBlockingPageBrowserTest(
      const SafeBrowsingBlockingPageBrowserTest&) = delete;
  SafeBrowsingBlockingPageBrowserTest& operator=(
      const SafeBrowsingBlockingPageBrowserTest&) = delete;

  ~SafeBrowsingBlockingPageBrowserTest() override {}

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    CertVerifierBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    // Test UI manager and test database manager should be set before
    // the browser is started but after threads are created.
    auto blocking_page_factory =
        std::make_unique<TestSafeBrowsingBlockingPageFactory>();
    raw_blocking_page_factory_ = blocking_page_factory.get();
    factory_.SetTestUIManager(
        new FakeSafeBrowsingUIManager(std::move(blocking_page_factory)));
    factory_.SetTestDatabaseManager(new FakeSafeBrowsingDatabaseManager(
        content::GetUIThreadTaskRunner({})));
    SafeBrowsingService::RegisterFactory(&factory_);
    ThreatDetails::RegisterFactory(&details_factory_);
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    SafeBrowsingService::RegisterFactory(nullptr);
    ThreatDetails::RegisterFactory(nullptr);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    CertVerifierBrowserTest::SetUpCommandLine(command_line);
    if (testing::get<1>(GetParam())) {
      content::IsolateAllSitesForTesting(command_line);
    }
    // TODO(crbug.com/40285326): This fails with the field trial testing config.
    command_line->AppendSwitch("disable-field-trial-config");
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  SBThreatType GetThreatType() const { return testing::get<0>(GetParam()); }

  void SetURLThreatType(const GURL& url, SBThreatType threat_type) {
    TestSafeBrowsingService* service = factory_.test_safe_browsing_service();
    ASSERT_TRUE(service);

    static_cast<FakeSafeBrowsingDatabaseManager*>(
        service->database_manager().get())
        ->AddDangerousUrl(url, threat_type);
  }

  void SetURLThreatPatternType(const GURL& url,
                               ThreatPatternType threat_pattern_type) {
    TestSafeBrowsingService* service = factory_.test_safe_browsing_service();
    ASSERT_TRUE(service);

    static_cast<FakeSafeBrowsingDatabaseManager*>(
        service->database_manager().get())
        ->AddDangerousUrlPattern(url, threat_pattern_type);
  }

  void ClearBadURL(const GURL& url) {
    TestSafeBrowsingService* service = factory_.test_safe_browsing_service();
    ASSERT_TRUE(service);

    static_cast<FakeSafeBrowsingDatabaseManager*>(
        service->database_manager().get())
        ->ClearDangerousUrl(url);
  }

  // The basic version of this method, which uses an HTTP test URL.
  GURL SetupWarningAndNavigate(Browser* browser) {
    return SetupWarningAndNavigateToURL(
        embedded_test_server()->GetURL(kEmptyPage), browser);
  }

  // The basic version of this method, which uses an HTTP test URL.
  GURL SetupWarningAndNavigateInNewTab(Browser* browser) {
    return SetupWarningAndNavigateToURLInNewTab(
        embedded_test_server()->GetURL(kEmptyPage), browser);
  }

  // Navigates to a warning on a valid HTTPS website.
  GURL SetupWarningAndNavigateToValidHTTPS() {
    EXPECT_TRUE(https_server_.Start());
    scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
    net::CertVerifyResult verify_result;
    verify_result.verified_cert = cert;
    verify_result.cert_status = 0;
    mock_cert_verifier()->AddResultForCert(cert.get(), verify_result, net::OK);
    GURL url = https_server_.GetURL(kHTTPSPage);
    return SetupWarningAndNavigateToURL(url, browser());
  }

  // Navigates through an HTTPS interstitial, then opens up a SB warning on that
  // same URL.
  GURL SetupWarningAndNavigateToInvalidHTTPS() {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
    EXPECT_TRUE(https_server_.Start());
    GURL url = https_server_.GetURL(kHTTPSPage);

    // Proceed through the HTTPS interstitial.
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    security_interstitials::SecurityInterstitialPage* ssl_blocking_page;

    EXPECT_TRUE(WaitForRenderFrameReady(contents->GetPrimaryMainFrame()));
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            contents);
    EXPECT_TRUE(helper);
    ssl_blocking_page =
        helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting();

    EXPECT_EQ(SSLBlockingPage::kTypeForTesting,
              ssl_blocking_page->GetTypeForTesting());
    content::TestNavigationObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    ssl_blocking_page->CommandReceived(base::NumberToString(
        security_interstitials::SecurityInterstitialCommand::CMD_PROCEED));
    // When both SB and SSL interstitials are committed navigations, we need
    // to wait for two navigations here, one is from the SSL interstitial to
    // the blocked site (which does not complete since SB blocks it) and the
    // second one is to the actual SB interstitial.
    observer.WaitForNavigationFinished();

    return SetupWarningAndNavigateToURL(url, browser());
  }

  // Adds a safebrowsing threat results to the fake safebrowsing service,
  // navigates to a page with a subresource containing the threat site, and
  // returns the url of the parent page.
  GURL SetupThreatOnSubresourceAndNavigate(std::string_view main_frame_url,
                                           std::string_view subresource_url) {
    GURL url = embedded_test_server()->GetURL(main_frame_url);
    GURL embedded_url = embedded_test_server()->GetURL(subresource_url);
    SetURLThreatType(embedded_url, GetThreatType());

    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(
        content::WaitForRenderFrameReady(contents->GetPrimaryMainFrame()));
    return url;
  }

  GURL GetWhitePaperUrl() {
    return google_util::AppendGoogleLocaleParam(
        GURL(security_interstitials::kSafeBrowsingWhitePaperUrl),
        factory_.test_safe_browsing_service()
            ->ui_manager()
            .get()
            ->app_locale());
  }

  void SendCommand(
      security_interstitials::SecurityInterstitialCommand command) {
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    SafeBrowsingBlockingPage* interstitial_page;
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            contents);
    ASSERT_TRUE(helper);
    interstitial_page = static_cast<SafeBrowsingBlockingPage*>(
        helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting());
    ASSERT_TRUE(interstitial_page);
    ASSERT_EQ(SafeBrowsingBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
    interstitial_page->CommandReceived(base::NumberToString(command));
  }

  void SetReportSentCallback(base::OnceClosure callback) {
    static_cast<FakeSafeBrowsingUIManager*>(
        factory_.test_safe_browsing_service()->ui_manager().get())
        ->set_threat_details_done_callback(std::move(callback));
  }

  std::string GetReportSent() {
    return static_cast<FakeSafeBrowsingUIManager*>(
               factory_.test_safe_browsing_service()->ui_manager().get())
        ->GetReport();
  }

  void SetExpectEmptyReportForHats(bool expect_empty_report_for_hats) {
    static_cast<FakeSafeBrowsingUIManager*>(
        factory_.test_safe_browsing_service()->ui_manager().get())
        ->SetExpectEmptyReportForHats(expect_empty_report_for_hats);
  }

  void SetExpectReportUrlForHats(bool expect_report_url_for_hats) {
    static_cast<FakeSafeBrowsingUIManager*>(
        factory_.test_safe_browsing_service()->ui_manager().get())
        ->SetExpectReportUrlForHats(expect_report_url_for_hats);
  }

  void SetExpectInterstitialInteractions(
      bool expect_interstitial_interactions) {
    static_cast<FakeSafeBrowsingUIManager*>(
        factory_.test_safe_browsing_service()->ui_manager().get())
        ->SetExpectInterstitialInteractions(expect_interstitial_interactions);
  }

  FakeSafeBrowsingUIManager* GetSafeBrowsingUiManager() {
    return static_cast<FakeSafeBrowsingUIManager*>(
        factory_.test_safe_browsing_service()->ui_manager().get());
  }

  content::RenderFrameHost* GetRenderFrameHost() {
    return ::safe_browsing::GetRenderFrameHost(browser());
  }

  Visibility GetVisibility(const std::string& node_id) {
    return ::safe_browsing::GetVisibility(browser(), node_id);
  }

  bool Click(const std::string& node_id) {
    return ::safe_browsing::Click(browser(), node_id);
  }

  bool ClickAndWaitForDetach(const std::string& node_id) {
    return ::safe_browsing::ClickAndWaitForDetach(browser(), node_id);
  }

  void AssertNoInterstitial() {
    return ::safe_browsing::AssertNoInterstitial(browser());
  }

  void TestReportingDisabledAndDontProceed(const GURL& url) {
    SetURLThreatType(url, GetThreatType());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    ASSERT_TRUE(WaitForReady(browser()));

    EXPECT_EQ(HIDDEN, GetVisibility("extended-reporting-opt-in"));
    EXPECT_EQ(HIDDEN, GetVisibility("opt-in-checkbox"));
    EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
    EXPECT_EQ(VISIBLE, GetVisibility("learn-more-link"));
    EXPECT_TRUE(Click("details-button"));
    EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));

    EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));
    AssertNoInterstitial();               // Assert the interstitial is gone
    EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
              browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetLastCommittedURL());
  }

  void VerifyResource(
      const ClientSafeBrowsingReportRequest& report,
      const ClientSafeBrowsingReportRequest::Resource& actual_resource,
      const std::string& expected_url,
      const std::string& expected_parent,
      int expected_child_size,
      const std::string& expected_tag_name) {
    EXPECT_EQ(expected_url, actual_resource.url());
    // Finds the parent url by comparing resource ids.
    for (auto resource : report.resources()) {
      if (actual_resource.parent_id() == resource.id()) {
        EXPECT_EQ(expected_parent, resource.url());
        break;
      }
    }
    EXPECT_EQ(expected_child_size, actual_resource.child_ids_size());
    EXPECT_EQ(expected_tag_name, actual_resource.tag_name());
  }

  void VerifyInteractionOccurrenceCount(
      const ClientSafeBrowsingReportRequest& report,
      const ClientSafeBrowsingReportRequest::InterstitialInteraction&
          actual_interaction,
      const ClientSafeBrowsingReportRequest::InterstitialInteraction::
          SecurityInterstitialInteraction& expected_interaction_type,
      const int& expected_occurrence_count) {
    // Find the interaction within the report by comparing
    // security_interstitial_interaction.
    for (auto interaction : report.interstitial_interactions()) {
      if (actual_interaction.security_interstitial_interaction() ==
          interaction.security_interstitial_interaction()) {
        EXPECT_EQ(expected_interaction_type,
                  interaction.security_interstitial_interaction());
        EXPECT_EQ(expected_occurrence_count, interaction.occurrence_count());
        break;
      }
    }
  }

  void VerifyElement(
      const ClientSafeBrowsingReportRequest& report,
      const HTMLElement& actual_element,
      const std::string& expected_tag_name,
      int expected_child_ids_size,
      const std::vector<mojom::AttributeNameValuePtr>& expected_attributes) {
    EXPECT_EQ(expected_tag_name, actual_element.tag());
    EXPECT_EQ(expected_child_ids_size, actual_element.child_ids_size());
    ASSERT_EQ(static_cast<int>(expected_attributes.size()),
              actual_element.attribute_size());
    for (size_t i = 0; i < expected_attributes.size(); ++i) {
      const mojom::AttributeNameValue& expected_attribute =
          *expected_attributes[i];
      const HTMLElement::Attribute& actual_attribute_pb =
          actual_element.attribute(i);
      EXPECT_EQ(expected_attribute.name, actual_attribute_pb.name());
      EXPECT_EQ(expected_attribute.value, actual_attribute_pb.value());
    }
  }

  void ExpectSecurityIndicatorDowngrade(content::WebContents* tab,
                                        net::CertStatus cert_status) {
    ::safe_browsing::ExpectSecurityIndicatorDowngrade(tab, cert_status);
  }

  void ExpectNoSecurityIndicatorDowngrade(content::WebContents* tab) {
    ::safe_browsing::ExpectNoSecurityIndicatorDowngrade(tab);
  }

  bool hit_report_sent() {
    return static_cast<FakeSafeBrowsingUIManager*>(
               factory_.test_safe_browsing_service()->ui_manager().get())
        ->hit_report_sent();
  }

  bool report_sent() {
    return static_cast<FakeSafeBrowsingUIManager*>(
               factory_.test_safe_browsing_service()->ui_manager().get())
        ->report_sent();
  }

  // Helper method for LearnMore test below. Implemented as a test fixture
  // method instead of in the test below because the whole test fixture class
  // is friended by SafeBrowsingBlockingPage.
  void MockHelpCenterUrl(SafeBrowsingBlockingPage* sb_interstitial) {
    ASSERT_TRUE(https_server_.Start());
    scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
    net::CertVerifyResult verify_result;
    verify_result.verified_cert = cert;
    verify_result.cert_status = 0;
    mock_cert_verifier()->AddResultForCert(cert.get(), verify_result, net::OK);

    security_interstitials::SecurityInterstitialControllerClient* client =
        sb_interstitial->controller();

    client->SetBaseHelpCenterUrlForTesting(
        https_server_.GetURL("/title1.html"));
  }

  void SetAlwaysShowBackToSafety(bool val) {
    raw_blocking_page_factory_->SetAlwaysShowBackToSafety(val);
  }

  MockTrustSafetySentimentService* mock_sentiment_service() {
    return raw_blocking_page_factory_->GetMockSentimentService();
  }

  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  }

 protected:
  TestThreatDetailsFactory details_factory_;

  net::EmbeddedTestServer& https_server() { return https_server_; }

 private:
  // Adds a safebrowsing result of the current test threat to the fake
  // safebrowsing service, navigates to that page, and returns the url.
  // The various wrappers supply different URLs.
  GURL SetupWarningAndNavigateToURL(GURL url, Browser* browser) {
    SetURLThreatType(url, GetThreatType());
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, url));
    EXPECT_TRUE(WaitForReady(browser));
    return url;
  }
  // Adds a safebrowsing result of the current test threat to the fake
  // safebrowsing service, navigates to that page, and returns the url.
  // The various wrappers supply different URLs.
  GURL SetupWarningAndNavigateToURLInNewTab(GURL url, Browser* browser) {
    SetURLThreatType(url, GetThreatType());
    ui_test_utils::NavigateToURLWithDisposition(
        browser, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
    content::TestNavigationObserver observer(
        browser->tab_strip_model()->GetActiveWebContents());
    observer.WaitForNavigationFinished();
    EXPECT_TRUE(WaitForReady(browser));
    return url;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  TestSafeBrowsingServiceFactory factory_;
  raw_ptr<TestSafeBrowsingBlockingPageFactory, DanglingUntriaged>
      raw_blocking_page_factory_;
  net::EmbeddedTestServer https_server_;
};

class SafeBrowsingHatsSurveyBrowserTest
    : public SafeBrowsingBlockingPageBrowserTest {
 public:
  SafeBrowsingHatsSurveyBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kRedWarningSurvey);
  }
  ~SafeBrowsingHatsSurveyBrowserTest() override = default;

  void SetUp() override { SafeBrowsingBlockingPageBrowserTest::SetUp(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, HardcodedUrls) {
  const GURL urls[] = {GURL(kChromeUISafeBrowsingMatchMalwareUrl),
                       GURL(kChromeUISafeBrowsingMatchPhishingUrl),
                       GURL(kChromeUISafeBrowsingMatchUnwantedUrl)};

  for (const GURL& url : urls) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_TRUE(WaitForReady(browser()));

    EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
    EXPECT_EQ(HIDDEN, GetVisibility("details"));
    EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
    EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
    EXPECT_TRUE(Click("details-button"));
    EXPECT_EQ(VISIBLE, GetVisibility("details"));
    EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));
    EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
    EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));

    AssertNoInterstitial();               // Assert the interstitial is gone
    EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
              browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetLastCommittedURL());
  }
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, DontProceed) {
  SetupWarningAndNavigate(browser());

  EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("details"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(Click("details-button"));
  EXPECT_EQ(VISIBLE, GetVisibility("details"));
  EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));

  AssertNoInterstitial();               // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, DontProceed_RTL) {
  base::i18n::SetICUDefaultLocale("ar");
  ASSERT_TRUE(base::i18n::IsRTL());

  SetupWarningAndNavigate(browser());

  EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("details"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(Click("details-button"));
  EXPECT_EQ(VISIBLE, GetVisibility("details"));
  EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));

  AssertNoInterstitial();               // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, Proceed) {
  GURL url = SetupWarningAndNavigate(browser());

  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial();  // Assert the interstitial is gone.
  EXPECT_EQ(url, browser()
                     ->tab_strip_model()
                     ->GetActiveWebContents()
                     ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, Proceed_RTL) {
  base::i18n::SetICUDefaultLocale("ar");
  ASSERT_TRUE(base::i18n::IsRTL());

  GURL url = SetupWarningAndNavigate(browser());

  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial();  // Assert the interstitial is gone.
  EXPECT_EQ(url, browser()
                     ->tab_strip_model()
                     ->GetActiveWebContents()
                     ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, IframeNoWarning) {
  SetupThreatOnSubresourceAndNavigate(kCrossSiteMaliciousPage,
                                      kMaliciousIframe);
  AssertNoInterstitial();
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, JsNoWarning) {
  SetupThreatOnSubresourceAndNavigate(kMaliciousJsPage, kMaliciousJs);
  AssertNoInterstitial();
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       MainFrameBlockedShouldHaveNoDOMDetailsWhenDontProceed) {
  SetExtendedReportingPrefForTests(browser()->profile()->GetPrefs(), true);
  const bool expect_threat_details =
      SafeBrowsingBlockingPage::ShouldReportThreatDetails(GetThreatType());

  base::RunLoop threat_report_sent_loop;
  if (expect_threat_details)
    SetReportSentCallback(threat_report_sent_loop.QuitClosure());

  // Navigate to a safe page which contains multiple potential DOM details.
  // (Despite the name, kMaliciousPage is not the page flagged as bad in this
  // test.)
  GURL safe_url(embedded_test_server()->GetURL(kMaliciousPage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), safe_url));

  EXPECT_EQ(nullptr, details_factory_.get_details());

  // Start navigation to bad page (kEmptyPage), which will be blocked before it
  // is committed.
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  GURL url = SetupWarningAndNavigate(browser());
  observer.WaitForNavigationFinished();
  ThreatDetails* threat_details = details_factory_.get_details();
  EXPECT_EQ(expect_threat_details, threat_details != nullptr);

  // Go back.
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));
  AssertNoInterstitial();  // Assert the interstitial is gone

  EXPECT_TRUE(IsExtendedReportingEnabled(*browser()->profile()->GetPrefs()));
  EXPECT_EQ(safe_url, browser()
                          ->tab_strip_model()
                          ->GetActiveWebContents()
                          ->GetLastCommittedURL());

  if (expect_threat_details) {
    threat_report_sent_loop.Run();
    std::string serialized = GetReportSent();
    ClientSafeBrowsingReportRequest report;
    ASSERT_TRUE(report.ParseFromString(serialized));
    // Verify the report is complete.
    EXPECT_TRUE(report.complete());
    EXPECT_EQ(url.spec(), report.page_url());
    EXPECT_EQ(url.spec(), report.url());
    ASSERT_EQ(1, report.resources_size());
    EXPECT_EQ(url.spec(), report.resources(0).url());
  }
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       MainFrameBlockedShouldHaveNoDOMDetailsWhenProceeding) {
  SetExtendedReportingPrefForTests(browser()->profile()->GetPrefs(), true);
  const bool expect_threat_details =
      SafeBrowsingBlockingPage::ShouldReportThreatDetails(GetThreatType());

  base::RunLoop threat_report_sent_loop;
  if (expect_threat_details)
    SetReportSentCallback(threat_report_sent_loop.QuitClosure());

  // Navigate to a safe page which contains multiple potential DOM details.
  // (Despite the name, kMaliciousPage is not the page flagged as bad in this
  // test.)
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kMaliciousPage)));

  EXPECT_EQ(nullptr, details_factory_.get_details());

  // Start navigation to bad page (kEmptyPage), which will be blocked before it
  // is committed.
  GURL url = SetupWarningAndNavigate(browser());

  ThreatDetails* threat_details = details_factory_.get_details();
  EXPECT_EQ(expect_threat_details, threat_details != nullptr);

  // Proceed through the warning.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial();  // Assert the interstitial is gone

  EXPECT_TRUE(IsExtendedReportingEnabled(*browser()->profile()->GetPrefs()));
  EXPECT_EQ(url, browser()
                     ->tab_strip_model()
                     ->GetActiveWebContents()
                     ->GetLastCommittedURL());

  if (expect_threat_details) {
    threat_report_sent_loop.Run();
    std::string serialized = GetReportSent();
    ClientSafeBrowsingReportRequest report;
    ASSERT_TRUE(report.ParseFromString(serialized));
    // Verify the report is complete.
    EXPECT_TRUE(report.complete());
    EXPECT_EQ(url.spec(), report.page_url());
    EXPECT_EQ(url.spec(), report.url());
    ASSERT_EQ(1, report.resources_size());
    EXPECT_EQ(url.spec(), report.resources(0).url());
  }
}

// Verifies that the "proceed anyway" link isn't available when it is disabled
// by the corresponding policy. Also verifies that sending the "proceed"
// command anyway doesn't advance to the unsafe site.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, ProceedDisabled) {
  // Simulate a policy disabling the "proceed anyway" link.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingProceedAnywayDisabled, true);

  SetupWarningAndNavigate(browser());

  EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("details"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("final-paragraph"));
  EXPECT_TRUE(Click("details-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("final-paragraph"));
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  SendCommand(security_interstitials::CMD_PROCEED);
  observer.WaitForNavigationFinished();

  // The "proceed" command should go back instead, if proceeding is disabled.
  AssertNoInterstitial();
  EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, NoBackToSafety) {
  SetAlwaysShowBackToSafety(false);
  SetupWarningAndNavigateInNewTab(browser());

  EXPECT_EQ(HIDDEN, GetVisibility("primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("details"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(Click("details-button"));
  EXPECT_EQ(VISIBLE, GetVisibility("details"));
  EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
}

// Verifies that the reporting checkbox is hidden when opt-in is
// disabled by policy. However, reports can still be sent if extended
// reporting is enabled (eg: by its own policy).
// Note: this combination will be deprecated along with the OptInAllowed
// policy, to be replaced by a policy on the SBER setting itself.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       ReportingDisabledByPolicy) {
  SetExtendedReportingPrefForTests(browser()->profile()->GetPrefs(), true);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingOptInAllowed, false);

  base::RunLoop threat_report_sent_loop;
  SetReportSentCallback(threat_report_sent_loop.QuitClosure());

  TestReportingDisabledAndDontProceed(
      embedded_test_server()->GetURL(kEmptyPage));
}

// Verifies that the enhanced protection message is still shown if the page is
// reloaded while the interstitial is showing.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       ReloadWhileInterstitialShowing) {
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingSurveysEnabled, false);
  // Start navigation to bad page (kEmptyPage), which will be blocked before it
  // is committed.
  const GURL url = SetupWarningAndNavigate(browser());

  // Checkbox should be showing.
  EXPECT_EQ(VISIBLE, GetVisibility("enhanced-protection-message"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  // Security indicator should be showing.
  ExpectSecurityIndicatorDowngrade(tab, 0u);

  // Check navigation entry state.
  NavigationController& controller = tab->GetController();
  ASSERT_TRUE(controller.GetVisibleEntry());
  EXPECT_EQ(url, controller.GetVisibleEntry()->GetURL());

  // "Reload" the tab.
  SetupWarningAndNavigate(browser());

  // Checkbox should be showing.
  EXPECT_EQ(VISIBLE, GetVisibility("enhanced-protection-message"));

  // Security indicator should be showing.
  ExpectSecurityIndicatorDowngrade(tab, 0u);
  // Check navigation entry state.
  ASSERT_TRUE(controller.GetVisibleEntry());
  EXPECT_EQ(url, controller.GetVisibleEntry()->GetURL());
}

#if (BUILDFLAG(IS_MAC) && !defined(NDEBUG)) || defined(MEMORY_SANITIZER)
// TODO(crbug.com/40721886): Address flaky timeout.
#define MAYBE_LearnMore DISABLED_LearnMore
#else
#define MAYBE_LearnMore LearnMore
#endif
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, MAYBE_LearnMore) {
  SetupWarningAndNavigate(browser());

  SafeBrowsingBlockingPage* sb_interstitial;
  WebContents* interstitial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(interstitial_tab);

  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          interstitial_tab);
  ASSERT_TRUE(helper);
  sb_interstitial = static_cast<SafeBrowsingBlockingPage*>(
      helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting());

  MockHelpCenterUrl(sb_interstitial);

  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  content::TestNavigationObserver nav_observer(nullptr);
  nav_observer.StartWatchingNewWebContents();
  SendCommand(security_interstitials::CMD_OPEN_HELP_CENTER);
  nav_observer.Wait();

  // A new tab has been opened.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Interstitial does not display in the foreground tab.
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  WebContents* new_tab = browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_tab);
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(new_tab));

  // Interstitial still displays in the background tab.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(interstitial_tab,
            browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       Histograms_DontProceed) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SafeBrowsingMetricsCollector* metrics_collector =
      SafeBrowsingMetricsCollectorFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  EXPECT_EQ(std::nullopt,
            metrics_collector->GetLatestEventTimestamp(
                SafeBrowsingMetricsCollector::EventType::
                    SECURITY_SENSITIVE_SAFE_BROWSING_INTERSTITIAL));
  base::HistogramTester histograms;
  SBThreatType threat_type = GetThreatType();
  std::string prefix = GetHistogramPrefix(threat_type);
  const std::string decision_histogram = "interstitial." + prefix + ".decision";
  const std::string interaction_histogram =
      "interstitial." + prefix + ".interaction";
  const std::string delay_histogram = "interstitial." + prefix + ".show_delay";
  const std::string threat_source = ".from_device_v4";

  // TODO(nparker): Check for *.from_device as well.

  // Histograms should start off empty.
  histograms.ExpectTotalCount(decision_histogram, 0);
  histograms.ExpectTotalCount(interaction_histogram, 0);

  // After navigating to the page, the totals should be set.
  SetupWarningAndNavigate(browser());
  histograms.ExpectTotalCount(decision_histogram, 1);
  histograms.ExpectBucketCount(decision_histogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectBucketCount(
      decision_histogram + kInterstitialPreCommitPageHistogramSuffix,
      security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectTimeBucketCount(delay_histogram, base::TimeDelta::Min(), 1);
  histograms.ExpectTimeBucketCount(delay_histogram + threat_source,
                                   base::TimeDelta::Min(), 1);
  histograms.ExpectTotalCount(interaction_histogram, 2);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::SHOW_ENHANCED_PROTECTION, 1);

  // Check if security sensitive event is added to prefs.
  EXPECT_NE(std::nullopt,
            metrics_collector->GetLatestEventTimestamp(
                SafeBrowsingMetricsCollector::EventType::
                    SECURITY_SENSITIVE_SAFE_BROWSING_INTERSTITIAL));

  // Decision should be recorded.
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));
  AssertNoInterstitial();  // Assert the interstitial is gone
  histograms.ExpectTotalCount(decision_histogram, 2);
  histograms.ExpectBucketCount(
      decision_histogram, security_interstitials::MetricsHelper::DONT_PROCEED,
      1);
  histograms.ExpectBucketCount(
      decision_histogram + kInterstitialPreCommitPageHistogramSuffix,
      security_interstitials::MetricsHelper::DONT_PROCEED, 1);
  histograms.ExpectTotalCount(interaction_histogram, 2);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::SHOW_ENHANCED_PROTECTION, 1);

  // CloseReason histograms.
  histograms.ExpectTotalCount(kInterstitialCloseHistogram, 2);
  histograms.ExpectBucketCount(
      kInterstitialCloseHistogram,
      security_interstitials::SecurityInterstitialTabHelper::
          InterstitialCloseReason::INTERSTITIAL_SHOWN,
      1);
  histograms.ExpectBucketCount(
      kInterstitialCloseHistogram,
      security_interstitials::SecurityInterstitialTabHelper::
          InterstitialCloseReason::NAVIGATE_AWAY,
      1);

  // Check that we are recording the UKM when interstitial is shown and we do
  // not record for the interstitial bypassed.
  auto ukm_entries =
      test_ukm_recorder.GetEntriesByName("SafeBrowsingInterstitial");
  EXPECT_EQ(1u, ukm_entries.size());
  test_ukm_recorder.ExpectEntryMetric(ukm_entries[0], "Shown", true);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       Histograms_Proceed) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  base::HistogramTester histograms;
  SBThreatType threat_type = GetThreatType();
  std::string prefix = GetHistogramPrefix(threat_type);
  const std::string decision_histogram = "interstitial." + prefix + ".decision";
  const std::string interaction_histogram =
      "interstitial." + prefix + ".interaction";
  const std::string delay_histogram = "interstitial." + prefix + ".show_delay";
  const std::string threat_source = ".from_device_v4";

  // Histograms should start off empty.
  histograms.ExpectTotalCount(decision_histogram, 0);
  histograms.ExpectTotalCount(interaction_histogram, 0);

  // After navigating to the page, the totals should be set.
  GURL url = SetupWarningAndNavigate(browser());
  histograms.ExpectTotalCount(decision_histogram, 1);
  histograms.ExpectBucketCount(decision_histogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectBucketCount(
      decision_histogram + kInterstitialPreCommitPageHistogramSuffix,
      security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectTimeBucketCount(delay_histogram, base::TimeDelta::Min(), 1);
  histograms.ExpectTimeBucketCount(delay_histogram + threat_source,
                                   base::TimeDelta::Min(), 1);
  histograms.ExpectTotalCount(interaction_histogram, 2);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::SHOW_ENHANCED_PROTECTION, 1);

  // Decision should be recorded.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial();  // Assert the interstitial is gone.
  histograms.ExpectTotalCount(decision_histogram, 2);
  histograms.ExpectBucketCount(
      decision_histogram, security_interstitials::MetricsHelper::PROCEED, 1);
  histograms.ExpectBucketCount(
      decision_histogram + kInterstitialPreCommitPageHistogramSuffix,
      security_interstitials::MetricsHelper::PROCEED, 1);
  histograms.ExpectTotalCount(interaction_histogram, 2);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::SHOW_ENHANCED_PROTECTION, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::CLOSE_INTERSTITIAL_WITHOUT_UI, 0);

  // CloseReason histograms.
  histograms.ExpectTotalCount(kInterstitialCloseHistogram, 2);
  histograms.ExpectBucketCount(
      kInterstitialCloseHistogram,
      security_interstitials::SecurityInterstitialTabHelper::
          InterstitialCloseReason::INTERSTITIAL_SHOWN,
      1);
  histograms.ExpectBucketCount(
      kInterstitialCloseHistogram,
      security_interstitials::SecurityInterstitialTabHelper::
          InterstitialCloseReason::NAVIGATE_AWAY,
      1);

  // Check that we are recording the UKM when interstitial is shown and we do
  // not record for the interstitial bypassed.
  auto ukm_entries =
      test_ukm_recorder.GetEntriesByName("SafeBrowsingInterstitial");
  EXPECT_EQ(2u, ukm_entries.size());
  test_ukm_recorder.ExpectEntryMetric(ukm_entries[0], "Shown", true);
  test_ukm_recorder.ExpectEntryMetric(ukm_entries[1], "Bypassed", true);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       Histograms_UserMadeNoDecision) {
  base::HistogramTester histograms;
  SBThreatType threat_type = GetThreatType();
  std::string prefix = GetHistogramPrefix(threat_type);
  const std::string interaction_histogram =
      "interstitial." + prefix + ".interaction";

  // Histograms should start off empty.
  histograms.ExpectTotalCount(interaction_histogram, 0);

  // Navigate to the page and show warning.
  GURL url = SetupWarningAndNavigate(browser());

  // Close tab without making an explicit choice on interstitial.
  chrome::CloseTab(browser());
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::CLOSE_INTERSTITIAL_WITHOUT_UI, 1);

  // CloseReason histograms.
  histograms.ExpectTotalCount(kInterstitialCloseHistogram, 2);
  histograms.ExpectBucketCount(
      kInterstitialCloseHistogram,
      security_interstitials::SecurityInterstitialTabHelper::
          InterstitialCloseReason::INTERSTITIAL_SHOWN,
      1);
  histograms.ExpectBucketCount(
      kInterstitialCloseHistogram,
      security_interstitials::SecurityInterstitialTabHelper::
          InterstitialCloseReason::CLOSE_TAB,
      1);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, AllowlistRevisit) {
  GURL url = SetupWarningAndNavigate(browser());

  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial();  // Assert the interstitial is gone.
  EXPECT_EQ(url, browser()
                     ->tab_strip_model()
                     ->GetActiveWebContents()
                     ->GetLastCommittedURL());

  // Unrelated pages should not be allowlisted now.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kUnrelatedUrl)));
  AssertNoInterstitial();

  // The allowlisted page should remain allowlisted.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  AssertNoInterstitial();
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, AllowlistUnsaved) {
  GURL url = SetupWarningAndNavigate(browser());

  // Navigate without making a decision.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kUnrelatedUrl)));
  AssertNoInterstitial();

  // The non-allowlisted page should now show an interstitial.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(WaitForReady(browser()));
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial();
}

#if (BUILDFLAG(IS_MAC) && !defined(NDEBUG)) || defined(MEMORY_SANITIZER)
// TODO(crbug.com/40721886): Address flay failure.
#define MAYBE_VerifyHitReportSentOnSBERAndNotIncognito \
  DISABLED_VerifyHitReportSentOnSBERAndNotIncognito
#else
#define MAYBE_VerifyHitReportSentOnSBERAndNotIncognito \
  VerifyHitReportSentOnSBERAndNotIncognito
#endif
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       MAYBE_VerifyHitReportSentOnSBERAndNotIncognito) {
  // The extended reporting opt-in is presented in the interstitial for malware,
  // phishing, and UwS threats.
  const bool expect_threat_details =
      SafeBrowsingBlockingPage::ShouldReportThreatDetails(GetThreatType());

  base::RunLoop threat_report_sent_loop;
  if (expect_threat_details)
    SetReportSentCallback(threat_report_sent_loop.QuitClosure());

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingScoutReportingEnabled, true);
  GURL url = SetupWarningAndNavigate(browser());  // not incognito
  EXPECT_TRUE(hit_report_sent());
  EXPECT_TRUE(report_sent());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       VerifyHitReportNotSentOnIncognito) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingSurveysEnabled, false);
  // The extended reporting opt-in is presented in the interstitial for malware,
  // phishing, and UwS threats.
  const bool expect_threat_details =
      SafeBrowsingBlockingPage::ShouldReportThreatDetails(GetThreatType());

  base::RunLoop threat_report_sent_loop;
  if (expect_threat_details)
    SetReportSentCallback(threat_report_sent_loop.QuitClosure());

  Browser* incognito_browser = CreateIncognitoBrowser();
  incognito_browser->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingScoutReportingEnabled, true);   // set up SBER
  GURL url = SetupWarningAndNavigate(incognito_browser);  // incognito
  // Check enhanced protection message is not shown.
  EXPECT_EQ(HIDDEN, ::safe_browsing::GetVisibility(
                        incognito_browser, "enhanced-protection-message"));

  EXPECT_FALSE(hit_report_sent());
  EXPECT_FALSE(report_sent());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       VerifyHitReportNotSentWithoutSBER) {
  // The extended reporting opt-in is presented in the interstitial for malware,
  // phishing, and UwS threats.
  const bool expect_threat_details =
      SafeBrowsingBlockingPage::ShouldReportThreatDetails(GetThreatType());

  base::RunLoop threat_report_sent_loop;
  if (expect_threat_details)
    SetReportSentCallback(threat_report_sent_loop.QuitClosure());

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingScoutReportingEnabled, false);  // set up SBER
  GURL url = SetupWarningAndNavigate(browser());          // not incognito
  EXPECT_FALSE(hit_report_sent());
  EXPECT_FALSE(report_sent());
}

namespace {

class SecurityStyleTestObserver : public content::WebContentsObserver {
 public:
  explicit SecurityStyleTestObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  SecurityStyleTestObserver(const SecurityStyleTestObserver&) = delete;
  SecurityStyleTestObserver& operator=(const SecurityStyleTestObserver&) =
      delete;

  std::optional<security_state::SecurityLevel> latest_security_level() const {
    return latest_security_level_;
  }

  // WebContentsObserver:
  void DidChangeVisibleSecurityState() override {
    auto* helper = SecurityStateTabHelper::FromWebContents(web_contents());
    latest_security_level_ = helper->GetSecurityLevel();
  }

 private:
  std::optional<security_state::SecurityLevel> latest_security_level_;
};

}  // namespace

// Test that the security indicator does not stay downgraded after
// clicking back from a Safe Browsing interstitial. Regression test for
// https://crbug.com/659709.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       SecurityStateGoBack) {
  // Navigate to a page so that there is somewhere to go back to.
  GURL start_url = GURL(kUnrelatedUrl);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));

  // The security indicator should be downgraded while the interstitial shows.
  GURL bad_url = embedded_test_server()->GetURL(kEmptyPage);
  SetupWarningAndNavigate(browser());
  WebContents* error_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(error_tab);
  ExpectSecurityIndicatorDowngrade(error_tab, 0u);
  content::NavigationEntry* entry =
      error_tab->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  ASSERT_EQ(bad_url, entry->GetURL());

  // Go back.
  EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("details"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(Click("details-button"));
  EXPECT_EQ(VISIBLE, GetVisibility("details"));
  EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));

  // The security indicator should *not* still be downgraded after going back.
  AssertNoInterstitial();
  WebContents* post_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(post_tab);
  entry = post_tab->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(start_url, entry->GetURL());
  ExpectNoSecurityIndicatorDowngrade(post_tab);

  ClearBadURL(bad_url);
  // Navigate to the URL that the interstitial was on, and check that it
  // is no longer marked as dangerous.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), bad_url));
  ExpectNoSecurityIndicatorDowngrade(
      browser()->tab_strip_model()->GetActiveWebContents());
}

// Test that the security indicator is downgraded after clicking through a
// Safe Browsing interstitial.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       SecurityState_HTTP) {
  // The security indicator should be downgraded while the interstitial shows.
  SetupWarningAndNavigate(browser());
  WebContents* error_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(error_tab);
  ExpectSecurityIndicatorDowngrade(error_tab, 0u);

  // The security indicator should still be downgraded post-interstitial.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial();
  WebContents* post_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(post_tab);
  ExpectSecurityIndicatorDowngrade(post_tab, 0u);
}

// Test that the security indicator is downgraded even if the website has valid
// HTTPS (meaning that the SB state overrides the HTTPS state).
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       SecurityState_ValidHTTPS) {
  WebContents* error_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(error_tab);
  SecurityStyleTestObserver observer(error_tab);

  // The security indicator should be downgraded while the interstitial shows.
  SetupWarningAndNavigateToValidHTTPS();
  ExpectSecurityIndicatorDowngrade(error_tab, 0u);

  // The security indicator should still be downgraded post-interstitial.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial();
  WebContents* post_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(post_tab);
  ExpectSecurityIndicatorDowngrade(post_tab, 0u);
}

// Test that the security indicator is still downgraded after two interstitials
// are shown in a row (one for Safe Browsing, one for invalid HTTPS).
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       SecurityState_InvalidHTTPS) {
  // The security indicator should be downgraded while the interstitial shows.
  SetupWarningAndNavigateToInvalidHTTPS();
  WebContents* error_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(error_tab);
  ExpectSecurityIndicatorDowngrade(error_tab, 0u);

  // The security indicator should still be downgraded post-interstitial.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial();
  WebContents* post_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(post_tab);
  // TODO(felt): Sometimes the cert status here is 0u, which is wrong.
  // Filed https://crbug.com/641187 to investigate.
  ExpectSecurityIndicatorDowngrade(post_tab, net::CERT_STATUS_INVALID);
}

// Test that no safe browsing interstitial will be shown, if URL matches
// enterprise safe browsing allowlist domains.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       VerifyEnterpriseAllowlist) {
  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  // Add test server domain into the enterprise allowlist.
  base::Value::List allowlist;
  allowlist.Append(url.host());
  browser()->profile()->GetPrefs()->SetList(
      prefs::kSafeBrowsingAllowlistDomains, std::move(allowlist));

  SetURLThreatType(url, GetThreatType());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  base::RunLoop().RunUntilIdle();
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(
      content::WaitForRenderFrameReady(contents->GetPrimaryMainFrame()));
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));
}

INSTANTIATE_TEST_SUITE_P(
    SafeBrowsingBlockingPageBrowserTestWithThreatTypeAndIsolationSetting,
    SafeBrowsingBlockingPageBrowserTest,
    testing::Combine(
        testing::Values(
            SBThreatType::SB_THREAT_TYPE_URL_MALWARE,  // Threat types
            SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
            SBThreatType::SB_THREAT_TYPE_URL_UNWANTED),
        testing::Bool()));  // If isolate all sites for testing.

// Check back and forward work correctly after clicking through an interstitial.
#if (BUILDFLAG(IS_MAC) && !defined(NDEBUG)) || defined(MEMORY_SANITIZER)
// TODO(crbug.com/40721886): Address flay failure.
#define MAYBE_NavigatingBackAndForth DISABLED_NavigatingBackAndForth
#else
#define MAYBE_NavigatingBackAndForth NavigatingBackAndForth
#endif
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       MAYBE_NavigatingBackAndForth) {
  // Load a safe page. (Despite the name, kMaliciousPage is not the page flagged
  // as bad in this test.)
  GURL safe_url(embedded_test_server()->GetURL(kMaliciousPage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), safe_url));
  // Navigate to a site that triggers a warning and click through it.
  const GURL bad_url = SetupWarningAndNavigate(browser());
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial();
  // Go back and check we are back on the safe site.
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver back_observer(contents);
  contents->GetController().GoBack();
  back_observer.Wait();
  EXPECT_EQ(safe_url, contents->GetLastCommittedURL());
  // Check forward takes us back to the flagged site with no interstitial.
  content::TestNavigationObserver forward_observer(contents);
  contents->GetController().GoForward();
  forward_observer.Wait();
  WaitForReady(browser());
  AssertNoInterstitial();
  EXPECT_EQ(bad_url, contents->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       TimestampInCSBRRClickedThroughBlockingPage) {
  SetExtendedReportingPrefForTests(browser()->profile()->GetPrefs(), true);
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  SetupWarningAndNavigate(browser());

  // Proceed to unsafe site, sending CSBRR.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  observer.WaitForNavigationFinished();

  // The "proceed" command should go back instead, if proceeding is disabled.
  AssertNoInterstitial();

  base::RunLoop threat_report_sent_loop;
  SetReportSentCallback(threat_report_sent_loop.QuitClosure());

  threat_report_sent_loop.Run();
  std::string serialized = GetReportSent();
  ClientSafeBrowsingReportRequest report;
  ASSERT_TRUE(report.ParseFromString(serialized));
  // The timstamp of the warning shown should be in CSBRRs.
  EXPECT_TRUE(report.has_warning_shown_timestamp_msec());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       FallbackCSBRRSentWithExpectedFieldsPopulated) {
  SetExtendedReportingPrefForTests(browser()->profile()->GetPrefs(), true);
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  base::RunLoop threat_report_sent_loop;
  SetReportSentCallback(threat_report_sent_loop.QuitClosure());
  SetupWarningAndNavigate(browser());
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));

  // Send CSBRR without interactions.
  chrome::CloseTab(browser());
  observer.WaitForNavigationFinished();
  threat_report_sent_loop.Run();

  std::string serialized = GetReportSent();
  ClientSafeBrowsingReportRequest report;
  ASSERT_TRUE(report.ParseFromString(serialized));
  // The timstamp of the warning shown should be in CSBRRs.
  EXPECT_TRUE(report.has_warning_shown_timestamp_msec());

  // The `client_properties` field should be populated in fallback CSBRRs.
  EXPECT_TRUE(report.has_client_properties());
  EXPECT_TRUE(report.client_properties().has_url_api_type());
  EXPECT_TRUE(report.client_properties().has_is_async_check());
  EXPECT_EQ(report.client_properties().url_api_type(),
            ClientSafeBrowsingReportRequest::PVER4_NATIVE);
  EXPECT_FALSE(report.client_properties().is_async_check());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       IgnoreFutureAutoRevocation) {
  GURL url = SetupWarningAndNavigate(browser());
  EXPECT_FALSE(
      safety_hub_util::IsAbusiveNotificationRevocationIgnored(hcsm(), url));
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial();  // Assert the interstitial is gone.
  if (GetThreatType() == SBThreatType::SB_THREAT_TYPE_URL_PHISHING) {
    EXPECT_TRUE(
        safety_hub_util::IsAbusiveNotificationRevocationIgnored(hcsm(), url));
  } else {
    EXPECT_FALSE(
        safety_hub_util::IsAbusiveNotificationRevocationIgnored(hcsm(), url));
  }
}

class AntiPhishingTelemetryBrowserTest
    : public SafeBrowsingBlockingPageBrowserTest {};

INSTANTIATE_TEST_SUITE_P(
    AntiPhishingTelemetryBrowserTestWithThreatTypeAndIsolationSetting,
    AntiPhishingTelemetryBrowserTest,
    testing::Combine(
        testing::Values(
            SBThreatType::SB_THREAT_TYPE_URL_PHISHING,  // Threat types
            SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING),
        testing::Bool()));  // If isolate all sites for testing.

IN_PROC_BROWSER_TEST_P(AntiPhishingTelemetryBrowserTest,
                       CheckReportListsInteractions) {
  SetExtendedReportingPrefForTests(browser()->profile()->GetPrefs(), true);
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  SetupWarningAndNavigate(browser());

  // Show details 3x to make sure map records all 3 occurrences in interstitial
  // interaction map.
  EXPECT_TRUE(Click("details-button"));
  SendCommand(security_interstitials::CMD_SHOW_MORE_SECTION);
  SendCommand(security_interstitials::CMD_SHOW_MORE_SECTION);

  // Proceed to unsafe site, sending CSBRR.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  observer.WaitForNavigationFinished();

  // The "proceed" command should go back instead, if proceeding is disabled.
  AssertNoInterstitial();

  scoped_refptr<content::MessageLoopRunner> threat_report_sent_runner(
      new content::MessageLoopRunner);
  SetReportSentCallback(threat_report_sent_runner->QuitClosure());

  threat_report_sent_runner->Run();
  std::string serialized = GetReportSent();
  ClientSafeBrowsingReportRequest report;
  ASSERT_TRUE(report.ParseFromString(serialized));

  EXPECT_EQ(report.url(), embedded_test_server()->GetURL(kEmptyPage));
  SBThreatType threat_type = GetThreatType();
  // SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING does not set the page_url because
  // its resource's navigation_url is empty.
  if (threat_type == SBThreatType::SB_THREAT_TYPE_URL_PHISHING) {
    EXPECT_EQ(report.page_url(), embedded_test_server()->GetURL(kEmptyPage));
  }
  // Create sorted vector of interstitial interactions. Sorted by
  // security_interstitial_interaction numeric value.
  std::vector<ClientSafeBrowsingReportRequest::InterstitialInteraction>
      interactions;
  for (auto interaction : report.interstitial_interactions()) {
    interactions.push_back(interaction);
  }
  std::sort(
      interactions.begin(), interactions.end(),
      [](const ClientSafeBrowsingReportRequest::InterstitialInteraction& a,
         const ClientSafeBrowsingReportRequest::InterstitialInteraction& b)
          -> bool {
        return a.security_interstitial_interaction() <
               b.security_interstitial_interaction();
      });

  // Verify the report interactions are complete and correct.
  EXPECT_EQ(report.interstitial_interactions_size(), 2);
  VerifyInteractionOccurrenceCount(
      report, interactions[0],
      ClientSafeBrowsingReportRequest::InterstitialInteraction::CMD_PROCEED, 1);
  VerifyInteractionOccurrenceCount(
      report, interactions[1],
      ClientSafeBrowsingReportRequest::InterstitialInteraction::
          CMD_SHOW_MORE_SECTION,
      3);
}

IN_PROC_BROWSER_TEST_P(AntiPhishingTelemetryBrowserTest,
                       CheckReportCloseTabOnInterstitial) {
  SetExtendedReportingPrefForTests(browser()->profile()->GetPrefs(), true);
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  scoped_refptr<content::MessageLoopRunner> threat_report_sent_runner(
      new content::MessageLoopRunner);
  SetReportSentCallback(threat_report_sent_runner->QuitClosure());
  SetupWarningAndNavigate(browser());
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));

  // Send CSBRR without interactions.
  chrome::CloseTab(browser());
  observer.WaitForNavigationFinished();
  threat_report_sent_runner->Run();

  std::string serialized = GetReportSent();
  ClientSafeBrowsingReportRequest report;
  ASSERT_TRUE(report.ParseFromString(serialized));

  EXPECT_EQ(report.url(), embedded_test_server()->GetURL(kEmptyPage));
  // Verify the report interactions only contain interstitial interactions.
  SBThreatType threat_type = GetThreatType();
  if (threat_type == SBThreatType::SB_THREAT_TYPE_URL_PHISHING) {
    EXPECT_EQ(report.type(),
              ClientSafeBrowsingReportRequest_ReportType_URL_PHISHING);
    EXPECT_EQ(report.page_url(), embedded_test_server()->GetURL(kEmptyPage));
  }
  if (threat_type == SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING) {
    EXPECT_EQ(
        report.type(),
        ClientSafeBrowsingReportRequest_ReportType_URL_CLIENT_SIDE_PHISHING);
  }
  EXPECT_EQ(report.interstitial_interactions_size(), 1);
  EXPECT_EQ(
      report.interstitial_interactions(0).security_interstitial_interaction(),
      ClientSafeBrowsingReportRequest::InterstitialInteraction::
          CMD_CLOSE_INTERSTITIAL_WITHOUT_UI);
  EXPECT_EQ(report.interstitial_interactions(0).occurrence_count(), 1);
}

IN_PROC_BROWSER_TEST_P(
    AntiPhishingTelemetryBrowserTest,
    CheckReportListsInteractionsNoExplicitInterstitialDecision) {
  SetExtendedReportingPrefForTests(browser()->profile()->GetPrefs(), true);

  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  scoped_refptr<content::MessageLoopRunner> threat_report_sent_runner(
      new content::MessageLoopRunner);
  SetReportSentCallback(threat_report_sent_runner->QuitClosure());

  // Navigate to the page and show warning.
  SetupWarningAndNavigate(browser());

  // Navigate away from interstitial without making an explicit choice through
  // the UI.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  observer.WaitForNavigationFinished();
  threat_report_sent_runner->Run();
  std::string serialized = GetReportSent();
  ClientSafeBrowsingReportRequest report;
  ASSERT_TRUE(report.ParseFromString(serialized));

  EXPECT_EQ(report.url(), embedded_test_server()->GetURL(kEmptyPage));
  SBThreatType threat_type = GetThreatType();
  if (threat_type == SBThreatType::SB_THREAT_TYPE_URL_PHISHING) {
    EXPECT_EQ(report.page_url(), embedded_test_server()->GetURL(kEmptyPage));
  }
  // Verify the report interaction only contains a
  // CMD_CLOSE_INTERSTITIAL_WITHOUT_UI interaction.
  EXPECT_EQ(report.interstitial_interactions_size(), 1);
  EXPECT_EQ(
      report.interstitial_interactions(0).security_interstitial_interaction(),
      ClientSafeBrowsingReportRequest::InterstitialInteraction::
          CMD_CLOSE_INTERSTITIAL_WITHOUT_UI);
  EXPECT_EQ(report.interstitial_interactions(0).occurrence_count(), 1);
}

INSTANTIATE_TEST_SUITE_P(
    SafeBrowsingHatsSurveyBrowserTestWithThreatTypeAndIsolationSetting,
    SafeBrowsingHatsSurveyBrowserTest,
    testing::Combine(
        // Threat types.
        testing::Values(SBThreatType::SB_THREAT_TYPE_URL_MALWARE),
        // If isolate all sites for testing.
        testing::Bool()));

IN_PROC_BROWSER_TEST_P(SafeBrowsingHatsSurveyBrowserTest,
                       ReportNotSentToSbButAttachedForHats) {
  SetExtendedReportingPrefForTests(browser()->profile()->GetPrefs(), false);
  SetExpectEmptyReportForHats(false);
  SetExpectReportUrlForHats(true);
  SetExpectInterstitialInteractions(true);
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  SetupWarningAndNavigate(browser());
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_CALL(*GetSafeBrowsingUiManager(), OnAttachThreatDetailsAndLaunchSurvey)
      .Times(1);

  // Generate interstitial interactions.
  EXPECT_TRUE(Click("details-button"));
  SendCommand(security_interstitials::CMD_SHOW_MORE_SECTION);
  SendCommand(security_interstitials::CMD_SHOW_MORE_SECTION);

  // Bypass warning.
  // This triggers AttachThreatDetailsAndLaunchSurvey.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  observer.WaitForNavigationFinished();

  std::string report = GetReportSent();
  EXPECT_TRUE(report.empty());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingHatsSurveyBrowserTest,
                       ReportSentToSbAndAttachedForHats) {
  SetExtendedReportingPrefForTests(browser()->profile()->GetPrefs(), true);
  SetExpectEmptyReportForHats(false);
  SetExpectReportUrlForHats(true);
  SetExpectInterstitialInteractions(true);
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  scoped_refptr<content::MessageLoopRunner> threat_report_sent_runner(
      new content::MessageLoopRunner);
  SetReportSentCallback(threat_report_sent_runner->QuitClosure());
  SetupWarningAndNavigate(browser());
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_CALL(*GetSafeBrowsingUiManager(), OnAttachThreatDetailsAndLaunchSurvey)
      .Times(1);
  // Generate interstitial interactions.
  EXPECT_TRUE(Click("details-button"));
  SendCommand(security_interstitials::CMD_SHOW_MORE_SECTION);
  SendCommand(security_interstitials::CMD_SHOW_MORE_SECTION);

  // Bypass warning.
  // This triggers AttachThreatDetailsAndLaunchSurvey.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  observer.WaitForNavigationFinished();
  threat_report_sent_runner->Run();
  std::string report = GetReportSent();
  EXPECT_FALSE(report.empty());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingHatsSurveyBrowserTest,
                       NoHatsSurveyWhenProceedDisabled) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingProceedAnywayDisabled, true);
  SetExtendedReportingPrefForTests(browser()->profile()->GetPrefs(), false);
  SetExpectEmptyReportForHats(true);
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  SetupWarningAndNavigate(browser());
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_CALL(*GetSafeBrowsingUiManager(), OnAttachThreatDetailsAndLaunchSurvey)
      .Times(0);

  // Generate interstitial interactions.
  EXPECT_TRUE(Click("details-button"));
  SendCommand(security_interstitials::CMD_SHOW_MORE_SECTION);
  SendCommand(security_interstitials::CMD_SHOW_MORE_SECTION);

  // Navigate away from the page.
  // This would trigger AttachThreatDetailsAndLaunchSurvey but does not because
  // proceed is disabled.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  observer.WaitForNavigationFinished();
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingHatsSurveyBrowserTest,
                       NoHatsSurveyWhenSafeBrowsingSurveysDisabled) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingSurveysEnabled, false);
  SetExtendedReportingPrefForTests(browser()->profile()->GetPrefs(), false);
  SetExpectEmptyReportForHats(true);
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  SetupWarningAndNavigate(browser());
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_CALL(*GetSafeBrowsingUiManager(), OnAttachThreatDetailsAndLaunchSurvey)
      .Times(0);

  // Generate interstitial interactions.
  EXPECT_TRUE(Click("details-button"));
  SendCommand(security_interstitials::CMD_SHOW_MORE_SECTION);
  SendCommand(security_interstitials::CMD_SHOW_MORE_SECTION);

  // Bypass warning.
  // This would trigger AttachThreatDetailsAndLaunchSurvey but does not because
  // Safe Browsing surveys are disabled.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  observer.WaitForNavigationFinished();
}

class TrustSafetySentimentSurveyV2BrowserTest
    : public SafeBrowsingBlockingPageBrowserTest {
 public:
  TrustSafetySentimentSurveyV2BrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kTrustSafetySentimentSurveyV2);
  }
  ~TrustSafetySentimentSurveyV2BrowserTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    metrics::DesktopSessionDurationTracker::Initialize();
#endif
    SafeBrowsingBlockingPageBrowserTest::SetUp();
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
#endif
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    TrustSafetySentimentSurveyV2BrowserTestWithThreatTypeAndIsolationSetting,
    TrustSafetySentimentSurveyV2BrowserTest,
    testing::Combine(
        testing::Values(
            SBThreatType::SB_THREAT_TYPE_URL_PHISHING,  // Threat types
            SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING,
            SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
            SBThreatType::SB_THREAT_TYPE_URL_UNWANTED),
        testing::Bool()));  // If isolate all sites for testing.

IN_PROC_BROWSER_TEST_P(TrustSafetySentimentSurveyV2BrowserTest,
                       TrustSafetySentimentTriggerredOnProceed) {
  GURL url = SetupWarningAndNavigate(browser());
  EXPECT_CALL(*mock_sentiment_service(),
              InteractedWithSafeBrowsingInterstitial(/*did_proceed=*/true,
                                                     GetThreatType()));
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial();  // Assert the interstitial is gone.
}

IN_PROC_BROWSER_TEST_P(TrustSafetySentimentSurveyV2BrowserTest,
                       TrustSafetySentimentTriggerredOnPrimaryButtonClick) {
  GURL url = SetupWarningAndNavigate(browser());
  EXPECT_CALL(*mock_sentiment_service(),
              InteractedWithSafeBrowsingInterstitial(/*did_proceed=*/false,
                                                     GetThreatType()));
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));
  AssertNoInterstitial();  // Assert the interstitial is gone.
}

IN_PROC_BROWSER_TEST_P(TrustSafetySentimentSurveyV2BrowserTest,
                       TrustSafetySentimentTriggeredOnCloseInterstitialTab) {
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  GURL url = SetupWarningAndNavigate(browser());
  EXPECT_CALL(*mock_sentiment_service(),
              InteractedWithSafeBrowsingInterstitial(/*did_proceed=*/false,
                                                     GetThreatType()));
  chrome::CloseTab(browser());
  observer.WaitForNavigationFinished();
}

using RedInterstitialUIBrowserTest = SafeBrowsingBlockingPageBrowserTest;

INSTANTIATE_TEST_SUITE_P(
    RedInterstitialUIBrowserTestWithThreatTypeAndIsolationSetting,
    RedInterstitialUIBrowserTest,
    testing::Combine(
        testing::Values(
            SBThreatType::SB_THREAT_TYPE_URL_PHISHING,  // Threat types
            SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
            SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING,
            SBThreatType::SB_THREAT_TYPE_URL_UNWANTED),
        testing::Bool()));  // If isolate all sites for testing.

IN_PROC_BROWSER_TEST_P(RedInterstitialUIBrowserTest,
                       TestInterstitialPageStringsEnhancedEnabled) {
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  SetReportSentCallback(threat_report_sent_runner->QuitClosure());
  GURL url = SetupWarningAndNavigate(browser());

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  SafeBrowsingBlockingPage* interstitial_page;
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          contents);
  ASSERT_TRUE(helper);
  interstitial_page = static_cast<SafeBrowsingBlockingPage*>(
      helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting());
  BaseSafeBrowsingErrorUI* temp_var = interstitial_page->sb_error_ui();
  base::Value::Dict load_time_data;
  temp_var->PopulateStringsForHtml(load_time_data);

  // Safe browsing blocking page should use correct heading and primary,
  // explanation, and proceed paragraph strings.
  ASSERT_EQ(
      load_time_data.Find("heading")->GetString(),
      base::UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_SAFEBROWSING_HEADING)));
  SBThreatType threat_type = GetThreatType();
  if (threat_type == SBThreatType::SB_THREAT_TYPE_URL_PHISHING ||
      threat_type == SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING) {
    ASSERT_EQ(load_time_data.Find("primaryParagraph")->GetString(),
              base::UTF16ToUTF8(l10n_util::GetStringUTF16(
                  IDS_PHISHING_V4_PRIMARY_PARAGRAPH)));
    ASSERT_EQ(load_time_data.Find("explanationParagraph")->GetString(),
              base::UTF16ToUTF8(l10n_util::GetStringUTF16(
                  IDS_PHISHING_V4_EXPLANATION_PARAGRAPH)));
    ASSERT_EQ(load_time_data.Find("finalParagraph")->GetString(),
              base::UTF16ToUTF8(l10n_util::GetStringUTF16(
                  IDS_PHISHING_V4_PROCEED_PARAGRAPH)));
  } else if (threat_type == SBThreatType::SB_THREAT_TYPE_URL_MALWARE) {
    ASSERT_EQ(load_time_data.Find("primaryParagraph")->GetString(),
              base::UTF16ToUTF8(
                  l10n_util::GetStringUTF16(IDS_MALWARE_V3_PRIMARY_PARAGRAPH)));
    ASSERT_EQ(load_time_data.Find("explanationParagraph")->GetString(),
              base::UTF16ToUTF8(l10n_util::GetStringUTF16(
                  IDS_MALWARE_V3_EXPLANATION_PARAGRAPH)));
    ASSERT_EQ(load_time_data.Find("finalParagraph")->GetString(),
              base::UTF16ToUTF8(
                  l10n_util::GetStringUTF16(IDS_MALWARE_V3_PROCEED_PARAGRAPH)));
  } else {
    ASSERT_EQ(load_time_data.Find("primaryParagraph")->GetString(),
              base::UTF16ToUTF8(
                  l10n_util::GetStringUTF16(IDS_HARMFUL_V3_PRIMARY_PARAGRAPH)));
    ASSERT_EQ(load_time_data.Find("explanationParagraph")->GetString(),
              base::UTF16ToUTF8(l10n_util::GetStringUTF16(
                  IDS_HARMFUL_V3_EXPLANATION_PARAGRAPH)));
    ASSERT_EQ(load_time_data.Find("finalParagraph")->GetString(),
              base::UTF16ToUTF8(
                  l10n_util::GetStringUTF16(IDS_HARMFUL_V3_PROCEED_PARAGRAPH)));
  }
}

IN_PROC_BROWSER_TEST_P(RedInterstitialUIBrowserTest,
                       TestInterstitialPageStringsStandardEnabled) {
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  SetReportSentCallback(threat_report_sent_runner->QuitClosure());
  GURL url = SetupWarningAndNavigate(browser());

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  SafeBrowsingBlockingPage* interstitial_page;
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          contents);
  ASSERT_TRUE(helper);
  interstitial_page = static_cast<SafeBrowsingBlockingPage*>(
      helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting());
  BaseSafeBrowsingErrorUI* temp_var = interstitial_page->sb_error_ui();
  base::Value::Dict load_time_data;
  temp_var->PopulateStringsForHtml(load_time_data);

  // Safe browsing blocking page should use correct header and enhanced
  // protection promo message strings.
  ASSERT_EQ(
      load_time_data.Find("heading")->GetString(),
      base::UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_SAFEBROWSING_HEADING)));
  ASSERT_EQ(
      load_time_data.Find(security_interstitials::kEnhancedProtectionMessage)
          ->GetString(),
      base::UTF16ToUTF8(l10n_util::GetStringUTF16(
          IDS_SAFE_BROWSING_ENHANCED_PROTECTION_MESSAGE)));
}

class SafeBrowsingBlockingPageDelayedWarningBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<
          testing::tuple<bool /* IsolateAllSitesForTesting */,
                         bool /* Show warning on mouse click */>> {
 public:
  SafeBrowsingBlockingPageDelayedWarningBrowserTest() = default;

  SafeBrowsingBlockingPageDelayedWarningBrowserTest(
      const SafeBrowsingBlockingPageDelayedWarningBrowserTest&) = delete;
  SafeBrowsingBlockingPageDelayedWarningBrowserTest& operator=(
      const SafeBrowsingBlockingPageDelayedWarningBrowserTest&) = delete;

  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    if (warning_on_mouse_click_enabled()) {
      enabled_features.push_back(base::test::FeatureRefAndParams(
          kDelayedWarnings, {{"mouse", "true"}}));
    } else {
      enabled_features.push_back(
          base::test::FeatureRefAndParams(kDelayedWarnings, {}));
    }

    std::vector<base::test::FeatureRef> disabled_features;
    GetAdditionalFeatures(&enabled_features, &disabled_features);

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (testing::get<0>(GetParam())) {
      content::IsolateAllSitesForTesting(command_line);
    }
    // TODO(crbug.com/40285326): This fails with the field trial testing config.
    command_line->AppendSwitch("disable-field-trial-config");
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    InProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    // Test UI manager and test database manager should be set before
    // the browser is started but after threads are created.
    factory_.SetTestUIManager(new FakeSafeBrowsingUIManager(
        std::make_unique<TestSafeBrowsingBlockingPageFactory>()));
    factory_.SetTestDatabaseManager(new FakeSafeBrowsingDatabaseManager(
        content::GetUIThreadTaskRunner({})));
    SafeBrowsingService::RegisterFactory(&factory_);
    ThreatDetails::RegisterFactory(&details_factory_);
  }

  static bool TypeAndWaitForInterstitial(Browser* browser) {
    // Type something. An interstitial should be shown.
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver observer(contents);
    input::NativeWebKeyboardEvent event(
        blink::WebKeyboardEvent::Type::kRawKeyDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    event.text[0] = 'a';
    content::RenderWidgetHost* rwh =
        contents->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget();
    rwh->ForwardKeyboardEvent(event);
    observer.WaitForNavigationFinished();
    return WaitForReady(browser);
  }

  static void MouseClick(Browser* browser) {
    blink::WebMouseEvent event(
        blink::WebInputEvent::Type::kMouseDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    event.button = blink::WebMouseEvent::Button::kLeft;
    event.SetPositionInWidget(100, 100);
    event.click_count = 1;
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::RenderWidgetHost* rwh =
        contents->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget();
    rwh->ForwardMouseEvent(event);
  }

  static bool MouseClickAndWaitForInterstitial(Browser* browser) {
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver observer(contents);
    MouseClick(browser);
    observer.WaitForNavigationFinished();
    return WaitForReady(browser);
  }

  static bool FullscreenAndWaitForInterstitial(Browser* browser) {
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver observer(contents);
    const char* const kScript = "document.body.webkitRequestFullscreen()";
    EXPECT_TRUE(content::ExecJs(contents, kScript));
    observer.WaitForNavigationFinished();
    return WaitForReady(browser);
  }

  static bool RequestPermissionAndWaitForInterstitial(Browser* browser) {
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver observer(contents);
    const char* const kScript = "Notification.requestPermission(function(){})";
    EXPECT_TRUE(content::ExecJs(contents, kScript));
    observer.WaitForNavigationFinished();
    return WaitForReady(browser);
  }

  static bool RequestDesktopCaptureAndWaitForInterstitial(Browser* browser) {
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver observer(contents);
    const char* const kScript = "navigator.mediaDevices.getDisplayMedia()";
    EXPECT_TRUE(content::ExecJs(contents, kScript,
                                content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
    observer.WaitForNavigationFinished();
    return WaitForReady(browser);
  }

 protected:
  // Subclasses can override to enable/disable features in SetUp().
  virtual void GetAdditionalFeatures(
      std::vector<base::test::FeatureRefAndParams>* enabled_features,
      std::vector<base::test::FeatureRef>* disabled_features) {}

  // Initiates a download and waits for it to be completed or cancelled.
  static void DownloadAndWaitForNavigation(Browser* browser) {
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver observer(contents);
    content::WebContentsConsoleObserver console_observer(contents);
    console_observer.SetPattern(
        "A SafeBrowsing warning is pending on this page*");

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser, GURL("data:application/octet-stream;base64,SGVsbG8=")));
    observer.WaitForNavigationFinished();
    ASSERT_TRUE(console_observer.Wait());

    ASSERT_EQ(1u, console_observer.messages().size());
  }

  void NavigateAndAssertNoInterstitial() {
    const GURL top_frame = embedded_test_server()->GetURL("/iframe.html");
    SetURLThreatType(top_frame, SBThreatType::SB_THREAT_TYPE_URL_PHISHING);

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), top_frame));
    AssertNoInterstitial(browser());
  }

  bool warning_on_mouse_click_enabled() const {
    return testing::get<1>(GetParam());
  }

  void SetURLThreatType(const GURL& url, SBThreatType threat_type) {
    TestSafeBrowsingService* service = factory_.test_safe_browsing_service();
    ASSERT_TRUE(service);

    static_cast<FakeSafeBrowsingDatabaseManager*>(
        service->database_manager().get())
        ->AddDangerousUrl(url, threat_type);
  }

  std::u16string GetSecuritySummaryTextFromPageInfo() {
    auto* page_info = PageInfoBubbleView::GetPageInfoBubbleForTesting();
    auto* summary_label = page_info->GetViewByID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_SECURITY_SUMMARY_LABEL);
    return static_cast<views::StyledLabel*>(summary_label)->GetText();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  TestSafeBrowsingServiceFactory factory_;
  TestThreatDetailsFactory details_factory_;
};

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       NoInteraction_WarningNotShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Navigate away without interacting with the page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       NotPhishing_WarningNotDelayed) {
  base::HistogramTester histograms;

  // Navigate to a non-phishing page. The warning should not be delayed.
  const GURL url = embedded_test_server()->GetURL("/empty.html");
  SetURLThreatType(url, SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(WaitForReady(browser()));

  // Navigate to about:blank to "flush" metrics, if any.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
}

// Close the tab while a user interaction observer is attached to the tab. It
// shouldn't crash.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       CloseTab_ShouldNotCrash) {
  base::HistogramTester histograms;
  chrome::NewTab(browser());
  NavigateAndAssertNoInterstitial();
  chrome::CloseTab(browser());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       KeyPress_WarningShown) {
  constexpr int kTimeOnPage = 10;
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Inject a test clock to test the histogram that records the time on the
  // delayed warning page before the warning shows or the user leaves the page.
  base::SimpleTestClock clock;
  SafeBrowsingUserInteractionObserver* observer =
      SafeBrowsingUserInteractionObserver::FromWebContents(web_contents);
  ASSERT_TRUE(observer);
  clock.SetNow(observer->GetCreationTimeForTesting());
  observer->SetClockForTesting(&clock);
  clock.Advance(base::Seconds(kTimeOnPage));

  // Type something. An interstitial should be shown.
  EXPECT_TRUE(TypeAndWaitForInterstitial(browser()));

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser());         // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),     // Back to "about:blank"
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       KeyPress_ESC_WarningNotShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Press ESC key. The interstitial should not be shown.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  input::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.windows_key_code = ui::VKEY_ESCAPE;
  // Browser expects a non-synthesized event to have an os_event. Make the
  // browser ignore this event instead.
  event.skip_if_unhandled = true;
  contents->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardKeyboardEvent(event);
  AssertNoInterstitial(browser());

  // Navigate to about:blank twice to "flush" metrics, if any. The delayed
  // warning user interaction observer may not have been deleted after the first
  // navigation.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       KeyPress_ModifierKey_WarningNotShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Press CTRL+A key. The interstitial should not be shown because we ignore
  // the CTRL modifier unless it's CTRL+C or CTRL+V.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  input::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kControlKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.windows_key_code = ui::VKEY_A;
  // Browser expects a non-synthesized event to have an os_event. Make the
  // browser ignore this event instead.
  event.skip_if_unhandled = true;
  contents->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardKeyboardEvent(event);
  AssertNoInterstitial(browser());

  // Navigate to about:blank twice to "flush" metrics, if any. The delayed
  // warning user interaction observer may not have been deleted after the first
  // navigation.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       KeyPress_CtrlC_WarningShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Press CTRL+C. The interstitial should be shown.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(contents);

  input::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kControlKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.windows_key_code = ui::VKEY_C;
  event.native_key_code = ui::VKEY_C;
  // We don't set event.skip_if_unhandled = true here because the event will be
  // consumed by UserInteractionObserver and not passed to the browser.
  contents->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardKeyboardEvent(event);

  observer.WaitForNavigationFinished();
  EXPECT_TRUE(WaitForReady(browser()));

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser());         // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),     // Back to "about:blank"
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
}

// Similar to KeyPress_ESC_WarningNotShown, but a character key is pressed after
// ESC. The warning should be shown.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       KeyPress_ESCAndCharacterKey_WarningShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Press ESC key. The interstitial should not be shown.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  input::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.windows_key_code = ui::VKEY_ESCAPE;
  // Browser expects a non-synthesized event to have an os_event. Make the
  // browser ignore this event instead.
  event.skip_if_unhandled = true;
  contents->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardKeyboardEvent(event);
  base::RunLoop().RunUntilIdle();
  AssertNoInterstitial(browser());

  // Now type something. The interstitial should be shown.
  EXPECT_TRUE(TypeAndWaitForInterstitial(browser()));
  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser());         // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),     // Back to "about:blank"
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
}

// Disabled due to flakiness. https://crbug.com/332097746.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       DISABLED_Fullscreen_WarningShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Page tries to enter fullscreen. An interstitial should be shown.
  EXPECT_TRUE(FullscreenAndWaitForInterstitial(browser()));
  EXPECT_FALSE(
      browser()->tab_strip_model()->GetActiveWebContents()->IsFullscreen());

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser());         // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),     // Back to "about:blank"
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       PermissionRequest_WarningShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Page tries to request a notification permission. The prompt should be
  // cancelled and an interstitial should be shown.
  EXPECT_TRUE(RequestPermissionAndWaitForInterstitial(browser()));

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser());         // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),     // Back to "about:blank"
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());

  histograms.ExpectTotalCount("Permissions.Action.Notifications", 1);
  histograms.ExpectBucketCount(
      "Permissions.Action.Notifications",
      static_cast<int>(permissions::PermissionAction::DENIED), 1);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       JavaScriptDialog_WarningShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Page tries to show a JavaScript dialog. The dialog should be
  // cancelled and an interstitial should be shown.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(contents);
  EXPECT_TRUE(content::ExecJs(contents, "alert('test')"));
  observer.WaitForNavigationFinished();
  EXPECT_TRUE(WaitForReady(browser()));

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser());         // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),     // Back to "about:blank"
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       DesktopCaptureRequest_WarningShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Page tries to request a desktop capture permission. The request should be
  // cancelled and an interstitial should be shown.
  EXPECT_TRUE(RequestDesktopCaptureAndWaitForInterstitial(browser()));
  EXPECT_FALSE(
      browser()->tab_strip_model()->GetActiveWebContents()->IsFullscreen());

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser());         // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),     // Back to "about:blank"
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       Paste_WarningShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Create a test context menu and send a paste command through it. This
  // should show the delayed interstitial.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(contents);
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(contents,
                                        contents->GetLastCommittedURL()));
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_PASTE, 0);
  observer.WaitForNavigationFinished();
  EXPECT_TRUE(WaitForReady(browser()));
}

// The user clicks on the page. Feature isn't configured to show a warning on
// mouse clicks. We should record that the user interacted with the page, but
// shouldn't shown an interstitial.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       MouseClick_WarningNotShown) {
  if (warning_on_mouse_click_enabled()) {
    return;
  }
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Click on the page. An interstitial shouldn't be shown because the feature
  // parameter is off.
  MouseClick(browser());
  AssertNoInterstitial(browser());

  // Navigate away to "flush" the metrics.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       MouseClick_WarningShown) {
  if (!warning_on_mouse_click_enabled()) {
    return;
  }
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Click on the page. An interstitial should be shown because the feature
  // parameter is on.
  EXPECT_TRUE(MouseClickAndWaitForInterstitial(browser()));

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser());         // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),     // Back to "about:blank"
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
}

// This test initiates a download when a warning is delayed. The download should
// be cancelled and the interstitial should not be shown.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       Download_CancelledWithNoInterstitial) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  DownloadAndWaitForNavigation(browser());
  AssertNoInterstitial(browser());

  // Navigate away to "flush" the metrics.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       InteractionAfterNonCommittingNavigation_Interstitial) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  const GURL url_204 = embedded_test_server()->GetURL("/page204.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_204));
  AssertNoInterstitial(browser());

  EXPECT_TRUE(TypeAndWaitForInterstitial(browser()));

  // Navigate away to "flush" the metrics.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
}

// This test navigates to a page with password form and submits a password. The
// warning should be delayed, the "Save Password" bubble should not be shown,
// and a histogram entry for the password save should be recorded.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       PasswordSaveDisabled) {
  base::HistogramTester histograms;

  // This is needed for tests using BubbleObserver
  content::WebContents* contents =
      PasswordManagerBrowserTestBase::GetNewTab(browser());

  // Navigate to the page.
  content::TestNavigationObserver observer1(contents);
  const GURL url =
      embedded_test_server()->GetURL("/password/password_form.html");
  SetURLThreatType(url, SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  observer1.Wait();

  // Submit a password.
  PasswordsNavigationObserver observer2(contents);
  std::unique_ptr<BubbleObserver> prompt_observer(new BubbleObserver(contents));
  std::string fill_and_submit =
      "document.getElementById('retry_password_field').value = 'pw';"
      "document.getElementById('retry_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(contents, fill_and_submit));
  ASSERT_TRUE(observer2.Wait());
  EXPECT_FALSE(prompt_observer->IsSavePromptShownAutomatically());
  PasswordManagerBrowserTestBase::WaitForPasswordStore(browser());
  AssertNoInterstitial(browser());

  // Navigate away to "flush" the metrics.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
}

INSTANTIATE_TEST_SUITE_P(
    SafeBrowsingBlockingPageWithDelayedWarningsBrowserTest,
    SafeBrowsingBlockingPageDelayedWarningBrowserTest,
    testing::Combine(
        testing::Values(false, true), /* IsolateAllSitesForTesting */
        testing::Values(false, true) /* Show warning on mouse click */));

// Test that SafeBrowsingBlockingPage properly decodes IDN URLs that are
// displayed.
class SafeBrowsingBlockingPageIDNTest
    : public SecurityInterstitialIDNTest,
      public testing::WithParamInterface<SBThreatType> {
 protected:
  // SecurityInterstitialIDNTest implementation
  security_interstitials::SecurityInterstitialPage* CreateInterstitial(
      content::WebContents* contents,
      const GURL& request_url) const override {
    SafeBrowsingUIManager::CreateAllowlistForTesting(contents);
    SafeBrowsingService* sb_service =
        g_browser_process->safe_browsing_service();
    auto* primary_main_frame = contents->GetPrimaryMainFrame();
    const content::GlobalRenderFrameHostId primary_main_frame_id =
        primary_main_frame->GetGlobalId();
    SafeBrowsingBlockingPage::UnsafeResource resource;

    resource.url = request_url;
    resource.threat_type = GetParam();
    resource.render_process_id = primary_main_frame_id.child_id;
    resource.render_frame_token = primary_main_frame->GetFrameToken().value();
    resource.threat_source = safe_browsing::ThreatSource::LOCAL_PVER4;

    auto* ui_manager = sb_service->ui_manager().get();
    return ui_manager->CreateBlockingPage(
        contents, request_url, {resource}, /*forward_extension_event=*/false,
        /*blocked_page_shown_timestamp=*/std::nullopt);
  }
};

// TODO(crbug.com/40666794): VerifyIDNDecoded does not work with committed
// interstitials, this test should be re-enabled once it is adapted.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageIDNTest,
                       DISABLED_SafeBrowsingBlockingPageDecodesIDN) {
  EXPECT_TRUE(VerifyIDNDecoded());
}

INSTANTIATE_TEST_SUITE_P(
    SafeBrowsingBlockingPageIDNTestWithThreatType,
    SafeBrowsingBlockingPageIDNTest,
    testing::Values(SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
                    SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
                    SBThreatType::SB_THREAT_TYPE_URL_UNWANTED));

class SafeBrowsingBlockingPageEnhancedProtectionMessageTest
    : public policy::PolicyTest {
 public:
  SafeBrowsingBlockingPageEnhancedProtectionMessageTest() = default;

  SafeBrowsingBlockingPageEnhancedProtectionMessageTest(
      const SafeBrowsingBlockingPageEnhancedProtectionMessageTest&) = delete;
  SafeBrowsingBlockingPageEnhancedProtectionMessageTest& operator=(
      const SafeBrowsingBlockingPageEnhancedProtectionMessageTest&) = delete;

  void SetUp() override { InProcessBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    policy::PolicyTest::CreatedBrowserMainParts(browser_main_parts);
    // Test UI manager and test database manager should be set before
    // the browser is started but after threads are created.
    factory_.SetTestUIManager(new FakeSafeBrowsingUIManager(
        std::make_unique<TestSafeBrowsingBlockingPageFactory>()));
    factory_.SetTestDatabaseManager(new FakeSafeBrowsingDatabaseManager(
        content::GetUIThreadTaskRunner({})));
    SafeBrowsingService::RegisterFactory(&factory_);
    ThreatDetails::RegisterFactory(&details_factory_);
  }

 protected:
  void SetupWarningAndNavigateToURL(GURL url, Browser* browser) {
    TestSafeBrowsingService* service = factory_.test_safe_browsing_service();
    ASSERT_TRUE(service);

    static_cast<FakeSafeBrowsingDatabaseManager*>(
        service->database_manager().get())
        ->AddDangerousUrl(url, SBThreatType::SB_THREAT_TYPE_URL_MALWARE);

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, url));
    EXPECT_TRUE(WaitForReady(browser));
  }

  // A test should call this function if it is expected to trigger a threat
  // report.
  void SetReportSentCallback(base::OnceClosure callback) {
    static_cast<FakeSafeBrowsingUIManager*>(
        factory_.test_safe_browsing_service()->ui_manager().get())
        ->set_threat_details_done_callback(std::move(callback));
  }

 private:
  TestSafeBrowsingServiceFactory factory_;
  TestThreatDetailsFactory details_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageEnhancedProtectionMessageTest,
                       VerifyEnhancedProtectionMessageShownAndClicked) {
  safe_browsing::SetExtendedReportingPrefForTests(
      browser()->profile()->GetPrefs(), true);
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  SetupWarningAndNavigateToURL(embedded_test_server()->GetURL("/empty.html"),
                               browser());

  // Check SBER opt in is not shown.
  EXPECT_EQ(HIDDEN, ::safe_browsing::GetVisibility(
                        browser(), "extended-reporting-opt-in"));
  // Check enhanced protection message is shown.
  EXPECT_EQ(VISIBLE, ::safe_browsing::GetVisibility(
                         browser(), "enhanced-protection-message"));
  WebContents* interstitial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(interstitial_tab);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));

  content::TestNavigationObserver nav_observer(nullptr);
  nav_observer.StartWatchingNewWebContents();
  // Click the enhanced protection link.
  EXPECT_TRUE(Click(browser(), "enhanced-protection-link"));

  nav_observer.Wait();

  // There are two tabs open.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  // The second tab is visible.
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Assert the interstitial is not present in the foreground tab.
  ASSERT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));

  // Foreground tab displays the setting page.
  WebContents* new_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(GURL(kEnhancedProtectionUrl), new_tab->GetLastCommittedURL());

  // Interstitial should still display in the background tab.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(interstitial_tab,
            browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));

  // Set threat report sent runner, since a report will be sent when web
  // contents are destroyed.
  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  SetReportSentCallback(threat_report_sent_runner->QuitClosure());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageEnhancedProtectionMessageTest,
                       VerifyEnhancedProtectionMessageNotShownAlreadyInEp) {
  safe_browsing::SetExtendedReportingPrefForTests(
      browser()->profile()->GetPrefs(), true);
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  SetupWarningAndNavigateToURL(embedded_test_server()->GetURL("/empty.html"),
                               browser());
  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  SetReportSentCallback(threat_report_sent_runner->QuitClosure());

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
  // Check enhanced protection message is not shown.
  EXPECT_EQ(HIDDEN, ::safe_browsing::GetVisibility(
                        browser(), "enhanced-protection-message"));
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageEnhancedProtectionMessageTest,
                       VerifyEnhancedProtectionMessageNotShownManaged) {
  policy::PolicyMap policies;
  policies.Set(policy::key::kSafeBrowsingProtectionLevel,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(/* standard protection */ 1), nullptr);
  UpdateProviderPolicy(policies);
  SetupWarningAndNavigateToURL(embedded_test_server()->GetURL("/empty.html"),
                               browser());

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
  // Check enhanced protection message is not shown.
  EXPECT_EQ(HIDDEN, ::safe_browsing::GetVisibility(
                        browser(), "enhanced-protection-message"));
}

class SafeBrowsingBlockingPageAsyncChecksTestBase
    : public InProcessBrowserTest {
 public:
  SafeBrowsingBlockingPageAsyncChecksTestBase() = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    InProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    // Test UI manager and test database manager should be set before
    // the browser is started but after threads are created.
    factory_.SetTestUIManager(new FakeSafeBrowsingUIManager(
        std::make_unique<TestSafeBrowsingBlockingPageFactory>()));
    factory_.SetTestDatabaseManager(new FakeSafeBrowsingDatabaseManager(
        content::GetUIThreadTaskRunner({})));
    SafeBrowsingService::RegisterFactory(&factory_);
  }

 protected:
  void SetupUrlRealTimeVerdictInCacheManager(GURL url,
                                             Profile* profile,
                                             bool is_unsafe) {
    safe_browsing::VerdictCacheManagerFactory::GetForProfile(profile)
        ->CacheArtificialRealTimeUrlVerdict(url.spec(), is_unsafe);
  }
  void SetUpEnterpriseUrlCheck() {
    browser()->profile()->GetPrefs()->SetInteger(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
        enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
    browser()->profile()->GetPrefs()->SetInteger(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
        policy::POLICY_SCOPE_MACHINE);
    SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));
  }
  void NavigateToURLAndWaitForAsyncChecks(GURL url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    SafeBrowsingBlockingPageTestHelper::MaybeWaitForAsyncChecksToComplete(
        browser()->tab_strip_model()->GetActiveWebContents(),
        factory_.test_safe_browsing_service()->ui_manager().get(),
        /*wait_for_load_stop=*/true);
  }

  TestSafeBrowsingServiceFactory factory_;
};

class SafeBrowsingBlockingPageAsyncChecksTest
    : public SafeBrowsingBlockingPageAsyncChecksTestBase,
      public testing::WithParamInterface<bool> {
 public:
  SafeBrowsingBlockingPageAsyncChecksTest() = default;

  void SetUp() override {
    bool is_async_check_enabled = GetParam();
    if (is_async_check_enabled) {
      feature_list_.InitAndEnableFeature(kSafeBrowsingAsyncRealTimeCheck);
    } else {
      feature_list_.InitAndDisableFeature(kSafeBrowsingAsyncRealTimeCheck);
    }
    SafeBrowsingBlockingPageAsyncChecksTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(AsyncCheckEnabled,
                         SafeBrowsingBlockingPageAsyncChecksTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageAsyncChecksTest,
                       EnterpriseRealTimeUrlCheck) {
  base::HistogramTester histogram_tester;
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  SetUpEnterpriseUrlCheck();

  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  SetupUrlRealTimeVerdictInCacheManager(url, browser()->profile(),
                                        /*is_unsafe=*/false);
  NavigateToURLAndWaitForAsyncChecks(url);
  ASSERT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));

  // Whether or not async checks are enabled, only a sync check is performed
  // (the enterprise URT check).
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.EnterpriseFullUrlLookup",
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageAsyncChecksTest,
                       ConsumerRealTimeUrlCheck) {
  base::HistogramTester histogram_tester;
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  browser()->profile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  SetupUrlRealTimeVerdictInCacheManager(url, browser()->profile(),
                                        /*is_unsafe=*/false);
  NavigateToURLAndWaitForAsyncChecks(url);
  ASSERT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));

  bool is_async_check_enabled = GetParam();
  if (is_async_check_enabled) {
    // When async checks are enabled, the sync check is an HPD check.
    histogram_tester.ExpectTotalCount(
        "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixDatabaseCheck",
        /*expected_count=*/1);
  } else {
    // When async checks are disabled, only a sync check is performed, which is
    // a consumer URT check.
    histogram_tester.ExpectTotalCount(
        "SafeBrowsing.BrowserThrottle.TotalDelay2.ConsumerFullUrlLookup",
        /*expected_count=*/1);
  }
}

class SafeBrowsingBlockingPageAsyncChecksTimingTestBase
    : public SafeBrowsingBlockingPageAsyncChecksTestBase {
 public:
  SafeBrowsingBlockingPageAsyncChecksTimingTestBase() = default;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {kSafeBrowsingAsyncRealTimeCheck,
         kCreateWarningShownClientSafeBrowsingReports},
        {kRedWarningSurvey});
    SafeBrowsingBlockingPageAsyncChecksTestBase::SetUp();
  }

  void TearDown() override {
    RealTimeUrlLookupServiceFactory::GetInstance()
        ->SetURLLoaderFactoryForTesting(nullptr);
    SafeBrowsingBlockingPageAsyncChecksTestBase::TearDown();
    ThreatDetails::RegisterFactory(nullptr);
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    SafeBrowsingBlockingPageAsyncChecksTestBase::CreatedBrowserMainParts(
        browser_main_parts);
    ThreatDetails::RegisterFactory(&details_factory_);
  }

 protected:
  // Helper struct for test cases.
  struct UrlAndIsUnsafe {
    std::string relative_url;
    bool is_unsafe;
  };

  void SetURLLoaderFactoryForTesting() {
    auto ref_counted_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    RealTimeUrlLookupServiceFactory::GetInstance()
        ->SetURLLoaderFactoryForTesting(ref_counted_url_loader_factory);
  }

  void EnableAsyncCheck() {
    SetURLLoaderFactoryForTesting();
    // Enable enhanced protection which enables real-time URL check which is
    // conducted asynchronously.
    safe_browsing::SetSafeBrowsingState(
        browser()->profile()->GetPrefs(),
        safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  }

  void SetURLThreatType(const GURL& url, SBThreatType threat_type) {
    TestSafeBrowsingService* service = factory_.test_safe_browsing_service();
    ASSERT_TRUE(service);

    static_cast<FakeSafeBrowsingDatabaseManager*>(
        service->database_manager().get())
        ->AddDangerousUrl(url, threat_type);
  }

  void SetURLHighConfidenceAllowlistMatch(const GURL& url,
                                          bool match_allowlist) {
    TestSafeBrowsingService* service = factory_.test_safe_browsing_service();
    ASSERT_TRUE(service);

    static_cast<FakeSafeBrowsingDatabaseManager*>(
        service->database_manager().get())
        ->SetHighConfidenceAllowlistMatchResult(url, match_allowlist);
  }

  void ReturnUrlRealTimeVerdictInUrlLoader(GURL url, bool is_unsafe) {
    constexpr char kRealTimeLookupUrl[] =
        "https://safebrowsing.google.com/safebrowsing/clientreport/realtime";
    RTLookupResponse response;
    RTLookupResponse::ThreatInfo* new_threat_info = response.add_threat_info();
    RTLookupResponse::ThreatInfo threat_info;
    if (is_unsafe) {
      threat_info.set_verdict_type(RTLookupResponse::ThreatInfo::DANGEROUS);
      threat_info.set_threat_type(
          RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING);
    } else {
      threat_info.set_verdict_type(RTLookupResponse::ThreatInfo::SAFE);
    }
    threat_info.set_cache_duration_sec(60);
    threat_info.set_cache_expression_using_match_type(url.host());
    threat_info.set_cache_expression_match_type(
        RTLookupResponse::ThreatInfo::COVERING_MATCH);
    *new_threat_info = threat_info;
    std::string expected_response_str;
    response.SerializeToString(&expected_response_str);
    test_url_loader_factory_.AddResponse(kRealTimeLookupUrl,
                                         expected_response_str);
  }

  // The following events happen in sequence:
  //   1. WillProcessResponse is called.
  //   2. Safe Browsing checks complete.
  //   3. Navigation finished.
  GURL SetupWarningShownBetweenProcessResponseAndFinishNavigationAndNavigate(
      std::vector<UrlAndIsUnsafe> url_and_server_redirects) {
    CHECK(!url_and_server_redirects.empty());
    GURL original_url = embedded_test_server()->GetURL(
        url_and_server_redirects.front().relative_url);
    GURL final_url = embedded_test_server()->GetURL(
        url_and_server_redirects.back().relative_url);
    content::TestNavigationManager navigation_manager(
        browser()->tab_strip_model()->GetActiveWebContents(), original_url);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), original_url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NO_WAIT);
    EXPECT_TRUE(navigation_manager.WaitForResponse());

    // At this point, WillProcessResponse is called so the async checker is
    // transferred to AsyncCheckTracker.
    AsyncCheckTracker* tracker =
        safe_browsing::AsyncCheckTracker::GetOrCreateForWebContents(
            browser()->tab_strip_model()->GetActiveWebContents(),
            factory_.test_safe_browsing_service()->ui_manager().get(),
            /*should_sync_checker_check_allowlist=*/false);
    EXPECT_EQ(tracker->PendingCheckersSizeForTesting(), 1u);

    GURL interstitial_url;
    for (const auto& url_and_server_redirect : url_and_server_redirects) {
      GURL url =
          embedded_test_server()->GetURL(url_and_server_redirect.relative_url);
      ReturnUrlRealTimeVerdictInUrlLoader(url,
                                          url_and_server_redirect.is_unsafe);
      if (url_and_server_redirect.is_unsafe) {
        interstitial_url = url;
      }
    }
    SafeBrowsingBlockingPageTestHelper::MaybeWaitForAsyncChecksToComplete(
        browser()->tab_strip_model()->GetActiveWebContents(),
        factory_.test_safe_browsing_service()->ui_manager().get(),
        /*wait_for_load_stop=*/false);

    // At this point, the async check is completed, but the navigation has not
    // yet finished.
    navigation_manager.ResumeNavigation();
    EXPECT_TRUE(navigation_manager.WaitForNavigationFinished());

    // After the navigation is finished, we need to wait for the navigation of
    // the interstitial to complete.
    content::TestNavigationManager interstitial_navigation_manager(
        browser()->tab_strip_model()->GetActiveWebContents(), interstitial_url);
    EXPECT_TRUE(interstitial_navigation_manager.WaitForNavigationFinished());
    content::WaitForLoadStop(
        browser()->tab_strip_model()->GetActiveWebContents());

    EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
        browser()->tab_strip_model()->GetActiveWebContents()));

    // Reset dangerous response so future URLs are not accidentally flagged
    // by real-time URL check.
    test_url_loader_factory_.ClearResponses();
    return final_url;
  }

  void NavigateAndAwaitNavigationFinished(std::string relative_url) {
    GURL original_url = embedded_test_server()->GetURL(relative_url);
    content::TestNavigationManager navigation_manager(
        browser()->tab_strip_model()->GetActiveWebContents(), original_url);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), original_url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NO_WAIT);
    EXPECT_TRUE(navigation_manager.WaitForNavigationFinished());
  }

  // This is expected to be used after NavigateAndAwaitNavigationFinished has
  // completed navigating to the same URLs. This method expects that at least
  // one of the URLs is unsafe.
  GURL ReturnUrlRealTimeVerdictsForUnsafeChain(
      std::vector<UrlAndIsUnsafe> url_and_server_redirects) {
    GURL final_url = embedded_test_server()->GetURL(
        url_and_server_redirects.back().relative_url);

    // At this point, the navigation has finished but the async check has not
    // yet completed.
    AsyncCheckTracker* tracker =
        safe_browsing::AsyncCheckTracker::GetOrCreateForWebContents(
            browser()->tab_strip_model()->GetActiveWebContents(),
            factory_.test_safe_browsing_service()->ui_manager().get(),
            /*should_sync_checker_check_allowlist=*/false);
    EXPECT_EQ(tracker->PendingCheckersSizeForTesting(), 1u);

    for (const auto& url_and_server_redirect : url_and_server_redirects) {
      GURL url =
          embedded_test_server()->GetURL(url_and_server_redirect.relative_url);
      ReturnUrlRealTimeVerdictInUrlLoader(url,
                                          url_and_server_redirect.is_unsafe);
    }
    SafeBrowsingBlockingPageTestHelper::MaybeWaitForAsyncChecksToComplete(
        browser()->tab_strip_model()->GetActiveWebContents(),
        factory_.test_safe_browsing_service()->ui_manager().get(),
        /*wait_for_load_stop=*/true);

    EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
        browser()->tab_strip_model()->GetActiveWebContents()));

    // Reset dangerous response so future URLs are not accidentally
    // flagged by real-time URL check.
    test_url_loader_factory_.ClearResponses();
    return final_url;
  }

  // The following events happen in sequence:
  //   1. Navigation finished.
  //   2. Safe Browsing checks complete.
  GURL SetupWarningShownAfterFinishNavigationAndNavigate(
      std::vector<UrlAndIsUnsafe> url_and_server_redirects) {
    CHECK(!url_and_server_redirects.empty());
    NavigateAndAwaitNavigationFinished(
        url_and_server_redirects.front().relative_url);
    return ReturnUrlRealTimeVerdictsForUnsafeChain(url_and_server_redirects);
  }

  void SetReportSentCallback(base::OnceClosure callback) {
    static_cast<FakeSafeBrowsingUIManager*>(
        factory_.test_safe_browsing_service()->ui_manager().get())
        ->set_threat_details_done_callback(std::move(callback));
  }

  std::string GetReportSent() {
    return static_cast<FakeSafeBrowsingUIManager*>(
               factory_.test_safe_browsing_service()->ui_manager().get())
        ->GetReport();
  }

  std::optional<bool> shown_report_sent_is_async_check() {
    return static_cast<FakeSafeBrowsingUIManager*>(
               factory_.test_safe_browsing_service()->ui_manager().get())
        ->report_sent_is_async_check();
  }

  base::HistogramTester histogram_tester_;
  TestThreatDetailsFactory details_factory_;

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::ScopedFeatureList feature_list_;
};

class SafeBrowsingBlockingPageAsyncChecksTimingTest
    : public SafeBrowsingBlockingPageAsyncChecksTimingTestBase,
      public testing::WithParamInterface<bool> {
 public:
  SafeBrowsingBlockingPageAsyncChecksTimingTest() = default;

  GURL SetupPostCommitInterstitialAndNavigate(
      std::vector<UrlAndIsUnsafe> url_and_server_redirects,
      base::OnceClosure report_sent_callback) {
    // Call SetupUrlRealTimeVerdictInCacheManager with a random URL to ensure
    // RealTimeUrlLookupServiceBase::CanCheckUrl returns true so the real time
    // check is performed.
    SetupUrlRealTimeVerdictInCacheManager(
        GURL("https://random.url"), browser()->profile(), /*is_unsafe=*/false);
    SetReportSentCallback(std::move(report_sent_callback));
    bool check_complete_after_navigation_finish = GetParam();
    if (check_complete_after_navigation_finish) {
      return SetupWarningShownAfterFinishNavigationAndNavigate(
          url_and_server_redirects);
    } else {
      return SetupWarningShownBetweenProcessResponseAndFinishNavigationAndNavigate(
          url_and_server_redirects);
    }
  }
};

class SafeBrowsingBlockingPageAsyncChecksPrerenderingTest
    : public SafeBrowsingBlockingPageAsyncChecksTimingTestBase {
 public:
  SafeBrowsingBlockingPageAsyncChecksPrerenderingTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SafeBrowsingBlockingPageAsyncChecksTimingTestBase::SetUpCommandLine(
        command_line);
    // |prerender_helper_| has a ScopedFeatureList so we needed to delay its
    // creation until now because
    // SafeBrowsingBlockingPageAsyncChecksTimingTestBase also uses a
    // ScopedFeatureList and initialization order matters.
    prerender_helper_ = std::make_unique<
        content::test::PrerenderTestHelper>(base::BindRepeating(
        &SafeBrowsingBlockingPageAsyncChecksPrerenderingTest::GetWebContents,
        base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    prerender_helper_->RegisterServerRequestMonitor(embedded_test_server());
    SafeBrowsingBlockingPageAsyncChecksTimingTestBase::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return *prerender_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
};

// Test that prerendering doesn't affect the primary frame's threat report.
IN_PROC_BROWSER_TEST_F(
    SafeBrowsingBlockingPageAsyncChecksPrerenderingTest,
    PostCommitInterstitialReportThreatDetails_DontContainPrerenderingInfo) {
  EnableAsyncCheck();

  // Navigate to unsafe page, but don't yet return unsafe for the Safe Browsing
  // lookup.
  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  SetReportSentCallback(threat_report_sent_runner->QuitClosure());
  // Call SetupUrlRealTimeVerdictInCacheManager with a random URL to ensure
  // RealTimeUrlLookupServiceBase::CanCheckUrl returns true so the real time
  // check is performed.
  SetupUrlRealTimeVerdictInCacheManager(
      GURL("https://random.url"), browser()->profile(), /*is_unsafe=*/false);
  std::vector<UrlAndIsUnsafe> url_and_server_redirects = {
      {kMaliciousPage, /* is_unsafe */ true}};
  NavigateAndAwaitNavigationFinished(
      url_and_server_redirects.front().relative_url);

  // Set up prerendering.
  GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  SetupUrlRealTimeVerdictInCacheManager(prerender_url, browser()->profile(),
                                        /*is_unsafe=*/false);
  content::FrameTreeNodeId host_id =
      prerender_helper().AddPrerender(prerender_url);
  content::RenderFrameHost* prerender_render_frame_host =
      prerender_helper().GetPrerenderedMainFrameHost(host_id);
  EXPECT_NE(prerender_render_frame_host, nullptr);
  EXPECT_EQ(prerender_url, prerender_render_frame_host->GetLastCommittedURL());

  // Return unsafe for the Safe Browsing lookup, which displays a post-commit
  // interstitial.
  GURL url = ReturnUrlRealTimeVerdictsForUnsafeChain(url_and_server_redirects);

  ThreatDetails* threat_details = details_factory_.get_details();
  EXPECT_TRUE(threat_details != nullptr);

  // Proceed through the warning.
  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "proceed-link"));
  AssertNoInterstitial(browser());  // Assert the interstitial is gone

  EXPECT_TRUE(IsExtendedReportingEnabled(*browser()->profile()->GetPrefs()));
  EXPECT_EQ(url, browser()
                     ->tab_strip_model()
                     ->GetActiveWebContents()
                     ->GetLastCommittedURL());

  threat_report_sent_runner->Run();
  std::string serialized = GetReportSent();
  ClientSafeBrowsingReportRequest report;
  ASSERT_TRUE(report.ParseFromString(serialized));
  // Verify the report is complete.
  EXPECT_TRUE(report.complete());
  // The threat report should not contain the prerender information.
  EXPECT_NE(prerender_url.spec(), report.page_url());
  EXPECT_NE(prerender_url.spec(), report.url());
  for (const auto& resource : report.resources()) {
    EXPECT_NE(prerender_url.spec(), resource.url());
  }
  // We don't check the specific size of resources here. The size can be either
  // 1 or 2 depending on whether DOM details have been collected when we
  // proceed.
  ASSERT_NE(0, report.resources_size());
}

INSTANTIATE_TEST_SUITE_P(CheckCompleteAfterNavigationFinish,
                         SafeBrowsingBlockingPageAsyncChecksTimingTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageAsyncChecksTimingTest,
                       VerifyHistogramsAndHitReport) {
  EnableAsyncCheck();
  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  GURL url = SetupPostCommitInterstitialAndNavigate(
      {{kMaliciousPage, /* is_unsafe */ true}},
      threat_report_sent_runner->QuitClosure());

  int hit_report_count =
      static_cast<FakeSafeBrowsingUIManager*>(
          factory_.test_safe_browsing_service()->ui_manager().get())
          ->hit_report_count();
  EXPECT_EQ(hit_report_count, 1);
  EXPECT_TRUE(shown_report_sent_is_async_check().value());

  histogram_tester_.ExpectUniqueSample(
      "interstitial.phishing.decision.after_page_shown",
      /*sample=*/security_interstitials::MetricsHelper::SHOW,
      /*expected_bucket_count=*/1);
}

// Confirm that duplicate hit reports aren't sent in the case where URT falls
// back to HPD due to a high-confidence allowlist match.
IN_PROC_BROWSER_TEST_P(
    SafeBrowsingBlockingPageAsyncChecksTimingTest,
    NoDuplicateHitReports_FallbackFromHighConfidenceAllowlistMatch) {
  EnableAsyncCheck();
  // Call SetupUrlRealTimeVerdictInCacheManager with a random URL to ensure
  // RealTimeUrlLookupServiceBase::CanCheckUrl returns true so the real time
  // check is performed.
  SetupUrlRealTimeVerdictInCacheManager(
      GURL("https://random.url"), browser()->profile(), /*is_unsafe=*/false);
  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  SetURLHighConfidenceAllowlistMatch(url, true);
  SetURLThreatType(url, SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
  NavigateToURLAndWaitForAsyncChecks(url);

  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  SetReportSentCallback(threat_report_sent_runner->QuitClosure());

  int hit_report_count =
      static_cast<FakeSafeBrowsingUIManager*>(
          factory_.test_safe_browsing_service()->ui_manager().get())
          ->hit_report_count();
  ASSERT_EQ(hit_report_count, 1);
  EXPECT_FALSE(shown_report_sent_is_async_check().value());
}

// Confirm that duplicate hit reports aren't sent in the case where URT is
// not eligible and HPD is used instead for the async check.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageAsyncChecksTimingTest,
                       NoDuplicateHitReports_UrlRealTimeUncheckable) {
  EnableAsyncCheck();
  // Do not call |SetupUrlRealTimeVerdictInCacheManager| with a random URL,
  // that way the real-time URL lookup will instead fall back to hash database
  // checks instead, since the URL is not eligible for real-time lookups.
  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  SetURLThreatType(url, SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
  NavigateToURLAndWaitForAsyncChecks(url);

  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  SetReportSentCallback(threat_report_sent_runner->QuitClosure());

  int hit_report_count =
      static_cast<FakeSafeBrowsingUIManager*>(
          factory_.test_safe_browsing_service()->ui_manager().get())
          ->hit_report_count();
  ASSERT_EQ(hit_report_count, 1);
  EXPECT_FALSE(shown_report_sent_is_async_check().value());
}

// Confirm that duplicate hit reports aren't sent for web UI URLs.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageAsyncChecksTimingTest,
                       NoDuplicateHitReports_WebUiUrl) {
  EnableAsyncCheck();
  NavigateToURLAndWaitForAsyncChecks(
      GURL(kChromeUISafeBrowsingMatchPhishingUrl));

  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  SetReportSentCallback(threat_report_sent_runner->QuitClosure());

  int hit_report_count =
      static_cast<FakeSafeBrowsingUIManager*>(
          factory_.test_safe_browsing_service()->ui_manager().get())
          ->hit_report_count();
  ASSERT_EQ(hit_report_count, 1);
  EXPECT_FALSE(shown_report_sent_is_async_check().value());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageAsyncChecksTimingTest,
                       PostCommitInterstitialDontProceed) {
  EnableAsyncCheck();
  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  SetupPostCommitInterstitialAndNavigate(
      {{kMaliciousPage, /* is_unsafe */ true}},
      threat_report_sent_runner->QuitClosure());

  EXPECT_EQ(VISIBLE, GetVisibility(browser(), "primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility(browser(), "details"));
  EXPECT_EQ(HIDDEN, GetVisibility(browser(), "proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility(browser(), "error-code"));
  EXPECT_TRUE(Click(browser(), "details-button"));
  EXPECT_EQ(VISIBLE, GetVisibility(browser(), "details"));
  EXPECT_EQ(VISIBLE, GetVisibility(browser(), "proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility(browser(), "error-code"));
  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));

  AssertNoInterstitial(browser());  // Assert the interstitial is gone

  EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageAsyncChecksTimingTest,
                       PostCommitInterstitialProceed) {
  EnableAsyncCheck();
  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  GURL url = SetupPostCommitInterstitialAndNavigate(
      {{kMaliciousPage, /* is_unsafe */ true}},
      threat_report_sent_runner->QuitClosure());

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "proceed-link"));
  AssertNoInterstitial(browser());  // Assert the interstitial is gone

  EXPECT_EQ(url, browser()
                     ->tab_strip_model()
                     ->GetActiveWebContents()
                     ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageAsyncChecksTimingTest,
                       PostCommitInterstitialServerRedirect_OriginIsUnsafe) {
  EnableAsyncCheck();
  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  GURL url = SetupPostCommitInterstitialAndNavigate(
      {{kRedirectToMalware, /* is_unsafe */ true},
       {kMaliciousPage, /* is_unsafe */ false}},
      threat_report_sent_runner->QuitClosure());

  // The original URL is unsafe, so it should be displayed in the URL bar.
  GURL original_url = embedded_test_server()->GetURL(kRedirectToMalware);
  EXPECT_EQ(
      original_url,
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "proceed-link"));
  AssertNoInterstitial(browser());  // Assert the interstitial is gone

  EXPECT_EQ(url, browser()
                     ->tab_strip_model()
                     ->GetActiveWebContents()
                     ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageAsyncChecksTimingTest,
                       PostCommitInterstitialServerRedirect_RedirectIsUnsafe) {
  EnableAsyncCheck();
  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  GURL url = SetupPostCommitInterstitialAndNavigate(
      {{kRedirectToMalware, /* is_unsafe */ false},
       {kMaliciousPage, /* is_unsafe */ true}},
      threat_report_sent_runner->QuitClosure());

  // The final URL is unsafe, so it should be displayed in the URL bar.
  GURL final_url = embedded_test_server()->GetURL(kMaliciousPage);
  EXPECT_EQ(
      final_url,
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "proceed-link"));
  AssertNoInterstitial(browser());  // Assert the interstitial is gone

  EXPECT_EQ(url, browser()
                     ->tab_strip_model()
                     ->GetActiveWebContents()
                     ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageAsyncChecksTimingTest,
                       PostCommitInterstitialReportThreatDetails) {
  EnableAsyncCheck();
  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  GURL url = SetupPostCommitInterstitialAndNavigate(
      {{kMaliciousPage, /* is_unsafe */ true}},
      threat_report_sent_runner->QuitClosure());

  ThreatDetails* threat_details = details_factory_.get_details();
  EXPECT_TRUE(threat_details != nullptr);
  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "proceed-link"));
  AssertNoInterstitial(browser());  // Assert the interstitial is gone

  EXPECT_EQ(url, browser()
                     ->tab_strip_model()
                     ->GetActiveWebContents()
                     ->GetLastCommittedURL());

  threat_report_sent_runner->Run();
  std::string serialized = GetReportSent();
  ClientSafeBrowsingReportRequest report;
  ASSERT_TRUE(report.ParseFromString(serialized));
  // Verify the report is complete.
  EXPECT_TRUE(report.complete());
  // Do some basic verification of report contents.
  EXPECT_EQ(url.spec(), report.page_url());
  EXPECT_EQ(url.spec(), report.url());
  // We don't check the specific size of resources here. The size can be either
  // 1 or 2 depending on whether DOM details have been collected when we
  // proceed.
  ASSERT_NE(0, report.resources_size());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageAsyncChecksTimingTest,
                       PostCommitInterstitialAllowlistRevisit) {
  EnableAsyncCheck();
  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  GURL url = SetupPostCommitInterstitialAndNavigate(
      {{kMaliciousPage, /* is_unsafe */ true}},
      threat_report_sent_runner->QuitClosure());

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "proceed-link"));
  AssertNoInterstitial(browser());  // Assert the interstitial is gone.
  EXPECT_EQ(url, browser()
                     ->tab_strip_model()
                     ->GetActiveWebContents()
                     ->GetLastCommittedURL());

  // Navigate to an unrelated page and revisit the allowlisted URL.
  SetupUrlRealTimeVerdictInCacheManager(GURL(kUnrelatedUrl),
                                        browser()->profile(),
                                        /*is_unsafe=*/false);
  NavigateToURLAndWaitForAsyncChecks(GURL(kUnrelatedUrl));
  AssertNoInterstitial(browser());

  // The allowlisted page should remain allowlisted.
  NavigateToURLAndWaitForAsyncChecks(url);
  AssertNoInterstitial(browser());
}

// Test that the security indicator gets updated on a Safe Browsing
// interstitial triggered post commit. Regression test for
// https://crbug.com/659713.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageAsyncChecksTimingTest,
                       SecurityStateDowngradedForPostCommitInterstitial) {
  EnableAsyncCheck();
  WebContents* error_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(error_tab);
  SecurityStyleTestObserver observer(error_tab);

  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  GURL url = SetupPostCommitInterstitialAndNavigate(
      {{kMaliciousPage, /* is_unsafe */ true}},
      threat_report_sent_runner->QuitClosure());

  // The security indicator should be downgraded while the interstitial shows.
  ExpectSecurityIndicatorDowngrade(error_tab, 0u);
  EXPECT_EQ(security_state::SecurityLevel::DANGEROUS,
            observer.latest_security_level());

  // The security indicator should still be downgraded post-interstitial.
  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "proceed-link"));
  AssertNoInterstitial(browser());
  WebContents* post_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(post_tab);
  ExpectSecurityIndicatorDowngrade(post_tab, 0u);
}

// Test that the security indicator does not stay downgraded after
// clicking back from a Safe Browsing interstitial triggered post commit.
// Regression test for https://crbug.com/659709.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageAsyncChecksTimingTest,
                       SecurityStateGoBackOnPostCommitInterstitial) {
  EnableAsyncCheck();

  // Navigate to a page so that there is somewhere to go back to.
  GURL start_url = embedded_test_server()->GetURL(kEmptyPage);
  NavigateToURLAndWaitForAsyncChecks(start_url);

  // The security indicator should be downgraded while the interstitial
  // shows.
  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  GURL main_url = SetupPostCommitInterstitialAndNavigate(
      {{kMaliciousPage, /* is_unsafe */ true}},
      threat_report_sent_runner->QuitClosure());
  WebContents* error_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(error_tab);
  ExpectSecurityIndicatorDowngrade(error_tab, 0u);

  // Go back.
  EXPECT_EQ(VISIBLE, GetVisibility(browser(), "primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility(browser(), "details"));
  EXPECT_EQ(HIDDEN, GetVisibility(browser(), "proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility(browser(), "error-code"));
  EXPECT_TRUE(Click(browser(), "details-button"));
  EXPECT_EQ(VISIBLE, GetVisibility(browser(), "details"));
  EXPECT_EQ(VISIBLE, GetVisibility(browser(), "proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility(browser(), "error-code"));
  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));

  // The security indicator should *not* still be downgraded after going back.
  AssertNoInterstitial(browser());
  WebContents* post_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(post_tab);
  content::NavigationEntry* entry = post_tab->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(start_url, entry->GetURL());
  ExpectNoSecurityIndicatorDowngrade(post_tab);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageAsyncChecksTimingTest,
                       SecurityStateGoBackFlaggedByBothChecks) {
  EnableAsyncCheck();

  // Navigate to a page so that there is somewhere to go back to.
  GURL start_url = embedded_test_server()->GetURL(kEmptyPage);
  NavigateToURLAndWaitForAsyncChecks(start_url);

  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  SetReportSentCallback(threat_report_sent_runner->QuitClosure());
  GURL url = embedded_test_server()->GetURL(kMaliciousPage);

  // Mark the URL as dangerous for both checks.
  SetupUrlRealTimeVerdictInCacheManager(url, browser()->profile(),
                                        /*is_unsafe=*/true);
  SetURLThreatType(url, SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
  NavigateToURLAndWaitForAsyncChecks(url);

  // The security indicator should be downgraded while the interstitial
  // shows.
  WebContents* error_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(error_tab);
  ExpectSecurityIndicatorDowngrade(error_tab, 0u);

  // Go back.
  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));

  // The security indicator should *not* still be downgraded after going back.
  AssertNoInterstitial(browser());
  WebContents* post_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(post_tab);
  content::NavigationEntry* entry = post_tab->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(start_url, entry->GetURL());
  ExpectNoSecurityIndicatorDowngrade(post_tab);
}

// Tests that commands work in a post commit interstitial if a pre commit
// interstitial has been shown previously on the same webcontents. Regression
// test for crbug.com/1021334
#if BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER)
// TODO(crbug.com/325491320): re-enable test
#define MAYBE_PostCommitInterstitialProceedAfterPreCommitInterstitial \
  DISABLED_PostCommitInterstitialProceedAfterPreCommitInterstitial
#else
#define MAYBE_PostCommitInterstitialProceedAfterPreCommitInterstitial \
  PostCommitInterstitialProceedAfterPreCommitInterstitial
#endif
IN_PROC_BROWSER_TEST_P(
    SafeBrowsingBlockingPageAsyncChecksTimingTest,
    MAYBE_PostCommitInterstitialProceedAfterPreCommitInterstitial) {
  EnableAsyncCheck();
  // Trigger a pre commit interstitial and go back.
  GURL start_url = embedded_test_server()->GetURL(kEmptyPage);
  SetURLThreatType(start_url, SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
  NavigateToURLAndWaitForAsyncChecks(start_url);
  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser());

  // Trigger a post commit interstitial.
  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  GURL main_url = SetupPostCommitInterstitialAndNavigate(
      {{kMaliciousPage, /* is_unsafe */ true}},
      threat_report_sent_runner->QuitClosure());

  // Commands should work.
  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "proceed-link"));
  AssertNoInterstitial(browser());  // Assert the interstitial is gone

  EXPECT_EQ(main_url, browser()
                          ->tab_strip_model()
                          ->GetActiveWebContents()
                          ->GetLastCommittedURL());
}

// Tests for real time URL check. To test it without making network requests to
// Safe Browsing servers, store an unsafe verdict in cache for the URL.
class SafeBrowsingBlockingPageRealTimeUrlCheckTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  SafeBrowsingBlockingPageRealTimeUrlCheckTest() = default;

  SafeBrowsingBlockingPageRealTimeUrlCheckTest(
      const SafeBrowsingBlockingPageRealTimeUrlCheckTest&) = delete;
  SafeBrowsingBlockingPageRealTimeUrlCheckTest& operator=(
      const SafeBrowsingBlockingPageRealTimeUrlCheckTest&) = delete;

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features = {};
    std::vector<base::test::FeatureRef> disabled_features = {kDelayedWarnings};
    if (GetParam()) {
      enabled_features.push_back(kSafeBrowsingAsyncRealTimeCheck);
    } else {
      disabled_features.push_back(kSafeBrowsingAsyncRealTimeCheck);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    InProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    // Test UI manager and test database manager should be set before
    // the browser is started but after threads are created.
    factory_.SetTestUIManager(new FakeSafeBrowsingUIManager(
        std::make_unique<TestSafeBrowsingBlockingPageFactory>()));
    factory_.SetTestDatabaseManager(new FakeSafeBrowsingDatabaseManager(
        content::GetUIThreadTaskRunner({})));
    SafeBrowsingService::RegisterFactory(&factory_);
  }

 protected:
  void SetupUnsafeVerdict(GURL url, Profile* profile) {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(
        "mark_as_real_time_phishing",
        embedded_test_server()->GetURL("/empty.html").spec());
    safe_browsing::VerdictCacheManagerFactory::GetForProfile(profile)
        ->CacheArtificialUnsafeRealTimeUrlVerdictFromSwitch();
  }
  void NavigateToURL(GURL url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    SafeBrowsingBlockingPageTestHelper::MaybeWaitForAsyncChecksToComplete(
        browser()->tab_strip_model()->GetActiveWebContents(),
        factory_.test_safe_browsing_service()->ui_manager().get(),
        /*wait_for_load_stop=*/true);
  }
  void SetReportSentCallback(base::OnceClosure callback) {
    static_cast<FakeSafeBrowsingUIManager*>(
        factory_.test_safe_browsing_service()->ui_manager().get())
        ->set_threat_details_done_callback(std::move(callback));
  }

  TestSafeBrowsingServiceFactory factory_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(AsyncCheckEnabled,
                         SafeBrowsingBlockingPageRealTimeUrlCheckTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageRealTimeUrlCheckTest,
                       WarningShown_EnhancedProtectionEnabled) {
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  GURL url = embedded_test_server()->GetURL("/empty.html");
  SetupUnsafeVerdict(url, browser()->profile());
  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  SetReportSentCallback(threat_report_sent_runner->QuitClosure());

  NavigateToURL(url);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageRealTimeUrlCheckTest,
                       WarningShown_MbbEnabled) {
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  browser()->profile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  GURL url = embedded_test_server()->GetURL("/empty.html");
  SetupUnsafeVerdict(url, browser()->profile());

  NavigateToURL(url);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageRealTimeUrlCheckTest,
                       WarningNotShown_MbbDisabled) {
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  browser()->profile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  GURL url = embedded_test_server()->GetURL("/empty.html");
  SetupUnsafeVerdict(url, browser()->profile());

  NavigateToURL(url);
  ASSERT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
}

// Tests for hash-prefix real-time check. To avoid redundant testing of the
// HashRealTimeService, this populates the local cache instead of mocking
// network requests.
class SafeBrowsingBlockingPageHashRealTimeCheckTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  SafeBrowsingBlockingPageHashRealTimeCheckTest() = default;
  SafeBrowsingBlockingPageHashRealTimeCheckTest(
      const SafeBrowsingBlockingPageHashRealTimeCheckTest&) = delete;
  SafeBrowsingBlockingPageHashRealTimeCheckTest& operator=(
      const SafeBrowsingBlockingPageHashRealTimeCheckTest&) = delete;

  void SetUp() override {
    InitFeatures();
    InProcessBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    InProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    // Test UI manager and test database manager should be set before
    // the browser is started but after threads are created.
    factory_.SetTestUIManager(new FakeSafeBrowsingUIManager(
        std::make_unique<TestSafeBrowsingBlockingPageFactory>()));
    factory_.SetTestDatabaseManager(new FakeSafeBrowsingDatabaseManager(
        content::GetUIThreadTaskRunner({})));
    SafeBrowsingService::RegisterFactory(&factory_);
  }

 protected:
  virtual void InitFeatures() {
    std::vector<base::test::FeatureRef> enabled_features = {
        kHashPrefixRealTimeLookups};
    std::vector<base::test::FeatureRef> disabled_features = {};
    if (GetParam()) {
      enabled_features.push_back(kSafeBrowsingAsyncRealTimeCheck);
    } else {
      disabled_features.push_back(kSafeBrowsingAsyncRealTimeCheck);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  void SetUpVerdict(GURL url, Profile* profile, bool is_unsafe) {
    safe_browsing::VerdictCacheManagerFactory::GetForProfile(profile)
        ->CacheArtificialHashRealTimeLookupVerdict(url.spec(), is_unsafe);
  }
  void SetUpAndNavigateToUrl(bool is_unsafe) {
    GURL url = embedded_test_server()->GetURL("/empty.html");
    SetUpVerdict(url, browser()->profile(), is_unsafe);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    SafeBrowsingBlockingPageTestHelper::MaybeWaitForAsyncChecksToComplete(
        browser()->tab_strip_model()->GetActiveWebContents(),
        factory_.test_safe_browsing_service()->ui_manager().get(),
        /*wait_for_load_stop=*/true);
  }
  bool IsShowingInterstitial() {
    return ::safe_browsing::IsShowingInterstitial(
        browser()->tab_strip_model()->GetActiveWebContents());
  }
  std::optional<ThreatSource> hit_report_sent_threat_source() {
    return static_cast<FakeSafeBrowsingUIManager*>(
               factory_.test_safe_browsing_service()->ui_manager().get())
        ->hit_report_sent_threat_source();
  }
  std::string GetReportSent() {
    return static_cast<FakeSafeBrowsingUIManager*>(
               factory_.test_safe_browsing_service()->ui_manager().get())
        ->GetReport();
  }
  void SetReportSentCallback(base::OnceClosure callback) {
    static_cast<FakeSafeBrowsingUIManager*>(
        factory_.test_safe_browsing_service()->ui_manager().get())
        ->set_threat_details_done_callback(std::move(callback));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  TestSafeBrowsingServiceFactory factory_;

 private:
  hash_realtime_utils::GoogleChromeBrandingPretenderForTesting apply_branding_;
};
class SafeBrowsingBlockingPageHashRealTimeCheckFeatureOffTest
    : public SafeBrowsingBlockingPageHashRealTimeCheckTest {
 protected:
  void InitFeatures() override {
    std::vector<base::test::FeatureRef> enabled_features = {};
    std::vector<base::test::FeatureRef> disabled_features = {
        kHashPrefixRealTimeLookups};
    if (GetParam()) {
      enabled_features.push_back(kSafeBrowsingAsyncRealTimeCheck);
    } else {
      disabled_features.push_back(kSafeBrowsingAsyncRealTimeCheck);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
};
INSTANTIATE_TEST_SUITE_P(AsyncCheckEnabled,
                         SafeBrowsingBlockingPageHashRealTimeCheckTest,
                         testing::Bool());
INSTANTIATE_TEST_SUITE_P(
    AsyncCheckEnabled,
    SafeBrowsingBlockingPageHashRealTimeCheckFeatureOffTest,
    testing::Bool());
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageHashRealTimeCheckTest,
                       ShowWarning) {
  base::HistogramTester histogram_tester;
  SetUpAndNavigateToUrl(/*is_unsafe=*/true);
  ASSERT_TRUE(IsShowingInterstitial());
  // The TotalDelay2 metric is logged for whichever check is run sync. When
  // async checks are disabled, that's the hash-prefix real-time check. When
  // they are enabled, it's the hash-prefix database check, since the
  // hash-prefix real-time check is run async.
  bool is_using_async_checks = GetParam();
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixRealTimeCheck",
      /*expected_count=*/is_using_async_checks ? 0 : 1);
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixDatabaseCheck",
      /*expected_count=*/is_using_async_checks ? 1 : 0);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.HPRT.Ineligible.IneligibleForSessionOrLocation",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "interstitial.phishing.decision.from_hash_prefix_real_time_check_v5",
      /*expected_count=*/1);
}
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageHashRealTimeCheckTest,
                       DontShowWarning_PageIsSafe) {
  base::HistogramTester histogram_tester;
  SetUpAndNavigateToUrl(/*is_unsafe=*/false);
  ASSERT_FALSE(IsShowingInterstitial());
  // The TotalDelay2 metric is logged for whichever check is run sync. When
  // async checks are disabled, that's the hash-prefix real-time check. When
  // they are enabled, it's the hash-prefix database check, since the
  // hash-prefix real-time check is run async.
  bool is_using_async_checks = GetParam();
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixRealTimeCheck",
      /*expected_count=*/is_using_async_checks ? 0 : 1);
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixDatabaseCheck",
      /*expected_count=*/is_using_async_checks ? 1 : 0);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.HPRT.Ineligible.IneligibleForSessionOrLocation",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "interstitial.phishing.decision.from_hash_prefix_real_time_check_v5",
      /*expected_count=*/0);
}
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageHashRealTimeCheckFeatureOffTest,
                       DontShowWarning_FeatureIsOff) {
  base::HistogramTester histogram_tester;
  SetUpAndNavigateToUrl(/*is_unsafe=*/true);
  ASSERT_FALSE(IsShowingInterstitial());
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixRealTimeCheck",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixDatabaseCheck",
      /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.HPRT.Ineligible.IneligibleForSessionOrLocation",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "interstitial.phishing.decision.from_hash_prefix_real_time_check_v5",
      /*expected_count=*/0);
}
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageHashRealTimeCheckTest,
                       TriggerHitReportAndClientSafeBrowsingReportRequest) {
  if (base::FeatureList::IsEnabled(kExtendedReportingRemovePrefDependency)) {
    // If the extended reporting pref dependency is removed, this test will not
    // be run since it is testing HPRT lookup cases.
    // TODO(crbug.com/362530516): Remove this test case and add a new test for
    // sampled HPRT lookups.
    return;
  }
  SetExtendedReportingPrefForTests(browser()->profile()->GetPrefs(), true);
  SetUpAndNavigateToUrl(/*is_unsafe=*/true);
  ASSERT_TRUE(IsShowingInterstitial());

  // Verify correct hit report is sent.
  EXPECT_EQ(hit_report_sent_threat_source(),
            ThreatSource::NATIVE_PVER5_REAL_TIME);

  // Verify correct CSBRR is sent.
  auto threat_report_sent_runner = std::make_unique<base::RunLoop>();
  SetReportSentCallback(threat_report_sent_runner->QuitClosure());
  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "proceed-link"));
  ASSERT_FALSE(IsShowingInterstitial());
  threat_report_sent_runner->Run();
  std::string serialized_report = GetReportSent();
  ClientSafeBrowsingReportRequest report;
  ASSERT_TRUE(report.ParseFromString(serialized_report));
  EXPECT_EQ(report.type(),
            ClientSafeBrowsingReportRequest_ReportType_URL_PHISHING);
  EXPECT_EQ(
      report.client_properties().url_api_type(),
      ClientSafeBrowsingReportRequest_SafeBrowsingUrlApiType_PVER5_NATIVE_REAL_TIME);
  bool is_using_async_checks = GetParam();
  EXPECT_EQ(report.client_properties().is_async_check(), is_using_async_checks);
}

class SafeBrowsingPrerenderBrowserTest
    : public SafeBrowsingBlockingPageBrowserTest {
 public:
  SafeBrowsingPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &SafeBrowsingPrerenderBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~SafeBrowsingPrerenderBrowserTest() override = default;
  SafeBrowsingPrerenderBrowserTest(const SafeBrowsingPrerenderBrowserTest&) =
      delete;
  SafeBrowsingPrerenderBrowserTest& operator=(
      const SafeBrowsingPrerenderBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    SafeBrowsingBlockingPageBrowserTest::SetUp();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Prerenders |prerender_url|, which triggers SafeBrowsing, then verifies that
  // the prerender is cancelled and that the security state of the primary page
  // is not affected.
  void PrerenderAndExpectCancellation(const GURL& prerender_url) {
    content::test::PrerenderHostObserver observer(*GetWebContents(),
                                                  prerender_url);
    prerender_helper().AddPrerenderAsync(prerender_url);
    observer.WaitForDestroyed();

    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingInterstitial(GetWebContents()));
    ExpectNoSecurityIndicatorDowngrade(GetWebContents());
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SafeBrowsingPrerenderBrowserTest,
    testing::Combine(
        testing::Values(
            SBThreatType::SB_THREAT_TYPE_URL_MALWARE,  // Threat types
            SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
            SBThreatType::SB_THREAT_TYPE_URL_UNWANTED),
        testing::Bool()));  // If isolate all sites for testing.

// Attempt to prerender an unsafe page. The prerender navigation should be
// cancelled and should not affect the security state of the primary page.
IN_PROC_BROWSER_TEST_P(SafeBrowsingPrerenderBrowserTest, UnsafePrerender) {
  base::HistogramTester histograms;
  const GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  const GURL prerender_url = embedded_test_server()->GetURL(kEmptyPage);
  SetURLThreatType(prerender_url, GetThreatType());

  PrerenderAndExpectCancellation(prerender_url);
  histograms.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      /*PrerenderFinalStatus::kBlockedByClient=*/28, 1);
}

class SafeBrowsingBlockingPageDelayedWarningPrerenderingBrowserTest
    : public SafeBrowsingBlockingPageDelayedWarningBrowserTest {
 public:
  SafeBrowsingBlockingPageDelayedWarningPrerenderingBrowserTest() = default;
  ~SafeBrowsingBlockingPageDelayedWarningPrerenderingBrowserTest() override =
      default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SafeBrowsingBlockingPageDelayedWarningBrowserTest::SetUpCommandLine(
        command_line);
    // |prerender_helper_| has a ScopedFeatureList so we needed to delay its
    // creation until now because
    // SafeBrowsingBlockingPageDelayedWarningBrowserTest also uses a
    // ScopedFeatureList and initialization order matters.
    prerender_helper_ = std::make_unique<content::test::PrerenderTestHelper>(
        base::BindRepeating(
            &SafeBrowsingBlockingPageDelayedWarningPrerenderingBrowserTest::
                GetWebContents,
            base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    prerender_helper_->RegisterServerRequestMonitor(embedded_test_server());
    SafeBrowsingBlockingPageDelayedWarningBrowserTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return *prerender_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SafeBrowsingBlockingPageDelayedWarningPrerenderingBrowserTest,
    testing::Combine(testing::Bool(), /* IsolateAllSitesForTesting */
                     testing::Bool() /* Show warning on mouse click */));

// This test loads a page in the prerender to ensure that the prerendering
// navigation is skipped at DidFinishNavigation() from
// SafeBrowsingUserInteractionObserver.
IN_PROC_BROWSER_TEST_P(
    SafeBrowsingBlockingPageDelayedWarningPrerenderingBrowserTest,
    DoNotRecordMetricsInPrerendering) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Load a page in the prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/simple.html");
  prerender_helper().AddPrerender(prerender_url);

  // Activating the prerendered page causes "flush" metrics.
  prerender_helper().NavigatePrimaryPage(prerender_url);
}

class WarningShownTimestampCSBRRDisabledBrowserTest
    : public SafeBrowsingBlockingPageBrowserTest {
 public:
  WarningShownTimestampCSBRRDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        safe_browsing::kAddWarningShownTSToClientSafeBrowsingReport);
  }
  ~WarningShownTimestampCSBRRDisabledBrowserTest() override = default;

  void SetUp() override { SafeBrowsingBlockingPageBrowserTest::SetUp(); }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void RunThreatReportSentLoop() {
    base::RunLoop threat_report_sent_loop;
    SetReportSentCallback(threat_report_sent_loop.QuitClosure());
    threat_report_sent_loop.Run();
  }

  void CheckCSBRRForTimestamp() {
    std::string serialized = GetReportSent();
    ClientSafeBrowsingReportRequest report;
    ASSERT_TRUE(report.ParseFromString(serialized));
    // The timestamp of the warning shown should not be in the report.
    EXPECT_FALSE(report.has_warning_shown_timestamp_msec());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    WarningShownTimestampCSBRRDisabledBrowserTestWithThreatTypeAndIsolationSetting,
    WarningShownTimestampCSBRRDisabledBrowserTest,
    testing::Combine(
        testing::Values(
            SBThreatType::SB_THREAT_TYPE_URL_PHISHING,  // Threat types
            SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING),
        testing::Bool()));  // If isolate all sites for testing.

IN_PROC_BROWSER_TEST_P(WarningShownTimestampCSBRRDisabledBrowserTest,
                       TimestampNotInCSBRRClickedThroughBlockingPage) {
  SetExtendedReportingPrefForTests(browser()->profile()->GetPrefs(), true);
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  SetupWarningAndNavigate(browser());

  // Proceed to unsafe site, sending CSBRR.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));

  observer.WaitForNavigationFinished();
  RunThreatReportSentLoop();
  CheckCSBRRForTimestamp();
}
IN_PROC_BROWSER_TEST_P(WarningShownTimestampCSBRRDisabledBrowserTest,
                       TimestampNotInFallbackCSBRRSent) {
  SetExtendedReportingPrefForTests(browser()->profile()->GetPrefs(), true);
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  SetupWarningAndNavigate(browser());

  // Send CSBRR without interactions.
  chrome::CloseTab(browser());

  observer.WaitForNavigationFinished();
  RunThreatReportSentLoop();
  CheckCSBRRForTimestamp();
}

}  // namespace safe_browsing
