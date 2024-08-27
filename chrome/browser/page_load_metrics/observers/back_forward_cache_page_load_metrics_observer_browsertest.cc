// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/back_forward_cache_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace {

using UkmEntry = ukm::builders::HistoryNavigation;
using page_load_metrics::PageEndReason;

class BackForwardCachePageLoadMetricsObserverBrowserTest
    : public MetricIntegrationTest {
 public:
  ~BackForwardCachePageLoadMetricsObserverBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{internal::kBackForwardCacheEmitZeroSamplesForKeyMetrics, {{}}}}),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());

    MetricIntegrationTest::SetUpCommandLine(command_line);
  }

 protected:
  content::RenderFrameHost* top_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents());
  }

  void ExpectMetricValueForUrl(const GURL& url,
                               const char* metric_name,
                               const int expected_value) {
    for (const ukm::mojom::UkmEntry* entry :
         ukm_recorder().GetEntriesByName(UkmEntry::kEntryName)) {
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
    for (const ukm::mojom::UkmEntry* entry :
         ukm_recorder().GetEntriesByName(UkmEntry::kEntryName)) {
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

  void VerifyHistoryNavPageEndReasons(const std::vector<PageEndReason>& reasons,
                                      const GURL& url) {
    unsigned int reason_index = 0;
    for (const ukm::mojom::UkmEntry* entry :
         ukm_recorder().GetEntriesByName(UkmEntry::kEntryName)) {
      auto* source = ukm_recorder().GetSourceForSourceId(entry->source_id);
      if (source->url() != url)
        continue;
      if (ukm_recorder().EntryHasMetric(
              entry,
              UkmEntry::kPageEndReasonAfterBackForwardCacheRestoreName)) {
        ASSERT_LT(reason_index, reasons.size());
        ukm_recorder().ExpectEntryMetric(
            entry, UkmEntry::kPageEndReasonAfterBackForwardCacheRestoreName,
            reasons[reason_index++]);
      }
    }
    // Should have been through all the reasons.
    EXPECT_EQ(reason_index, reasons.size());
  }

  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

// TODO(crbug.com/334416161): Re-enble this test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_FirstPaintAfterBackForwardCacheRestore \
  DISABLED_FirstPaintAfterBackForwardCacheRestore
#else
#define MAYBE_FirstPaintAfterBackForwardCacheRestore \
  FirstPaintAfterBackForwardCacheRestore
#endif
IN_PROC_BROWSER_TEST_F(BackForwardCachePageLoadMetricsObserverBrowserTest,
                       MAYBE_FirstPaintAfterBackForwardCacheRestore) {
  Start();
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHostWrapper rfh_a(top_frame_host());

  // Navigate to B.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Go back to A.
  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kFirstPaintAfterBackForwardCacheRestore);
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
    EXPECT_NE(rfh_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);

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

  // Navigate to B again.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Go back to A again.
  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kFirstPaintAfterBackForwardCacheRestore);
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
    EXPECT_NE(rfh_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);

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
  content::RenderFrameHostWrapper rfh_a(top_frame_host());

  // Navigate to B.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

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
    EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
    EXPECT_NE(rfh_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);

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

// TODO(https://crbug.com/40799125): Test is flaky on Windows and Mac.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_FirstInputDelayAfterBackForwardCacheRestoreBackground \
  DISABLED_FirstInputDelayAfterBackForwardCacheRestoreBackground
#else
#define MAYBE_FirstInputDelayAfterBackForwardCacheRestoreBackground \
  FirstInputDelayAfterBackForwardCacheRestoreBackground
#endif
IN_PROC_BROWSER_TEST_F(
    BackForwardCachePageLoadMetricsObserverBrowserTest,
    MAYBE_FirstInputDelayAfterBackForwardCacheRestoreBackground) {
  Start();
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHostWrapper rfh_a(top_frame_host());

  // Navigate to B.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

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
    EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
    EXPECT_NE(rfh_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);

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
                       IsAmpPageAfterBackForwardCacheRestore) {
  Start();
  GURL url_a(embedded_test_server()->GetURL(
      "amp.com", "/page_load_metrics/amp_basic.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddLoadingBehaviorExpectation(
      blink::kLoadingBehaviorAmpDocumentLoaded);

  // Navigate to A.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHost* rfh_a = top_frame_host();

  // Make sure there is time to sync loading behavior flags to the browser
  // side. We wait here instead of after the bfcache restore because the
  // relevant loading behavior flag is only encountered during the initial page
  // load and not as part of a bfcache restore.
  waiter->Wait();
  waiter = nullptr;

  // Navigate to B.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
  EXPECT_NE(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Verify that the HistoryNavigation has the appropriate flag set.
  ExpectMetricCountForUrl(url_a, UkmEntry::kBackForwardCache_IsAmpPageName, 1);
  ExpectMetricValueForUrl(url_a, UkmEntry::kBackForwardCache_IsAmpPageName, 1);
}

// TODO(crbug.com/334416161): Re-enble this test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_CumulativeLayoutShiftAfterBackForwardCacheRestore \
  DISABLED_CumulativeLayoutShiftAfterBackForwardCacheRestore
#else
#define MAYBE_CumulativeLayoutShiftAfterBackForwardCacheRestore \
  CumulativeLayoutShiftAfterBackForwardCacheRestore
#endif
IN_PROC_BROWSER_TEST_F(
    BackForwardCachePageLoadMetricsObserverBrowserTest,
    MAYBE_CumulativeLayoutShiftAfterBackForwardCacheRestore) {
  Start();

  const char path[] = "/layout-instability/simple-block-movement.html";
  GURL url_a(embedded_test_server()->GetURL("a.com", path));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kFirstPaint);
    waiter->AddPageLayoutShiftExpectation();
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
    waiter->Wait();
  }

  content::RenderFrameHost* rfh_a = top_frame_host();

  // Navigate to B.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Go back to A.
  double next_score;
  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kFirstPaintAfterBackForwardCacheRestore);
    waiter->AddPageLayoutShiftExpectation();
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
    EXPECT_NE(rfh_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);

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
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

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
  EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
  EXPECT_NE(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Navigate to B again.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

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

// TODO(crbug.com/40752530): Disabled for being flaky.
IN_PROC_BROWSER_TEST_F(
    BackForwardCachePageLoadMetricsObserverBrowserTest,
    DISABLED_RequestAnimationFramesAfterBackForwardCacheRestore) {
  Start();
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHost* rfh_a = top_frame_host();

  // Navigate to B.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Go back to A.
  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kRequestAnimationFrameAfterBackForwardCacheRestore);
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
    EXPECT_NE(rfh_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);

    waiter->Wait();
    histogram_tester().ExpectTotalCount(
        internal::
            kHistogramFirstRequestAnimationFrameAfterBackForwardCacheRestore,
        1);
    histogram_tester().ExpectTotalCount(
        internal::
            kHistogramSecondRequestAnimationFrameAfterBackForwardCacheRestore,
        1);
    histogram_tester().ExpectTotalCount(
        internal::
            kHistogramThirdRequestAnimationFrameAfterBackForwardCacheRestore,
        1);
    ExpectMetricCountForUrl(
        url_a, "FirstRequestAnimationFrameAfterBackForwardCacheRestore", 1);
    ExpectMetricCountForUrl(
        url_a, "SecondRequestAnimationFrameAfterBackForwardCacheRestore", 1);
    ExpectMetricCountForUrl(
        url_a, "ThirdRequestAnimationFrameAfterBackForwardCacheRestore", 1);
  }
}

// TODO(crbug.com/334416161): Re-enble this test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_LayoutShiftNormalization_AfterBackForwardCacheRestore \
  DISABLED_LayoutShiftNormalization_AfterBackForwardCacheRestore
#else
#define MAYBE_LayoutShiftNormalization_AfterBackForwardCacheRestore \
  LayoutShiftNormalization_AfterBackForwardCacheRestore
#endif
IN_PROC_BROWSER_TEST_F(
    BackForwardCachePageLoadMetricsObserverBrowserTest,
    MAYBE_LayoutShiftNormalization_AfterBackForwardCacheRestore) {
  Start();

  const char path[] = "/layout-instability/simple-block-movement.html";
  GURL url_a(embedded_test_server()->GetURL("a.com", path));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kFirstPaint);
    waiter->AddPageLayoutShiftExpectation();
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
    waiter->Wait();
  }

  content::RenderFrameHost* rfh_a = top_frame_host();

  // Navigate to B.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Go back to A.
  float next_score;
  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kFirstPaintAfterBackForwardCacheRestore);
    waiter->AddPageLayoutShiftExpectation();
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
    EXPECT_NE(rfh_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);

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
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  ExpectMetricValueForUrl(url_a,
                          "CumulativeShiftScoreAfterBackForwardCacheRestore",
                          page_load_metrics::LayoutShiftUkmValue(next_score));
  ExpectMetricValueForUrl(
      url_a,
      "MaxCumulativeShiftScoreAfterBackForwardCacheRestore."
      "SessionWindow.Gap1000ms.Max5000ms2",
      page_load_metrics::LayoutShiftUmaValue10000(next_score));
  // Go back to A again.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
  EXPECT_NE(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Navigate to B again.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  ExpectMetricCountForUrl(
      url_a, "CumulativeShiftScoreAfterBackForwardCacheRestore", 2);
  ExpectMetricCountForUrl(url_a,
                          "MaxCumulativeShiftScoreAfterBackForwardCacheRestore."
                          "SessionWindow.Gap1000ms.Max5000ms",
                          2);
  histogram_tester().ExpectTotalCount(
      "PageLoad.LayoutInstability.MaxCumulativeShiftScore."
      "AfterBackForwardCacheRestore.SessionWindow.Gap1000ms.Max5000ms2",
      2);
}

