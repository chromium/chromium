// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/live_tab_count_page_load_metrics_observer.h"

#include <array>
#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/metrics/tab_count_metrics.h"
#include "chrome/browser/page_load_metrics/observers/histogram_suffixes.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/tab_count_metrics/tab_count_metrics.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using BucketCountArray =
    std::array<size_t, tab_count_metrics::kNumTabCountBuckets>;
using page_load_metrics::PageLoadMetricsTestWaiter;
using TimingField = page_load_metrics::PageLoadMetricsTestWaiter::TimingField;

class LiveTabCountPageLoadMetricsBrowserTest : public InProcessBrowserTest {
 public:
  LiveTabCountPageLoadMetricsBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &LiveTabCountPageLoadMetricsBrowserTest::GetActiveWebContents,
            base::Unretained(this))) {}

  LiveTabCountPageLoadMetricsBrowserTest(
      const LiveTabCountPageLoadMetricsBrowserTest&) = delete;
  LiveTabCountPageLoadMetricsBrowserTest& operator=(
      const LiveTabCountPageLoadMetricsBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  GURL GetTestURL() { return embedded_test_server()->GetURL("/title1.html"); }

  std::unique_ptr<PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiterForForegroundTab() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<PageLoadMetricsTestWaiter>(web_contents);
  }

  void ValidateHistograms(const char* suffix,
                          BucketCountArray& expected_counts) {
    const std::string histogram_prefix =
        std::string(internal::kHistogramPrefixLiveTabCount) +
        std::string(suffix);
    for (size_t bucket = 0; bucket < expected_counts.size(); bucket++) {
      histogram_tester_.ExpectTotalCount(
          tab_count_metrics::HistogramName(histogram_prefix,
                                           /* live_tabs_only = */ true, bucket),
          expected_counts[bucket]);
    }
  }

  base::HistogramTester histogram_tester_;
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(LiveTabCountPageLoadMetricsBrowserTest,
                       LoadSingleTabInForeground) {
  BucketCountArray counts = {0};

  auto waiter = CreatePageLoadMetricsTestWaiterForForegroundTab();
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestURL()));
  waiter->Wait();

  size_t live_tab_count = tab_count_metrics::LiveTabCount();
  EXPECT_EQ(live_tab_count, 1u);
  ++counts[tab_count_metrics::BucketForTabCount(live_tab_count)];
  ValidateHistograms(internal::kHistogramFirstContentfulPaintSuffix, counts);
}

IN_PROC_BROWSER_TEST_F(LiveTabCountPageLoadMetricsBrowserTest,
                       LoadSingleTabInBackground) {
  // Open a tab in the background, but don't wait for it to load; we need its
  // WebContents to create a PageLoadMetricsTestWaiter.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetTestURL(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_TRUE(web_contents);
  auto waiter = std::make_unique<PageLoadMetricsTestWaiter>(web_contents);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);

  // Switch tabs so the paint events occur.
  browser()->tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));

  waiter->Wait();

  BucketCountArray counts = {0};
  ValidateHistograms(internal::kHistogramFirstContentfulPaintSuffix, counts);
}

IN_PROC_BROWSER_TEST_F(LiveTabCountPageLoadMetricsBrowserTest,
                       LoadMultipleTabsInForeground) {
  // Test opening 5 tabs, which spans the first few buckets.
  constexpr size_t num_test_tabs = 5;

  BucketCountArray counts = {0};

  // Load the first tab separately, without inserting a new tab.
  auto waiter = CreatePageLoadMetricsTestWaiterForForegroundTab();
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestURL()));
  waiter->Wait();

  size_t live_tab_count = tab_count_metrics::LiveTabCount();
  EXPECT_EQ(live_tab_count, 1u);
  ++counts[tab_count_metrics::BucketForTabCount(live_tab_count)];
  ValidateHistograms(internal::kHistogramFirstContentfulPaintSuffix, counts);

  // Insert new tabs for the rest.
  for (size_t tab = 1; tab < num_test_tabs; tab++) {
    // Create the tab, but don't wait for it to load; we need its WebContents to
    // create a PageLoadMetricsTestWaiter.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GetTestURL(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_NONE);

    auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(tab);
    EXPECT_TRUE(web_contents);
    waiter = std::make_unique<PageLoadMetricsTestWaiter>(web_contents);
    waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);

    waiter->Wait();

    live_tab_count = tab_count_metrics::LiveTabCount();
    EXPECT_EQ(live_tab_count, tab + 1);
    ++counts[tab_count_metrics::BucketForTabCount(live_tab_count)];

    ValidateHistograms(internal::kHistogramFirstContentfulPaintSuffix, counts);
  }
}

IN_PROC_BROWSER_TEST_F(LiveTabCountPageLoadMetricsBrowserTest,
                       LoadSingleTabWithPrerendering) {
  BucketCountArray counts = {0};

  // Navigate to an initial page.
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // No contribution due to empty page.
  ValidateHistograms(internal::kHistogramFirstContentfulPaintSuffix, counts);

  // Start a prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  prerender_helper_.AddPrerender(prerender_url);

  // Activate and wait for FCP.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      GetActiveWebContents());
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstContentfulPaint);
  prerender_helper_.NavigatePrimaryPage(prerender_url);
  waiter->Wait();

  // Only check existence because the correction of time is difficult to check
  // in browsertest. Rely on tests of
  // CorrectEventAsNavigationOrActivationOrigined.
  size_t live_tab_count = tab_count_metrics::LiveTabCount();
  EXPECT_EQ(live_tab_count, 1u);
  ++counts[tab_count_metrics::BucketForTabCount(live_tab_count)];
  ValidateHistograms(internal::kHistogramFirstContentfulPaintSuffix, counts);
}
