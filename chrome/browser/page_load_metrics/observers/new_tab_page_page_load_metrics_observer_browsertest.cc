// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/new_tab_page_page_load_metrics_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

class NewTabPagePageLoadMetricsBrowserTest : public InProcessBrowserTest {
 public:
  NewTabPagePageLoadMetricsBrowserTest() = default;

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }
};

// Checks that metrics are recorded for the new tab page.
IN_PROC_BROWSER_TEST_F(NewTabPagePageLoadMetricsBrowserTest,
                       MetricsAreRecordedForNTP) {
  page_load_metrics::PageLoadMetricsTestWaiter metrics_waiter(
      GetActiveWebContents());
  base::HistogramTester histogram_tester;

  metrics_waiter.AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
          kFirstContentfulPaint);
  metrics_waiter.AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
          kLargestContentfulPaint);

  // Navigate to New Tab Page.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(chrome::kChromeUINewTabPageURL)));
  metrics_waiter.Wait();

  // LCP is only collected at the end of page lifecycle. Navigate to flush.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));

  histogram_tester.ExpectTotalCount("NewTabPage.LoadTime.FirstContentfulPaint",
                                    1);
  histogram_tester.ExpectTotalCount(
      "NewTabPage.LoadTime.LargestContentfulPaint", 1);
}
