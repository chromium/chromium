// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/initial_webui_page_load_metrics_observer.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

class InitialWebUIPageLoadMetricsObserverBrowserTest
    : public InProcessBrowserTest {
 public:
  InitialWebUIPageLoadMetricsObserverBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton}, {});
  }

  InitialWebUIPageLoadMetricsObserverBrowserTest(
      const InitialWebUIPageLoadMetricsObserverBrowserTest&) = delete;
  InitialWebUIPageLoadMetricsObserverBrowserTest& operator=(
      const InitialWebUIPageLoadMetricsObserverBrowserTest&) = delete;

  ~InitialWebUIPageLoadMetricsObserverBrowserTest() override = default;

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
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kFirstPaint);
    // Navigate to the specified URL.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    metrics_waiter->Wait();
  }

  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that InitialWebUIPageLoadMetricsObserverBrowserTest does NOT record
// histograms when the target WebUI is not the InitialWebUI.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       NotRecordForNonInitialWebUI) {
  // Navigate to a non-initial WebUI page.
  NavigateAndWaitForMetrics(GURL("chrome://version"));

  // Navigate away to force histogram recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Verify that initial WebUI metrics were NOT recorded.
  histogram_tester_->ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstPaint", 0);
  histogram_tester_->ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstContentfulPaint", 0);
}

// Test that InitialWebUIPageLoadMetricsObserverBrowserTest does NOT record
// histograms for non-chrome schemes.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       NotRecordForNonChromeScheme) {
  // Navigate to a data: URL (non-chrome scheme).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<html><body>Hello world</body></html>")));

  // Navigate away to force histogram recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Verify that initial WebUI metrics were NOT recorded.
  histogram_tester_->ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstPaint", 0);
  histogram_tester_->ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstContentfulPaint", 0);
}
