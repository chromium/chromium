// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/multi_tab_loading_page_load_metrics_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/page_load_metrics/observers/histogram_suffixes.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class MultiTabLoadingPageLoadMetricsBrowserTest : public InProcessBrowserTest {
 public:
  MultiTabLoadingPageLoadMetricsBrowserTest() {}
  ~MultiTabLoadingPageLoadMetricsBrowserTest() override {}

 protected:
  GURL GetTestURL() { return embedded_test_server()->GetURL("/simple.html"); }

  void NavigateToURLWithoutWaiting(GURL url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NO_WAIT);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  std::string HistogramNameWithSuffix(const char* suffix) {
    return std::string(internal::kHistogramPrefixMultiTabLoading)
        .append(suffix);
  }
};

IN_PROC_BROWSER_TEST_F(MultiTabLoadingPageLoadMetricsBrowserTest, SingleTab) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestURL()));

  // Navigate away to force the histogram recording.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  histogram_tester.ExpectTotalCount(
      HistogramNameWithSuffix(internal::kHistogramLoadEventFiredSuffix), 0);
  histogram_tester.ExpectTotalCount(
      HistogramNameWithSuffix(
          internal::kHistogramLoadEventFiredBackgroundSuffix),
      0);
}

// TODO(crbug.com/40830313): Test is flaky on Linux, lacros, Chrome OS, Mac.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#define MAYBE_MultiTabForeground DISABLED_MultiTabForeground
#else
#define MAYBE_MultiTabForeground MultiTabForeground
#endif
IN_PROC_BROWSER_TEST_F(MultiTabLoadingPageLoadMetricsBrowserTest,
                       MAYBE_MultiTabForeground) {
  base::HistogramTester histogram_tester;

  NavigateToURLWithoutWaiting(embedded_test_server()->GetURL("/hung"));

  // Open a new foreground tab.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), GetTestURL(), 1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Navigate away to force the histogram recording.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  histogram_tester.ExpectTotalCount(
      HistogramNameWithSuffix(internal::kHistogramLoadEventFiredSuffix), 1);
  histogram_tester.ExpectTotalCount(
      HistogramNameWithSuffix(
          internal::kHistogramLoadEventFiredBackgroundSuffix),
      0);
}

IN_PROC_BROWSER_TEST_F(MultiTabLoadingPageLoadMetricsBrowserTest,
                       MultiTabBackground) {
  base::HistogramTester histogram_tester;

  NavigateToURLWithoutWaiting(embedded_test_server()->GetURL("/hung"));

  // Open a tab in the background.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), GetTestURL(), 1, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Close the foreground tab.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContentsDestroyedWatcher destroyed_watcher(
      tab_strip_model->GetWebContentsAt(0));
  int previous_tab_count = tab_strip_model->count();
  tab_strip_model->CloseWebContentsAt(0, 0);
  EXPECT_EQ(previous_tab_count - 1, tab_strip_model->count());
  destroyed_watcher.Wait();
  // Now the background tab should have moved to the foreground.

  // Navigate away to force the histogram recording.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  histogram_tester.ExpectTotalCount(
      HistogramNameWithSuffix(internal::kHistogramLoadEventFiredSuffix), 0);
  histogram_tester.ExpectTotalCount(
      HistogramNameWithSuffix(
          internal::kHistogramLoadEventFiredBackgroundSuffix),
      1);
}
