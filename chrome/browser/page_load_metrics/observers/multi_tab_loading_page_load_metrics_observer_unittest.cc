// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/multi_tab_loading_page_load_metrics_observer.h"

#include <memory>

#include "chrome/browser/page_load_metrics/observers/histogram_suffixes.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

const char kDefaultTestUrl[] = "https://google.com";

class TestMultiTabLoadingPageLoadMetricsObserver
    : public MultiTabLoadingPageLoadMetricsObserver {
 public:
  explicit TestMultiTabLoadingPageLoadMetricsObserver(
      int number_of_tabs_with_inflight_load)
      : number_of_tabs_with_inflight_load_(number_of_tabs_with_inflight_load) {}
  ~TestMultiTabLoadingPageLoadMetricsObserver() override = default;

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
    timing.parse_timing->parse_start = base::Milliseconds(300);
    timing.paint_timing->first_contentful_paint = base::Milliseconds(300);
    timing.paint_timing->first_meaningful_paint = base::Milliseconds(700);
    timing.paint_timing->largest_contentful_paint->largest_text_paint =
        base::Milliseconds(800);
    timing.paint_timing->largest_contentful_paint->largest_text_paint_size =
        100u;
    timing.document_timing->dom_content_loaded_event_start =
        base::Milliseconds(600);
    timing.document_timing->load_event_start = base::Milliseconds(1000);
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

    // Navigate to about:blank to force histogram recording.
    NavigateAndCommit(GURL("about:blank"));
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<TestMultiTabLoadingPageLoadMetricsObserver>(
            number_of_tabs_with_inflight_load_.value()));
  }

  void ValidateHistograms(const char* suffix,
                          base::HistogramBase::Count expected_1_or_more,
                          base::HistogramBase::Count expected_2_or_more,
                          base::HistogramBase::Count expected_5_or_more,
                          base::HistogramBase::Count expected_0,
                          base::HistogramBase::Count expected_1,
                          base::HistogramBase::Count expected_2,
                          base::HistogramBase::Count expected_3,
                          base::HistogramBase::Count expected_4,
                          base::HistogramBase::Count expected_5) {
    tester()->histogram_tester().ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading).append(suffix),
        expected_1_or_more);
    tester()->histogram_tester().ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading1OrMore)
            .append(suffix),
        expected_1_or_more);
    tester()->histogram_tester().ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading2OrMore)
            .append(suffix),
        expected_2_or_more);
    tester()->histogram_tester().ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading5OrMore)
            .append(suffix),
        expected_5_or_more);
    tester()->histogram_tester().ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading0).append(suffix),
        expected_0);
    tester()->histogram_tester().ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading1).append(suffix),
        expected_1);
    tester()->histogram_tester().ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading2).append(suffix),
        expected_2);
    tester()->histogram_tester().ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading3).append(suffix),
        expected_3);
    tester()->histogram_tester().ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading4).append(suffix),
        expected_4);
    tester()->histogram_tester().ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading5).append(suffix),
        expected_5);
  }

 private:
  absl::optional<int> number_of_tabs_with_inflight_load_;
};

