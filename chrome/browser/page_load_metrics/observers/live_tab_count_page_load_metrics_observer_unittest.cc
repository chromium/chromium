// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/live_tab_count_page_load_metrics_observer.h"

#include <array>
#include <memory>

#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/observers/histogram_suffixes.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/tab_count_metrics/tab_count_metrics.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

const char kDefaultTestUrl[] = "https://google.com";

enum TabState { kForeground, kBackground };

using BucketCountArray =
    std::array<size_t, tab_count_metrics::kNumTabCountBuckets>;

class TestLiveTabCountPageLoadMetricsObserver
    : public LiveTabCountPageLoadMetricsObserver {
 public:
  explicit TestLiveTabCountPageLoadMetricsObserver(size_t live_tab_count)
      : live_tab_count_(live_tab_count) {}
  ~TestLiveTabCountPageLoadMetricsObserver() override {}

 private:
  size_t GetLiveTabCount() const override { return live_tab_count_; }

  const int live_tab_count_;
};

class LiveTabCountPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness,
      public testing::WithParamInterface<TabState> {
 public:
  LiveTabCountPageLoadMetricsObserverTest() : live_tab_count_(0) {}

  void SimulatePageLoad(int live_tab_count, TabState tab_state) {
    live_tab_count_ = live_tab_count;

    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::FromDoubleT(1);
    timing.paint_timing->first_contentful_paint =
        base::TimeDelta::FromMilliseconds(300);
    timing.paint_timing->first_meaningful_paint =
        base::TimeDelta::FromMilliseconds(700);
    timing.interactive_timing->first_input_delay =
        base::TimeDelta::FromMilliseconds(5);
    timing.interactive_timing->first_input_timestamp =
        base::TimeDelta::FromMilliseconds(4780);
    PopulateRequiredTimingFields(&timing);

    if (tab_state == kBackground) {
      // Start in background.
      web_contents()->WasHidden();
    }

    NavigateAndCommit(GURL(kDefaultTestUrl));

    if (tab_state == kBackground) {
      // Foreground the tab.
      web_contents()->WasShown();
    }

    tester()->SimulateTimingUpdate(timing);
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<TestLiveTabCountPageLoadMetricsObserver>(
            live_tab_count_));
  }

  void ValidateHistograms(const char* page_load_histogram_suffix,
                          BucketCountArray& expected_counts) {
    const std::string histogram_prefix =
        std::string(internal::kHistogramPrefixLiveTabCount) +
        std::string(page_load_histogram_suffix);
    for (size_t bucket = 0; bucket < expected_counts.size(); bucket++) {
      tester()->histogram_tester().ExpectTotalCount(
          tab_count_metrics::HistogramName(histogram_prefix,
                                           /* live_tabs_only = */ true, bucket),
          expected_counts[bucket]);
    }
  }

 private:
  size_t live_tab_count_;
};

TEST_P(LiveTabCountPageLoadMetricsObserverTest, LoadTabs100) {
  TabState tab_state = GetParam();
  BucketCountArray counts = {0};

  size_t bucket;
  constexpr size_t kMaxLiveTabCount = 100;
  // Simulate loading pages with the number of live tabs ranging from 0 to
  // kMaxLiveTabCount.
  for (size_t num_tabs = 0; num_tabs <= kMaxLiveTabCount; num_tabs++) {
    bucket = tab_count_metrics::BucketForTabCount(num_tabs);
    SimulatePageLoad(num_tabs, tab_state);
    // We only record metrics if the tab was foregrounded the entire time
    // preceding the event that caused the metrics to be recorded.
    if (tab_state == TabState::kForeground)
      ++counts[bucket];
    ValidateHistograms(internal::kHistogramFirstContentfulPaintSuffix, counts);
    ValidateHistograms(internal::kHistogramFirstMeaningfulPaintSuffix, counts);
    ValidateHistograms(internal::kHistogramFirstInputDelaySuffix, counts);
  }
  // Make sure we are testing each bucket.
  EXPECT_EQ(bucket, tab_count_metrics::kNumTabCountBuckets - 1);
}

INSTANTIATE_TEST_SUITE_P(Foreground,
                         LiveTabCountPageLoadMetricsObserverTest,
                         testing::Values(TabState::kForeground));

INSTANTIATE_TEST_SUITE_P(Background,
                         LiveTabCountPageLoadMetricsObserverTest,
                         testing::Values(TabState::kBackground));
