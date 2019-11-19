// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/lookalikes/lookalike_url_interstitial_page.h"
#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "ui/base/window_open_disposition.h"

namespace {

using security_interstitials::MetricsHelper;
using security_interstitials::SecurityInterstitialCommand;
using UkmEntry = ukm::builders::LookalikeUrl_NavigationSuggestion;

using MatchType = LookalikeUrlInterstitialPage::MatchType;
using UserAction = LookalikeUrlInterstitialPage::UserAction;

enum class UIStatus {
  kDisabled,
  kEnabledForSiteEngagement,
  kEnabledForSiteEngagementAndTopDomains
};

// An engagement score above MEDIUM.
const int kHighEngagement = 20;

// An engagement score below MEDIUM.
const int kLowEngagement = 1;

// The UMA metric names registered by metrics_helper.
const char kInterstitialDecisionMetric[] = "interstitial.lookalike.decision";
const char kInterstitialInteractionMetric[] =
    "interstitial.lookalike.interaction";

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
  SiteEngagementService::Get(browser->profile())
      ->ResetBaseScoreForURL(url, score);
}

bool IsUrlShowing(Browser* browser) {
  return !browser->location_bar_model()->GetFormattedFullURL().empty();
}

// Simulates a link click navigation. We don't use
// ui_test_utils::NavigateToURL(const GURL&) because it simulates the user
// typing the URL, causing the site to have a site engagement score of at
// least LOW.
void NavigateToURL(Browser* browser, const GURL& url) {
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_LINK);
  params.initiator_origin = url::Origin::Create(GURL("about:blank"));
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.is_renderer_initiated = true;
  ui_test_utils::NavigateToURL(&params);
}

// Same as NavigateToUrl, but wait for the load to complete before returning.
void NavigateToURLSync(Browser* browser, const GURL& url) {
  content::TestNavigationObserver navigation_observer(
      browser->tab_strip_model()->GetActiveWebContents(), 1);
  NavigateToURL(browser, url);
  navigation_observer.Wait();
}

// Load given URL and verify that it loaded an interstitial and hid the URL.
void LoadAndCheckInterstitialAt(Browser* browser, const GURL& url) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(nullptr, GetCurrentInterstitial(web_contents));

  NavigateToURLSync(browser, url);
  EXPECT_EQ(LookalikeUrlInterstitialPage::kTypeForTesting,
            GetInterstitialType(web_contents));
  EXPECT_FALSE(IsUrlShowing(browser));
}

void SendInterstitialCommand(content::WebContents* web_contents,
                             SecurityInterstitialCommand command) {
  GetCurrentInterstitial(web_contents)
      ->CommandReceived(base::NumberToString(command));
}

