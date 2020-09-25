// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/observers/back_forward_cache_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace {

using UkmEntry = ukm::builders::HistoryNavigation;

class BackForwardCachePageLoadMetricsObserverBrowserTest
    : public MetricIntegrationTest {
 public:
  ~BackForwardCachePageLoadMetricsObserverBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCache,
          {{"TimeToLiveInBackForwardCacheInSeconds", "3600"}}},
         {internal::kBackForwardCacheEmitZeroSamplesForKeyMetrics, {{}}}},
        {});

    MetricIntegrationTest::SetUpCommandLine(command_line);
  }

 protected:
  content::RenderFrameHost* top_frame_host() {
    return web_contents()->GetMainFrame();
  }

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents());
  }

  void ExpectMetricValueForUrl(const GURL& url,
                               const char* metric_name,
                               const int expected_value) {
    for (auto* entry : ukm_recorder().GetEntriesByName(UkmEntry::kEntryName)) {
      // As the source ID is generated from the back-forward restore navigation,
      // this should not match with the source ID by the ID used by the initial
      // navigation which loaded the page.
      DCHECK_NE(top_frame_host()->GetPageUkmSourceId(), entry->source_id);

      auto* source = ukm_recorder().GetSourceForSourceId(entry->source_id);
      DCHECK(source);
      if (source->url() != url)
        continue;
      if (!ukm_recorder().EntryHasMetric(entry, metric_name))
        continue;
      ukm_recorder().ExpectEntryMetric(entry, metric_name, expected_value);
    }
  }

  void ExpectMetricCountForUrl(const GURL& url,
                               const char* metric_name,
                               const int expected_count) {
    int count = 0;
    for (auto* entry : ukm_recorder().GetEntriesByName(UkmEntry::kEntryName)) {
      // As the source ID is generated from the back-forward restore navigation,
      // this should not match with the source ID by the ID used by the initial
      // navigation which loaded the page.
      DCHECK_NE(top_frame_host()->GetPageUkmSourceId(), entry->source_id);

      auto* source = ukm_recorder().GetSourceForSourceId(entry->source_id);
      DCHECK(source);
      if (source->url() != url)
        continue;
      if (!ukm_recorder().EntryHasMetric(entry, metric_name))
        continue;
      count++;
    }
    EXPECT_EQ(count, expected_count);
  }

  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BackForwardCachePageLoadMetricsObserverBrowserTest,
                       FirstPaintAfterBackForwardCacheRestore) {
  Start();
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHost* rfh_a = top_frame_host();

  // Navigate to B.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Go back to A.
  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kFirstPaintAfterBackForwardCacheRestore);
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    EXPECT_EQ(rfh_a, top_frame_host());
    EXPECT_FALSE(rfh_a->IsInBackForwardCache());

    waiter->Wait();
    histogram_tester().ExpectTotalCount(
        internal::kHistogramFirstPaintAfterBackForwardCacheRestore, 1);
    ExpectMetricCountForUrl(
        url_a, "NavigationToFirstPaintAfterBackForwardCacheRestore", 1);

    // 0 values are emitted for non-back-forward-cache metrics due to the flag
    // kBackForwardCacheEmitZeroSamplesForKeyMetrics.
    histogram_tester().ExpectBucketCount(internal::kHistogramFirstPaint, 0, 1);
    histogram_tester().ExpectBucketCount(
        internal::kHistogramFirstContentfulPaint, 0, 1);
    histogram_tester().ExpectBucketCount(
        internal::kHistogramLargestContentfulPaint, 0, 1);
  }

  // The RenderFrameHost for the page B was likely in the back-forward cache
  // just after the history navigation, but now this might be evicted due to
  // outstanding-network request.

  // Navigate to B again.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Go back to A again.
  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kFirstPaintAfterBackForwardCacheRestore);
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    EXPECT_EQ(rfh_a, top_frame_host());
    EXPECT_FALSE(rfh_a->IsInBackForwardCache());

    waiter->Wait();
    histogram_tester().ExpectTotalCount(
        internal::kHistogramFirstPaintAfterBackForwardCacheRestore, 2);
    ExpectMetricCountForUrl(
        url_a, "NavigationToFirstPaintAfterBackForwardCacheRestore", 2);

    histogram_tester().ExpectBucketCount(internal::kHistogramFirstPaint, 0, 2);
    histogram_tester().ExpectBucketCount(
        internal::kHistogramFirstContentfulPaint, 0, 2);
    histogram_tester().ExpectBucketCount(
        internal::kHistogramLargestContentfulPaint, 0, 2);
  }
}

IN_PROC_BROWSER_TEST_F(BackForwardCachePageLoadMetricsObserverBrowserTest,
                       FirstPaintAfterBackForwardCacheRestoreBackground) {
  Start();
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHost* rfh_a = top_frame_host();

  // Navigate to B.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Go back to A.
  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kFirstPaintAfterBackForwardCacheRestore);

    web_contents()->GetController().GoBack();

    // Make the tab backgrounded before the tab goes back.
    web_contents()->WasHidden();

    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    EXPECT_EQ(rfh_a, top_frame_host());
    EXPECT_FALSE(rfh_a->IsInBackForwardCache());

    web_contents()->WasShown();

    waiter->Wait();

    // As the tab goes to the background before the first paint, the UMA and the
    // UKM are not recorded.
    histogram_tester().ExpectTotalCount(
        internal::kHistogramFirstPaintAfterBackForwardCacheRestore, 0);
    ExpectMetricCountForUrl(
        url_a, "NavigationToFirstPaintAfterBackForwardCacheRestore", 0);

    histogram_tester().ExpectBucketCount(internal::kHistogramFirstPaint, 0, 0);
    histogram_tester().ExpectBucketCount(
        internal::kHistogramFirstContentfulPaint, 0, 0);
    histogram_tester().ExpectBucketCount(
        internal::kHistogramLargestContentfulPaint, 0, 0);
  }
}

