// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/side_search_page_load_metrics_observer.h"

#include <memory>

#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/ui/side_search/side_search_side_contents_helper.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/test/test_utils.h"

namespace {

constexpr char kExampleUrl[] = "https://www.example.com";
constexpr char kExampleUrl2[] = "https://www.example2.com";

}  // namespace

class SideSearchPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  // page_load_metrics::PageLoadMetricsObserverTestHarness:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<SideSearchPageLoadMetricsObserver>());
  }
};

TEST_F(SideSearchPageLoadMetricsObserverTest, PageLoadMetricsNonBackgrounded) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(10);
  timing.paint_timing->first_paint = base::Milliseconds(20);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(30);
  timing.paint_timing->first_meaningful_paint = base::Milliseconds(40);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(50);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  timing.interactive_timing->first_input_delay = base::Milliseconds(50);
  timing.interactive_timing->first_input_timestamp = base::Milliseconds(1400);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL(kExampleUrl));
  tester()->SimulateTimingUpdate(timing);

  page_load_metrics::mojom::FrameRenderDataUpdate render_data(1.0, 1.0, {});
  tester()->SimulateRenderDataUpdate(render_data);
  render_data.layout_shift_delta = 1.5;
  render_data.layout_shift_delta_before_input_or_scroll = 0.0;
  render_data.new_layout_shifts.emplace_back(
      page_load_metrics::mojom::LayoutShift::New(base::TimeTicks::Now(), 0.5));
  tester()->SimulateRenderDataUpdate(render_data);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(
      internal::kSideSearchFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectUniqueSample(
      internal::kSideSearchFirstMeaningfulPaint,
      timing.paint_timing->first_meaningful_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectUniqueSample(
      internal::kSideSearchLargestContentfulPaint,
      timing.paint_timing->largest_contentful_paint->largest_text_paint.value()
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectUniqueSample(
      internal::kSideSearchInteractiveInputDelay,
      timing.interactive_timing->first_input_delay.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectUniqueSample(
      internal::kSideSearchMaxCumulativeShiftScore, 5000, 1);
}

TEST_F(SideSearchPageLoadMetricsObserverTest,
       HiddenContentsDoesNotEmitMetrics) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(1);

  page_load_metrics::mojom::PageLoadTiming timing2;
  page_load_metrics::InitPageLoadTimingForTest(&timing2);
  timing2.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  timing2.parse_timing->parse_start = base::Milliseconds(100);
  timing2.paint_timing->first_contentful_paint = base::Milliseconds(100);

  page_load_metrics::mojom::PageLoadTiming timing3;
  page_load_metrics::InitPageLoadTimingForTest(&timing3);
  timing3.navigation_start = base::Time::FromSecondsSinceUnixEpoch(3);
  timing3.parse_timing->parse_start = base::Milliseconds(1000);
  timing3.paint_timing->first_contentful_paint = base::Milliseconds(1000);

  PopulateRequiredTimingFields(&timing);
  PopulateRequiredTimingFields(&timing2);
  PopulateRequiredTimingFields(&timing3);

  NavigateAndCommit(GURL(kExampleUrl));
  tester()->SimulateTimingUpdate(timing);

  NavigateAndCommit(GURL(kExampleUrl2));
  web_contents()->WasHidden();
  tester()->SimulateTimingUpdate(timing2);

  NavigateAndCommit(GURL(kExampleUrl));
  tester()->SimulateTimingUpdate(timing3);

  // Navigate again to force logging. We expect to log timing for the first page
  // but not the second or third since the web contents was backgrounded.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kSideSearchFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);
}

TEST_F(SideSearchPageLoadMetricsObserverTest,
       MetricsEmittedCorrectlyWhenAppBackgrounded) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(10);
  timing.paint_timing->first_paint = base::Milliseconds(20);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(30);
  timing.paint_timing->first_meaningful_paint = base::Milliseconds(40);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(50);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  timing.interactive_timing->first_input_delay = base::Milliseconds(50);
  timing.interactive_timing->first_input_timestamp = base::Milliseconds(1400);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL(kExampleUrl));
  tester()->SimulateTimingUpdate(timing);

  page_load_metrics::mojom::FrameRenderDataUpdate render_data(1.0, 1.0, {});
  tester()->SimulateRenderDataUpdate(render_data);
  render_data.layout_shift_delta = 1.5;
  render_data.layout_shift_delta_before_input_or_scroll = 0.0;
  render_data.new_layout_shifts.emplace_back(
      page_load_metrics::mojom::LayoutShift::New(base::TimeTicks::Now(), 0.5));
  tester()->SimulateRenderDataUpdate(render_data);

  // Metrics that track events immediately following a page load should have
  // been emitted.
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kSideSearchFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kSideSearchFirstMeaningfulPaint,
      timing.paint_timing->first_meaningful_paint.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kSideSearchInteractiveInputDelay,
      timing.interactive_timing->first_input_delay.value().InMilliseconds(), 1);

  // Session end metrics should not have been emitted.
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kSideSearchLargestContentfulPaint,
      timing.paint_timing->largest_contentful_paint->largest_text_paint.value()
          .InMilliseconds(),
      0);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kSideSearchMaxCumulativeShiftScore, 5000, 0);

  // Simulate entering the background state, this should cause the observer to
  // emit session end metrics.
  tester()->SimulateAppEnterBackground();

  tester()->histogram_tester().ExpectUniqueSample(
      internal::kSideSearchFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kSideSearchFirstMeaningfulPaint,
      timing.paint_timing->first_meaningful_paint.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kSideSearchInteractiveInputDelay,
      timing.interactive_timing->first_input_delay.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kSideSearchLargestContentfulPaint,
      timing.paint_timing->largest_contentful_paint->largest_text_paint.value()
          .InMilliseconds(),
      1);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kSideSearchMaxCumulativeShiftScore, 5000, 1);
}

TEST_F(SideSearchPageLoadMetricsObserverTest,
       ObserverOnlyCreatedForSidePanelContents) {
  // The SideSearchSideContentsHelper exists only on the side panel WebContents
  // that is created when the feature is enabled. Ensure that we do not create
  // the observer when this helper is missing.
  auto web_contents = CreateTestWebContents();
  EXPECT_EQ(nullptr, SideSearchPageLoadMetricsObserver::CreateIfNeeded(
                         web_contents.get()));

  // If the helper exists this indicates the WebContents is a side search side
  // panel contents and we should create the observer.
  SideSearchSideContentsHelper::CreateForWebContents(web_contents.get());
  EXPECT_NE(nullptr, SideSearchPageLoadMetricsObserver::CreateIfNeeded(
                         web_contents.get()));
}
