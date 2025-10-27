// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/initial_webui_page_load_metrics_observer.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
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
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kInitialWebUIMetrics},
        {});
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
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kMonotonicFirstPaint);
    metrics_waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kMonotonicFirstContentfulPaint);
    // Navigate to the specified URL.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    metrics_waiter->Wait();
    // Navigate away to flush metrics.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  }

  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that InitialWebUIPageLoadMetricsObserverBrowserTest record histograms
// only once across multiple navigations. Disabled due to flakiness.
// TODO(crbug.com/455508156): Re-enable this test.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       DISABLED_RecordOnlyOnceForInitialWebUI) {
  // TODO(crbug.com/448794588): Should check initial histogram here, but test
  // doesn't support waiting for the webcontents embededed in topchrome UI.

  // 1. Navigate to an initial WebUI page.
  NavigateAndWaitForMetrics(GURL(chrome::kChromeUIReloadButtonURL));

  // The metrics should be only be recorded once from the browser UI itself.
  // Subsequent navigation will not be FirstPaint.
  histogram_tester_->ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstPaint", 1);
  histogram_tester_->ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstContentfulPaint", 1);

  // 2. Navigate to a non-initial WebUI page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version")));

  // The metrics should not increase.
  histogram_tester_->ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstPaint", 1);
  histogram_tester_->ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstContentfulPaint", 1);

  // 3. Navigate to a data: URL (non-chrome scheme).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<html><body>Hello world</body></html>")));

  // The metrics should not increase.
  histogram_tester_->ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstPaint", 1);
  histogram_tester_->ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstContentfulPaint", 1);
}
