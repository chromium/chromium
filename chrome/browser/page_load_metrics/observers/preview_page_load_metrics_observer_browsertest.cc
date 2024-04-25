// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/preview_page_load_metrics_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/preloading/preview/preview_manager.h"
#include "chrome/browser/preloading/preview/preview_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PreviewPageLoadMetricsObserverBrowserTest : public InProcessBrowserTest {
 public:
  PreviewPageLoadMetricsObserverBrowserTest()
      : helper_(base::BindRepeating(
            &PreviewPageLoadMetricsObserverBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~PreviewPageLoadMetricsObserverBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);

    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_->SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());

    histogram_tester_.emplace();
  }

  void ActivateTab(int index) {
    browser()->tab_strip_model()->ActivateTabAt(index);
  }

  void NavigateToOriginPath(const std::string& path) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestURL(path)));
    base::RunLoop().RunUntilIdle();
  }

  void NavigateToOriginPathFromRenderer(const std::string& path) {
    ASSERT_TRUE(content::NavigateToURLFromRenderer(
        GetWebContents()->GetPrimaryMainFrame(), GetTestURL(path)));
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

  GURL GetTestURL(const std::string& path) {
    return https_server_->GetURL("a.test", path);
  }

  void DisableBackForwardCache() {
    GetWebContents()->GetController().GetBackForwardCache().DisableForTesting(
        content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  }

  base::HistogramTester& histogram_tester() { return *histogram_tester_; }
  test::PreviewTestHelper& helper() { return helper_; }

  content::WebContents* GetWebContents() {
    // We keep holding the initiator WebContents under testing in the first tab.
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;

  std::optional<base::HistogramTester> histogram_tester_;
  test::PreviewTestHelper helper_;
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

IN_PROC_BROWSER_TEST_F(PreviewPageLoadMetricsObserverBrowserTest,
                       PreviewFinalStatus) {
  // Navigates to an initial page.
  NavigateToOriginPath("/title1.html");

  // Open a preview page.
  helper().InitiatePreview(GetTestURL("/title2.html"));
  helper().WaitUntilLoadFinished();

  // Flush metrics.
  helper().CloseAndWaitUntilFinished();
  NavigateAway();

  // TotalForegroundDuration.AllVisit should count only non-previewed page.
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.AllVisit", 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.PreviewFinalStatus", 1);
  histogram_tester().ExpectBucketCount(
      "PageLoad.Experimental.PreviewFinalStatus",
      PreviewPageLoadMetricsObserver::PreviewFinalStatus::kPreviewed, 1);
  histogram_tester().ExpectBucketCount(
      "PageLoad.Experimental.PreviewFinalStatus",
      PreviewPageLoadMetricsObserver::PreviewFinalStatus::kPromoted, 0);

  // Open a preview page.
  helper().InitiatePreview(GetTestURL("/title2.html"));
  helper().WaitUntilLoadFinished();
  test::PreviewTestHelper::Waiter waiter = helper().CreateActivationWaiter();
  helper().PromoteToNewTab();
  waiter.Wait();

  // Flush metrics.
  NavigateAway();

  // TotalForegroundDuration.AllVisit should count only non-previewed page.
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.TotalForegroundDuration.AllVisit", 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Experimental.PreviewFinalStatus", 2);
  histogram_tester().ExpectBucketCount(
      "PageLoad.Experimental.PreviewFinalStatus",
      PreviewPageLoadMetricsObserver::PreviewFinalStatus::kPreviewed, 1);
  histogram_tester().ExpectBucketCount(
      "PageLoad.Experimental.PreviewFinalStatus",
      PreviewPageLoadMetricsObserver::PreviewFinalStatus::kPromoted, 1);
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(PreviewPageLoadMetricsObserverBrowserTest,
                       LinkPreviewUsageNotUsed) {
  DisableBackForwardCache();

  EXPECT_THAT(
      histogram_tester().GetAllSamples("PageLoad.Clients.LinkPreview.Usage"),
      testing::ElementsAre());

  auto complete_waiter =
      std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
          GetWebContents());
  complete_waiter->AddOnCompleteCalledExpectation();

  // Navigates to an initial page that is associated with `complete_waiter`.
  NavigateToOriginPath("/title1.html");

  // Navigates to another page to wipe out metrics observers for the initial
  // page.
  NavigateToOriginPath("/title2.html");

  // Ensures the observer is completed, and verify histograms.
  complete_waiter->Wait();

  EXPECT_THAT(
      histogram_tester().GetAllSamples("PageLoad.Clients.LinkPreview.Usage"),
      testing::ElementsAre(base::Bucket(PreviewManager::Usage::kNotUsed, 1)));
}

IN_PROC_BROWSER_TEST_F(PreviewPageLoadMetricsObserverBrowserTest,
                       LinkPreviewUsageUsedButNotPromoted) {
  DisableBackForwardCache();

  EXPECT_THAT(
      histogram_tester().GetAllSamples("PageLoad.Clients.LinkPreview.Usage"),
      testing::ElementsAre());

  auto complete_waiter =
      std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
          GetWebContents());
  complete_waiter->AddOnCompleteCalledExpectation();

  // Navigates to an initial page that is associated with `complete_waiter`.
  NavigateToOriginPath("/title1.html");

  // Open a preview page that changes the initiator status to
  // kUsedButNotPromoted.
  helper().InitiatePreview(GetTestURL("/title2.html?preview"));
  helper().WaitUntilLoadFinished();

  // Navigates to another page to wipe out metrics observers for the initial
  // page.
  NavigateToOriginPath("/title2.html");

  // Ensures the observer is completed, and verify histograms.
  complete_waiter->Wait();

  EXPECT_THAT(
      histogram_tester().GetAllSamples("PageLoad.Clients.LinkPreview.Usage"),
      testing::ElementsAre(
          base::Bucket(PreviewManager::Usage::kUsedButNotPromoted, 1)));
}

