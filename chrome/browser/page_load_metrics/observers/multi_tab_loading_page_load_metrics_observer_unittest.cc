// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/multi_tab_loading_page_load_metrics_observer.h"

#include <memory>

#include "base/optional.h"
#include "chrome/browser/page_load_metrics/observers/histogram_suffixes.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"

namespace {

const char kDefaultTestUrl[] = "https://google.com";

class TestMultiTabLoadingPageLoadMetricsObserver
    : public MultiTabLoadingPageLoadMetricsObserver {
 public:
  explicit TestMultiTabLoadingPageLoadMetricsObserver(
      int number_of_tabs_with_inflight_load)
      : number_of_tabs_with_inflight_load_(number_of_tabs_with_inflight_load) {}
  ~TestMultiTabLoadingPageLoadMetricsObserver() override {}

 private:
  int NumberOfTabsWithInflightLoad(
      content::NavigationHandle* navigation_handle) override {
    return number_of_tabs_with_inflight_load_;
  }

  const int number_of_tabs_with_inflight_load_;
};

}  // namespace

class MultiTabLoadingPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  enum TabState { Foreground, Background };

  void SimulatePageLoad(int number_of_tabs_with_inflight_load,
                        TabState tab_state) {
    number_of_tabs_with_inflight_load_ = number_of_tabs_with_inflight_load;

    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::FromDoubleT(1);
    timing.paint_timing->first_contentful_paint =
        base::TimeDelta::FromMilliseconds(300);
    timing.paint_timing->first_meaningful_paint =
        base::TimeDelta::FromMilliseconds(700);
    timing.document_timing->dom_content_loaded_event_start =
        base::TimeDelta::FromMilliseconds(600);
    timing.document_timing->load_event_start =
        base::TimeDelta::FromMilliseconds(1000);
    PopulateRequiredTimingFields(&timing);

    if (tab_state == Background) {
      // Start in background.
      web_contents()->WasHidden();
    }

    NavigateAndCommit(GURL(kDefaultTestUrl));

    if (tab_state == Background) {
      // Foreground the tab.
      web_contents()->WasShown();
    }

    tester()->SimulateTimingUpdate(timing);
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<TestMultiTabLoadingPageLoadMetricsObserver>(
            number_of_tabs_with_inflight_load_.value()));
  }

  void ValidateHistograms(const char* suffix,
                          base::HistogramBase::Count expected_base,
                          base::HistogramBase::Count expected_2_or_more,
                          base::HistogramBase::Count expected_5_or_more) {
    tester()->histogram_tester().ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading).append(suffix),
        expected_base);
    tester()->histogram_tester().ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading2OrMore)
            .append(suffix),
        expected_2_or_more);
    tester()->histogram_tester().ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading5OrMore)
            .append(suffix),
        expected_5_or_more);
  }

 private:
  base::Optional<int> number_of_tabs_with_inflight_load_;
};

TEST_F(MultiTabLoadingPageLoadMetricsObserverTest, SingleTabLoading) {
  SimulatePageLoad(0, Foreground);

  ValidateHistograms(internal::kHistogramFirstContentfulPaintSuffix, 0, 0, 0);
  ValidateHistograms(internal::kHistogramForegroundToFirstContentfulPaintSuffix,
                     0, 0, 0);
  ValidateHistograms(internal::kHistogramFirstMeaningfulPaintSuffix, 0, 0, 0);
  ValidateHistograms(internal::kHistogramForegroundToFirstMeaningfulPaintSuffix,
                     0, 0, 0);
  ValidateHistograms(internal::kHistogramDOMContentLoadedEventFiredSuffix, 0, 0,
                     0);
  ValidateHistograms(
      internal::kHistogramDOMContentLoadedEventFiredBackgroundSuffix, 0, 0, 0);
  ValidateHistograms(internal::kHistogramLoadEventFiredSuffix, 0, 0, 0);
  ValidateHistograms(internal::kHistogramLoadEventFiredBackgroundSuffix, 0, 0,
                     0);
}

