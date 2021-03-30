// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/lookalikes/digital_asset_links_cross_validator.h"
#include "chrome/browser/lookalikes/lookalike_url_blocking_page.h"
#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lookalikes/core/features.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/reputation/core/safety_tip_test_utils.h"
#include "components/reputation/core/safety_tips_config.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/signed_exchange_browser_test_helper.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/window_open_disposition.h"

namespace {

using security_interstitials::MetricsHelper;
using security_interstitials::SecurityInterstitialCommand;
using UkmEntry = ukm::builders::LookalikeUrl_NavigationSuggestion;

// An engagement score above MEDIUM.
const int kHighEngagement = 20;

// An engagement score below MEDIUM.
const int kLowEngagement = 1;

// The UMA metric names registered by metrics_helper.
const char kInterstitialDecisionMetric[] = "interstitial.lookalike.decision";
const char kInterstitialInteractionMetric[] =
    "interstitial.lookalike.interaction";

const char kConsoleMessage[] =
    "Chrome has determined that * could be fake or fraudulent*";

static std::unique_ptr<net::test_server::HttpResponse>
NetworkErrorResponseHandler(const net::test_server::HttpRequest& request) {
  return std::unique_ptr<net::test_server::HttpResponse>(
      new net::test_server::RawHttpResponse("", ""));
}

security_interstitials::SecurityInterstitialPage* GetCurrentInterstitial(
    content::WebContents* web_contents) {
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          web_contents);
  if (!helper) {
    return nullptr;
  }
  return helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting();
}

security_interstitials::SecurityInterstitialPage::TypeID GetInterstitialType(
    content::WebContents* web_contents) {
  security_interstitials::SecurityInterstitialPage* page =
      GetCurrentInterstitial(web_contents);
  if (!page) {
    return nullptr;
  }
  return page->GetTypeForTesting();
}

// Sets the absolute Site Engagement |score| for the testing origin.
void SetEngagementScore(Browser* browser, const GURL& url, double score) {
  site_engagement::SiteEngagementService::Get(browser->profile())
      ->ResetBaseScoreForURL(url, score);
}

bool IsUrlShowing(Browser* browser) {
  return !browser->location_bar_model()->GetFormattedFullURL().empty();
}

// Navigate to |url| and wait for the load to complete before returning.
// Simulates a link click navigation. We don't use
// ui_test_utils::NavigateToURL(const GURL&) because it simulates the user
// typing the URL, causing the site to have a site engagement score of at
// least LOW.
void NavigateToURLSync(Browser* browser, const GURL& url) {
  content::TestNavigationObserver navigation_observer(
      browser->tab_strip_model()->GetActiveWebContents(), 1);

  NavigateParams params(browser, url, ui::PAGE_TRANSITION_LINK);
  params.initiator_origin = url::Origin::Create(GURL("about:blank"));
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.is_renderer_initiated = true;
  ui_test_utils::NavigateToURL(&params);

  navigation_observer.Wait();
}

// Load given URL and verify that it loaded an interstitial and hid the URL.
void LoadAndCheckInterstitialAt(Browser* browser, const GURL& url) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetPattern(kConsoleMessage);

  EXPECT_EQ(nullptr, GetCurrentInterstitial(web_contents));

  NavigateToURLSync(browser, url);
  EXPECT_EQ(LookalikeUrlBlockingPage::kTypeForTesting,
            GetInterstitialType(web_contents));
  EXPECT_FALSE(IsUrlShowing(browser));

  console_observer.Wait();
  EXPECT_TRUE(
      base::MatchPattern(console_observer.GetMessageAt(0u), kConsoleMessage));
}

void SendInterstitialCommand(content::WebContents* web_contents,
                             SecurityInterstitialCommand command) {
  GetCurrentInterstitial(web_contents)
      ->CommandReceived(base::NumberToString(command));
}

void SendInterstitialCommandSync(Browser* browser,
                                 SecurityInterstitialCommand command,
                                 bool punycode_interstitial = false) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(LookalikeUrlBlockingPage::kTypeForTesting,
            GetInterstitialType(web_contents));

  content::TestNavigationObserver navigation_observer(web_contents, 1);
  SendInterstitialCommand(web_contents, command);
  navigation_observer.Wait();

  EXPECT_EQ(nullptr, GetCurrentInterstitial(web_contents));
  if (punycode_interstitial) {
    // "Back to safety" button on the punycode interstitial goes to the New
    // Tab Page which doesn't show the URL.
    EXPECT_FALSE(IsUrlShowing(browser));
  } else {
    EXPECT_TRUE(IsUrlShowing(browser));
  }
}

// Verify that no interstitial is shown, regardless of feature state.
void TestInterstitialNotShown(Browser* browser, const GURL& navigated_url) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  NavigateToURLSync(browser, navigated_url);
  EXPECT_EQ(nullptr, GetCurrentInterstitial(web_contents));

  // Navigate to an empty page. This will happen after any
  // LookalikeUrlService tasks, so will effectively wait for those tasks to
  // finish.
  NavigateToURLSync(browser, GURL("about:blank"));
  EXPECT_EQ(nullptr, GetCurrentInterstitial(web_contents));
}

}  // namespace

class LookalikeUrlNavigationThrottleBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 protected:
  void SetUp() override {
    std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        enabled_features;
    std::vector<base::Feature> disabled_features;

    if (target_embedding_enabled()) {
      base::FieldTrialParams params;
      enabled_features.emplace_back(
          lookalikes::features::kDetectTargetEmbeddingLookalikes, params);
      enabled_features.emplace_back(features::kSignedHTTPExchange, params);
    } else {
      disabled_features.push_back(
          lookalikes::features::kDetectTargetEmbeddingLookalikes);
    }

    if (punycode_interstitial_enabled()) {
      enabled_features.emplace_back(
          lookalikes::features::kLookalikeInterstitialForPunycode,
          base::FieldTrialParams());
    } else {
      disabled_features.push_back(
          lookalikes::features::kLookalikeInterstitialForPunycode);
    }

    if (digital_asset_links_enabled()) {
      enabled_features.emplace_back(
          lookalikes::features::kLookalikeDigitalAssetLinks,
          base::FieldTrialParams());
    } else {
      disabled_features.push_back(
          lookalikes::features::kLookalikeDigitalAssetLinks);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
    reputation::InitializeSafetyTipConfig();
    InProcessBrowserTest::SetUp();
  }

  bool target_embedding_enabled() const { return std::get<0>(GetParam()); }
  bool punycode_interstitial_enabled() const { return std::get<1>(GetParam()); }
  bool digital_asset_links_enabled() const { return std::get<2>(GetParam()); }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    const base::Time kNow = base::Time::FromDoubleT(1000);
    test_clock_.SetNow(kNow);

    LookalikeUrlService* lookalike_service =
        LookalikeUrlService::Get(browser()->profile());
    lookalike_service->SetClockForTesting(&test_clock_);
  }

  GURL GetURL(const char* hostname) const {
    return embedded_test_server()->GetURL(hostname, "/title1.html");
  }

  GURL GetURLWithoutPath(const char* hostname) const {
    return GetURL(hostname).GetWithEmptyPath();
  }

  GURL GetLongRedirect(const char* via_hostname1,
                       const char* via_hostname2,
                       const char* dest_hostname) const {
    GURL dest = GetURL(dest_hostname);
    GURL mid = embedded_test_server()->GetURL(
        via_hostname2, "/server-redirect?" + dest.spec());
    return embedded_test_server()->GetURL(via_hostname1,
                                          "/server-redirect?" + mid.spec());
  }

  // Checks that UKM recorded an event for each URL in |navigated_urls| with the
  // given metric value.
  template <typename T>
  void CheckUkm(const std::vector<GURL>& navigated_urls,
                const std::string& metric_name,
                T metric_value) {
    auto entries = test_ukm_recorder()->GetEntriesByName(UkmEntry::kEntryName);
    ASSERT_EQ(navigated_urls.size(), entries.size());
    int entry_count = 0;
    for (const auto* const entry : entries) {
      test_ukm_recorder()->ExpectEntrySourceHasUrl(entry,
                                                   navigated_urls[entry_count]);
      test_ukm_recorder()->ExpectEntryMetric(entry, metric_name,
                                             static_cast<int>(metric_value));
      entry_count++;
    }
  }

  // Checks that UKM did not record any lookalike URL metrics.
  void CheckNoUkm() {
    EXPECT_TRUE(
        test_ukm_recorder()->GetEntriesByName(UkmEntry::kEntryName).empty());
  }

  // Tests that the histogram event |expected_event| is recorded, the
  // interstitial is displayed and clicking the link on the interstitial works.
  void TestMetricsRecordedAndInterstitialShown(
      Browser* browser,
      const base::HistogramTester& histograms,
      const GURL& navigated_url,
      const GURL& expected_suggested_url,
      NavigationSuggestionEvent expected_event,
      bool expect_signed_exchange = false) {
    history::HistoryService* const history_service =
        HistoryServiceFactory::GetForProfile(
            browser->profile(), ServiceAccessType::EXPLICIT_ACCESS);
    ui_test_utils::WaitForHistoryToLoad(history_service);

    LoadAndCheckInterstitialAt(browser, navigated_url);

    if (expect_signed_exchange) {
      LookalikeUrlBlockingPage* interstitial =
          static_cast<LookalikeUrlBlockingPage*>(GetCurrentInterstitial(
              browser->tab_strip_model()->GetActiveWebContents()));
      EXPECT_TRUE(interstitial->is_signed_exchange_for_testing());
    }

    SendInterstitialCommandSync(browser,
                                SecurityInterstitialCommand::CMD_DONT_PROCEED);
    EXPECT_EQ(expected_suggested_url,
              browser->tab_strip_model()->GetActiveWebContents()->GetURL());

    // Clicking the link in the interstitial should also remove the original
    // URL from history.
    ui_test_utils::HistoryEnumerator enumerator(browser->profile());
    EXPECT_FALSE(base::Contains(enumerator.urls(), navigated_url));

    histograms.ExpectTotalCount(lookalikes::kHistogramName, 1);
    histograms.ExpectBucketCount(lookalikes::kHistogramName, expected_event, 1);

    histograms.ExpectTotalCount(kInterstitialDecisionMetric, 2);
    histograms.ExpectBucketCount(kInterstitialDecisionMetric,
                                 MetricsHelper::SHOW, 1);
    histograms.ExpectBucketCount(kInterstitialDecisionMetric,
                                 MetricsHelper::DONT_PROCEED, 1);

    histograms.ExpectTotalCount(kInterstitialInteractionMetric, 1);
    histograms.ExpectBucketCount(kInterstitialInteractionMetric,
                                 MetricsHelper::TOTAL_VISITS, 1);
  }

  // Tests that the histogram event |expected_event| is recorded, the
  // interstitial is displayed and clicking "Back to safety" on the interstitial
  // works.
  void TestPunycodeInterstitialShown(Browser* browser,
                                     const GURL& navigated_url,
                                     NavigationSuggestionEvent expected_event) {
    base::HistogramTester histograms;

    history::HistoryService* const history_service =
        HistoryServiceFactory::GetForProfile(
            browser->profile(), ServiceAccessType::EXPLICIT_ACCESS);
    ui_test_utils::WaitForHistoryToLoad(history_service);

    LoadAndCheckInterstitialAt(browser, navigated_url);

    // Clicking "Back to safety" should go to the new tab page.
    SendInterstitialCommandSync(browser,
                                SecurityInterstitialCommand::CMD_DONT_PROCEED,
                                /*punycode_interstitial=*/true);
    EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
              browser->tab_strip_model()->GetActiveWebContents()->GetURL());

    histograms.ExpectTotalCount(lookalikes::kHistogramName, 1);
    histograms.ExpectBucketCount(lookalikes::kHistogramName, expected_event, 1);

    histograms.ExpectTotalCount(kInterstitialDecisionMetric, 2);
    histograms.ExpectBucketCount(kInterstitialDecisionMetric,
                                 MetricsHelper::SHOW, 1);
    histograms.ExpectBucketCount(kInterstitialDecisionMetric,
                                 MetricsHelper::DONT_PROCEED, 1);

    histograms.ExpectTotalCount(kInterstitialInteractionMetric, 1);
    histograms.ExpectBucketCount(kInterstitialInteractionMetric,
                                 MetricsHelper::TOTAL_VISITS, 1);
  }

  // Tests that the histogram event |expected_event| is recorded, the
  // interstitial is displayed and clicking through the interstitial works.
  void TestHistogramEventsRecordedWhenInterstitialIgnored(
      Browser* browser,
      base::HistogramTester* histograms,
      const GURL& navigated_url,
      NavigationSuggestionEvent expected_event) {

    history::HistoryService* const history_service =
        HistoryServiceFactory::GetForProfile(
            browser->profile(), ServiceAccessType::EXPLICIT_ACCESS);
    ui_test_utils::WaitForHistoryToLoad(history_service);

    LoadAndCheckInterstitialAt(browser, navigated_url);

    // Clicking the ignore button in the interstitial should remove the
    // interstitial and navigate to the original URL.
    SendInterstitialCommandSync(browser,
                                SecurityInterstitialCommand::CMD_PROCEED);
    EXPECT_EQ(navigated_url,
              browser->tab_strip_model()->GetActiveWebContents()->GetURL());

    // Clicking the link should cause the original URL to appear in history.
    ui_test_utils::HistoryEnumerator enumerator(browser->profile());
    EXPECT_TRUE(base::Contains(enumerator.urls(), navigated_url));

    histograms->ExpectTotalCount(lookalikes::kHistogramName, 1);
    histograms->ExpectBucketCount(lookalikes::kHistogramName, expected_event,
                                  1);

    histograms->ExpectTotalCount(kInterstitialDecisionMetric, 2);
    histograms->ExpectBucketCount(kInterstitialDecisionMetric,
                                  MetricsHelper::SHOW, 1);
    histograms->ExpectBucketCount(kInterstitialDecisionMetric,
                                  MetricsHelper::PROCEED, 1);

    histograms->ExpectTotalCount(kInterstitialInteractionMetric, 1);
    histograms->ExpectBucketCount(kInterstitialInteractionMetric,
                                  MetricsHelper::TOTAL_VISITS, 1);

    TestInterstitialNotShown(browser, navigated_url);
  }

  ukm::TestUkmRecorder* test_ukm_recorder() { return test_ukm_recorder_.get(); }

  base::SimpleTestClock* test_clock() { return &test_clock_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  base::SimpleTestClock test_clock_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    LookalikeUrlNavigationThrottleBrowserTest,
    testing::Combine(testing::Bool() /* target_embedding_enabled */,
                     testing::Bool() /* punycode_interstitial_enabled */,
                     testing::Bool() /* digital_asset_links_enabled */));

// Navigating to a non-IDN shouldn't show an interstitial or record metrics.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       NonIdn_NoMatch) {
  TestInterstitialNotShown(browser(), GetURL("google.com"));
  CheckNoUkm();
}

// Navigating to a domain whose visual representation does not look like a
// top domain shouldn't show an interstitial or record metrics.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       NonTopDomainIdn_NoInterstitial) {
  TestInterstitialNotShown(browser(), GetURL("éxample.com"));
  CheckNoUkm();
}

// If the user has engaged with the domain before, metrics shouldn't be recorded
// and the interstitial shouldn't be shown, even if the domain is visually
// similar to a top domain.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       Idn_TopDomain_EngagedSite_NoMatch) {
  const GURL url = GetURL("googlé.com");
  SetEngagementScore(browser(), url, kHighEngagement);
  TestInterstitialNotShown(browser(), url);
  CheckNoUkm();
}

// Navigate to a domain whose visual representation looks like a top domain.
// This should record metrics. It should also show a lookalike warning
// interstitial if configured via a feature param.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       Idn_TopDomain_Match) {
  const GURL kNavigatedUrl = GetURL("googlé.com");
  const GURL kExpectedSuggestedUrl = GetURLWithoutPath("google.com");
  // Even if the navigated site has a low engagement score, it should be
  // considered for lookalike suggestions.
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);

  base::HistogramTester histograms;
  TestMetricsRecordedAndInterstitialShown(
      browser(), histograms, kNavigatedUrl, kExpectedSuggestedUrl,
      NavigationSuggestionEvent::kMatchSkeletonTop500);

  CheckUkm({kNavigatedUrl}, "MatchType",
           LookalikeUrlMatchType::kSkeletonMatchTop500);
  CheckUkm({kNavigatedUrl}, "TriggeredByInitialUrl", false);
}

// Navigate to a domain that would trigger the warning, but doesn't because it
// fails-safe when the allowlist isn't available.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       NoMatchOnAllowlistMissing) {
  const GURL kNavigatedUrl = GetURL("googlé.com");

  // Clear out any existing proto.
  reputation::SetSafetyTipsRemoteConfigProto(nullptr);

  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  TestInterstitialNotShown(browser(), kNavigatedUrl);
  CheckNoUkm();
}

// Embedding a top domain should show an interstitial when enabled. If disabled
// this would trigger safety tips when target embedding feature parameter is
// enabled for safety tips.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       TargetEmbedding_TopDomain_Match) {
  const GURL kNavigatedUrl = GetURL("google.com-test.com");
  const GURL kExpectedSuggestedUrl = GetURLWithoutPath("google.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  base::HistogramTester histograms;

  // |TestMetricsRecordedAndInterstitialShown| assumes everything should be
  // recorded if target embedding is not disabled. But only for target embedding
  // checks, if TargetEmbedding is not explicitly enabled, it should be treated
  // just like it is disabled. So we make sure an interstitial is not shown if
  // target embedding is not enabled. And defer to
  // |TestMetricsRecordedAndInterstitialShown| otherwise.
  if (!target_embedding_enabled()) {
    TestInterstitialNotShown(browser(), kNavigatedUrl);
    histograms.ExpectTotalCount(lookalikes::kHistogramName, 1);
    histograms.ExpectBucketCount(
        lookalikes::kHistogramName,
        NavigationSuggestionEvent::kMatchTargetEmbedding, 1);
  } else {
    TestMetricsRecordedAndInterstitialShown(
        browser(), histograms, kNavigatedUrl, kExpectedSuggestedUrl,
        NavigationSuggestionEvent::kMatchTargetEmbedding);
  }
  CheckUkm({kNavigatedUrl}, "MatchType",
           LookalikeUrlMatchType::kTargetEmbedding);
  CheckUkm({kNavigatedUrl}, "TriggeredByInitialUrl", false);
}

