// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/navigation_predictor/navigation_predictor.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace {

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramUntilCountReached(base::HistogramTester* histogram_tester,
                                        const std::string& histogram_name,
                                        size_t count) {
  base::RunLoop().RunUntilIdle();
  for (size_t attempt = 0; attempt < 50; ++attempt) {
    const std::vector<base::Bucket> buckets =
        histogram_tester->GetAllSamples(histogram_name);
    size_t total_count = 0;
    for (const auto& bucket : buckets)
      total_count += bucket.count;
    if (total_count >= count)
      return;
    content::FetchHistogramsFromChildProcesses();
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
  }
}

// Verifies that all URLs specified in |expected_urls| are present in
// |urls_from_observed_prediction|. Ordering of URLs is NOT verified.
void VerifyURLsPresent(const std::vector<GURL>& urls_from_observed_prediction,
                       const std::vector<std::string>& expected_urls) {
  for (const auto& expected_url : expected_urls) {
    EXPECT_NE(urls_from_observed_prediction.end(),
              std::find(urls_from_observed_prediction.begin(),
                        urls_from_observed_prediction.end(), expected_url));
  }
}

class NavigationPredictorBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  NavigationPredictorBrowserTest()
      : subresource_filter::SubresourceFilterBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kNavigationPredictor, {});
  }

  void SetUp() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/navigation_predictor");
    ASSERT_TRUE(https_server_->Start());

    http_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTP));
    http_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/navigation_predictor");
    ASSERT_TRUE(http_server_->Start());

    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    subresource_filter::SubresourceFilterBrowserTest::SetUpOnMainThread();
    host_resolver()->ClearRules();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  const GURL GetTestURL(const char* file) const {
    return https_server_->GetURL(file);
  }

  const GURL GetHttpTestURL(const char* file) const {
    return http_server_->GetURL(file);
  }

  void WaitForLayout() {
    const char* entry_name =
        ukm::builders::NavigationPredictorPageLinkMetrics::kEntryName;

    if (ukm_recorder_->GetEntriesByName(entry_name).empty()) {
      base::RunLoop run_loop;
      ukm_recorder_->SetOnAddEntryCallback(entry_name, run_loop.QuitClosure());
      run_loop.Run();
    }
  }

  void ResetUKM() {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(NavigationPredictorBrowserTest);
};

class TestObserver : public NavigationPredictorKeyedService::Observer {
 public:
  TestObserver() {}
  ~TestObserver() override {}

  base::Optional<NavigationPredictorKeyedService::Prediction> last_prediction()
      const {
    return last_prediction_;
  }

  size_t count_predictions() const { return count_predictions_; }

  // Waits until the count if received notifications is at least
  // |expected_notifications_count|.
  void WaitUntilNotificationsCountReached(size_t expected_notifications_count) {
    // Ensure that |wait_loop_| is null implying there is no ongoing wait.
    ASSERT_FALSE(!!wait_loop_);

    if (count_predictions_ >= expected_notifications_count)
      return;
    expected_notifications_count_ = expected_notifications_count;
    wait_loop_ = std::make_unique<base::RunLoop>();
    wait_loop_->Run();
    wait_loop_.reset();
  }

 private:
  void OnPredictionUpdated(
      const base::Optional<NavigationPredictorKeyedService::Prediction>&
          prediction) override {
    ++count_predictions_;
    last_prediction_ = prediction;
    if (wait_loop_ && count_predictions_ >= expected_notifications_count_) {
      wait_loop_->Quit();
    }
  }

  // Count of prediction notifications received so far.
  size_t count_predictions_ = 0u;

  // last prediction received.
  base::Optional<NavigationPredictorKeyedService::Prediction> last_prediction_;

  // If |wait_loop_| is non-null, then it quits as soon as count of received
  // notifications are at least |expected_notifications_count_|.
  std::unique_ptr<base::RunLoop> wait_loop_;
  base::Optional<size_t> expected_notifications_count_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, Pipeline) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 5, 1);
  // Same document anchor element should be removed after merge.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
}

// Test that no metrics are recorded in off-the-record profiles.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, PipelineOffTheRecord) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  Browser* incognito = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(incognito, url);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 0);
  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 0);
}

// Test that the browser does not process anchor element metrics from an http
// web page on page load.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, PipelineHttp) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetHttpTestURL("/simple_page_with_anchors.html");
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 0);
  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 0);
}