// Verifies that the app resumes HistoryNavigation logging for a page if the
// page restores from the bf-cache after the app has backgrounded and
// re-foregrounded.
IN_PROC_BROWSER_TEST_F(
    BackForwardCachePageLoadMetricsObserverBrowserTest,
    ResumesLoggingAfterRestoringFromCacheAfterBackgrounding) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));

  // Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // Navigate to A again, using history navigation.
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  // Navigate to B again, using history navigation.
  web_contents()->GetController().GoForward();
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  std::vector<PageEndReason> expected_reasons_a;
  expected_reasons_a.push_back(page_load_metrics::END_FORWARD_BACK);
  VerifyHistoryNavPageEndReasons(expected_reasons_a, url_a);

  // No page end expected for url_b.
  ExpectMetricCountForUrl(
      url_b, UkmEntry::kPageEndReasonAfterBackForwardCacheRestoreName, 0);

  // Simulate an app background. This is a bit fake but the best we can do in a
  // browsertest.
  auto* observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents());
  observer->FlushMetricsOnAppEnterBackground();

  // B's observer should have logged a page end reason.
  std::vector<PageEndReason> expected_reasons_b;
  expected_reasons_b.push_back(page_load_metrics::END_APP_ENTER_BACKGROUND);
  VerifyHistoryNavPageEndReasons(expected_reasons_b, url_b);

  // Go back to A, restoring it from the back-forward cache.
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  // Nothing new should have been logged for url_b - its page end happened
  // when the backgrounding did.
  VerifyHistoryNavPageEndReasons(expected_reasons_b, url_b);

  // Navigate to B again - this should trigger the
  // BackForwardCachePageLoadMetricsObserver for A.
  web_contents()->GetController().GoForward();
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  expected_reasons_a.push_back(page_load_metrics::END_FORWARD_BACK);
  VerifyHistoryNavPageEndReasons(expected_reasons_a, url_a);

  // Go back to A, restoring it from the back-forward cache (again)
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  expected_reasons_b.push_back(page_load_metrics::END_FORWARD_BACK);
  VerifyHistoryNavPageEndReasons(expected_reasons_b, url_b);
}