// Same as TargetEmbedding_TopDomain_Match, but has a redirect where the first
// and last URLs are both target embedding matches. Should only record
// metrics for the first URL. Regression test for crbug.com/1136296.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       TargetEmbedding_TopDomain_Redirect_Match) {
  const GURL kNavigatedUrl = GetLongRedirect("google.com-test.com", "site.com",
                                             "youtube.com-test.com");
  // UKM will record the final URL of the redirect:
  const GURL kLastUrl = GetURL("youtube.com-test.com");
  const GURL kExpectedSuggestedUrl = GetURLWithoutPath("google.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  base::HistogramTester histograms;

  // |TestMetricsRecordedAndInterstitialShown| assumes everything should be
  // recorded if target embedding is not disabled. But only for target embedding
  // checks, if TargetEmbedding is not explicitly enabled, it should be treated
  // just like it is disabled. So we make sure an interstitial is not shown if
  // target embedding is not enabled. And defer to
  // |TestMetricsRecordedAndInterstitialShown| otherwise.
  if (!target_embedding_enabled()) {
    TestInterstitialNotShown(browser(), kNavigatedUrl);
    histograms.ExpectTotalCount(lookalikes::kHistogramName, 1);
    histograms.ExpectBucketCount(
        lookalikes::kHistogramName,
        NavigationSuggestionEvent::kMatchTargetEmbedding, 1);
  } else {
    TestMetricsRecordedAndInterstitialShown(
        browser(), histograms, kNavigatedUrl, kExpectedSuggestedUrl,
        NavigationSuggestionEvent::kMatchTargetEmbedding);
  }
  CheckUkm({kLastUrl}, "MatchType", LookalikeUrlMatchType::kTargetEmbedding);
  CheckUkm({kLastUrl}, "TriggeredByInitialUrl", true);
}

// Target embedding should not trigger on allowlisted embedder domains.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       TargetEmbedding_EmbedderAllowlist) {
  const GURL kNavigatedUrl = GetURL("google.com.allowlisted.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  reputation::SetSafetyTipAllowlistPatterns({"allowlisted.com/"}, {});
  TestInterstitialNotShown(browser(), kNavigatedUrl);
  CheckNoUkm();
}

// Target embedding should not trigger on allowlisted target domains.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       TargetEmbedding_TargetAllowlist) {
  const GURL kNavigatedUrl = GetURL("foo.scholar.google.com.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  reputation::SetSafetyTipAllowlistPatterns({}, {"scholar\\.google\\.com"});
  TestInterstitialNotShown(browser(), kNavigatedUrl);
  CheckNoUkm();
}

// Navigate to a domain target embedding a domain with no separators, but that
// matches the target allowlist.  Regression test for crbug.com/1127450.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       TargetEmbedding_TargetAllowlistWithNoSeparators) {
  const GURL kNavigatedUrl = GetURL("googlecom.example.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  reputation::SetSafetyTipAllowlistPatterns({}, {"google\\.com"});
  TestInterstitialNotShown(browser(), kNavigatedUrl);
  CheckNoUkm();
}

// Similar to Idn_TopDomain_Match but the domain is not in top 500. Should not
// show an interstitial, but should still record metrics.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       Idn_TopDomain_Match_Not500) {
  const GURL kNavigatedUrl = GetURL("googlé.sk");
  // Even if the navigated site has a low engagement score, it should be
  // considered for lookalike suggestions.
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);

  base::HistogramTester histograms;
  TestInterstitialNotShown(browser(), kNavigatedUrl);
  histograms.ExpectTotalCount(lookalikes::kHistogramName, 1);
  histograms.ExpectBucketCount(lookalikes::kHistogramName,
                               NavigationSuggestionEvent::kMatchSkeletonTop5k,
                               1);
  CheckUkm({kNavigatedUrl}, "MatchType",
           LookalikeUrlMatchType::kSkeletonMatchTop5k);
}

// Same as Idn_TopDomain_Match, but this time the domain contains characters
// from different scripts, failing the checks in IDN spoof checker before
// reaching the top domain check. In this case, the end result is the same, but
// the reason we fall back to punycode is different.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       Idn_TopDomainMixedScript_Match) {
  const GURL kNavigatedUrl = GetURL("аррӏе.com");
  const GURL kExpectedSuggestedUrl = GetURLWithoutPath("apple.com");
  // Even if the navigated site has a low engagement score, it should be
  // considered for lookalike suggestions.
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);

  base::HistogramTester histograms;
  TestMetricsRecordedAndInterstitialShown(
      browser(), histograms, kNavigatedUrl, kExpectedSuggestedUrl,
      NavigationSuggestionEvent::kMatchSkeletonTop500);

  CheckUkm({kNavigatedUrl}, "MatchType",
           LookalikeUrlMatchType::kSkeletonMatchTop500);
}

// The navigated domain will fall back to punycode because it fails standard
// ICU spoof checks in IDN spoof checker. If the feature flag to show
// interstitials for punycode fallback is enabled, this will show a punycode
// interstitial. Otherwise, the navigation won't be blocked.
// but there won't be an interstitial because the domain
// doesn't match a top domain.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       Punycode_NoSuggestedUrl_NoInterstitial) {
  const GURL kNavigatedUrl = GetURL("ɴoτ-τoρ-ďoᛖaiɴ.com");

  if (!punycode_interstitial_enabled()) {
    TestInterstitialNotShown(browser(), kNavigatedUrl);
  } else {
    TestPunycodeInterstitialShown(
        browser(), kNavigatedUrl,
        NavigationSuggestionEvent::kFailedSpoofChecks);
  }
  CheckUkm({kNavigatedUrl}, "MatchType",
           LookalikeUrlMatchType::kFailedSpoofChecks);
}

// The navigated domain will fall back to punycode because it fails spoof checks
// in IDN spoof checker. The heuristic that changes this domain to punycode
// (latin middle dot) is configured to show a punycode interstitial.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       Punycode_NoSuggestedUrl_Interstitial) {
  if (!punycode_interstitial_enabled()) {
    return;
  }
  // Navigate to a domain that doesn't trigger target embedding:
  const GURL kNavigatedUrl = GetURL("example·com.com");
  TestPunycodeInterstitialShown(browser(), kNavigatedUrl,
                                NavigationSuggestionEvent::kFailedSpoofChecks);
  CheckUkm({kNavigatedUrl}, "MatchType",
           LookalikeUrlMatchType::kFailedSpoofChecks);
}

// The navigated domain will fall back to punycode because it fails spoof checks
// in IDN spoof checker. The heuristic that changes this domain to punycode
// (latin middle dot) is configured to show a punycode interstitial. The domain
// is also caught by the target embedding heuristic. Target embedding should
// take priority.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       PunycodeAndTargetEmbedding_NoSuggestedUrl_Interstitial) {
  if (!(target_embedding_enabled() && punycode_interstitial_enabled())) {
    return;
  }
  // Navigate to a domain that triggers target embedding:
  const GURL kNavigatedUrl = GetURL("google·com.com");
  const GURL kExpectedSuggestedUrl = GetURLWithoutPath("google.com");
  base::HistogramTester histograms;
  TestMetricsRecordedAndInterstitialShown(
      browser(), histograms, kNavigatedUrl, kExpectedSuggestedUrl,
      NavigationSuggestionEvent::kMatchTargetEmbedding);
  CheckUkm({kNavigatedUrl}, "MatchType",
           LookalikeUrlMatchType::kTargetEmbedding);
}

// The navigated domain itself is a top domain or a subdomain of a top domain.
// Should not record metrics. The top domain list doesn't contain any IDN, so
// this only tests the case where the subdomains are IDNs.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       TopDomainIdnSubdomain_NoMatch) {
  TestInterstitialNotShown(browser(), GetURL("tést.google.com"));
  CheckNoUkm();

  // blogspot.com is a private registry, so the eTLD+1 of "tést.blogspot.com" is
  // itself, instead of just "blogspot.com". This is different than
  // tést.google.com whose eTLD+1 is google.com, and it should be handled
  // correctly.
  TestInterstitialNotShown(browser(), GetURL("tést.blogspot.com"));
  CheckNoUkm();
}

// Schemes other than HTTP and HTTPS should be ignored.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       TopDomainChromeUrl_NoMatch) {
  TestInterstitialNotShown(browser(), GURL("chrome://googlé.com"));
  CheckNoUkm();
}

// Navigate to a domain within an edit distance of 1 to an engaged domain.
// This should record metrics, but should not show a lookalike warning
// interstitial yet.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       EditDistance_EngagedDomain_Match) {
  base::HistogramTester histograms;
  SetEngagementScore(browser(), GURL("https://test-site.com"), kHighEngagement);

  // The skeleton of this domain is one 1 edit away from the skeleton of
  // test-site.com.
  const GURL kNavigatedUrl = GetURL("best-sité.com");
  // Even if the navigated site has a low engagement score, it should be
  // considered for lookalike suggestions.
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  // Advance clock to force a fetch of new engaged sites list.
  test_clock()->Advance(base::TimeDelta::FromHours(1));

  TestInterstitialNotShown(browser(), kNavigatedUrl);
  histograms.ExpectTotalCount(lookalikes::kHistogramName, 1);
  histograms.ExpectBucketCount(
      lookalikes::kHistogramName,
      NavigationSuggestionEvent::kMatchEditDistanceSiteEngagement, 1);

  CheckUkm({kNavigatedUrl}, "MatchType",
           LookalikeUrlMatchType::kEditDistanceSiteEngagement);
}

// Navigate to a domain within an edit distance of 1 to a top domain.
// This should record metrics, but should not show a lookalike warning
// interstitial yet.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       EditDistance_TopDomain_Match) {
  base::HistogramTester histograms;

  // The skeleton of this domain, gooogle.corn, is one 1 edit away from
  // google.corn, the skeleton of google.com.
  const GURL kNavigatedUrl = GetURL("goooglé.com");
  // Even if the navigated site has a low engagement score, it should be
  // considered for lookalike suggestions.
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);

  TestInterstitialNotShown(browser(), kNavigatedUrl);
  histograms.ExpectTotalCount(lookalikes::kHistogramName, 1);
  histograms.ExpectBucketCount(lookalikes::kHistogramName,
                               NavigationSuggestionEvent::kMatchEditDistance,
                               1);

  CheckUkm({kNavigatedUrl}, "MatchType", LookalikeUrlMatchType::kEditDistance);
}