void SendInterstitialCommandSync(Browser* browser,
                                 SecurityInterstitialCommand command) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(LookalikeUrlInterstitialPage::kTypeForTesting,
            GetInterstitialType(web_contents));

  content::TestNavigationObserver navigation_observer(web_contents, 1);
  SendInterstitialCommand(web_contents, command);
  navigation_observer.Wait();

  EXPECT_EQ(nullptr, GetCurrentInterstitial(web_contents));
  EXPECT_TRUE(IsUrlShowing(browser));
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
      public testing::WithParamInterface<UIStatus> {
 protected:
  void SetUp() override {
    switch (ui_status()) {
      case UIStatus::kDisabled:
        feature_list_.InitAndDisableFeature(
            features::kLookalikeUrlNavigationSuggestionsUI);
        break;

      case UIStatus::kEnabledForSiteEngagement:
        feature_list_.InitAndEnableFeature(
            features::kLookalikeUrlNavigationSuggestionsUI);
        break;

      case UIStatus::kEnabledForSiteEngagementAndTopDomains:
        feature_list_.InitAndEnableFeatureWithParameters(
            features::kLookalikeUrlNavigationSuggestionsUI,
            {{"topsites", "true"}});
    }
    InProcessBrowserTest::SetUp();
  }

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

  // Returns true if the current test parameter should result in showing an
  // interstitial for |expected_event|.
  bool ShouldExpectInterstitial(
      LookalikeUrlNavigationThrottle::NavigationSuggestionEvent expected_event)
      const {
    if (!ui_enabled()) {
      return false;
    }
    if (expected_event == LookalikeUrlNavigationThrottle::
                              NavigationSuggestionEvent::kMatchSiteEngagement) {
      return true;
    }
    if (expected_event == LookalikeUrlNavigationThrottle::
                              NavigationSuggestionEvent::kMatchTopSite &&
        ui_status() == UIStatus::kEnabledForSiteEngagementAndTopDomains) {
      return true;
    }
    return false;
  }

  // Tests that the histogram event |expected_event| is recorded. If the UI is
  // enabled, additional events for interstitial display and link click will
  // also be tested.
  void TestMetricsRecordedAndMaybeInterstitialShown(
      Browser* browser,
      const GURL& navigated_url,
      const GURL& expected_suggested_url,
      LookalikeUrlNavigationThrottle::NavigationSuggestionEvent
          expected_event) {
    base::HistogramTester histograms;
    if (!ShouldExpectInterstitial(expected_event)) {
      TestInterstitialNotShown(browser, navigated_url);
      histograms.ExpectTotalCount(
          LookalikeUrlNavigationThrottle::kHistogramName, 1);
      histograms.ExpectBucketCount(
          LookalikeUrlNavigationThrottle::kHistogramName, expected_event, 1);

      return;
    }

    history::HistoryService* const history_service =
        HistoryServiceFactory::GetForProfile(
            browser->profile(), ServiceAccessType::EXPLICIT_ACCESS);
    ui_test_utils::WaitForHistoryToLoad(history_service);

    LoadAndCheckInterstitialAt(browser, navigated_url);
    SendInterstitialCommandSync(browser,
                                SecurityInterstitialCommand::CMD_DONT_PROCEED);
    EXPECT_EQ(expected_suggested_url,
              browser->tab_strip_model()->GetActiveWebContents()->GetURL());

    // Clicking the link in the interstitial should also remove the original
    // URL from history.
    ui_test_utils::HistoryEnumerator enumerator(browser->profile());
    EXPECT_FALSE(base::Contains(enumerator.urls(), navigated_url));

    histograms.ExpectTotalCount(LookalikeUrlNavigationThrottle::kHistogramName,
                                1);
    histograms.ExpectBucketCount(LookalikeUrlNavigationThrottle::kHistogramName,
                                 expected_event, 1);

    histograms.ExpectTotalCount(kInterstitialDecisionMetric, 2);
    histograms.ExpectBucketCount(kInterstitialDecisionMetric,
                                 MetricsHelper::SHOW, 1);
    histograms.ExpectBucketCount(kInterstitialDecisionMetric,
                                 MetricsHelper::DONT_PROCEED, 1);

    histograms.ExpectTotalCount(kInterstitialInteractionMetric, 1);
    histograms.ExpectBucketCount(kInterstitialInteractionMetric,
                                 MetricsHelper::TOTAL_VISITS, 1);
  }

  // Tests that the histogram event |expected_event| is recorded. If the UI is
  // enabled, additional events for interstitial display and dismissal will also
  // be tested.
  void TestHistogramEventsRecordedWhenInterstitialIgnored(
      Browser* browser,
      base::HistogramTester* histograms,
      const GURL& navigated_url,
      LookalikeUrlNavigationThrottle::NavigationSuggestionEvent
          expected_event) {
    if (!ui_enabled()) {
      TestInterstitialNotShown(browser, navigated_url);
      histograms->ExpectTotalCount(
          LookalikeUrlNavigationThrottle::kHistogramName, 1);
      histograms->ExpectBucketCount(
          LookalikeUrlNavigationThrottle::kHistogramName, expected_event, 1);

      return;
    }

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

    histograms->ExpectTotalCount(LookalikeUrlNavigationThrottle::kHistogramName,
                                 1);
    histograms->ExpectBucketCount(
        LookalikeUrlNavigationThrottle::kHistogramName, expected_event, 1);

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

  virtual bool ui_enabled() const { return GetParam() != UIStatus::kDisabled; }
  virtual UIStatus ui_status() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  base::SimpleTestClock test_clock_;
};

class LookalikeUrlInterstitialPageBrowserTest
    : public LookalikeUrlNavigationThrottleBrowserTest {
 protected:
  bool ui_enabled() const override { return true; }
  UIStatus ui_status() const override {
    return UIStatus::kEnabledForSiteEngagementAndTopDomains;
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    LookalikeUrlNavigationThrottleBrowserTest,
    ::testing::Values(UIStatus::kDisabled,
                      UIStatus::kEnabledForSiteEngagement,
                      UIStatus::kEnabledForSiteEngagementAndTopDomains));

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

  TestMetricsRecordedAndMaybeInterstitialShown(
      browser(), kNavigatedUrl, kExpectedSuggestedUrl,
      LookalikeUrlNavigationThrottle::NavigationSuggestionEvent::kMatchTopSite);

  CheckUkm({kNavigatedUrl}, "MatchType", MatchType::kTopSite);
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
  histograms.ExpectTotalCount(LookalikeUrlNavigationThrottle::kHistogramName,
                              1);
  histograms.ExpectBucketCount(
      LookalikeUrlNavigationThrottle::kHistogramName,
      LookalikeUrlNavigationThrottle::NavigationSuggestionEvent::kMatchTopSite,
      1);
  CheckUkm({kNavigatedUrl}, "MatchType", MatchType::kTopSite);
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

  TestMetricsRecordedAndMaybeInterstitialShown(
      browser(), kNavigatedUrl, kExpectedSuggestedUrl,
      LookalikeUrlNavigationThrottle::NavigationSuggestionEvent::kMatchTopSite);

  CheckUkm({kNavigatedUrl}, "MatchType", MatchType::kTopSite);
}

// The navigated domain will fall back to punycode because it fails spoof checks
// in IDN spoof checker, but there won't be an interstitial because the domain
// doesn't match a top domain.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       Punycode_NoMatch) {
  TestInterstitialNotShown(browser(), GetURL("ɴoτ-τoρ-ďoᛖaiɴ.com"));
  CheckNoUkm();
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
  histograms.ExpectTotalCount(LookalikeUrlNavigationThrottle::kHistogramName,
                              1);
  histograms.ExpectBucketCount(
      LookalikeUrlNavigationThrottle::kHistogramName,
      LookalikeUrlNavigationThrottle::NavigationSuggestionEvent::
          kMatchEditDistanceSiteEngagement,
      1);

  CheckUkm({kNavigatedUrl}, "MatchType",
           MatchType::kEditDistanceSiteEngagement);
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
  histograms.ExpectTotalCount(LookalikeUrlNavigationThrottle::kHistogramName,
                              1);
  histograms.ExpectBucketCount(
      LookalikeUrlNavigationThrottle::kHistogramName,
      LookalikeUrlNavigationThrottle::NavigationSuggestionEvent::
          kMatchEditDistance,
      1);

  CheckUkm({kNavigatedUrl}, "MatchType", MatchType::kEditDistance);
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

// Navigate to a domain whose visual representation looks like a domain with a
// site engagement score above a certain threshold. This should record metrics.
// It should also show lookalike warning interstitial if configured via
// a feature param.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       Idn_SiteEngagement_Match) {
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

    TestMetricsRecordedAndMaybeInterstitialShown(
        browser(), kNavigatedUrl, kExpectedSuggestedUrl,
        LookalikeUrlNavigationThrottle::NavigationSuggestionEvent::
            kMatchSiteEngagement);

    ukm_urls.push_back(kNavigatedUrl);
    CheckUkm(ukm_urls, "MatchType", MatchType::kSiteEngagement);
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
// TODO(meacer): Consider allowing this case.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       Idn_SiteEngagement_UnsafeRedirect) {
  const GURL kExpectedSuggestedUrl = GetURLWithoutPath("site1.com");
  const GURL kMidUrl = embedded_test_server()->GetURL(
      "sité1.com", "/server-redirect?" + kExpectedSuggestedUrl.spec());
  const GURL kNavigatedUrl = embedded_test_server()->GetURL(
      "other-site.test", "/server-redirect?" + kMidUrl.spec());

  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  SetEngagementScore(browser(), kExpectedSuggestedUrl, kHighEngagement);
  TestMetricsRecordedAndMaybeInterstitialShown(
      browser(), kNavigatedUrl, kExpectedSuggestedUrl,
      LookalikeUrlNavigationThrottle::NavigationSuggestionEvent::
          kMatchSiteEngagement);
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

  TestMetricsRecordedAndMaybeInterstitialShown(
      browser(), kNavigatedUrl, kExpectedSuggestedUrl,
      LookalikeUrlNavigationThrottle::NavigationSuggestionEvent::
          kMatchSiteEngagement);

  CheckUkm({kNavigatedUrl}, "MatchType", MatchType::kSiteEngagement);
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
    TestMetricsRecordedAndMaybeInterstitialShown(
        browser(), kNavigatedUrl, kEngagedUrl,
        LookalikeUrlNavigationThrottle::NavigationSuggestionEvent::
            kMatchSiteEngagement);

    ukm_urls.push_back(kNavigatedUrl);
    CheckUkm(ukm_urls, "MatchType", MatchType::kSiteEngagement);
  }

  // Incognito shouldn't record metrics because there are no engaged sites.
  {
    base::HistogramTester histograms;
    test_clock()->Advance(base::TimeDelta::FromHours(1));
    TestInterstitialNotShown(incognito, kNavigatedUrl);
    histograms.ExpectTotalCount(LookalikeUrlNavigationThrottle::kHistogramName,
                                0);
  }

  // Now reverse the scores: Set low engagement in the main profile and high
  // engagement in incognito.
  SetEngagementScore(browser(), kEngagedUrl, kLowEngagement);
  SetEngagementScore(incognito, kEngagedUrl, kHighEngagement);

  // Incognito should start recording metrics and main profile should stop.
  {
    test_clock()->Advance(base::TimeDelta::FromHours(1));

    TestMetricsRecordedAndMaybeInterstitialShown(
        incognito, kNavigatedUrl, kEngagedUrl,
        LookalikeUrlNavigationThrottle::NavigationSuggestionEvent::
            kMatchSiteEngagement);
    ukm_urls.push_back(kNavigatedUrl);
    CheckUkm(ukm_urls, "MatchType", MatchType::kSiteEngagement);
  }

  // Main profile shouldn't record metrics because there are no engaged sites.
  {
    base::HistogramTester histograms;
    test_clock()->Advance(base::TimeDelta::FromHours(1));
    TestInterstitialNotShown(browser(), kNavigatedUrl);
    histograms.ExpectTotalCount(LookalikeUrlNavigationThrottle::kHistogramName,
                                0);
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
  histograms.ExpectTotalCount(LookalikeUrlNavigationThrottle::kHistogramName,
                              0);
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
  histograms.ExpectTotalCount(LookalikeUrlNavigationThrottle::kHistogramName,
                              0);
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

  histograms.ExpectTotalCount(LookalikeUrlNavigationThrottle::kHistogramName,
                              0);
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
      LookalikeUrlNavigationThrottle::NavigationSuggestionEvent::
          kMatchSiteEngagement);

  CheckUkm({kNavigatedUrl}, "MatchType", MatchType::kSiteEngagement);
}