TEST_F(MultiTabLoadingPageLoadMetricsObserverTest, MultiTabLoading1) {
  SimulatePageLoad(1, Foreground);

  ValidateHistograms(internal::kHistogramFirstContentfulPaintSuffix, 1, 0, 0);
  ValidateHistograms(internal::kHistogramForegroundToFirstContentfulPaintSuffix,
                     0, 0, 0);
  ValidateHistograms(internal::kHistogramFirstMeaningfulPaintSuffix, 1, 0, 0);
  ValidateHistograms(internal::kHistogramForegroundToFirstMeaningfulPaintSuffix,
                     0, 0, 0);
  ValidateHistograms(internal::kHistogramDOMContentLoadedEventFiredSuffix, 1, 0,
                     0);
  ValidateHistograms(
      internal::kHistogramDOMContentLoadedEventFiredBackgroundSuffix, 0, 0, 0);
  ValidateHistograms(internal::kHistogramLoadEventFiredSuffix, 1, 0, 0);
  ValidateHistograms(internal::kHistogramLoadEventFiredBackgroundSuffix, 0, 0,
                     0);
}

TEST_F(MultiTabLoadingPageLoadMetricsObserverTest, MultiTabLoading2) {
  SimulatePageLoad(2, Foreground);

  ValidateHistograms(internal::kHistogramFirstContentfulPaintSuffix, 1, 1, 0);
  ValidateHistograms(internal::kHistogramForegroundToFirstContentfulPaintSuffix,
                     0, 0, 0);
  ValidateHistograms(internal::kHistogramFirstMeaningfulPaintSuffix, 1, 1, 0);
  ValidateHistograms(internal::kHistogramForegroundToFirstMeaningfulPaintSuffix,
                     0, 0, 0);
  ValidateHistograms(internal::kHistogramDOMContentLoadedEventFiredSuffix, 1, 1,
                     0);
  ValidateHistograms(
      internal::kHistogramDOMContentLoadedEventFiredBackgroundSuffix, 0, 0, 0);
  ValidateHistograms(internal::kHistogramLoadEventFiredSuffix, 1, 1, 0);
  ValidateHistograms(internal::kHistogramLoadEventFiredBackgroundSuffix, 0, 0,
                     0);
}

TEST_F(MultiTabLoadingPageLoadMetricsObserverTest, MultiTabLoading5) {
  SimulatePageLoad(5, Foreground);

  ValidateHistograms(internal::kHistogramFirstContentfulPaintSuffix, 1, 1, 1);
  ValidateHistograms(internal::kHistogramForegroundToFirstContentfulPaintSuffix,
                     0, 0, 0);
  ValidateHistograms(internal::kHistogramFirstMeaningfulPaintSuffix, 1, 1, 1);
  ValidateHistograms(internal::kHistogramForegroundToFirstMeaningfulPaintSuffix,
                     0, 0, 0);
  ValidateHistograms(internal::kHistogramDOMContentLoadedEventFiredSuffix, 1, 1,
                     1);
  ValidateHistograms(
      internal::kHistogramDOMContentLoadedEventFiredBackgroundSuffix, 0, 0, 0);
  ValidateHistograms(internal::kHistogramLoadEventFiredSuffix, 1, 1, 1);
  ValidateHistograms(internal::kHistogramLoadEventFiredBackgroundSuffix, 0, 0,
                     0);
}

TEST_F(MultiTabLoadingPageLoadMetricsObserverTest, MultiTabBackground) {
  SimulatePageLoad(1, Background);

  ValidateHistograms(internal::kHistogramFirstContentfulPaintSuffix, 0, 0, 0);
  ValidateHistograms(internal::kHistogramForegroundToFirstContentfulPaintSuffix,
                     1, 0, 0);
  ValidateHistograms(internal::kHistogramFirstMeaningfulPaintSuffix, 0, 0, 0);
  ValidateHistograms(internal::kHistogramForegroundToFirstMeaningfulPaintSuffix,
                     1, 0, 0);
  ValidateHistograms(internal::kHistogramDOMContentLoadedEventFiredSuffix, 0, 0,
                     0);
  ValidateHistograms(
      internal::kHistogramDOMContentLoadedEventFiredBackgroundSuffix, 1, 0, 0);
  ValidateHistograms(internal::kHistogramLoadEventFiredSuffix, 0, 0, 0);
  ValidateHistograms(internal::kHistogramLoadEventFiredBackgroundSuffix, 1, 0,
                     0);
}
