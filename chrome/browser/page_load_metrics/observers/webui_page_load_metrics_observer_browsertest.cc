// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/webui_page_load_metrics_observer.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/prefs/pref_service.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

class WebUIPageLoadMetricsObserverBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  WebUIPageLoadMetricsObserverBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (GetParam()) {
      enabled_features.push_back(features::kInitialWebUI);
      enabled_features.push_back(features::kWebUIReloadButton);
    } else {
      disabled_features.push_back(features::kInitialWebUI);
      disabled_features.push_back(features::kWebUIReloadButton);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

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
    // Navigate away to flush metrics.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  }

  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
};

// Test that regular WebUI pages record metrics.
IN_PROC_BROWSER_TEST_P(WebUIPageLoadMetricsObserverBrowserTest,
                       WebUIRecordsMetrics) {
  // Navigate to a regular WebUI page.
  NavigateAndWaitForMetrics(GURL("chrome://version"));

  // Verify that WebUI metrics were recorded.
  // Note: We can't easily test specific FCP/LCP values in a browser test since
  // they depend on actual rendering, but we can verify the histograms exist.
  histogram_tester_->ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.WebUI.version", 1);
  histogram_tester_->ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint.WebUI.version",
      1);
}

// Test that visits to internal WebUI pages do NOT record metrics.
IN_PROC_BROWSER_TEST_P(WebUIPageLoadMetricsObserverBrowserTest,
                       InternalWebUIDoesNotRecordMetrics) {
  // Enable internal debug pages.
  g_browser_process->local_state()->SetBoolean(
      chrome_urls::kInternalOnlyUisEnabled, true);

  // Navigate to an internal debug WebUI page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://omnibox")));
  // Verify that WebUI metrics ARE NOT recorded for internal debug WebUI.
  // Note: When InitialWebUI is enabled, a background WebContents may load a
  // regular WebUI page (e.g. New Tab Page) and record FCP early, polluting
  // full WebUI metrics histograms. We expect 1 sample for FCP if enabled, 0 if
  // disabled. However, LCP is not recorded for background pages, so we still
  // expect 0.
  histogram_tester_->ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.WebUI.omnibox", 0);

  histogram_tester_->ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint.WebUI.omnibox",
      0);

  // Navigate to a WebUI page that is not an internal debug WebUI.
  NavigateAndWaitForMetrics(GURL("chrome://version"));
  // Verify that WebUI metrics ARE recorded for version WebUI but are not
  // recorded for previous WebUI navigation. If aggregate metrics fail, omnibox
  // is no longer an internal debug WebUI or WebUIPageLoadMetricsObserver
  // recorded metrics incorrectly.

  histogram_tester_->ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint.WebUI.version",
      1);
  histogram_tester_->ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.WebUI.version", 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebUIPageLoadMetricsObserverBrowserTest,
                         ::testing::Bool());
