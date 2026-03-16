// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/top_chrome_webui_metrics_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"

class TopChromeWebUIMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<TopChromeWebUIMetricsObserver>("TestWebUI"));
  }

  bool IsNonTabWebUI() const override { return true; }
};

TEST_F(TopChromeWebUIMetricsObserverTest, RecordsMetrics) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.paint_timing->first_paint = base::Milliseconds(5);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(10);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("chrome://version"));

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging FCP.
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueTimeSample(
      "TopChromeUI.TestWebUI.RequestToFirstContentfulPaint",
      base::Milliseconds(10), 1);
}

TEST_F(TopChromeWebUIMetricsObserverTest, StaticHelperRecordsMetrics) {
  base::HistogramTester histogram_tester;
  TopChromeWebUIMetricsObserver::RecordFirstContentfulPaint(
      "TestNativeUI", base::Milliseconds(15));

  histogram_tester.ExpectUniqueTimeSample(
      "TopChromeUI.TestNativeUI.RequestToFirstContentfulPaint",
      base::Milliseconds(15), 1);
}