// Navigate to a domain within an edit distance of 1 to a top domain, but that
// matches the allowlist. This should neither record metrics nor show an
// interstitial.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       EditDistance_TopDomain_Target_Allowlist) {
  base::HistogramTester histograms;
  reputation::SetSafetyTipAllowlistPatterns({}, {"google\\.com"});

  // The skeleton of this domain, gooogle.corn, is one 1 edit away from
  // google.corn, the skeleton of google.com.
  const GURL kNavigatedUrl = GetURL("goooglé.com");
  // Even if the navigated site has a low engagement score, it should be
  // considered for lookalike suggestions.
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);

  TestInterstitialNotShown(browser(), kNavigatedUrl);
  histograms.ExpectTotalCount(lookalikes::kHistogramName, 0);
  CheckNoUkm();
}

// Navigate to a domain within an edit distance of 1 to an engaged domain, but
// that matches the allowlist. This should neither record metrics nor show an
// interstitial.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       EditDistance_EngagedDomain_Target_Allowlist) {
  base::HistogramTester histograms;
  SetEngagementScore(browser(), GURL("https://test-site.com"), kHighEngagement);
  reputation::SetSafetyTipAllowlistPatterns({}, {"test-site\\.com"});

  // The skeleton of this domain is one 1 edit away from the skeleton of
  // test-site.com.
  const GURL kNavigatedUrl = GetURL("best-sité.com");
  // Even if the navigated site has a low engagement score, it should be
  // considered for lookalike suggestions.
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  // Advance clock to force a fetch of new engaged sites list.
  test_clock()->Advance(base::TimeDelta::FromHours(1));

  TestInterstitialNotShown(browser(), kNavigatedUrl);
  histograms.ExpectTotalCount(lookalikes::kHistogramName, 0);
  CheckNoUkm();
}

// Tests negative examples for the edit distance.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       EditDistance_TopDomain_NoMatch) {
  // Matches google.com.tr but only differs in registry.
  TestInterstitialNotShown(browser(), GetURL("google.com.tw"));
  CheckNoUkm();

  // Matches academia.edu but is a top domain itself.
  TestInterstitialNotShown(browser(), GetURL("academic.ru"));
  CheckNoUkm();

  // Matches ask.com but is too short.
  TestInterstitialNotShown(browser(), GetURL("bsk.com"));
  CheckNoUkm();
}

// Tests negative examples for the edit distance with engaged sites.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       EditDistance_SiteEngagement_NoMatch) {
  SetEngagementScore(browser(), GURL("https://test-site.com.tr"),
                     kHighEngagement);
  SetEngagementScore(browser(), GURL("https://1234.com"), kHighEngagement);
  SetEngagementScore(browser(), GURL("https://gooogle.com"), kHighEngagement);
  // Advance clock to force a fetch of new engaged sites list.
  test_clock()->Advance(base::TimeDelta::FromHours(1));

  // Matches test-site.com.tr but only differs in registry.
  TestInterstitialNotShown(browser(), GetURL("test-site.com.tw"));
  CheckNoUkm();

  // Matches gooogle.com but is a top domain itself.
  TestInterstitialNotShown(browser(), GetURL("google.com"));
  CheckNoUkm();

  // Matches 1234.com but is too short.
  TestInterstitialNotShown(browser(), GetURL("123.com"));
  CheckNoUkm();
}

// Test that the heuristics are not triggered with net errors.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       NetError_SiteEngagement_Interstitial) {
  // Create a test server that returns invalid responses.
  net::EmbeddedTestServer custom_test_server;
  custom_test_server.RegisterRequestHandler(
      base::BindRepeating(&NetworkErrorResponseHandler));
  ASSERT_TRUE(custom_test_server.Start());

  SetEngagementScore(browser(), GURL("http://site1.com"), kHighEngagement);
  // Advance clock to force a fetch of new engaged sites list.
  test_clock()->Advance(base::TimeDelta::FromHours(1));

  TestInterstitialNotShown(
      browser(), custom_test_server.GetURL("sité1.com", "/title1.html"));
}

// Same as NetError_SiteEngagement_Interstitial, but triggered by a top domain.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       NetError_TopDomain_Interstitial) {
  // Create a test server that returns invalid responses.
  net::EmbeddedTestServer custom_test_server;
  custom_test_server.RegisterRequestHandler(
      base::BindRepeating(&NetworkErrorResponseHandler));
  ASSERT_TRUE(custom_test_server.Start());
  TestInterstitialNotShown(browser(),
                           custom_test_server.GetURL("googlé.com", "/"));
}

// TODO(https://crbug.com/1122078): Enable test when MacOS flake is fixed.
#if defined(OS_MAC)
#define MAYBE_Idn_SiteEngagement_Match DISABLED_Idn_SiteEngagement_Match
#else
#define MAYBE_Idn_SiteEngagement_Match Idn_SiteEngagement_Match
#endif

// Navigate to a domain whose visual representation looks like a domain with a
// site engagement score above a certain threshold. This should record metrics.
// It should also show lookalike warning interstitial if configured via
// a feature param.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       MAYBE_Idn_SiteEngagement_Match) {
  const char* const kEngagedSites[] = {
      "http://site1.com", "http://www.site2.com", "http://sité3.com",
      "http://www.sité4.com"};

  for (const char* const kSite : kEngagedSites) {
    SetEngagementScore(browser(), GURL(kSite), kHighEngagement);
  }

  // The domains here should not be private domains (e.g. site.test), otherwise
  // they might test the wrong thing. Also note that site5.com is in the top
  // domain list, so it shouldn't be used here.
  const struct SiteEngagementTestCase {
    const char* const navigated;
    const char* const suggested;
  } kSiteEngagementTestCases[] = {
      {"sité1.com", "site1.com"},
      {"mail.www.sité1.com", "site1.com"},
      // Same as above two but ending with dots.
      {"sité1.com.", "site1.com"},
      {"mail.www.sité1.com.", "site1.com"},

      // These should match since the comparison uses eTLD+1s.
      {"sité2.com", "site2.com"},
      {"mail.sité2.com", "site2.com"},

      {"síté3.com", "sité3.com"},
      {"mail.síté3.com", "sité3.com"},

      {"síté4.com", "sité4.com"},
      {"mail.síté4.com", "sité4.com"},
  };

  std::vector<GURL> ukm_urls;
  for (const auto& test_case : kSiteEngagementTestCases) {
    const GURL kNavigatedUrl = GetURL(test_case.navigated);
    const GURL kExpectedSuggestedUrl = GetURLWithoutPath(test_case.suggested);

    // Even if the navigated site has a low engagement score, it should be
    // considered for lookalike suggestions.
    SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
    // Advance the clock to force LookalikeUrlService to fetch a new engaged
    // site list.
    test_clock()->Advance(base::TimeDelta::FromHours(1));

    base::HistogramTester histograms;
    TestMetricsRecordedAndInterstitialShown(
        browser(), histograms, kNavigatedUrl, kExpectedSuggestedUrl,
        NavigationSuggestionEvent::kMatchSiteEngagement);

    ukm_urls.push_back(kNavigatedUrl);
    CheckUkm(ukm_urls, "MatchType", LookalikeUrlMatchType::kSiteEngagement);
  }
}

// The site redirects to the matched site, this should not show
// an interstitial.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       Idn_SiteEngagement_SafeRedirect) {
  const GURL kExpectedSuggestedUrl = GetURLWithoutPath("site1.com");
  const GURL kNavigatedUrl = embedded_test_server()->GetURL(
      "sité1.com", "/server-redirect?" + kExpectedSuggestedUrl.spec());
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  SetEngagementScore(browser(), kExpectedSuggestedUrl, kHighEngagement);

  TestInterstitialNotShown(browser(), kNavigatedUrl);
}

// The site redirects to the matched site, but the redirect chain has more than
// two redirects.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       Idn_SiteEngagement_MidRedirectSpoofsIgnored) {
  const GURL kFinalUrl = GetURLWithoutPath("site1.com");
  const GURL kMidUrl = embedded_test_server()->GetURL(
      "sité1.com", "/server-redirect?" + kFinalUrl.spec());
  const GURL kNavigatedUrl = embedded_test_server()->GetURL(
      "other-site.test", "/server-redirect?" + kMidUrl.spec());

  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  SetEngagementScore(browser(), kFinalUrl, kHighEngagement);
  TestInterstitialNotShown(browser(), kNavigatedUrl);
  CheckNoUkm();
}

// The site is allowed by the component updater.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       AllowedByComponentUpdater) {
  reputation::SetSafetyTipAllowlistPatterns(
      {"xn--googl-fsa.com/",  // googlé.com in punycode
       "site.test/", "another-site.test/"},
      {});
  TestInterstitialNotShown(browser(), GetURL("googlé.com"));
  CheckNoUkm();

  // Try a non-HTTP URL. Shouldn't crash.
  TestInterstitialNotShown(browser(), GURL("data:text/html, test"));
  CheckNoUkm();
}

// The site is allowed by enterprise policy.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       AllowedByPolicy) {
  const GURL kNavigatedUrl = GetURL("xn--googl-fsa.com");
  SetEnterpriseAllowlistForTesting(browser()->profile()->GetPrefs(),
                                   {"xn--googl-fsa.com"});

  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  TestInterstitialNotShown(browser(), kNavigatedUrl);
  CheckNoUkm();
}

