// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/webui_page_load_metrics_observer.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/test/browser_test.h"

class WebUIPageLoadMetricsObserverBrowserTest : public InProcessBrowserTest {
 public:
  WebUIPageLoadMetricsObserverBrowserTest() = default;

  WebUIPageLoadMetricsObserverBrowserTest(
      const WebUIPageLoadMetricsObserverBrowserTest&) = delete;
  WebUIPageLoadMetricsObserverBrowserTest& operator=(
      const WebUIPageLoadMetricsObserverBrowserTest&) = delete;

  ~WebUIPageLoadMetricsObserverBrowserTest() override = default;

 protected:
  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents);
  }

  void NavigateAndWaitForMetrics(const GURL& url) {
    auto metrics_waiter = CreatePageLoadMetricsTestWaiter();
    metrics_waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kLargestContentfulPaint);
    // Navigate to the specified URL.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    metrics_waiter->Wait();
  }

  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Test that regular WebUI pages record metrics.
IN_PROC_BROWSER_TEST_F(WebUIPageLoadMetricsObserverBrowserTest,
                       RegularWebUIRecordsMetrics) {
  // Navigate to a regular WebUI page.
  NavigateAndWaitForMetrics(GURL("chrome://version"));

  // Verify that WebUI metrics were recorded.
  // Note: We can't easily test specific FCP/LCP values in a browser test since
  // they depend on actual rendering, but we can verify the histograms exist.
  histogram_tester_->ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.WebUI", 1);
}

// Test that internal WebUI pages (InternalWebUIConfig) do NOT record metrics.
IN_PROC_BROWSER_TEST_F(WebUIPageLoadMetricsObserverBrowserTest,
                       InternalWebUIDoesNotRecordMetrics) {
  // Navigate to an internal WebUI page that should be filtered out.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://omnibox")));

  // Navigate to a regular WebUI page.
  NavigateAndWaitForMetrics(GURL("chrome://version"));

  // Navigate to another internal WebUI page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://discards")));

  // Verify that WebUI metrics were NOT recorded for internal pages.
  histogram_tester_->ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.WebUI", 1);
  histogram_tester_->ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint.WebUI", 1);
}