// Test that anchor elements within an iframe tagged as an ad are discarded when
// predicting next navigation.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, PipelineAdsFrameTagged) {
  // iframe_ads_simple_page_with_anchors.html is an iframe referenced by
  // page_with_ads_iframe.html.
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "iframe_ads_simple_page_with_anchors.html"));

  base::HistogramTester histogram_tester;

  GURL url = GetTestURL("/page_with_ads_iframe.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 5, 1);
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester, "AnchorElementMetrics.IsAdFrameElement", 4);

  histogram_tester.ExpectTotalCount("AnchorElementMetrics.IsAdFrameElement", 7);
  histogram_tester.ExpectBucketCount("AnchorElementMetrics.IsAdFrameElement",
                                     0 /* false */, 5);
  histogram_tester.ExpectBucketCount("AnchorElementMetrics.IsAdFrameElement",
                                     1 /* true */, 2);
}

// Test that anchor elements within an iframe not tagged as ad are not discarded
// when predicting next navigation.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       PipelineAdsFrameNotTagged) {
  base::HistogramTester histogram_tester;

  GURL url = GetTestURL("/page_with_ads_iframe.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 7, 1);
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester, "AnchorElementMetrics.IsAdFrameElement", 7);

  histogram_tester.ExpectUniqueSample("AnchorElementMetrics.IsAdFrameElement",
                                      0 /* false */, 7);
}

// Test that navigation score of anchor elements can be calculated on page load.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, NavigationScore) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.HighestNavigationScore", 1);
}

// Simulate a click at the anchor element.
// Test that timing info (DurationLoadToFirstClick) can be recorded.
// And that the navigation score can be looked up.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, ClickAnchorElement) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.DurationLoadToFirstClick", 1);
  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.NavigationScore", 1);

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kNone, 1);
}

// Simulate a click at the anchor element.
// Test that the action accuracy is properly recorded.
// User clicks on an anchor element that points to a origin different than the
// origin of the URL prefetched.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       ActionAccuracy_DifferentOrigin) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 2, 1);
  // Same document anchor element should be removed after merge.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kPrefetch, 1);

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.LinkClickedPrerenderResult",
      NavigationPredictor::PrerenderResult::kCrossOriginAboveThreshold, 1);
}

// Disabled because it fails when SingleProcessMash feature is enabled. Since
// Navigation Predictor is not going to be enabled on Chrome OS, disabling the
// browser test on that platform is fine.
#if defined(OS_CHROMEOS)
#define DISABLE_ON_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_CHROMEOS(x) x
#endif

// Simulate a click at the anchor element.
// Test that the action accuracy is properly recorded.
// User clicks on an anchor element that points to a origin different than the
// origin of the URL prefetched.
IN_PROC_BROWSER_TEST_F(
    NavigationPredictorBrowserTest,
    DISABLE_ON_CHROMEOS(ActionAccuracy_DifferentOrigin_VisibilityChanged)) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 2, 1);
  // Same document anchor element should be removed after merge.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kPrefetch, 1);

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.LinkClickedPrerenderResult",
      NavigationPredictor::PrerenderResult::kCrossOriginAboveThreshold, 1);
}

class NavigationPredictorBrowserTestWithDefaultPredictorEnabled
    : public NavigationPredictorBrowserTest {
 public:
  NavigationPredictorBrowserTestWithDefaultPredictorEnabled() {
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kNavigationPredictor, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that the action accuracy is properly recorded and when same origin
// preconnections are enabled, then navigation predictor initiates the
// preconnection.
IN_PROC_BROWSER_TEST_F(
    NavigationPredictorBrowserTestWithDefaultPredictorEnabled,
    DISABLE_ON_CHROMEOS(
        ActionAccuracy_DifferentOrigin_VisibilityChangedPreconnectEnabled)) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 2, 1);
  // Same document anchor element should be removed after merge.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kPrefetch, 1);

}

