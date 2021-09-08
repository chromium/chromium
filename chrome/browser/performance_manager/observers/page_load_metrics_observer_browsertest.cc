// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/page_load_metrics_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"

class PageLoadMetricsObserverPrerenderBrowserTest
    : public InProcessBrowserTest {
 public:
  PageLoadMetricsObserverPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &PageLoadMetricsObserverPrerenderBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~PageLoadMetricsObserverPrerenderBrowserTest() override = default;
  PageLoadMetricsObserverPrerenderBrowserTest(
      const PageLoadMetricsObserverPrerenderBrowserTest&) = delete;

  PageLoadMetricsObserverPrerenderBrowserTest& operator=(
      const PageLoadMetricsObserverPrerenderBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }
  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(PageLoadMetricsObserverPrerenderBrowserTest,
                       PrerenderingDontRecordUKMMetricsAndUMAHistogram) {
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::LoadCountsPerTopLevelDocument::kEntryName);
  EXPECT_EQ(1u, entries.size());
  histogram_tester()->ExpectBucketCount(
      "Stability.Experimental.PageLoads",
      performance_manager::LoadType::kVisibleTabBase, 1);

  // Load a page in the prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  const int host_id = prerender_test_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  EXPECT_FALSE(host_observer.was_activated());
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::LoadCountsPerTopLevelDocument::kEntryName);
  // Prerendering should not increase the entry size and the bucket count.
  EXPECT_EQ(1u, entries.size());
  histogram_tester()->ExpectBucketCount(
      "Stability.Experimental.PageLoads",
      performance_manager::LoadType::kVisibleTabBase, 1);

  // Activate the prerender page.
  prerender_test_helper().NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::LoadCountsPerTopLevelDocument::kEntryName);
  EXPECT_EQ(2u, entries.size());
  histogram_tester()->ExpectBucketCount(
      "Stability.Experimental.PageLoads",
      performance_manager::LoadType::kVisibleTabBase, 2);
}
