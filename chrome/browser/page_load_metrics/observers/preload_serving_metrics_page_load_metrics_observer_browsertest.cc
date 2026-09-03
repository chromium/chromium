// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/prefetch_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

using page_load_metrics::PageLoadMetricsTestWaiter;

// Helper to simulate a link click navigation from the current page to
// `target_url`.
void NavigateViaLinkClick(content::WebContents& web_contents,
                          const GURL& target_url) {
  EXPECT_TRUE(content::ExecJs(
      &web_contents, content::JsReplace(R"(let a = document.createElement('a');
                            a.href = $1;
                            document.body.appendChild(a);
                            a.click();)",
                                        target_url.spec())));
}

}  // namespace

// Browser test for `PreloadServingMetricsPageLoadMetricsObserver`.
class PreloadServingMetricsPageLoadMetricsObserverBrowserTest
    : public InProcessBrowserTest {
 public:
  PreloadServingMetricsPageLoadMetricsObserverBrowserTest()
      : prerender_helper_(base::BindRepeating(
            [](PreloadServingMetricsPageLoadMetricsObserverBrowserTest* test) {
              return &test->web_contents();
            },
            base::Unretained(this))),
        https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {
    content::InitBackForwardCacheFeature(&scoped_feature_list_,
                                         /*enable_back_forward_cache=*/true);
  }

  ~PreloadServingMetricsPageLoadMetricsObserverBrowserTest() override = default;

  void SetUp() override {
    https_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
    prerender_helper_.RegisterServerRequestMonitor(&https_server_);
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(https_server_.Start());
  }

  content::WebContents& web_contents() {
    return *browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::test_server::EmbeddedTestServer& https_server() { return https_server_; }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  std::unique_ptr<PageLoadMetricsTestWaiter> CreatePageLoadMetricsTestWaiter() {
    return std::make_unique<PageLoadMetricsTestWaiter>(&web_contents());
  }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
  content::test::PrerenderTestHelper prerender_helper_;
  net::test_server::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies metrics recording for a standard navigation without instant loading
// technology.
IN_PROC_BROWSER_TEST_F(PreloadServingMetricsPageLoadMetricsObserverBrowserTest,
                       NoInstantLoad) {
  base::HistogramTester histogram_tester;

  GURL initial_url = https_server().GetURL("a.test", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  GURL target_url = https_server().GetURL("a.test", "/title1.html");
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kFirstContentfulPaint);
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);
  NavigateViaLinkClick(web_contents(), target_url);
  waiter->Wait();

  // Navigate away to flush PreloadServingMetrics.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.LinkClick.All",
                                      0 /* kNoInstantLoad */, 1);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.LinkClick.All.All",
      1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.LinkClick.All.NoInstantLoad",
      1);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToLargestContentfulPaint2.LinkClick.All.All",
      1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToLargestContentfulPaint2.LinkClick.All.NoInstantLoad",
      1);
}

// Verifies metrics recording for a navigation using prefetch.
IN_PROC_BROWSER_TEST_F(PreloadServingMetricsPageLoadMetricsObserverBrowserTest,
                       Prefetch) {
  base::HistogramTester histogram_tester;

  GURL initial_url = https_server().GetURL("a.test", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  GURL target_url = https_server().GetURL("a.test", "/title1.html");
  content::test::TestPrefetchWatcher test_prefetch_watcher;
  prerender_helper().AddPrefetchAsync(target_url);
  test_prefetch_watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                                           target_url);

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kFirstContentfulPaint);
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);
  NavigateViaLinkClick(web_contents(), target_url);
  waiter->Wait();

  EXPECT_TRUE(test_prefetch_watcher.PrefetchUsedInLastNavigation());

  // Navigate away to flush PreloadServingMetrics.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.LinkClick.All",
                                      1 /* kPrefetch */, 1);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.LinkClick.All.All",
      1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.LinkClick.All.Prefetch",
      1);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToLargestContentfulPaint2.LinkClick.All.All",
      1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToLargestContentfulPaint2.LinkClick.All.Prefetch",
      1);
}