IN_PROC_BROWSER_TEST_F(
    NavigationPredictorBrowserTestWithDefaultPredictorEnabled,
    DISABLE_ON_CHROMEOS(PreconnectNonSearch)) {
  base::HistogramTester histogram_tester;

  // This page only has non-same host links.
  const GURL& url = GetTestURL("/anchors_different_area.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kNone, 1);
}

class NavigationPredictorBrowserTestWithPrefetchAfterPreconnect
    : public NavigationPredictorBrowserTest {
 public:
  NavigationPredictorBrowserTestWithPrefetchAfterPreconnect()
      : NavigationPredictorBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kNavigationPredictor,
        {{"prefetch_after_preconnect", "true"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    NavigationPredictorBrowserTestWithPrefetchAfterPreconnect,
    DISABLE_ON_CHROMEOS(PrefetchAfterPreconnect)) {
  prerender::PrerenderManager::SetMode(
      prerender::PrerenderManager::PRERENDER_MODE_NOSTATE_PREFETCH);

  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder =
      std::make_unique<ukm::TestAutoSetUkmRecorder>();

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('example').click();"));
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kPrefetch, 1);

  histogram_tester.ExpectTotalCount(
      "NavigationPredictor.LinkClickedPrerenderResult", 1);

  const auto& entries = ukm_recorder->GetMergedEntriesByName(
      ukm::builders::NoStatePrefetch::kEntryName);
  EXPECT_EQ(1u, entries.size());

  for (const auto& kv : entries) {
    EXPECT_TRUE(ukm_recorder->EntryHasMetric(
        kv.second.get(),
        ukm::builders::NoStatePrefetch::kPrefetchedRecently_FinalStatusName));
    EXPECT_TRUE(ukm_recorder->EntryHasMetric(
        kv.second.get(),
        ukm::builders::NoStatePrefetch::kPrefetchedRecently_OriginName));
    EXPECT_TRUE(ukm_recorder->EntryHasMetric(
        kv.second.get(),
        ukm::builders::NoStatePrefetch::kPrefetchedRecently_PrefetchAgeName));
  }
}

// Simulate a click at the anchor element.
// Test that the action accuracy is properly recorded.
// User clicks on an anchor element that points to same URL as the URL
// prefetched.
// https://crbug.com/1008307
#if (defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_CHROMEOS))
#define MAYBE_ActionAccuracy_SameOrigin DISABLED_ActionAccuracy_SameOrigin
#else
#define MAYBE_ActionAccuracy_SameOrigin ActionAccuracy_SameOrigin
#endif
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       MAYBE_ActionAccuracy_SameOrigin) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('example').click();"));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 2, 1);
  // Same document anchor element should be removed after merge.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kPrefetch, 1);

  histogram_tester.ExpectTotalCount(
      "NavigationPredictor.LinkClickedPrerenderResult", 1);
}

// Simulate a click at the anchor element in off-the-record profile. Metrics
// should not be recorded.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       ClickAnchorElementOffTheRecord) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");

  Browser* incognito = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(incognito, url);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(content::ExecuteScript(
      incognito->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.HighestNavigationScore", 0);
}

// Simulate click at the anchor element.
// Test that correct area ranks are recorded.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, AreaRank) {
  base::HistogramTester histogram_tester;

  // This test file contains 5 anchors with different size.
  const GURL& url = GetTestURL("/anchors_different_area.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('medium').click();"));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample("AnchorElementMetrics.Clicked.AreaRank",
                                      2, 1);
  histogram_tester.ExpectTotalCount("AnchorElementMetrics.Visible.RatioArea",
                                    5);
}

// Test that MergeMetricsSameTargetUrl merges anchor elements having the same
// href. The html file contains two anchor elements having the same href.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       MergeMetricsSameTargetUrl_ClickHrefWithNoMergedImage) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/anchors_same_href.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  histogram_tester.ExpectTotalCount("AnchorElementMetrics.Visible.RatioArea",
                                    2);

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('diffHref').click();"));
  base::RunLoop().RunUntilIdle();

  // Anchor element with id 'diffHref' points to an href. No image in the
  // webpage also points to an image. So, clicking on this non-image anchor
  // element, should not be recorded as "ContainsImage".
  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.RatioContainsImage_ContainsImage", 0);
}

// Test that MergeMetricsSameTargetUrl merges anchor elements having the same
// href. The html file contains two anchor elements having the same href.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       MergeMetricsSameTargetUrl_ClickHrefWithMergedImage) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/anchors_same_href.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  histogram_tester.ExpectTotalCount("AnchorElementMetrics.Visible.RatioArea",
                                    2);

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  // Anchor element with id 'google' points to an href. Another image in the
  // webpage also points to an image. So, even though we clicked on a non-image
  // anchor element, it should be recorded as "ContainsImage".
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Clicked.RatioContainsImage_ContainsImage", 1);
}

IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       AnchorElementClickedOnSearchEnginePage) {
  static const char kShortName[] = "test";
  static const char kSearchURL[] = "/anchors_same_href.html?q={searchTerms}";

  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16(kShortName));
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());

  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/anchors_same_href.html?q=cats");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();
  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  // Anchor element with id 'google' points to an href that's on a different
  // host.
  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.RatioContainsImage_ContainsImage", 1);
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Clicked.OnDSE.SameHost", 0, 1);
}

IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       AnchorElementClickedOnNonSearchEnginePage) {
  static const char kShortName[] = "test";
  static const char kSearchURL[] = "/somne_other_url.html?q={searchTerms}";

  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16(kShortName));
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());

  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/anchors_same_href.html?q=cats");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  // Anchor element with id 'google' points to an href that's on a different
  // host.
  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.RatioContainsImage_ContainsImage", 1);
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Clicked.OnNonDSE.SameHost", 0, 1);
}

IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       ActionPrefetch_NoSameHostAnchorElement) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 5, 1);
  // Same document anchor element should be removed after merge.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kNone, 1);
}

IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       ActionPrefetch_SameHostAnchorElement) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 2, 1);
  // Same document anchor element should be removed after merge.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kPrefetch, 1);
}

// Tests that the browser receives anchors from anywhere on the page.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       ViewportOnlyAndUrlIncrementByOne) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/long_page_with_anchors-1.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 3, 1);
}

// Test that navigation score of anchor elements can be calculated on page load
// and the predicted URLs for the next navigation are dispatched to the single
// observer.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       NavigationScoreSingleObserver) {
  TestObserver observer;

  NavigationPredictorKeyedService* service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(
          browser()->profile());
  EXPECT_NE(nullptr, service);
  service->AddObserver(&observer);

  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();
  observer.WaitUntilNotificationsCountReached(1);

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.HighestNavigationScore", 1);
  service->RemoveObserver(&observer);

  EXPECT_EQ(1u, observer.count_predictions());
  EXPECT_EQ(url, observer.last_prediction()->source_document_url());
  EXPECT_EQ(2u, observer.last_prediction()->sorted_predicted_urls().size());
  EXPECT_NE(
      observer.last_prediction()->sorted_predicted_urls().end(),
      std::find(observer.last_prediction()->sorted_predicted_urls().begin(),
                observer.last_prediction()->sorted_predicted_urls().end(),
                "https://google.com/"));
  EXPECT_NE(
      observer.last_prediction()->sorted_predicted_urls().end(),
      std::find(observer.last_prediction()->sorted_predicted_urls().begin(),
                observer.last_prediction()->sorted_predicted_urls().end(),
                "https://example.com/"));

  // Doing another navigation after removing the observer should not cause a
  // crash.
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();
  EXPECT_EQ(1u, observer.count_predictions());
}

// Same as NavigationScoreSingleObserver test but with more than one observer.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       NavigationScore_TwoObservers) {
  TestObserver observer_1;
  TestObserver observer_2;

  NavigationPredictorKeyedService* service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(
          browser()->profile());
  service->AddObserver(&observer_1);
  service->AddObserver(&observer_2);

  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();
  observer_1.WaitUntilNotificationsCountReached(1);
  observer_2.WaitUntilNotificationsCountReached(1);

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.HighestNavigationScore", 1);
  service->RemoveObserver(&observer_1);

  EXPECT_EQ(1u, observer_1.count_predictions());
  EXPECT_EQ(url, observer_1.last_prediction()->source_document_url());
  EXPECT_EQ(2u, observer_1.last_prediction()->sorted_predicted_urls().size());
  VerifyURLsPresent(observer_1.last_prediction()->sorted_predicted_urls(),
                    {"https://google.com/", "https://example.com/"});
  EXPECT_EQ(1u, observer_2.count_predictions());
  EXPECT_EQ(url, observer_2.last_prediction()->source_document_url());

  // Only |observer_2| should get the notification since |observer_1| has
  // been removed from receiving the notifications.
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();
  observer_2.WaitUntilNotificationsCountReached(2);
  EXPECT_EQ(1u, observer_1.count_predictions());
  EXPECT_EQ(2u, observer_2.count_predictions());
  VerifyURLsPresent(observer_2.last_prediction()->sorted_predicted_urls(),
                    {"https://google.com/", "https://example.com/"});
}

