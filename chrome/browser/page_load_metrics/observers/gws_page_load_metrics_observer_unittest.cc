// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/gws_page_load_metrics_observer.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace {

constexpr char kGoogleSearchResultsUrl[] = "https://www.google.com/search?q=d";

}  // namespace

class GWSPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  // page_load_metrics::PageLoadMetricsObserverTestHarness:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    auto observer = std::make_unique<GWSPageLoadMetricsObserver>();
    observer_ = observer.get();
    tracker->AddObserver(std::move(observer));
  }

  void SimulateTimingWithoutPaint() {
    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::FromDoubleT(1);
    tester()->SimulateTimingUpdate(timing);
  }

  void SimulateTimingWithFirstPaint() {
    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.parse_timing->parse_start = base::Milliseconds(0);
    timing.navigation_start = base::Time::FromDoubleT(1);
    timing.paint_timing->first_paint = base::Milliseconds(0);
    PopulateRequiredTimingFields(&timing);
    tester()->SimulateTimingUpdate(timing);
  }

 protected:
  raw_ptr<GWSPageLoadMetricsObserver> observer_ = nullptr;
};

class GWSPageLoadMetricsLoggerTest : public testing::Test {};

TEST_F(GWSPageLoadMetricsObserverTest, Search) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(10);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(100);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSParseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSParseStart, 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSFirstContentfulPaint, 10, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSLargestContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSLargestContentfulPaint, 100, 1);
}

TEST_F(GWSPageLoadMetricsObserverTest, NonSearch) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(10);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(100);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.com/foo&q=test"));

  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSParseStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSFirstContentfulPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSLargestContentfulPaint, 0);
}

TEST_F(GWSPageLoadMetricsObserverTest, SearchBackground) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.parse_timing->parse_start = base::Seconds(60);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_contentful_paint = base::Seconds(60);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Seconds(60);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  web_contents()->WasHidden();
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSParseStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSFirstContentfulPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSLargestContentfulPaint, 0);
}

TEST_F(GWSPageLoadMetricsObserverTest, SearchBackgroundLater) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.parse_timing->parse_start = base::Microseconds(1);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_contentful_paint = base::Microseconds(1);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Microseconds(1);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  // Sleep to make sure the backgrounded time is > than the paint time, even
  // for low resolution timers.
  base::PlatformThread::Sleep(base::Milliseconds(50));
  web_contents()->WasHidden();
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSParseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSParseStart, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSFirstContentfulPaint, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSLargestContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSLargestContentfulPaint, 0, 1);
}