// Verifies metrics recording for a navigation using prerender.
IN_PROC_BROWSER_TEST_F(PreloadServingMetricsPageLoadMetricsObserverBrowserTest,
                       Prerender) {
  base::HistogramTester histogram_tester;

  GURL initial_url = https_server().GetURL("a.test", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  GURL target_url = https_server().GetURL("a.test", "/title1.html");
  prerender_helper().AddPrerender(target_url);

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kFirstContentfulPaint);
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);
  NavigateViaLinkClick(web_contents(), target_url);
  waiter->Wait();

  // Navigate away to flush PreloadServingMetrics.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.LinkClick.All",
                                      2 /* kPrerender */, 1);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.LinkClick.All.All",
      1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.LinkClick.All.Prerender",
      1);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToLargestContentfulPaint2.LinkClick.All.All",
      1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToLargestContentfulPaint2.LinkClick.All.Prerender",
      1);
}

// Verifies metrics recording for BackForwardCache restore navigation.
//
// Scenario:
//
// 1. Navigate to Page A.
// 2. Navigate to Page B (Page A enters BFCache).
// 3. Go back to Page A (restore from BFCache).
// 4. Flush Page A's BFCache restore session by navigating to `about:blank`.
IN_PROC_BROWSER_TEST_F(PreloadServingMetricsPageLoadMetricsObserverBrowserTest,
                       BFCacheRestore) {
  base::HistogramTester histogram_tester;

  GURL url_a(https_server().GetURL("a.test", "/title1.html"));
  GURL url_b(https_server().GetURL("b.test", "/title1.html"));

  // 1. Navigate to Page A.
  auto waiter_a = CreatePageLoadMetricsTestWaiter();
  waiter_a->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kFirstContentfulPaint);
  waiter_a->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  waiter_a->Wait();
  content::RenderFrameHost* rfh_a = web_contents().GetPrimaryMainFrame();

  // 2. Navigate to Page B (Page A enters BFCache).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Initial navigation to Page A flushes as kNoInstantLoad (0).
  histogram_tester.ExpectBucketCount("PreloadServingMetrics.Other.All",
                                     0 /* kNoInstantLoad */, 1);

  // 3. Go back to Page A (restore from BFCache).
  web_contents().GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(&web_contents()));

  // 4. Navigate away to flush Page A's BFCache restore session.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // The BFCache restore session for Page A flushes as kBFCache (3).
  histogram_tester.ExpectBucketCount("PreloadServingMetrics.Backward.All",
                                     3 /* kBFCache */, 1);

  // FCP and LCP are not recorded for the BFCache restore.
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.Backward.All.All",
      0);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToLargestContentfulPaint2.Backward.All.All",
      0);
}

// Browser test for `PreloadServingMetricsPageLoadMetricsObserver` with prefetch
// ahead of prerender enabled.
class
    PreloadServingMetricsPageLoadMetricsObserverPrefetchAheadOfPrerenderBrowserTest
    : public PreloadServingMetricsPageLoadMetricsObserverBrowserTest {
 public:
  PreloadServingMetricsPageLoadMetricsObserverPrefetchAheadOfPrerenderBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrerender2FallbackPrefetchSpecRules, {}}}, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies metrics recording for a navigation using prerender when prefetch
// is served ahead of prerender.
IN_PROC_BROWSER_TEST_F(
    PreloadServingMetricsPageLoadMetricsObserverPrefetchAheadOfPrerenderBrowserTest,
    PrerenderWithPrefetchAheadOfPrerender) {
  base::HistogramTester histogram_tester;

  GURL initial_url = https_server().GetURL("a.test", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  GURL target_url = https_server().GetURL("a.test", "/title1.html");
  prerender_helper().AddPrerender(target_url);

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kFirstContentfulPaint);
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);
  NavigateViaLinkClick(web_contents(), target_url);
  waiter->Wait();

  // Navigate away to flush PreloadServingMetrics.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.LinkClick.All",
                                      2 /* kPrerender */, 1);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.LinkClick.All.All",
      1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.LinkClick.All.Prerender",
      1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.LinkClick.All.Prefetch",
      0);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToLargestContentfulPaint2.LinkClick.All.All",
      1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToLargestContentfulPaint2.LinkClick.All.Prerender",
      1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToLargestContentfulPaint2.LinkClick.All.Prefetch",
      0);

  // Ensure prefetch response and serving to prerender occurred.
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", 200, 1);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Prerender.Served."
      "SpeculationRule_Immediate2",
      1);
}