// Tests negative examples for all heuristics.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       NonUniqueDomains_NoMatch) {
  // Unknown registry.
  TestInterstitialNotShown(browser(), GetURL("google.cóm"));
  CheckNoUkm();

  // Engaged site is localhost, navigated site has unknown registry. This
  // is intended to test that nonunique domains in the engaged site list is
  // filtered out. However, it doesn't quite test that: We'll bail out early
  // because the navigated site has unknown registry (and not because there is
  // no engaged nonunique site).
  SetEngagementScore(browser(), GURL("http://localhost6.localhost"),
                     kHighEngagement);
  test_clock()->Advance(base::TimeDelta::FromHours(1));
  // The skeleton of this URL is localhost6.localpost which is at one edit
  // distance from localhost6.localhost. We use localpost here to prevent an
  // early return in LookalikeUrlNavigationThrottle::HandleThrottleRequest().
  TestInterstitialNotShown(browser(), GURL("http://localhóst6.localpost"));
  CheckNoUkm();
}

// Navigate to a domain whose visual representation looks both like a domain
// with a site engagement score and also a top domain. This should record
// metrics for a site engagement match because of the order of checks. It should
// also show lookalike warning interstitial if configured via a feature param.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       Idn_SiteEngagementAndTopDomain_Match) {
  const GURL kNavigatedUrl = GetURL("googlé.com");
  const GURL kExpectedSuggestedUrl = GetURLWithoutPath("google.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  SetEngagementScore(browser(), kExpectedSuggestedUrl, kHighEngagement);

  // Advance the clock to force LookalikeUrlService to fetch a new engaged
  // site list.
  test_clock()->Advance(base::TimeDelta::FromHours(1));

  base::HistogramTester histograms;
  TestMetricsRecordedAndInterstitialShown(
      browser(), histograms, kNavigatedUrl, kExpectedSuggestedUrl,
      NavigationSuggestionEvent::kMatchSiteEngagement);

  CheckUkm({kNavigatedUrl}, "MatchType",
           LookalikeUrlMatchType::kSiteEngagement);
}

// Similar to Idn_SiteEngagement_Match, but tests a single domain. Also checks
// that the list of engaged sites in incognito and the main profile don't affect
// each other.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       Idn_SiteEngagement_Match_Incognito) {
  const GURL kNavigatedUrl = GetURL("sité1.com");
  const GURL kEngagedUrl = GetURLWithoutPath("site1.com");

  // Set high engagement scores in the main profile and low engagement scores
  // in incognito. Main profile should record metrics, incognito shouldn't.
  Browser* incognito = CreateIncognitoBrowser();
  LookalikeUrlService::Get(incognito->profile())
      ->SetClockForTesting(test_clock());
  SetEngagementScore(browser(), kEngagedUrl, kHighEngagement);
  SetEngagementScore(incognito, kEngagedUrl, kLowEngagement);

  std::vector<GURL> ukm_urls;
  // Main profile should record metrics because there are engaged sites.
  {
    // Advance the clock to force LookalikeUrlService to fetch a new engaged
    // site list.
    test_clock()->Advance(base::TimeDelta::FromHours(1));
    base::HistogramTester histograms;
    TestMetricsRecordedAndInterstitialShown(
        browser(), histograms, kNavigatedUrl, kEngagedUrl,
        NavigationSuggestionEvent::kMatchSiteEngagement);

    ukm_urls.push_back(kNavigatedUrl);
    CheckUkm(ukm_urls, "MatchType", LookalikeUrlMatchType::kSiteEngagement);
  }

  // Incognito shouldn't record metrics because there are no engaged sites.
  {
    base::HistogramTester histograms;
    test_clock()->Advance(base::TimeDelta::FromHours(1));
    TestInterstitialNotShown(incognito, kNavigatedUrl);
    histograms.ExpectTotalCount(lookalikes::kHistogramName, 0);
  }

  // Now reverse the scores: Set low engagement in the main profile and high
  // engagement in incognito.
  SetEngagementScore(browser(), kEngagedUrl, kLowEngagement);
  SetEngagementScore(incognito, kEngagedUrl, kHighEngagement);

  // Incognito should start recording metrics and main profile should stop.
  {
    test_clock()->Advance(base::TimeDelta::FromHours(1));

    base::HistogramTester histograms;
    TestMetricsRecordedAndInterstitialShown(
        incognito, histograms, kNavigatedUrl, kEngagedUrl,
        NavigationSuggestionEvent::kMatchSiteEngagement);
    ukm_urls.push_back(kNavigatedUrl);
    CheckUkm(ukm_urls, "MatchType", LookalikeUrlMatchType::kSiteEngagement);
  }

  // Main profile shouldn't record metrics because there are no engaged sites.
  {
    base::HistogramTester histograms;
    test_clock()->Advance(base::TimeDelta::FromHours(1));
    TestInterstitialNotShown(browser(), kNavigatedUrl);
    histograms.ExpectTotalCount(lookalikes::kHistogramName, 0);
  }
}

// Test that navigations to a site with a high engagement score shouldn't
// record metrics or show interstitial.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       Idn_SiteEngagement_Match_IgnoreHighlyEngagedSite) {
  base::HistogramTester histograms;
  SetEngagementScore(browser(), GURL("http://site-not-in-top-domain-list.com"),
                     kHighEngagement);
  const GURL high_engagement_url = GetURL("síte-not-ín-top-domaín-líst.com");
  SetEngagementScore(browser(), high_engagement_url, kHighEngagement);
  TestInterstitialNotShown(browser(), high_engagement_url);
  histograms.ExpectTotalCount(lookalikes::kHistogramName, 0);
}

// Test that an engaged site with a scheme other than HTTP or HTTPS should be
// ignored.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       Idn_SiteEngagement_IgnoreChromeUrl) {
  base::HistogramTester histograms;
  SetEngagementScore(browser(),
                     GURL("chrome://site-not-in-top-domain-list.com"),
                     kHighEngagement);
  const GURL low_engagement_url("http://síte-not-ín-top-domaín-líst.com");
  SetEngagementScore(browser(), low_engagement_url, kLowEngagement);
  TestInterstitialNotShown(browser(), low_engagement_url);
  histograms.ExpectTotalCount(lookalikes::kHistogramName, 0);
}

// IDNs with a single label should be properly handled. There are two cases
// where this might occur:
// 1. The navigated URL is an IDN with a single label.
// 2. One of the engaged sites is an IDN with a single label.
// Neither of these should cause a crash.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       IdnWithSingleLabelShouldNotCauseACrash) {
  base::HistogramTester histograms;

  // Case 1: Navigating to an IDN with a single label shouldn't cause a crash.
  TestInterstitialNotShown(browser(), GetURL("é"));

  // Case 2: An IDN with a single label with a site engagement score shouldn't
  // cause a crash.
  SetEngagementScore(browser(), GURL("http://tést"), kHighEngagement);
  TestInterstitialNotShown(browser(), GetURL("tést.com"));

  histograms.ExpectTotalCount(lookalikes::kHistogramName, 0);
  CheckNoUkm();
}

// Ensure that dismissing the interstitial works, and the result is remembered
// in the current tab.  This should record metrics on the first visit, but not
// the second.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       Interstitial_Dismiss) {
  base::HistogramTester histograms;

  const GURL kNavigatedUrl = GetURL("sité1.com");
  const GURL kEngagedUrl = GetURL("site1.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  SetEngagementScore(browser(), kEngagedUrl, kHighEngagement);

  TestHistogramEventsRecordedWhenInterstitialIgnored(
      browser(), &histograms, kNavigatedUrl,
      NavigationSuggestionEvent::kMatchSiteEngagement);

  CheckUkm({kNavigatedUrl}, "MatchType",
           LookalikeUrlMatchType::kSiteEngagement);
}

// Navigate to lookalike domains that redirect to benign domains and ensure that
// we display an interstitial along the way.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       Interstitial_CapturesRedirects) {
  {
    // Verify it works when the lookalike domain is the first in the chain
    const GURL kNavigatedUrl =
        GetLongRedirect("googlé.com", "example.net", "example.com");
    SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
    LoadAndCheckInterstitialAt(browser(), kNavigatedUrl);
  }

  // LoadAndCheckInterstitialAt assumes there's not an interstitial already
  // showing (since otherwise it can't be sure that the navigation caused it).
  NavigateToURLSync(browser(), GetURL("example.com"));

  {
    // ...but not when it's in the middle of the chain
    const GURL kNavigatedUrl =
        GetLongRedirect("example.net", "googlé.com", "example.com");
    SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
    TestInterstitialNotShown(browser(), kNavigatedUrl);
  }

  NavigateToURLSync(browser(), GetURL("example.com"));

  {
    // ...but definitely when it's last in the chain.
    const GURL kNavigatedUrl =
        GetLongRedirect("example.net", "example.com", "googlé.com");
    SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
    LoadAndCheckInterstitialAt(browser(), kNavigatedUrl);
  }
}

// Verify that a warning, when ignored, applies to the entire eTLD+1, not just
// the navigated origin.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       AllowlistAppliesToETLDPlusOne) {
  {
    const GURL kNavigatedUrl = GetURL("sub1.googlé.com");
    SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
    LoadAndCheckInterstitialAt(browser(), kNavigatedUrl);
    SendInterstitialCommandSync(browser(),
                                SecurityInterstitialCommand::CMD_PROCEED);
  }

  // TestInterstitialNotShown assumes there's not an interstitial already
  // showing (since otherwise it can't be sure that the navigation caused it).
  NavigateToURLSync(browser(), GetURL("example.com"));

  {
    const GURL kNavigatedUrl = GetURL("sub2.googlé.com");
    SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
    TestInterstitialNotShown(browser(), kNavigatedUrl);
  }

  // We respect private registries for this manual allowlisting so that
  // different (independent) subdomains each show their own warning.
  if (!target_embedding_enabled()) {
    // Since subdomains are only used for target embedding, if that's not
    // enabled, we can bail out now.
    return;
  }
  NavigateToURLSync(browser(), GetURL("example.com"));
  {
    // Note: This uses blogspot.cv because blogspot.com is a top domain, and top
    // domains don't show warnings.
    const GURL kNavigatedUrl = GetURL("google-com.blogspot.cv");
    SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
    LoadAndCheckInterstitialAt(browser(), kNavigatedUrl);
    SendInterstitialCommandSync(browser(),
                                SecurityInterstitialCommand::CMD_PROCEED);
  }
  NavigateToURLSync(browser(), GetURL("example.com"));
  {
    const GURL kNavigatedUrl = GetURL("google-com-unrelated.blogspot.cv");
    SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
    LoadAndCheckInterstitialAt(browser(), kNavigatedUrl);
  }
}