// Test that the navigation predictor keyed service is null for incognito
// profiles.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, Incognito) {
  Browser* incognito = CreateIncognitoBrowser();
  NavigationPredictorKeyedService* incognito_service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(
          incognito->profile());
  EXPECT_EQ(nullptr, incognito_service);
}

// Verify that the observers are notified of predictions on search results page.
IN_PROC_BROWSER_TEST_F(
    NavigationPredictorBrowserTestWithPrefetchAfterPreconnect,
    DISABLE_ON_CHROMEOS(ObserverNotifiedOnSearchPage)) {
  TestObserver observer;

  NavigationPredictorKeyedService* service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(
          browser()->profile());
  service->AddObserver(&observer);

  static const char kShortName[] = "test";
  static const char kSearchURL[] =
      "/anchors_different_area.html?q={searchTerms}";

  // Set up default search engine.
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16(kShortName));
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());

  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  base::HistogramTester histogram_tester;

  EXPECT_EQ(0u, observer.count_predictions());

  // This page only has non-same host links.
  const GURL& url = GetTestURL("/anchors_different_area.html?q=cats");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();
  observer.WaitUntilNotificationsCountReached(1u);

  histogram_tester.ExpectUniqueSample("NavigationPredictor.OnDSE.ActionTaken",
                                      NavigationPredictor::Action::kNone, 1);
  EXPECT_EQ(1u, observer.count_predictions());
  EXPECT_EQ(url, observer.last_prediction()->source_document_url());
  EXPECT_EQ(5u, observer.last_prediction()->sorted_predicted_urls().size());
  VerifyURLsPresent(
      observer.last_prediction()->sorted_predicted_urls(),
      {"https://example.com/2", "https://google.com/", "https://example.com/1",
       "https://example.com/", "https://dummy.com/"});
}

const base::Feature kNavigationPredictorMultiplePrerenders{
    "NavigationPredictorMultiplePrerenders", base::FEATURE_ENABLED_BY_DEFAULT};

class NavigationPredictorBrowserTestMultiplePrerender
    : public NavigationPredictorBrowserTest {
 public:
  NavigationPredictorBrowserTestMultiplePrerender() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kNavigationPredictor,
          {{"prefetch_after_preconnect", "true"}}},
         {kNavigationPredictorMultiplePrerenders, {{"prerender_limit", "4"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that multiple prerenders is working.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTestMultiplePrerender,
                       DISABLE_ON_CHROMEOS(MultiplePrerendersRecordsMetrics)) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/anchors_large.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout();

  // Force prerenders to happen quickly.
  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();
  browser()->tab_strip_model()->GetActiveWebContents()->WasShown();

  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();
  browser()->tab_strip_model()->GetActiveWebContents()->WasShown();

  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();
  browser()->tab_strip_model()->GetActiveWebContents()->WasShown();

  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();
  browser()->tab_strip_model()->GetActiveWebContents()->WasShown();

  // Force recording stats.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.CountOfURLsAboveThreshold", 5, 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.CountOfURLsAboveThreshold.CrossOrigin", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.CountOfURLsAboveThreshold.SameOrigin", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.CountOfURLsInPredictedSet.CrossOrigin", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.CountOfURLsInPredictedSet.SameOrigin", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.CountOfStartedPrerenders", 3, 1);

  // Same origin links in anchors_large.html
  std::vector<GURL> prerendered_urls = {
      GetTestURL("/1.html"), GetTestURL("/2.html"), GetTestURL("/3.html")};

  for (auto& url : prerendered_urls) {
    auto test_ukm_recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    ResetUKM();

    // Load page from cache.
    ui_test_utils::NavigateToURL(browser(), url);
    WaitForLayout();
    // Force recording PageLoad UKM.
    ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

    // Check that the page was loaded from cache.
    auto entries = test_ukm_recorder->GetMergedEntriesByName(
        ukm::builders::PageLoad::kEntryName);
    EXPECT_EQ(1u, entries.size());
    for (const auto& kv : entries) {
      auto* const cached_load_entry = kv.second.get();
      test_ukm_recorder->ExpectEntrySourceHasUrl(cached_load_entry, url);

      EXPECT_TRUE(test_ukm_recorder->EntryHasMetric(
          cached_load_entry, ukm::builders::PageLoad::kWasCachedName));
    }
  }
}

}  // namespace
