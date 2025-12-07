// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/page_load_metrics_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"

class PageLoadMetricsObserverBrowserTest : public InProcessBrowserTest {
 public:
  PageLoadMetricsObserverBrowserTest() = default;
  ~PageLoadMetricsObserverBrowserTest() override = default;
  PageLoadMetricsObserverBrowserTest(
      const PageLoadMetricsObserverBrowserTest&) = delete;

  PageLoadMetricsObserverBrowserTest& operator=(
      const PageLoadMetricsObserverBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }
  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

class PageLoadMetricsObserverPrerenderBrowserTest
    : public PageLoadMetricsObserverBrowserTest {
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
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    PageLoadMetricsObserverBrowserTest::SetUp();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

class PageLoadMetricsObserverFencedFrameBrowserTest
    : public PageLoadMetricsObserverBrowserTest {
 public:
  PageLoadMetricsObserverFencedFrameBrowserTest() = default;
  ~PageLoadMetricsObserverFencedFrameBrowserTest() override = default;
  PageLoadMetricsObserverFencedFrameBrowserTest(
      const PageLoadMetricsObserverFencedFrameBrowserTest&) = delete;

  PageLoadMetricsObserverFencedFrameBrowserTest& operator=(
      const PageLoadMetricsObserverFencedFrameBrowserTest&) = delete;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(PageLoadMetricsObserverPrerenderBrowserTest,
                       PrerenderDoesNotCountAsPageLoad) {
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
  const content::FrameTreeNodeId host_id =
      prerender_test_helper().AddPrerender(prerender_url);
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

IN_PROC_BROWSER_TEST_F(PageLoadMetricsObserverFencedFrameBrowserTest,
                       FencedFrameCountsAsSubFramePageLoad) {
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::LoadCountsPerTopLevelDocument::kEntryName);
  EXPECT_EQ(1u, entries.size());
  histogram_tester()->ExpectBucketCount(
      "Stability.Experimental.PageLoads",
      performance_manager::LoadType::kVisibleTabBase, 1);

  // Load a fenced frame.
  GURL fenced_frame_url =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          GetWebContents()->GetPrimaryMainFrame(), fenced_frame_url);

  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::LoadCountsPerTopLevelDocument::kEntryName);
  // Fenced frames should not increase the entry size and the bucket count.
  EXPECT_EQ(1u, entries.size());
  histogram_tester()->ExpectBucketCount(
      "Stability.Experimental.PageLoads",
      performance_manager::LoadType::kVisibleTabBase, 1);

  histogram_tester()->ExpectBucketCount(
      "Stability.Experimental.PageLoads",
      performance_manager::LoadType::kVisibleTabSubFrameDifferentDocument, 1);

  // Navigate the fenced frame again.
  fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
      fenced_frame_host,
      embedded_test_server()->GetURL("/fenced_frames/title2.html"));
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::LoadCountsPerTopLevelDocument::kEntryName);
  EXPECT_EQ(1u, entries.size());
  histogram_tester()->ExpectBucketCount(
      "Stability.Experimental.PageLoads",
      performance_manager::LoadType::kVisibleTabBase, 1);

  histogram_tester()->ExpectBucketCount(
      "Stability.Experimental.PageLoads",
      performance_manager::LoadType::kVisibleTabSubFrameDifferentDocument, 2);
}
