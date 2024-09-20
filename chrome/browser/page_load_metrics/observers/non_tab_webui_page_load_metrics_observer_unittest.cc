// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/non_tab_webui_page_load_metrics_observer.h"

#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"

class NonTabPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<NonTabPageLoadMetricsObserver>("Test"));
  }

  bool IsNonTabWebUI() const override { return true; }
};

TEST_F(NonTabPageLoadMetricsObserverTest, RecordsHistogramsIfEmbedderIsWebUI) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(10);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(100);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("chrome://version"));

  tester()->SimulateTimingUpdate(timing);
  tester()->histogram_tester().ExpectUniqueTimeSample(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.NonTabWebUI."
      "Test",
      base::Milliseconds(10), 1);
  tester()->histogram_tester().ExpectUniqueTimeSample(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.NonTabWebUI",
      base::Milliseconds(10), 1);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectUniqueTimeSample(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.NonTabWebUI."
      "Test",
      base::Milliseconds(100), 1);
  tester()->histogram_tester().ExpectUniqueTimeSample(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.NonTabWebUI",
      base::Milliseconds(100), 1);
  // The regular LCP histogram shouldn't be logged from this path.
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContenfulPaint2", 0);
}

TEST_F(NonTabPageLoadMetricsObserverTest,
       DoesntRecordHistogramsIfEmbedderIsWebUIAndNonChromeScheme) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(10);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(100);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("data:text/html,Hello world"));

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.NonTabWebUI."
      "Test",
      0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.NonTabWebUI",
      0);
  // The regular LCP histogram shouldn't be logged from this path.
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContenfulPaint2", 0);
}

class NonTabPageLoadMetricsObserverNonWebUITest
    : public NonTabPageLoadMetricsObserverTest {
 protected:
  bool IsNonTabWebUI() const override { return false; }
};

TEST_F(NonTabPageLoadMetricsObserverNonWebUITest,
       DoesntRecordHistogramsIfEmbedderIsNotWebUI) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(10);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(100);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("chrome://version"));

  tester()->SimulateTimingUpdate(timing);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.NonTabWebUI."
      "Test",
      0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.NonTabWebUI", 0);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.NonTabWebUI."
      "Test",
      0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.NonTabWebUI",
      0);
  // The regular LCP histogram shouldn't be logged from this path.
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContenfulPaint2", 0);
}
