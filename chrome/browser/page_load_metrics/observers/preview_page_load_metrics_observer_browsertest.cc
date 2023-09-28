// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/preview_page_load_metrics_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PreviewPageLoadMetricsObserverBrowserTest : public InProcessBrowserTest {
 public:
  PreviewPageLoadMetricsObserverBrowserTest() = default;
  ~PreviewPageLoadMetricsObserverBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    histogram_tester_.emplace();
  }

  void NavigateToOriginPath(const std::string& path) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetOriginURL(path)));
    base::RunLoop().RunUntilIdle();
  }

  void NavigateToOriginPathFromRenderer(const std::string& path) {
    ASSERT_TRUE(content::NavigateToURLFromRenderer(
        GetWebContents()->GetPrimaryMainFrame(), GetOriginURL(path)));
    base::RunLoop().RunUntilIdle();
  }

  void NavigateBack() {
    content::WebContents* web_contents = GetWebContents();
    web_contents->GetController().GoBack();
    content::WaitForLoadStop(web_contents);
  }

  void NavigateAway() {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
    base::RunLoop().RunUntilIdle();
  }

  GURL GetOriginURL(const std::string& path) {
    return embedded_test_server()->GetURL("origin.com", path);
  }

  void DisableBackForwardCache() {
    GetWebContents()->GetController().GetBackForwardCache().DisableForTesting(
        content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  }

  base::HistogramTester& histogram_tester() { return *histogram_tester_; }

 private:
  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  absl::optional<base::HistogramTester> histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(PreviewPageLoadMetricsObserverBrowserTest,
                       IndependentVisit) {
  // Disables BFCache to make the page lifecycle management easy in testing.
  DisableBackForwardCache();

  // Runs a single browser initiated navigation.
  NavigateToOriginPath("/title1.html");
  NavigateAway();

  histogram_tester().ExpectUniqueSample(
      "PageLoad.Experimental.PageVisitType2",
      PreviewPageLoadMetricsObserver::PageVisitType::kIndependentUIVisit, 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.IndependentUIVisit", 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.AllVisit", 1);
}

IN_PROC_BROWSER_TEST_F(PreviewPageLoadMetricsObserverBrowserTest,
                       SequentialVisits) {
  // Disables BFCache to make the page lifecycle management easy in testing.
  DisableBackForwardCache();

  // Runs a series of navigation.
  NavigateToOriginPath("/title1.html");
  NavigateToOriginPathFromRenderer("/title2.html");

  histogram_tester().ExpectBucketCount(
      "PageLoad.Experimental.PageVisitType2",
      PreviewPageLoadMetricsObserver::PageVisitType::kOriginUIVisit, 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.OriginUIVisit", 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.AllVisit", 1);

  NavigateToOriginPathFromRenderer("/title3.html");
  histogram_tester().ExpectBucketCount(
      "PageLoad.Experimental.PageVisitType2",
      PreviewPageLoadMetricsObserver::PageVisitType::kPassingVisit, 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.PassingVisit", 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.AllVisit", 2);

  NavigateAway();
  histogram_tester().ExpectBucketCount(
      "PageLoad.Experimental.PageVisitType2",
      PreviewPageLoadMetricsObserver::PageVisitType::kTerminalVisit, 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.TerminalVisit", 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.AllVisit", 3);
}

IN_PROC_BROWSER_TEST_F(PreviewPageLoadMetricsObserverBrowserTest,
                       ComplexNavigationWithoutBFCache) {
  // Disables BFCache to make the page lifecycle management easy in testing.
  DisableBackForwardCache();

  // Runs a series of navigation.
  NavigateToOriginPath("/title1.html");
  NavigateToOriginPathFromRenderer("/title2.html");

  histogram_tester().ExpectBucketCount(
      "PageLoad.Experimental.PageVisitType2",
      PreviewPageLoadMetricsObserver::PageVisitType::kOriginUIVisit, 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.OriginUIVisit", 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.AllVisit", 1);

  // Back navigation.
  // It marks title2.html as a terminal visit, and goes back to the title1.html.
  NavigateBack();

  histogram_tester().ExpectBucketCount(
      "PageLoad.Experimental.PageVisitType2",
      PreviewPageLoadMetricsObserver::PageVisitType::kTerminalVisit, 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.TerminalVisit", 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.AllVisit", 2);

  // Make another navigation that makes a history branch.
  NavigateToOriginPathFromRenderer("/title3.html");

  histogram_tester().ExpectBucketCount(
      "PageLoad.Experimental.PageVisitType2",
      PreviewPageLoadMetricsObserver::PageVisitType::kHistoryVisit, 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.HistoryVisit", 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.AllVisit", 3);

  // Wrap up.
  NavigateAway();

  histogram_tester().ExpectBucketCount(
      "PageLoad.Experimental.PageVisitType2",
      PreviewPageLoadMetricsObserver::PageVisitType::kTerminalVisit, 2);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.TerminalVisit", 2);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.AllVisit", 4);
}

IN_PROC_BROWSER_TEST_F(PreviewPageLoadMetricsObserverBrowserTest,
                       ComplexNavigationWithBFCache) {
  if (!content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    // Disabled on low-end bots or linux-bfcache-rel.
    return;
  }

  // Runs a series of navigation.
  NavigateToOriginPath("/title1.html");
  NavigateToOriginPathFromRenderer("/title2.html");

  histogram_tester().ExpectBucketCount(
      "PageLoad.Experimental.PageVisitType2",
      PreviewPageLoadMetricsObserver::PageVisitType::kOriginUIVisit, 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.OriginUIVisit", 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.AllVisit", 1);

  // Back navigation.
  // title1.html is restored from BFCache, and doesn't create a new observer.
  NavigateBack();
  histogram_tester().ExpectBucketCount(
      "PageLoad.Experimental.PageVisitType2",
      PreviewPageLoadMetricsObserver::PageVisitType::kTerminalVisit, 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.TerminalVisit", 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.AllVisit", 2);

  // Make another navigation that makes a history branch.
  NavigateToOriginPathFromRenderer("/title3.html");

  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.AllVisit", 2);

  // Wrap up.
  // No new metrics are recorded because of the BFCache restoration.
  NavigateAway();
  histogram_tester().ExpectBucketCount(
      "PageLoad.Experimental.PageVisitType2",
      PreviewPageLoadMetricsObserver::PageVisitType::kTerminalVisit, 2);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.TerminalVisit", 2);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.AllVisit", 3);
}

}  // namespace
