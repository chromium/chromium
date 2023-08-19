// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/lcp_critical_path_predictor_page_load_metrics_observer.h"

#include <memory>

#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/predictors/loading_data_collector.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"

#include "third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom.h"

class LcpCriticalPathPredictorPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void SetUp() final {
    page_load_metrics::PageLoadMetricsObserverTestHarness::SetUp();

    page_load_metrics::InitPageLoadTimingForTest(&timing_);
    timing_.navigation_start = base::Time::FromDoubleT(1);
    timing_.parse_timing->parse_start = base::Milliseconds(10);
    timing_.paint_timing->first_paint = base::Seconds(2);
    timing_.paint_timing->first_contentful_paint = base::Seconds(3);
    timing_.paint_timing->first_meaningful_paint = base::Seconds(4);

    timing_.paint_timing->largest_contentful_paint->largest_image_paint =
        base::Seconds(5);
    timing_.paint_timing->largest_contentful_paint->largest_image_paint_size =
        100u;

    PopulateRequiredTimingFields(&timing_);
  }

  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<LcpCriticalPathPredictorPageLoadMetricsObserver>());
  }

  void TestHistogramsRecorded(bool provide_lcpp_hint) {
    const GURL main_frame_url("https://test.example");

    std::unique_ptr<content::NavigationSimulator> navigation =
        content::NavigationSimulator::CreateBrowserInitiated(main_frame_url,
                                                             web_contents());

    navigation->Start();
    if (provide_lcpp_hint) {
      blink::mojom::LCPCriticalPathPredictorNavigationTimeHint hint;
      hint.lcp_element_locators = {"dummy"};
      navigation->GetNavigationHandle()->SetLCPPNavigationHint(hint);
    }
    navigation->Commit();
    tester()->SimulateTimingUpdate(timing_);

    // Navigate to about:blank to force histogram recording.
    NavigateAndCommit(GURL("about:blank"));

    base::Histogram::Count expected_count = provide_lcpp_hint ? 1 : 0;
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramLCPPFirstContentfulPaint, expected_count);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramLCPPLargestContentfulPaint, expected_count);
  }

  page_load_metrics::mojom::PageLoadTiming timing_;
};

TEST_F(LcpCriticalPathPredictorPageLoadMetricsObserverTest,
       MetricsRecordedWhenHintProvided) {
  TestHistogramsRecorded(true);
}

TEST_F(LcpCriticalPathPredictorPageLoadMetricsObserverTest,
       MetricsNotRecordedWithoutHint) {
  TestHistogramsRecorded(false);
}