IN_PROC_BROWSER_TEST_F(BackForwardCachePageLoadMetricsObserverBrowserTest,
                       FirstInputDelayAfterBackForwardCacheRestoreBackground) {
  Start();
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHost* rfh_a = top_frame_host();

  // Navigate to B.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstInputDelayAfterBackForwardCacheRestore, 0);

  // Go back to A.
  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kFirstInputDelayAfterBackForwardCacheRestore);

    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    EXPECT_EQ(rfh_a, top_frame_host());
    EXPECT_FALSE(rfh_a->IsInBackForwardCache());

    content::SimulateMouseClick(web_contents(), 0,
                                blink::WebPointerProperties::Button::kLeft);

    waiter->Wait();

    histogram_tester().ExpectTotalCount(
        internal::kHistogramFirstInputDelayAfterBackForwardCacheRestore, 1);
    ExpectMetricCountForUrl(url_a,
                            "FirstInputDelayAfterBackForwardCacheRestore", 1);

    // 0 values are emitted for non-back-forward-cache metrics due to the flag
    // kBackForwardCacheEmitZeroSamplesForKeyMetrics.
    histogram_tester().ExpectBucketCount(internal::kHistogramFirstInputDelay, 0,
                                         1);
  }
}

IN_PROC_BROWSER_TEST_F(BackForwardCachePageLoadMetricsObserverBrowserTest,
                       CumulativeLayoutShiftAfterBackForwardCacheRestore) {
  Start();

  const char path[] = "/layout-instability/simple-block-movement.html";
  GURL url_a(embedded_test_server()->GetURL("a.com", path));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kFirstPaint);
    waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                   TimingField::kLayoutShift);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
    waiter->Wait();
  }

  content::RenderFrameHost* rfh_a = top_frame_host();

  // Navigate to B.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Go back to A.
  double next_score;
  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kFirstPaintAfterBackForwardCacheRestore);
    waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                   TimingField::kLayoutShift);
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    EXPECT_EQ(rfh_a, top_frame_host());
    EXPECT_FALSE(rfh_a->IsInBackForwardCache());

    base::ListValue expectations =
        EvalJs(web_contents(), "cls_run_tests").ExtractList();
    next_score = EvalJs(web_contents(),
                        R"((async() => {
const shifter = document.querySelector('#shifter');
const currentTop =
  parseInt(getComputedStyle(shifter).getPropertyValue('top'), 10);
// With too big newTop (e.g., 320), the calculated score and the actual score
// can differ probably due to the window height. Use 300 here.
const newTop = 300;
shifter.style.top = newTop + 'px';
const score = computeExpectedScore(
    300 * (200 + newTop - currentTop), newTop - currentTop);
return score;
})())")
                     .ExtractDouble();
    waiter->Wait();
  }

  // The RenderFrameHost for the page B was likely in the back-forward cache
  // just after the history navigation, but now this might be evicted due to
  // outstanding-network request.
  //
  // TODO(hajimehoshi): This is not 100% sure. Evict B explicitly?

  // Navigate to B again.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  auto samples = histogram_tester().GetAllSamples(
      internal::
          kHistogramCumulativeShiftScoreMainFrameAfterBackForwardCacheRestore);
  EXPECT_EQ(1ul, samples.size());
  EXPECT_EQ(base::Bucket(page_load_metrics::LayoutShiftUmaValue(next_score), 1),
            samples[0]);

  histogram_tester().ExpectTotalCount(
      internal::
          kHistogramCumulativeShiftScoreMainFrameAfterBackForwardCacheRestore,
      1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramCumulativeShiftScoreAfterBackForwardCacheRestore, 1);

  ExpectMetricValueForUrl(url_a,
                          "CumulativeShiftScoreAfterBackForwardCacheRestore",
                          page_load_metrics::LayoutShiftUkmValue(next_score));

  // 0 values are emitted for non-back-forward-cache metrics due to the flag
  // kBackForwardCacheEmitZeroSamplesForKeyMetrics.
  // As back-foward cache is used twice (once for A and once for B), the current
  // total count is 2.
  histogram_tester().ExpectBucketCount(
      "PageLoad.LayoutInstability.CumulativeShiftScore.MainFrame", 0, 2);
  histogram_tester().ExpectBucketCount(
      "PageLoad.LayoutInstability.CumulativeShiftScore", 0, 2);

  // Go back to A again.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(rfh_a, top_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());

  // Navigate to B again.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  histogram_tester().ExpectTotalCount(
      internal::
          kHistogramCumulativeShiftScoreMainFrameAfterBackForwardCacheRestore,
      2);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramCumulativeShiftScoreAfterBackForwardCacheRestore, 2);

  ExpectMetricCountForUrl(
      url_a, "CumulativeShiftScoreAfterBackForwardCacheRestore", 2);

  // As back-foward cache is used fourth in total.
  histogram_tester().ExpectBucketCount(
      "PageLoad.LayoutInstability.CumulativeShiftScore.MainFrame", 0, 4);
  histogram_tester().ExpectBucketCount(
      "PageLoad.LayoutInstability.CumulativeShiftScore", 0, 4);
}
