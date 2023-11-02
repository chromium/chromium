// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"

namespace {

typedef InProcessBrowserTest NavigationMetricsRecorderBrowserTest;

// A site engagement score that falls into the range for HIGH engagement level.
const int kHighEngagementScore = 50;

IN_PROC_BROWSER_TEST_F(NavigationMetricsRecorderBrowserTest, TestMetrics) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NavigationMetricsRecorder* recorder =
      content::WebContentsUserData<NavigationMetricsRecorder>::FromWebContents(
          web_contents);
  ASSERT_TRUE(recorder);

  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html, <html></html>")));
  histograms.ExpectTotalCount(navigation_metrics::kMainFrameScheme, 1);
  histograms.ExpectBucketCount(navigation_metrics::kMainFrameScheme,
                               5 /* data: */, 1);
  histograms.ExpectTotalCount(navigation_metrics::kMainFrameSchemeDifferentPage,
                              1);
  histograms.ExpectBucketCount(
      navigation_metrics::kMainFrameSchemeDifferentPage, 5 /* data: */, 1);
}

// crbug.com/1292471: the test is flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_Navigation_EngagementLevel DISABLED_Navigation_EngagementLevel
#else
#define MAYBE_Navigation_EngagementLevel Navigation_EngagementLevel
#endif
IN_PROC_BROWSER_TEST_F(NavigationMetricsRecorderBrowserTest,
                       MAYBE_Navigation_EngagementLevel) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NavigationMetricsRecorder* recorder =
      content::WebContentsUserData<NavigationMetricsRecorder>::FromWebContents(
          web_contents);
  ASSERT_TRUE(recorder);

  const GURL url("https://google.com");
  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  histograms.ExpectTotalCount("Navigation.MainFrame.SiteEngagementLevel", 1);
  histograms.ExpectBucketCount("Navigation.MainFrame.SiteEngagementLevel",
                               blink::mojom::EngagementLevel::NONE, 1);

  site_engagement::SiteEngagementService::Get(browser()->profile())
      ->ResetBaseScoreForURL(url, kHighEngagementScore);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  histograms.ExpectTotalCount("Navigation.MainFrame.SiteEngagementLevel", 2);
  histograms.ExpectBucketCount("Navigation.MainFrame.SiteEngagementLevel",
                               blink::mojom::EngagementLevel::NONE, 1);
  histograms.ExpectBucketCount("Navigation.MainFrame.SiteEngagementLevel",
                               blink::mojom::EngagementLevel::HIGH, 1);
}

IN_PROC_BROWSER_TEST_F(NavigationMetricsRecorderBrowserTest,
                       FormSubmission_EngagementLevel) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/form.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Submit a form and check the histograms. Before doing so, we set a high site
  // engagement score so that a single form submission doesn't affect the score
  // much.
  site_engagement::SiteEngagementService::Get(browser()->profile())
      ->ResetBaseScoreForURL(url, kHighEngagementScore);
  base::HistogramTester histograms;
  content::TestNavigationObserver observer(web_contents);
  const char* const kScript = "document.getElementById('form').submit()";
  EXPECT_TRUE(content::ExecuteScript(web_contents, kScript));
  observer.WaitForNavigationFinished();

  histograms.ExpectTotalCount(
      "Navigation.MainFrameFormSubmission.SiteEngagementLevel", 1);
  histograms.ExpectBucketCount(
      "Navigation.MainFrameFormSubmission.SiteEngagementLevel",
      blink::mojom::EngagementLevel::HIGH, 1);
}

class NavigationMetricsRecorderPrerenderBrowserTest
    : public NavigationMetricsRecorderBrowserTest {
 public:
  NavigationMetricsRecorderPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &NavigationMetricsRecorderPrerenderBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~NavigationMetricsRecorderPrerenderBrowserTest() override = default;
  NavigationMetricsRecorderPrerenderBrowserTest(
      const NavigationMetricsRecorderPrerenderBrowserTest&) = delete;

  NavigationMetricsRecorderPrerenderBrowserTest& operator=(
      const NavigationMetricsRecorderPrerenderBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    NavigationMetricsRecorderBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    NavigationMetricsRecorderBrowserTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(NavigationMetricsRecorderPrerenderBrowserTest,
                       PrerenderingShouldNotRecordSiteEngagementLevelMetric) {
  NavigationMetricsRecorder* recorder =
      content::WebContentsUserData<NavigationMetricsRecorder>::FromWebContents(
          GetWebContents());
  ASSERT_TRUE(recorder);

  base::HistogramTester histograms;

  GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  histograms.ExpectTotalCount("Navigation.MainFrame.SiteEngagementLevel", 1);

  // Load a prerender page and prerendering should not increase the total count.
  GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  int host_id = prerender_test_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  EXPECT_FALSE(host_observer.was_activated());
  histograms.ExpectTotalCount("Navigation.MainFrame.SiteEngagementLevel", 1);

  // Activate the prerender page.
  prerender_test_helper().NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());
  histograms.ExpectTotalCount("Navigation.MainFrame.SiteEngagementLevel", 2);
}

}  // namespace