// Navigate to lookalike domains that redirect to benign domains and ensure that
// we display an interstitial along the way.
IN_PROC_BROWSER_TEST_F(LookalikeUrlInterstitialPageBrowserTest,
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
    // ...or when it's later in the chain
    const GURL kNavigatedUrl =
        GetLongRedirect("example.net", "googlé.com", "example.com");
    SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
    LoadAndCheckInterstitialAt(browser(), kNavigatedUrl);
  }

  NavigateToURLSync(browser(), GetURL("example.com"));

  {
    // ...or when it's last in the chain
    const GURL kNavigatedUrl =
        GetLongRedirect("example.net", "example.com", "googlé.com");
    SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
    LoadAndCheckInterstitialAt(browser(), kNavigatedUrl);
  }
}

// Verify that the user action in UKM is recorded properly when the interstitial
// is not shown.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationThrottleBrowserTest,
                       UkmRecordedWhenNoInterstitialShown) {
  // UI tests are handled explicitly below.
  if (ui_enabled())
    return;

  const GURL navigated_url = GetURL("googlé.com");
  TestInterstitialNotShown(browser(), navigated_url);
  CheckUkm({navigated_url}, "UserAction", UserAction::kInterstitialNotShown);
}

// Verify that the user action in UKM is recorded even when we navigate away
// from the interstitial without interacting with it.
IN_PROC_BROWSER_TEST_F(LookalikeUrlInterstitialPageBrowserTest,
                       UkmRecordedAfterNavigateAway) {
  const GURL navigated_url = GetURL("googlé.com");
  const GURL subsequent_url = GetURL("example.com");

  LoadAndCheckInterstitialAt(browser(), navigated_url);
  NavigateToURLSync(browser(), subsequent_url);
  CheckUkm({navigated_url}, "UserAction", UserAction::kCloseOrBack);
}

