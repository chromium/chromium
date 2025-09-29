// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/non_tab_webui_page_load_metrics_observer.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_embedder_interface.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/test/browser_test.h"

class NonTabWebUIPageLoadMetricsObserverBrowserTest
    : public InProcessBrowserTest {
 public:
  NonTabWebUIPageLoadMetricsObserverBrowserTest() = default;

  NonTabWebUIPageLoadMetricsObserverBrowserTest(
      const NonTabWebUIPageLoadMetricsObserverBrowserTest&) = delete;
  NonTabWebUIPageLoadMetricsObserverBrowserTest& operator=(
      const NonTabWebUIPageLoadMetricsObserverBrowserTest&) = delete;

  ~NonTabWebUIPageLoadMetricsObserverBrowserTest() override = default;

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

  page_load_metrics::PageLoadMetricsEmbedderInterface* GetEmbedderInterface() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    auto* observer =
        page_load_metrics::MetricsWebContentsObserver::FromWebContents(
            web_contents);
    return observer->GetEmbedderInterfaceForTesting();
  }

  void NavigateToTabWebUIAndWaitForMetrics(const GURL& url) {
    auto metrics_waiter = CreatePageLoadMetricsTestWaiter();
    metrics_waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kFirstContentfulPaint);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    // Verify that the embedder interface recognizes this as not being non-tab
    // WebUI. If this changes, the test needs to be updated to use a new WebUI
    // URL.
    ASSERT_FALSE(GetEmbedderInterface()->IsNonTabWebUI(url));
    metrics_waiter->Wait();
  }

  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Test that NonTabWebUIPageLoadMetricsObserver does NOT record histograms when
// WebUI is designed for a tab.
IN_PROC_BROWSER_TEST_F(NonTabWebUIPageLoadMetricsObserverBrowserTest,
                       DoesntRecordHistogramsForTabWebUI) {
  // Navigate to a chrome:// URL in a regular tab (not a non-tab WebUI context).
  NavigateToTabWebUIAndWaitForMetrics(GURL("chrome://version"));

  // Navigate away to force histogram recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Verify that NonTabWebUI metrics were NOT recorded. In a regular browser
  // tab, IsNonTabWebUI() returns false, so no metrics should be recorded.
  histogram_tester_->ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.NonTabWebUI", 0);
  histogram_tester_->ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.NonTabWebUI",
      0);
}

// Test that NonTabWebUIPageLoadMetricsObserver does NOT record histograms for
// non-chrome schemes.
IN_PROC_BROWSER_TEST_F(NonTabWebUIPageLoadMetricsObserverBrowserTest,
                       DoesntRecordHistogramsForNonChromeScheme) {
  // Navigate to a data: URL (non-chrome scheme).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<html><body>Hello world</body></html>")));

  // Navigate away to force histogram recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Verify that NonTabWebUI metrics were NOT recorded for non-chrome scheme.
  histogram_tester_->ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.NonTabWebUI", 0);
  histogram_tester_->ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.NonTabWebUI",
      0);
}
