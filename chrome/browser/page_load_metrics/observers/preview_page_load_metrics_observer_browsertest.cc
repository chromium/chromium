// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/preview_page_load_metrics_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PreviewPageLoadMetricsObserverBrowserTest : public InProcessBrowserTest {
 public:
  PreviewPageLoadMetricsObserverBrowserTest() = default;
  ~PreviewPageLoadMetricsObserverBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    histogram_tester_.emplace();
  }

  void NavigateToOriginPath(const std::string& path) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetOriginURL(path)));
    base::RunLoop().RunUntilIdle();
  }

  void NavigateToOriginPathFromRenderer(const std::string& path) {
    ASSERT_TRUE(content::NavigateToURLFromRenderer(browser()
                                                       ->tab_strip_model()
                                                       ->GetActiveWebContents()
                                                       ->GetPrimaryMainFrame(),
                                                   GetOriginURL(path)));
    base::RunLoop().RunUntilIdle();
  }

  void NavigateAway() {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
    base::RunLoop().RunUntilIdle();
  }

  GURL GetOriginURL(const std::string& path) {
    return embedded_test_server()->GetURL("origin.com", path);
  }

  base::HistogramTester& histogram_tester() { return *histogram_tester_; }

 private:
  absl::optional<base::HistogramTester> histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(PreviewPageLoadMetricsObserverBrowserTest,
                       PageVisitType) {
  // Constants defined in ukm_page_load_metrics_observer.cc.
  const int kIndependentPageVisitType = 0;
  const int kOriginPageVisitType = 1;
  const int kPassingPageVisitType = 2;
  const int kTerminalPageVisitType = 3;

  // Disables BFCache to make the page lifecycle management easy in testing.
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetController()
      .GetBackForwardCache()
      .DisableForTesting(content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // Runs a single browser initiated navigation.
  NavigateToOriginPath("/title1.html");
  NavigateAway();

  histogram_tester().ExpectUniqueSample("PageLoad.Experimental.PageVisitType",
                                        kIndependentPageVisitType, 1);

  // Runs a series of navigation.
  NavigateToOriginPath("/title1.html");
  NavigateToOriginPathFromRenderer("/title2.html");

  histogram_tester().ExpectBucketCount("PageLoad.Experimental.PageVisitType",
                                       kOriginPageVisitType, 1);

  NavigateToOriginPathFromRenderer("/title3.html");
  histogram_tester().ExpectBucketCount("PageLoad.Experimental.PageVisitType",
                                       kPassingPageVisitType, 1);

  NavigateAway();
  histogram_tester().ExpectBucketCount("PageLoad.Experimental.PageVisitType",
                                       kTerminalPageVisitType, 1);
}

// TODO(b:299566150): Add more cases that have back navigation and branching.

}  // namespace