// Verify that the user action in UKM is recorded properly when the user accepts
// the navigation suggestion.
IN_PROC_BROWSER_TEST_F(LookalikeUrlInterstitialPageBrowserTest,
                       UkmRecordedAfterSuggestionAccepted) {
  const GURL navigated_url = GetURL("googlé.com");

  LoadAndCheckInterstitialAt(browser(), navigated_url);
  SendInterstitialCommandSync(browser(),
                              SecurityInterstitialCommand::CMD_DONT_PROCEED);
  CheckUkm({navigated_url}, "UserAction", UserAction::kAcceptSuggestion);
}

// Verify that the user action in UKM is recorded properly when the user ignores
// the navigation suggestion.
IN_PROC_BROWSER_TEST_F(LookalikeUrlInterstitialPageBrowserTest,
                       UkmRecordedAfterSuggestionIgnored) {
  const GURL navigated_url = GetURL("googlé.com");

  LoadAndCheckInterstitialAt(browser(), navigated_url);
  SendInterstitialCommandSync(browser(),
                              SecurityInterstitialCommand::CMD_PROCEED);
  CheckUkm({navigated_url}, "UserAction", UserAction::kClickThrough);
}

// Verify that the URL shows normally on pages after a lookalike interstitial.
IN_PROC_BROWSER_TEST_F(LookalikeUrlInterstitialPageBrowserTest,
                       UrlShownAfterInterstitial) {
  LoadAndCheckInterstitialAt(browser(), GetURL("googlé.com"));

  // URL should be showing again when we navigate to a normal URL
  NavigateToURLSync(browser(), GetURL("example.com"));
  EXPECT_TRUE(IsUrlShowing(browser()));
}

// Verify that bypassing warnings in the main profile does not affect incognito.
IN_PROC_BROWSER_TEST_F(LookalikeUrlInterstitialPageBrowserTest,
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
IN_PROC_BROWSER_TEST_F(LookalikeUrlInterstitialPageBrowserTest,
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
IN_PROC_BROWSER_TEST_F(LookalikeUrlInterstitialPageBrowserTest,
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

    EXPECT_EQ(LookalikeUrlInterstitialPage::kTypeForTesting,
              GetInterstitialType(web_contents));
    EXPECT_FALSE(IsUrlShowing(browser()));
  }

  // Go to the affected site directly. This should not result in an
  // interstitial.
  TestInterstitialNotShown(browser(),
                           embedded_test_server()->GetURL("example.net", "/"));
}