// Verify that the user action in UKM is recorded even when we navigate away
// from the interstitial without interacting with it.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       UkmRecordedAfterNavigateAway) {
  const GURL navigated_url = GetURL("googlé.com");
  const GURL subsequent_url = GetURL("example.com");

  LoadAndCheckInterstitialAt(browser(), navigated_url);
  NavigateToURLSync(browser(), subsequent_url);
  CheckUkm({navigated_url}, "UserAction",
           LookalikeUrlBlockingPageUserAction::kCloseOrBack);
}

// Verify that the user action in UKM is recorded properly when the user accepts
// the navigation suggestion.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       UkmRecordedAfterSuggestionAccepted) {
  const GURL navigated_url = GetURL("googlé.com");

  LoadAndCheckInterstitialAt(browser(), navigated_url);
  SendInterstitialCommandSync(browser(),
                              SecurityInterstitialCommand::CMD_DONT_PROCEED);
  CheckUkm({navigated_url}, "UserAction",
           LookalikeUrlBlockingPageUserAction::kAcceptSuggestion);
}

// Verify that the user action in UKM is recorded properly when the user ignores
// the navigation suggestion.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       UkmRecordedAfterSuggestionIgnored) {
  const GURL navigated_url = GetURL("googlé.com");

  LoadAndCheckInterstitialAt(browser(), navigated_url);
  SendInterstitialCommandSync(browser(),
                              SecurityInterstitialCommand::CMD_PROCEED);
  CheckUkm({navigated_url}, "UserAction",
           LookalikeUrlBlockingPageUserAction::kClickThrough);
}

// Verify that the URL shows normally on pages after a lookalike interstitial.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       UrlShownAfterInterstitial) {
  LoadAndCheckInterstitialAt(browser(), GetURL("googlé.com"));

  // URL should be showing again when we navigate to a normal URL
  NavigateToURLSync(browser(), GetURL("example.com"));
  EXPECT_TRUE(IsUrlShowing(browser()));
}

// Verify that bypassing warnings in the main profile does not affect incognito.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       MainProfileDoesNotAffectIncognito) {
  const GURL kNavigatedUrl = GetURL("googlé.com");

  // Set low engagement scores in the main profile and in incognito.
  Browser* incognito = CreateIncognitoBrowser();
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  SetEngagementScore(incognito, kNavigatedUrl, kLowEngagement);

  LoadAndCheckInterstitialAt(browser(), kNavigatedUrl);
  // PROCEEDing will disable the interstitial on subsequent navigations
  SendInterstitialCommandSync(browser(),
                              SecurityInterstitialCommand::CMD_PROCEED);

  LoadAndCheckInterstitialAt(incognito, kNavigatedUrl);
}

// Verify that bypassing warnings in incognito does not affect the main profile.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       IncognitoDoesNotAffectMainProfile) {
  const GURL kNavigatedUrl = GetURL("sité1.com");
  const GURL kEngagedUrl = GetURL("site1.com");

  // Set engagement scores in the main profile and in incognito.
  Browser* incognito = CreateIncognitoBrowser();
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  SetEngagementScore(incognito, kNavigatedUrl, kLowEngagement);
  SetEngagementScore(browser(), kEngagedUrl, kHighEngagement);
  SetEngagementScore(incognito, kEngagedUrl, kHighEngagement);

  LoadAndCheckInterstitialAt(incognito, kNavigatedUrl);
  // PROCEEDing will disable the interstitial on subsequent navigations
  SendInterstitialCommandSync(incognito,
                              SecurityInterstitialCommand::CMD_PROCEED);

  LoadAndCheckInterstitialAt(browser(), kNavigatedUrl);
}

// Verify reloading the page does not result in dismissing an interstitial.
// Regression test for crbug/941886.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       RefreshDoesntDismiss) {
  // Verify it works when the lookalike domain is the first in the chain.
  const GURL kNavigatedUrl =
      GetLongRedirect("googlé.com", "example.net", "example.com");

  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  LoadAndCheckInterstitialAt(browser(), kNavigatedUrl);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Reload the interstitial twice. Should still work.
  for (size_t i = 0; i < 2; i++) {
    content::TestNavigationObserver navigation_observer(web_contents);
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    navigation_observer.Wait();

    EXPECT_EQ(LookalikeUrlBlockingPage::kTypeForTesting,
              GetInterstitialType(web_contents));
    EXPECT_FALSE(IsUrlShowing(browser()));
  }

  // Go to the affected site directly. This should not result in an
  // interstitial.
  TestInterstitialNotShown(browser(),
                           embedded_test_server()->GetURL("example.net", "/"));
}

scoped_refptr<net::X509Certificate> LoadCertificate() {
  constexpr char kCertFileName[] = "prime256v1-sha256-google-com.public.pem";

  base::ScopedAllowBlockingForTesting allow_io;
  base::FilePath dir_path;
  base::PathService::Get(content::DIR_TEST_DATA, &dir_path);
  dir_path = dir_path.Append(FILE_PATH_LITERAL("sxg"));

  return net::CreateCertificateChainFromFile(
      dir_path, kCertFileName, net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
}

// Tests for Signed Exchanges.
class LookalikeUrlNavigationThrottleSignedExchangeBrowserTest
    : public LookalikeUrlNavigationThrottleBrowserTest {
 public:
  LookalikeUrlNavigationThrottleSignedExchangeBrowserTest() {
    net::EmbeddedTestServer::RegisterTestCerts();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void SetUp() override {
    sxg_test_helper_.SetUp();
    LookalikeUrlNavigationThrottleBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    https_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    https_server_.ServeFilesFromSourceDirectory("content/test/data");
    https_server_.RegisterRequestMonitor(base::BindRepeating(
        &LookalikeUrlNavigationThrottleSignedExchangeBrowserTest::
            MonitorRequest,
        base::Unretained(this)));
    ASSERT_TRUE(https_server_.Start());

    LookalikeUrlNavigationThrottleBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    sxg_test_helper_.TearDownOnMainThread();
  }

  bool HadSignedExchangeInAcceptHeader(const GURL& url) const {
    const auto it = url_accept_header_map_.find(url);
    if (it == url_accept_header_map_.end())
      return false;
    return it->second.find("application/signed-exchange") != std::string::npos;
  }

  void InstallMockCert() {
    sxg_test_helper_.InstallMockCert(mock_cert_verifier_.mock_cert_verifier());

    // Make the MockCertVerifier treat the certificate
    // "prime256v1-sha256-google-com.public.pem" as valid for
    // "google-com.example.org".
    scoped_refptr<net::X509Certificate> original_cert = LoadCertificate();
    net::CertVerifyResult dummy_result;
    dummy_result.verified_cert = original_cert;
    dummy_result.cert_status = net::OK;
    dummy_result.ocsp_result.response_status = net::OCSPVerifyResult::PROVIDED;
    dummy_result.ocsp_result.revocation_status =
        net::OCSPRevocationStatus::GOOD;
    mock_cert_verifier_.mock_cert_verifier()->AddResultForCertAndHost(
        original_cert, "google-com.example.org", dummy_result, net::OK);
  }

  void InstallMockCertChainInterceptor() {
    sxg_test_helper_.InstallMockCertChainInterceptor();
    sxg_test_helper_.InstallUrlInterceptor(
        GURL("https://google-com.example.org/cert.msg"),
        "content/test/data/sxg/google-com.example.org.public.pem.cbor");
  }

 protected:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  content::SignedExchangeBrowserTestHelper sxg_test_helper_;
  content::ContentMockCertVerifier mock_cert_verifier_;

 private:
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    const auto it = request.headers.find("Accept");
    if (it == request.headers.end())
      return;
    url_accept_header_map_[request.base_url.Resolve(request.relative_url)] =
        it->second;
  }

  std::map<GURL, std::string> url_accept_header_map_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    LookalikeUrlNavigationThrottleSignedExchangeBrowserTest,
    testing::Combine(testing::Bool() /* target_embedding_enabled */,
                     testing::Bool() /* punycode_interstitial_enabled */,
                     testing::Bool() /* digital_asset_links_enabled */));

// Navigates to a 127.0.0.1 URL that serves a signed exchange for
// google-com.example.org. This navigation should be blocked by the target
// embedding interstitial. We only test target embedding here because we can
// test it with a subdomain of example.org (which is the domain used by SGX test
// code). Testing an ETLD+1 such as googlé.com would require generating a custom
// cert.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleSignedExchangeBrowserTest,
                       InnerUrlIsLookalike_ShouldBlock) {
  if (!target_embedding_enabled()) {
    return;
  }
  InstallMockCert();
  InstallMockCertChainInterceptor();

  sxg_test_helper_.InstallUrlInterceptor(
      GURL("https://google-com.example.org/test/"),
      "content/test/data/sxg/fallback.html");
  const GURL kNavigatedUrl =
      https_server_.GetURL("/sxg/google-com.example.org_test.sxg");
  const GURL kExpectedSuggestedUrl("https://google.com");

  base::HistogramTester histograms;
  TestMetricsRecordedAndInterstitialShown(
      browser(), histograms, kNavigatedUrl, kExpectedSuggestedUrl,
      NavigationSuggestionEvent::kMatchTargetEmbedding,
      true /* expect_signed_exchange */);

  // Check that the SXG file was handled as a Signed Exchange.
  ASSERT_TRUE(HadSignedExchangeInAcceptHeader(kNavigatedUrl));
}