TEST_F(MultiTabLoadingPageLoadMetricsObserverTest, SingleTabLoading) {
  SimulatePageLoad(0, Foreground);

  ValidateHistograms(internal::kHistogramFirstContentfulPaintSuffix,
                     /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/1,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramLargestContentfulPaintSuffix,
                     /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/1,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramForegroundToFirstContentfulPaintSuffix,
                     /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramFirstMeaningfulPaintSuffix,
                     /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/1,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramDOMContentLoadedEventFiredSuffix,
                     /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/1,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(
      internal::kHistogramDOMContentLoadedEventFiredBackgroundSuffix,
      /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
      /*expected_5_or_more=*/0, /*expected_0=*/0, /*expected_1=*/0,
      /*expected_2=*/0, /*expected_3=*/0, /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramLoadEventFiredSuffix,
                     /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/1,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramLoadEventFiredBackgroundSuffix,
                     /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
}

TEST_F(MultiTabLoadingPageLoadMetricsObserverTest, MultiTabLoading1) {
  SimulatePageLoad(1, Foreground);

  ValidateHistograms(internal::kHistogramFirstContentfulPaintSuffix,
                     /*expected_1_or_more=*/1, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/1, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramLargestContentfulPaintSuffix,
                     /*expected_1_or_more=*/1, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/1, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramForegroundToFirstContentfulPaintSuffix,
                     /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramFirstMeaningfulPaintSuffix,
                     /*expected_1_or_more=*/1, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/1, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramDOMContentLoadedEventFiredSuffix,
                     /*expected_1_or_more=*/1, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/1, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(
      internal::kHistogramDOMContentLoadedEventFiredBackgroundSuffix,
      /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
      /*expected_5_or_more=*/0, /*expected_0=*/0, /*expected_1=*/0,
      /*expected_2=*/0, /*expected_3=*/0, /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramLoadEventFiredSuffix,
                     /*expected_1_or_more=*/1, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/1, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramLoadEventFiredBackgroundSuffix,
                     /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
}

TEST_F(MultiTabLoadingPageLoadMetricsObserverTest, MultiTabLoading2) {
  SimulatePageLoad(2, Foreground);

  ValidateHistograms(internal::kHistogramFirstContentfulPaintSuffix,
                     /*expected_1_or_more=*/1, /*expected_2_or_more=*/1,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/1, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramLargestContentfulPaintSuffix,
                     /*expected_1_or_more=*/1, /*expected_2_or_more=*/1,
                     /*expected_5_or_more=*/0,
                     /*expected_0=*/0, /*expected_1=*/0, /*expected_2=*/1,
                     /*expected_3=*/0, /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramForegroundToFirstContentfulPaintSuffix,
                     /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramFirstMeaningfulPaintSuffix,
                     /*expected_1_or_more=*/1, /*expected_2_or_more=*/1,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/1, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramDOMContentLoadedEventFiredSuffix,
                     /*expected_1_or_more=*/1, /*expected_2_or_more=*/1,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/1, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(
      internal::kHistogramDOMContentLoadedEventFiredBackgroundSuffix,
      /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
      /*expected_5_or_more=*/0, /*expected_0=*/0, /*expected_1=*/0,
      /*expected_2=*/0, /*expected_3=*/0, /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramLoadEventFiredSuffix,
                     /*expected_1_or_more=*/1, /*expected_2_or_more=*/1,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/1, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramLoadEventFiredBackgroundSuffix,
                     /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
}

TEST_F(MultiTabLoadingPageLoadMetricsObserverTest, MultiTabLoading5) {
  SimulatePageLoad(5, Foreground);

  ValidateHistograms(internal::kHistogramFirstContentfulPaintSuffix,
                     /*expected_1_or_more=*/1, /*expected_2_or_more=*/1,
                     /*expected_5_or_more=*/1, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/1);
  ValidateHistograms(internal::kHistogramLargestContentfulPaintSuffix,
                     /*expected_1_or_more=*/1, /*expected_2_or_more=*/1,
                     /*expected_5_or_more=*/1,
                     /*expected_0=*/0, /*expected_1=*/0, /*expected_2=*/0,
                     /*expected_3=*/0, /*expected_4=*/0, /*expected_5=*/1);
  ValidateHistograms(internal::kHistogramForegroundToFirstContentfulPaintSuffix,
                     /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramFirstMeaningfulPaintSuffix,
                     /*expected_1_or_more=*/1, /*expected_2_or_more=*/1,
                     /*expected_5_or_more=*/1, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/1);
  ValidateHistograms(internal::kHistogramDOMContentLoadedEventFiredSuffix,
                     /*expected_1_or_more=*/1, /*expected_2_or_more=*/1,
                     /*expected_5_or_more=*/1, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/1);
  ValidateHistograms(
      internal::kHistogramDOMContentLoadedEventFiredBackgroundSuffix,
      /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
      /*expected_5_or_more=*/0, /*expected_0=*/0, /*expected_1=*/0,
      /*expected_2=*/0, /*expected_3=*/0, /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramLoadEventFiredSuffix,
                     /*expected_1_or_more=*/1, /*expected_2_or_more=*/1,
                     /*expected_5_or_more=*/1, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/1);
  ValidateHistograms(internal::kHistogramLoadEventFiredBackgroundSuffix,
                     /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
}

TEST_F(MultiTabLoadingPageLoadMetricsObserverTest, MultiTabBackground) {
  SimulatePageLoad(1, Background);

  ValidateHistograms(internal::kHistogramFirstContentfulPaintSuffix,
                     /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramLargestContentfulPaintSuffix,
                     /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramForegroundToFirstContentfulPaintSuffix,
                     /*expected_1_or_more=*/1, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/1, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramFirstMeaningfulPaintSuffix,
                     /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramDOMContentLoadedEventFiredSuffix,
                     /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(
      internal::kHistogramDOMContentLoadedEventFiredBackgroundSuffix,
      /*expected_1_or_more=*/1, /*expected_2_or_more=*/0,
      /*expected_5_or_more=*/0, /*expected_0=*/0, /*expected_1=*/1,
      /*expected_2=*/0, /*expected_3=*/0, /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramLoadEventFiredSuffix,
                     /*expected_1_or_more=*/0, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/0, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
  ValidateHistograms(internal::kHistogramLoadEventFiredBackgroundSuffix,
                     /*expected_1_or_more=*/1, /*expected_2_or_more=*/0,
                     /*expected_5_or_more=*/0, /*expected_0=*/0,
                     /*expected_1=*/1, /*expected_2=*/0, /*expected_3=*/0,
                     /*expected_4=*/0, /*expected_5=*/0);
}