IN_PROC_BROWSER_TEST_F(PreviewPageLoadMetricsObserverBrowserTest,
                       LinkPreviewUsageResetOnPrimaryNavigation) {
  DisableBackForwardCache();

  EXPECT_THAT(
      histogram_tester().GetAllSamples("PageLoad.Clients.LinkPreview.Usage"),
      testing::ElementsAre());

  auto complete_waiter1 =
      std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
          GetWebContents());
  complete_waiter1->AddOnCompleteCalledExpectation();

  // Navigates to an initial page that is associated with `complete_waiter1`.
  NavigateToOriginPath("/title1.html");

  // Open a preview page that changes the initiator status to
  // kUsedButNotPromoted.
  helper().InitiatePreview(GetTestURL("/title2.html?preview"));
  helper().WaitUntilLoadFinished();

  // Navigates to another page that is associated with `complete_waiter2` in
  // order to wipe out metrics observers for the initial page.
  auto complete_waiter2 =
      std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
          GetWebContents());
  complete_waiter2->AddOnCompleteCalledExpectation();
  NavigateToOriginPath("/title2.html");

  // Ensures the observer is completed, and verify histograms.
  complete_waiter1->Wait();

  EXPECT_THAT(
      histogram_tester().GetAllSamples("PageLoad.Clients.LinkPreview.Usage"),
      testing::ElementsAre(
          base::Bucket(PreviewManager::Usage::kUsedButNotPromoted, 1)));

  // Navigates to yet another page to wipte out metrics observers for the second
  // page.
  NavigateToOriginPath("/title3.html");

  // Ensures the observer is completed, and verify histograms.
  complete_waiter2->Wait();

  EXPECT_THAT(
      histogram_tester().GetAllSamples("PageLoad.Clients.LinkPreview.Usage"),
      testing::ElementsAre(
          base::Bucket(PreviewManager::Usage::kNotUsed, 1),
          base::Bucket(PreviewManager::Usage::kUsedButNotPromoted, 1)));
}

IN_PROC_BROWSER_TEST_F(PreviewPageLoadMetricsObserverBrowserTest,
                       LinkPreviewUsageUsedAndPromoted) {
  DisableBackForwardCache();

  EXPECT_THAT(
      histogram_tester().GetAllSamples("PageLoad.Clients.LinkPreview.Usage"),
      testing::ElementsAre());

  auto complete_waiter =
      std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
          GetWebContents());
  complete_waiter->AddOnCompleteCalledExpectation();

  // Navigates to an initial page that is associated with `complete_waiter1`.
  NavigateToOriginPath("/title1.html");

  // Opens a preview page that changes the initiator status to
  // kUsedButNotPromoted.
  helper().InitiatePreview(GetTestURL("/title2.html?preview"));
  helper().WaitUntilLoadFinished();

  // Promotes the preview page that changes the initiator status to
  // kUsedAndPromoted.
  test::PreviewTestHelper::Waiter promote_waiter =
      helper().CreateActivationWaiter();
  helper().PromoteToNewTab();
  promote_waiter.Wait();

  // Flush initiator page metrics.
  ActivateTab(0);
  NavigateToOriginPath("/title3.html");
  complete_waiter->Wait();

  EXPECT_THAT(
      histogram_tester().GetAllSamples("PageLoad.Clients.LinkPreview.Usage"),
      testing::ElementsAre(
          base::Bucket(PreviewManager::Usage::kUsedAndPromoted, 1)));
}
#endif

}  // namespace