// Navigates to a lookalike URL (google-com.test.com) that serves a signed
// exchange for test.example.org. This should not be blocked.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleSignedExchangeBrowserTest,
                       OuterUrlIsLookalike_ShouldNotBlock) {
  if (!target_embedding_enabled()) {
    return;
  }

  InstallMockCert();
  InstallMockCertChainInterceptor();

  const GURL kSgxTargetUrl("https://test.example.org/test/");
  sxg_test_helper_.InstallUrlInterceptor(kSgxTargetUrl,
                                         "content/test/data/sxg/fallback.html");
  const GURL kNavigatedUrl = https_server_.GetURL(
      "google-com.test.com", "/sxg/test.example.org_test.sxg");

  TestInterstitialNotShown(browser(), kNavigatedUrl);

  // Check that the SXG file was handled as a Signed Exchange.
  // MonitorRequest() sees kNavigatedUrl with an IP address instead of
  // domain name, so check it instead.
  const GURL kResolvedNavigatedUrl =
      https_server_.GetURL("/sxg/test.example.org_test.sxg");
  ASSERT_TRUE(HadSignedExchangeInAcceptHeader(kResolvedNavigatedUrl));
}

// Navigates to a lookalike URL (google-com.test.com) that serves a signed
// exchange for test.example.org. This should not be blocked.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleSignedExchangeBrowserTest,
                       OuterUrlIsLookalikeButNotSignedExchange_ShouldNotBlock) {
  if (!target_embedding_enabled()) {
    return;
  }

  InstallMockCert();
  InstallMockCertChainInterceptor();

  const GURL kSgxTargetUrl("https://test.example.org/test/");
  sxg_test_helper_.InstallUrlInterceptor(kSgxTargetUrl,
                                         "content/test/data/sxg/fallback.html");
  const GURL kSgxCacheUrl = https_server_.GetURL(
      "google-com.test.com", "/sxg/test.example.org_test.sxg");
  const GURL kNavigatedUrl = embedded_test_server()->GetURL(
      "apple-com.site.com", "/server-redirect?" + kSgxCacheUrl.spec());

  TestInterstitialNotShown(browser(), kNavigatedUrl);

  // Check that the SXG file was handled as a Signed Exchange.
  // MonitorRequest() sees kNavigatedUrl with an IP address instead of
  // domain name, so check it instead.
  const GURL kResolvedNavigatedUrl =
      https_server_.GetURL("/sxg/test.example.org_test.sxg");
  ASSERT_TRUE(HadSignedExchangeInAcceptHeader(kResolvedNavigatedUrl));
}

// Navigates to a lookalike URL (google-com.test.com) that serves a signed
// exchange for google-com.example.org.
// Both the outer URL (i.e. cache) and the inner URL are lookalikes so this
// should be blocked.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleSignedExchangeBrowserTest,
                       InnerAndOuterUrlsAreLookalikes_ShouldBlock) {
  if (!target_embedding_enabled()) {
    return;
  }
  InstallMockCert();
  InstallMockCertChainInterceptor();

  sxg_test_helper_.InstallUrlInterceptor(
      GURL("https://google-com.example.org/test/"),
      "content/test/data/sxg/fallback.html");
  const GURL kNavigatedUrl = https_server_.GetURL(
      "google-com.test.com", "/sxg/google-com.example.org_test.sxg");
  const GURL kExpectedSuggestedUrl("https://google.com");

  base::HistogramTester histograms;
  TestMetricsRecordedAndInterstitialShown(
      browser(), histograms, kNavigatedUrl, kExpectedSuggestedUrl,
      NavigationSuggestionEvent::kMatchTargetEmbedding,
      true /* expect_signed_exchange */);

  // Check that the SXG file was handled as a Signed Exchange.
  // MonitorRequest() sees kNavigatedUrl with an IP address instead of
  // domain name, so check it instead.
  const GURL kResolvedNavigatedUrl =
      https_server_.GetURL("/sxg/google-com.example.org_test.sxg");
  ASSERT_TRUE(HadSignedExchangeInAcceptHeader(kResolvedNavigatedUrl));
}

// TODO(meacer): Add a test for a failed SGX response. It should be treated
// as a normal redirect. In fact, InnerAndOuterUrlsLookalikes_ShouldBlock
// is actually testing this right now, fix it.

// Tests for Digital Asset Links.
class LookalikeUrlNavigationThrottleDigitalAssetLinksBrowserTest
    : public LookalikeUrlNavigationThrottleBrowserTest {
 public:
  struct TestSite {
    std::string hostname;
    std::string manifest;
    bool slow_load = false;
  };

  enum SlowLoad { kNone, kLookalike, kTarget };
  // Sets up the site |lookalike_hostname| to serve a manifest with contents
  // |lookalike_manifest|, and the site |target_hostname| to serve a manifest
  // with contents |target_manifest| and navigates to |lookalike_hostname|.
  // Expects an interstitial with the suggested hostname
  // |expected_suggested_hostname|.
  void TestExpectInterstitial(const base::HistogramTester& histograms,
                              const char* lookalike_hostname,
                              const std::string& lookalike_manifest,
                              const char* target_hostname,
                              const std::string& target_manifest,
                              const char* expected_suggested_hostname,
                              bool is_target_embedding,
                              SlowLoad slow_load = SlowLoad::kNone) {
    const GURL kNavigatedUrl = MakeURL(lookalike_hostname);
    const std::vector<TestSite> sites{
        {lookalike_hostname, lookalike_manifest,
         slow_load == SlowLoad::kLookalike},
        {target_hostname, target_manifest, slow_load == SlowLoad::kTarget},
    };
    NavigationSuggestionEvent expected_event =
        is_target_embedding ? NavigationSuggestionEvent::kMatchTargetEmbedding
                            : NavigationSuggestionEvent::kMatchSkeletonTop500;
    SetUpManifests(sites);
    TestMetricsRecordedAndInterstitialShown(
        browser(), histograms, kNavigatedUrl,
        MakeURL(expected_suggested_hostname), expected_event);
    CheckUkm({kNavigatedUrl}, "MatchType",
             is_target_embedding ? LookalikeUrlMatchType::kTargetEmbedding
                                 : LookalikeUrlMatchType::kSkeletonMatchTop500);
    // Ensure that there was indeed a lookalike match.
    histograms.ExpectTotalCount(lookalikes::kHistogramName, 1);
    histograms.ExpectBucketCount(lookalikes::kHistogramName, expected_event, 1);
  }

  // Sets up the site |lookalike_hostname| to serve a manifest with contents
  // |lookalike_manifest|, and the site |target_hostname| to serve a manifest
  // with contents |target_manifest| and navigates to |lookalike_hostname|.
  // Expects no interstitial.
  void TestNoInterstitial(const base::HistogramTester& histograms,
                          const char* lookalike_hostname,
                          const std::string& lookalike_manifest,
                          const char* target_hostname,
                          const std::string& target_manifest) {
    const std::vector<TestSite> sites{
        {lookalike_hostname, lookalike_manifest},
        {target_hostname, target_manifest},
    };
    SetUpManifests(sites);

    TestInterstitialNotShown(browser(),  // histograms,
                             MakeURL(lookalike_hostname));
    CheckNoUkm();
    // Ensure that there was indeed a lookalike match.
    histograms.ExpectTotalCount(lookalikes::kHistogramName, 1);
    histograms.ExpectBucketCount(
        lookalikes::kHistogramName,
        NavigationSuggestionEvent::kMatchSkeletonTop500, 1);
  }

  void SetUpManifests(const std::vector<TestSite>& sites) {
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            &LookalikeUrlNavigationThrottleDigitalAssetLinksBrowserTest::
                OnIntercept,
            base::Unretained(this), sites));
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  bool OnIntercept(const std::vector<TestSite>& sites,
                   content::URLLoaderInterceptor::RequestParams* params) {
    for (const TestSite& site : sites) {
      if (params->url_request.url == MakeManifestURL(site.hostname)) {
        if (site.slow_load) {
          test_clock()->Advance(base::TimeDelta::FromMinutes(15));
        }
        if (!site.manifest.empty()) {
          // Serve manifest contents:
          std::string headers =
              "HTTP/1.1 200 OK\nContent-Type: application/json; "
              "charset=utf-8\n";
          content::URLLoaderInterceptor::WriteResponse(headers, site.manifest,
                                                       params->client.get());
        } else {
          // Serve error:
          params->client->OnComplete(
              network::URLLoaderCompletionStatus(net::ERR_CONNECTION_RESET));
        }
        return true;
      }
      if (params->url_request.url == MakeURL(site.hostname)) {
        content::URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n",
            "<html>Test page</html>", params->client.get());
        return true;
      }
    }
    return false;
  }

  static GURL MakeManifestURL(const std::string& hostname) {
    return GURL("https://" + hostname + "/.well-known/assetlinks.json");
  }

  static GURL MakeURL(const std::string& hostname) {
    return GURL("https://" + hostname);
  }

  static std::string MakeManifestWithTarget(const char* target_domain,
                                            bool invalid = false) {
    const char* const format = R"([{
        "relation": ["%s"],
        "target": {
          "namespace": "web",
          "site": "https://%s"
        }
      }]
      )";
    // Go through MakeURL to convert target_domain to punycode.
    return base::StringPrintf(format,
                              (invalid ? "junkvalue" : "lookalikes/allowlist"),
                              MakeURL(target_domain).host().c_str());
  }

 private:
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    LookalikeUrlNavigationThrottleDigitalAssetLinksBrowserTest,
    testing::Combine(testing::Bool() /* target_embedding_enabled */,
                     testing::Bool() /* punycode_interstitial_enabled */,
                     testing::Values(true) /* digital_asset_links_enabled */));