// TODO(crbug.com/40937315): Test is flaky on MSAN.
// TODO(https://crbug.com/40799125): Test is flaky on Windows and Mac.
#if defined(MEMORY_SANITIZER) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_ResponsivenessMetricsNormalizationWithSendingAllLatencies \
  DISABLED_ResponsivenessMetricsNormalizationWithSendingAllLatencies
#else
#define MAYBE_ResponsivenessMetricsNormalizationWithSendingAllLatencies \
  ResponsivenessMetricsNormalizationWithSendingAllLatencies
#endif
IN_PROC_BROWSER_TEST_F(
    BackForwardCachePageLoadMetricsObserverBrowserTest,
    MAYBE_ResponsivenessMetricsNormalizationWithSendingAllLatencies) {
  Start();
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  // Navigate to A.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHostWrapper rfh_a(top_frame_host());

  // Navigate to B.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Go back to A.
  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kFirstInputDelayAfterBackForwardCacheRestore);

    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
    EXPECT_NE(rfh_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);

    // Simulate clicks.
    content::SimulateMouseClick(web_contents(), 0,
                                blink::WebPointerProperties::Button::kLeft);
    content::SimulateMouseClick(web_contents(), 0,
                                blink::WebPointerProperties::Button::kLeft);
    content::SimulateMouseClick(web_contents(), 0,
                                blink::WebPointerProperties::Button::kLeft);

    waiter->Wait();
  }

  // Navigate to B again.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // Go back to A again.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  std::vector<std::string> ukm_list = {
      "WorstUserInteractionLatencyAfterBackForwardCacheRestore."
      "MaxEventDuration2",
      "UserInteractionLatencyAfterBackForwardCacheRestore."
      "HighPercentile2.MaxEventDuration",
      "NumInteractionsAfterBackForwardCacheRestore"};

  for (auto& ukm : ukm_list) {
    SCOPED_TRACE(ukm);
    ExpectMetricCountForUrl(url_a, ukm.c_str(), 1);
    ExpectMetricCountForUrl(url_b, ukm.c_str(), 0);
  }

  std::vector<std::string> uma_list = {
      internal::
          kUserInteractionLatencyHighPercentile2_MaxEventDuration_AfterBackForwardCacheRestore,
      internal::
          kWorstUserInteractionLatency_MaxEventDuration_AfterBackForwardCacheRestore};

  for (auto& uma : uma_list) {
    histogram_tester().ExpectTotalCount(uma, 1);
  }
}
