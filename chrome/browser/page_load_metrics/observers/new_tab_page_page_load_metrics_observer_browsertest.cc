// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/new_tab_page_page_load_metrics_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/browser/visibility.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
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
                                     chrome::ChromeUINewTabPageURLAsGURL()));
  metrics_waiter.Wait();

  // LCP is only collected at the end of page lifecycle. Navigate to flush.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));

  histogram_tester.ExpectTotalCount("NewTabPage.LoadTime.FirstContentfulPaint2",
                                    1);
  histogram_tester.ExpectTotalCount(
      "NewTabPage.LoadTime.LargestContentfulPaint", 1);
}

// Checks that metrics are not recorded when the new tab page is navigated in a
// background tab and activated later.
IN_PROC_BROWSER_TEST_F(NewTabPagePageLoadMetricsBrowserTest,
                       MetricsAreNotRecordedForBackgroundNTP) {
  base::HistogramTester histogram_tester;

  // Create a background tab first so PageLoadMetricsTestWaiter can attach
  // before the New Tab Page navigation starts.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::kNewBackgroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_EQ(2, tab_strip_model->count());
  content::WebContents* background_contents =
      tab_strip_model->GetWebContentsAt(1);
  ASSERT_EQ(content::Visibility::HIDDEN, background_contents->GetVisibility());

  page_load_metrics::PageLoadMetricsTestWaiter metrics_waiter(
      background_contents);
  metrics_waiter.AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
          kFirstContentfulPaint);
  metrics_waiter.AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
          kLargestContentfulPaint);

  // Navigate the hidden background tab to the New Tab Page.
  ASSERT_TRUE(content::NavigateToURL(background_contents,
                                     chrome::ChromeUINewTabPageURLAsGURL()));
  ASSERT_EQ(content::Visibility::HIDDEN, background_contents->GetVisibility());

  // Activate the tab so FCP and LCP are painted.
  tab_strip_model->ActivateTabAt(1);
  ASSERT_EQ(background_contents, GetActiveWebContents());
  metrics_waiter.Wait();

  // Navigate away to flush session-end metrics.
  ASSERT_TRUE(content::NavigateToURL(background_contents,
                                     GURL(url::kAboutBlankURL)));

  histogram_tester.ExpectTotalCount("NewTabPage.LoadTime.FirstContentfulPaint2",
                                    0);
  histogram_tester.ExpectTotalCount(
      "NewTabPage.LoadTime.LargestContentfulPaint", 0);
}