// Neither site serves a manifest.
IN_PROC_BROWSER_TEST_P(
    LookalikeUrlNavigationThrottleDigitalAssetLinksBrowserTest,
    NoAssetLinks_ShowInterstitial) {
  base::HistogramTester histograms;
  TestExpectInterstitial(histograms, "googlé.com", std::string(), "google.com",
                         std::string(),
                         /*expected_suggested_hostname=*/"google.com",
                         /*is_target_embedding=*/false);

  histograms.ExpectTotalCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName, 2);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kStarted, 1);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kLookalikeManifestFailed, 1);
}

// Both lookalike and target sites serve valid asset link manifests pointing to
// each other.
IN_PROC_BROWSER_TEST_P(
    LookalikeUrlNavigationThrottleDigitalAssetLinksBrowserTest,
    ValidAssetLinks_IgnoreInterstitial) {
  base::HistogramTester histograms;

  TestNoInterstitial(histograms, "googlé.com",
                     MakeManifestWithTarget("google.com"), "google.com",
                     MakeManifestWithTarget("googlé.com"));
  histograms.ExpectTotalCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName, 2);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kStarted, 1);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kValidationSucceeded, 1);

  // Try again. The first try should have added the lookalike site to the
  // allowlist so we shouldn't fetch the manifests again.
  TestInterstitialNotShown(browser(), MakeURL("googlé.com"));
  CheckNoUkm();
  // Ensure that there was indeed a lookalike match.
  histograms.ExpectTotalCount(lookalikes::kHistogramName, 1);
  histograms.ExpectBucketCount(lookalikes::kHistogramName,
                               NavigationSuggestionEvent::kMatchSkeletonTop500,
                               1);
  // Validator histogram should remain unchanged.
  histograms.ExpectTotalCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName, 2);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kStarted, 1);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kValidationSucceeded, 1);
}

// Similar to ValidAssetLinks_IgnoreInterstitial, but the lookalike manifest
// fetch times out.
IN_PROC_BROWSER_TEST_P(
    LookalikeUrlNavigationThrottleDigitalAssetLinksBrowserTest,
    ValidAssetLinks_LookalikeTimeout_ShowInterstitial) {
  base::HistogramTester histograms;

  TestExpectInterstitial(histograms, "googlé.com",
                         MakeManifestWithTarget("google.com"), "google.com",
                         MakeManifestWithTarget("googlé.com"),
                         /*expected_suggested_hostname=*/"google.com",
                         /*is_target_embedding=*/false, SlowLoad::kLookalike);

  histograms.ExpectTotalCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName, 2);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kStarted, 1);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kLookalikeManifestTimedOut, 1);
}

// Similar to ValidAssetLinks_IgnoreInterstitial, but the target manifest fetch
// times out.
IN_PROC_BROWSER_TEST_P(
    LookalikeUrlNavigationThrottleDigitalAssetLinksBrowserTest,
    ValidAssetLinks_TargetTimeout_ShowInterstitial) {
  base::HistogramTester histograms;

  TestExpectInterstitial(histograms, "googlé.com",
                         MakeManifestWithTarget("google.com"), "google.com",
                         MakeManifestWithTarget("googlé.com"),
                         /*expected_suggested_hostname=*/"google.com",
                         /*is_target_embedding=*/false, SlowLoad::kTarget);

  histograms.ExpectTotalCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName, 2);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kStarted, 1);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kTargetManifestTimedOut, 1);
}

// Both lookalike and target sites serve asset links. Lookalike site's manifest
// has an entry for the target, but the target site's manifest doesn't have one
// for the lookalike.
IN_PROC_BROWSER_TEST_P(
    LookalikeUrlNavigationThrottleDigitalAssetLinksBrowserTest,
    ValidAssetLinks_TargetManifestNotMatching_ShowInterstitial) {
  base::HistogramTester histograms;
  TestExpectInterstitial(histograms, "góógle.com",
                         MakeManifestWithTarget("google.com"), "google.com",
                         MakeManifestWithTarget("site.test"),
                         /*expected_suggested_hostname=*/"google.com",
                         /*is_target_embedding=*/false);

  histograms.ExpectTotalCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName, 2);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kStarted, 1);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kTargetManifestFailed, 1);
}

// Both lookalike and target sites serve asset links. Lookalike site's manifest
// has an entry for the target, but the target site's manifest doesn't have
// valid content.
IN_PROC_BROWSER_TEST_P(
    LookalikeUrlNavigationThrottleDigitalAssetLinksBrowserTest,
    ValidAssetLinks_TargetManifestInvalid_ShowInterstitial) {
  base::HistogramTester histograms;
  TestExpectInterstitial(histograms, "góógle.com",
                         MakeManifestWithTarget("google.com"), "google.com",
                         MakeManifestWithTarget("góógle.com", /*invalid=*/true),
                         /*expected_suggested_hostname=*/"google.com",
                         /*is_target_embedding=*/false);

  histograms.ExpectTotalCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName, 2);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kStarted, 1);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kTargetManifestFailed, 1);
}

// Both lookalike and target sites are subdomains and serve valid asset links.
// However, lookalike heuristics match to eTLD+1s, so the lookalike's manifest
// will fail to validate because it points to the full URL instead.
// TODO(meacer): Support this use case.
IN_PROC_BROWSER_TEST_P(
    LookalikeUrlNavigationThrottleDigitalAssetLinksBrowserTest,
    ValidSubdomainAssetLinks_NoMatch_ShowInterstitial) {
  base::HistogramTester histograms;
  TestExpectInterstitial(
      histograms, "docs.góógle.com", MakeManifestWithTarget("docs.google.com"),
      "docs.google.com", MakeManifestWithTarget("docs.góógle.com"),
      /*expected_suggested_hostname=*/"google.com",
      /*is_target_embedding=*/false);

  histograms.ExpectTotalCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName, 2);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kStarted, 1);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kLookalikeManifestFailed, 1);
}

// Both lookalike and target sites are subdomains and serve valid asset links.
// This time, the lookalike's manifest points to the eTLD+1 of the matching
// domain so the validation is successful.
IN_PROC_BROWSER_TEST_P(
    LookalikeUrlNavigationThrottleDigitalAssetLinksBrowserTest,
    ValidSubdomainAssetLinks_IgnoreInterstitial) {
  base::HistogramTester histograms;
  TestNoInterstitial(histograms, "docs.góógle.com",
                     MakeManifestWithTarget("google.com"), "google.com",
                     MakeManifestWithTarget("docs.góógle.com"));

  histograms.ExpectTotalCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName, 2);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kStarted, 1);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kValidationSucceeded, 1);
}

// Similar to ValidSubdomainAssetLinks_IgnoreInterstitial with a few
// differences:
// - Lookalike is a subdomain but target is an eTLD+1 and lookalike's manifest
//   points to this eTLD+1.
// - This time, the target's manifest points to the eTLD+1 of the lookalike
//   instead of its fully qualified domain name.
// Validation is still successful despite the fact that the target manifest
// doesn't list the FQDN of the lookalike.
IN_PROC_BROWSER_TEST_P(
    LookalikeUrlNavigationThrottleDigitalAssetLinksBrowserTest,
    ValidSubdomainAssetLinks_TargetDoesntListLookalikeFQDN_IgnoreInterstitial) {
  base::HistogramTester histograms;
  TestNoInterstitial(
      histograms,
      /*lookalike_hostname=*/"docs.góógle.com",
      /*lookalike_manifest=*/MakeManifestWithTarget("google.com"),
      /*target_hostname=*/"google.com",
      /*target_manifest=*/MakeManifestWithTarget("góógle.com"));

  histograms.ExpectTotalCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName, 2);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kStarted, 1);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kValidationSucceeded, 1);
}

// Similar to ValidSubdomainAssetLinks_NoMatch_ShowInterstitial but the target
// is a subdomain. In some cases the target embedding heuristic can suggest
// subdomains instead of eTLD+1s. This doesn't show an interstitial.
// TODO(crbug.com/1174862): This currently isn't testing the right thing because
// Chrome suggests targetembedding.com instead of test.targetembedding.com.
IN_PROC_BROWSER_TEST_P(
    LookalikeUrlNavigationThrottleDigitalAssetLinksBrowserTest,
    ValidSubdomainAssetLinks_TargetIsSubdomain_ShowInterstitial) {
  if (!target_embedding_enabled()) {
    return;
  }

  const char* kSuggestedSite = "targetembedding.com";
  SetEngagementScore(browser(), GetURLWithoutPath(kSuggestedSite),
                     kHighEngagement);

  base::HistogramTester histograms;
  TestExpectInterstitial(
      histograms,
      /*lookalike_hostname=*/"test-targetembedding-com.com",
      /*lookalike_manifest=*/MakeManifestWithTarget("test.targetembedding.com"),
      /*target_hostname=*/"test.targetembedding.com",
      /*target_manifest=*/
      MakeManifestWithTarget("test-targetembedding-com.com"),
      /*expected_suggested_hostname=*/kSuggestedSite,
      /*is_target_embedding=*/true);

  histograms.ExpectTotalCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName, 2);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kStarted, 1);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kLookalikeManifestFailed, 1);
}
