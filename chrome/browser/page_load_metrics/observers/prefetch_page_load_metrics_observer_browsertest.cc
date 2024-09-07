// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/prefetch_page_load_metrics_observer.h"

#include <memory>

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrefetchPageLoadMetricsObserverBrowserTest : public InProcessBrowserTest {
 public:
  PrefetchPageLoadMetricsObserverBrowserTest() = default;
  ~PrefetchPageLoadMetricsObserverBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/subresource_loading");
    ASSERT_TRUE(embedded_test_server()->Start());

    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void NavigateTo(const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    base::RunLoop().RunUntilIdle();
  }

  void NavigateToOriginPath(const std::string& path) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("origin.com", path)));
    base::RunLoop().RunUntilIdle();
  }

  void NavigateAway() {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
    base::RunLoop().RunUntilIdle();
  }

  void VerifyNoUKM() {
    auto entries = ukm_recorder_->GetEntriesByName(
        ukm::builders::PrefetchProxy::kEntryName);
    EXPECT_TRUE(entries.empty());
  }

  GURL GetOriginURL(const std::string& path) {
    return embedded_test_server()->GetURL("origin.com", path);
  }

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

// TODO(http://crbug.com/1025737) Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_HistoryPlumbing DISABLED_HistoryPlumbing
#else
#define MAYBE_HistoryPlumbing HistoryPlumbing
#endif
IN_PROC_BROWSER_TEST_F(PrefetchPageLoadMetricsObserverBrowserTest,
                       MAYBE_HistoryPlumbing) {
  base::HistogramTester histogram_tester;
  NavigateToOriginPath("/index.html");
  NavigateAway();
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", false, 1);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0);

  // Revisit and expect a 0 days-ago entry.
  NavigateToOriginPath("/index.html");
  NavigateAway();

  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", true, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", false, 1);
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0, 1);
}

IN_PROC_BROWSER_TEST_F(PrefetchPageLoadMetricsObserverBrowserTest,
                       RecordNothingOnUntrackedPage) {
  base::HistogramTester histogram_tester;

  NavigateAway();
  NavigateAway();

  VerifyNoUKM();
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", 0);
}

class PrefetchPageLoadMetricsObserverPrerenderBrowserTest
    : public PrefetchPageLoadMetricsObserverBrowserTest {
 public:
  PrefetchPageLoadMetricsObserverPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &PrefetchPageLoadMetricsObserverPrerenderBrowserTest::
                GetWebContents,
            base::Unretained(this))) {}
  ~PrefetchPageLoadMetricsObserverPrerenderBrowserTest() override = default;
  PrefetchPageLoadMetricsObserverPrerenderBrowserTest(
      const PrefetchPageLoadMetricsObserverPrerenderBrowserTest&) = delete;

  PrefetchPageLoadMetricsObserverPrerenderBrowserTest& operator=(
      const PrefetchPageLoadMetricsObserverPrerenderBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    PrefetchPageLoadMetricsObserverBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    PrefetchPageLoadMetricsObserverBrowserTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(PrefetchPageLoadMetricsObserverPrerenderBrowserTest,
                       PrerenderingShouldNotRecordMetrics) {
  base::HistogramTester histogram_tester;

  GURL initial_url = embedded_test_server()->GetURL("/redirect_to_index.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Load a prerender page and prerendering should not increase the total count.
  GURL prerender_url = embedded_test_server()->GetURL("/index.html");
  content::FrameTreeNodeId host_id =
      prerender_test_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  EXPECT_FALSE(host_observer.was_activated());

  // Activate the prerender page.
  prerender_test_helper().NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());
}
