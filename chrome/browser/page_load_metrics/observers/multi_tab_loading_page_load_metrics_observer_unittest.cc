// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/multi_tab_loading_page_load_metrics_observer.h"

#include <memory>
#include <optional>

#include "chrome/browser/page_load_metrics/observers/histogram_suffixes.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"

namespace {

const char kDefaultTestUrl[] = "https://google.com";

class TestMultiTabLoadingPageLoadMetricsObserver
    : public MultiTabLoadingPageLoadMetricsObserver {
 public:
  explicit TestMultiTabLoadingPageLoadMetricsObserver(
      int number_of_tabs_with_inflight_load,
      int number_of_tabs)
      : number_of_tabs_with_inflight_load_(number_of_tabs_with_inflight_load),
        number_of_tabs_(number_of_tabs) {}
  ~TestMultiTabLoadingPageLoadMetricsObserver() override = default;

 private:
  int NumberOfTabsWithInflightLoad(
      content::NavigationHandle* navigation_handle) override {
    return number_of_tabs_with_inflight_load_;
  }

  int NumberOfTabs(content::NavigationHandle* navigation_handle) override {
    return number_of_tabs_;
  }

  const int number_of_tabs_with_inflight_load_;
  const int number_of_tabs_;
};

}  // namespace

class MultiTabLoadingPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  enum TabState { Foreground, Background };

  void SimulatePageLoad(int number_of_tabs_with_inflight_load,
                        int number_of_tabs,
                        TabState tab_state) {
    number_of_tabs_with_inflight_load_ = number_of_tabs_with_inflight_load;
    number_of_tabs_ = number_of_tabs;

    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
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
            number_of_tabs_with_inflight_load_.value(),
            number_of_tabs_.value()));
  }

  void ValidateHistograms(const base::Location& location,
                          const char* suffix,
                          base::HistogramBase::Count expected_loading_1_or_more,
                          base::HistogramBase::Count expected_loading_2_or_more,
                          base::HistogramBase::Count expected_loading_5_or_more,
                          base::HistogramBase::Count expected_loading_0,
                          base::HistogramBase::Count expected_loading_1,
                          base::HistogramBase::Count expected_loading_2,
                          base::HistogramBase::Count expected_loading_3,
                          base::HistogramBase::Count expected_loading_4,
                          base::HistogramBase::Count expected_loading_5,
                          base::HistogramBase::Count expected_tab_count_0,
                          base::HistogramBase::Count expected_tab_count_1,
                          base::HistogramBase::Count expected_tab_count_2,
                          base::HistogramBase::Count expected_tab_count_4,
                          base::HistogramBase::Count expected_tab_count_8,
                          base::HistogramBase::Count expected_tab_count_16,
                          base::HistogramBase::Count expected_tab_count_32,
                          base::HistogramBase::Count expected_tab_count_64) {
    const base::HistogramTester& t = tester()->histogram_tester();
    t.ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading).append(suffix),
        expected_loading_1_or_more, location);
    t.ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading1OrMore)
            .append(suffix),
        expected_loading_1_or_more, location);
    t.ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading2OrMore)
            .append(suffix),
        expected_loading_2_or_more, location);
    t.ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading5OrMore)
            .append(suffix),
        expected_loading_5_or_more, location);
    t.ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading0).append(suffix),
        expected_loading_0, location);
    t.ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading1).append(suffix),
        expected_loading_1, location);
    t.ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading2).append(suffix),
        expected_loading_2, location);
    t.ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading3).append(suffix),
        expected_loading_3, location);
    t.ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading4).append(suffix),
        expected_loading_4, location);
    t.ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTabLoading5).append(suffix),
        expected_loading_5, location);
    t.ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTab0).append(suffix),
        expected_tab_count_0, location);
    t.ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTab1).append(suffix),
        expected_tab_count_1, location);
    t.ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTab2).append(suffix),
        expected_tab_count_2, location);
    t.ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTab4).append(suffix),
        expected_tab_count_4, location);
    t.ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTab8).append(suffix),
        expected_tab_count_8, location);
    t.ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTab16).append(suffix),
        expected_tab_count_16, location);
    t.ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTab32).append(suffix),
        expected_tab_count_32, location);
    t.ExpectTotalCount(
        std::string(internal::kHistogramPrefixMultiTab64).append(suffix),
        expected_tab_count_64, location);
  }

 private:
  std::optional<int> number_of_tabs_with_inflight_load_;
  std::optional<int> number_of_tabs_;
};

TEST_F(MultiTabLoadingPageLoadMetricsObserverTest, SingleTabLoading) {
  SimulatePageLoad(/*number_of_tabs_with_inflight_load=*/0,
                   /*number_of_tabs=*/0, Foreground);

  ValidateHistograms(FROM_HERE, internal::kHistogramFirstContentfulPaintSuffix,
                     /*expected_loading_1_or_more=*/0,
                     /*expected_loading_2_or_more=*/0,
                     /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/1,
                     /*expected_loading_1=*/0, /*expected_loading_2=*/0,
                     /*expected_loading_3=*/0, /*expected_loading_4=*/0,
                     /*expected_loading_5=*/0, /*expected_tab_count_0=*/1,
                     /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
                     /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
                     /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
                     /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramLargestContentfulPaintSuffix,
      /*expected_loading_1_or_more=*/0, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/1,
      /*expected_loading_1=*/0, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/1,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramForegroundToFirstContentfulPaintSuffix,
      /*expected_loading_1_or_more=*/0, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/0, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(FROM_HERE, internal::kHistogramFirstMeaningfulPaintSuffix,
                     /*expected_loading_1_or_more=*/0,
                     /*expected_loading_2_or_more=*/0,
                     /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/1,
                     /*expected_loading_1=*/0, /*expected_loading_2=*/0,
                     /*expected_loading_3=*/0, /*expected_loading_4=*/0,
                     /*expected_loading_5=*/0, /*expected_tab_count_0=*/1,
                     /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
                     /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
                     /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
                     /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramDOMContentLoadedEventFiredSuffix,
      /*expected_loading_1_or_more=*/0, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/1,
      /*expected_loading_1=*/0, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/1,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramDOMContentLoadedEventFiredBackgroundSuffix,
      /*expected_loading_1_or_more=*/0, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/0, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(FROM_HERE, internal::kHistogramLoadEventFiredSuffix,
                     /*expected_loading_1_or_more=*/0,
                     /*expected_loading_2_or_more=*/0,
                     /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/1,
                     /*expected_loading_1=*/0, /*expected_loading_2=*/0,
                     /*expected_loading_3=*/0, /*expected_loading_4=*/0,
                     /*expected_loading_5=*/0, /*expected_tab_count_0=*/1,
                     /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
                     /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
                     /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
                     /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramLoadEventFiredBackgroundSuffix,
      /*expected_loading_1_or_more=*/0, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/0, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
}

TEST_F(MultiTabLoadingPageLoadMetricsObserverTest, MultiTabLoading1) {
  SimulatePageLoad(/*number_of_tabs_with_inflight_load=*/1,
                   /*number_of_tabs=*/1, Foreground);

  ValidateHistograms(FROM_HERE, internal::kHistogramFirstContentfulPaintSuffix,
                     /*expected_loading_1_or_more=*/1,
                     /*expected_loading_2_or_more=*/0,
                     /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
                     /*expected_loading_1=*/1, /*expected_loading_2=*/0,
                     /*expected_loading_3=*/0, /*expected_loading_4=*/0,
                     /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
                     /*expected_tab_count_1=*/1, /*expected_tab_count_2=*/0,
                     /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
                     /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
                     /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramLargestContentfulPaintSuffix,
      /*expected_loading_1_or_more=*/1, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/1, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/1, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramForegroundToFirstContentfulPaintSuffix,
      /*expected_loading_1_or_more=*/0, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/0, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(FROM_HERE, internal::kHistogramFirstMeaningfulPaintSuffix,
                     /*expected_loading_1_or_more=*/1,
                     /*expected_loading_2_or_more=*/0,
                     /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
                     /*expected_loading_1=*/1, /*expected_loading_2=*/0,
                     /*expected_loading_3=*/0, /*expected_loading_4=*/0,
                     /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
                     /*expected_tab_count_1=*/1, /*expected_tab_count_2=*/0,
                     /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
                     /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
                     /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramDOMContentLoadedEventFiredSuffix,
      /*expected_loading_1_or_more=*/1, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/1, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/1, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramDOMContentLoadedEventFiredBackgroundSuffix,
      /*expected_loading_1_or_more=*/0, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/0, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(FROM_HERE, internal::kHistogramLoadEventFiredSuffix,
                     /*expected_loading_1_or_more=*/1,
                     /*expected_loading_2_or_more=*/0,
                     /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
                     /*expected_loading_1=*/1, /*expected_loading_2=*/0,
                     /*expected_loading_3=*/0, /*expected_loading_4=*/0,
                     /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
                     /*expected_tab_count_1=*/1, /*expected_tab_count_2=*/0,
                     /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
                     /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
                     /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramLoadEventFiredBackgroundSuffix,
      /*expected_loading_1_or_more=*/0, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/0, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
}

TEST_F(MultiTabLoadingPageLoadMetricsObserverTest, MultiTabLoading2) {
  SimulatePageLoad(/*number_of_tabs_with_inflight_load=*/2,
                   /*number_of_tabs=*/2, Foreground);

  ValidateHistograms(FROM_HERE, internal::kHistogramFirstContentfulPaintSuffix,
                     /*expected_loading_1_or_more=*/1,
                     /*expected_loading_2_or_more=*/1,
                     /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
                     /*expected_loading_1=*/0, /*expected_loading_2=*/1,
                     /*expected_loading_3=*/0, /*expected_loading_4=*/0,
                     /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
                     /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/1,
                     /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
                     /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
                     /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramLargestContentfulPaintSuffix,
      /*expected_loading_1_or_more=*/1, /*expected_loading_2_or_more=*/1,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/0, /*expected_loading_2=*/1,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/1,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramForegroundToFirstContentfulPaintSuffix,
      /*expected_loading_1_or_more=*/0, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/0, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(FROM_HERE, internal::kHistogramFirstMeaningfulPaintSuffix,
                     /*expected_loading_1_or_more=*/1,
                     /*expected_loading_2_or_more=*/1,
                     /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
                     /*expected_loading_1=*/0, /*expected_loading_2=*/1,
                     /*expected_loading_3=*/0, /*expected_loading_4=*/0,
                     /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
                     /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/1,
                     /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
                     /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
                     /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramDOMContentLoadedEventFiredSuffix,
      /*expected_loading_1_or_more=*/1, /*expected_loading_2_or_more=*/1,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/0, /*expected_loading_2=*/1,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/1,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramDOMContentLoadedEventFiredBackgroundSuffix,
      /*expected_loading_1_or_more=*/0, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/0, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(FROM_HERE, internal::kHistogramLoadEventFiredSuffix,
                     /*expected_loading_1_or_more=*/1,
                     /*expected_loading_2_or_more=*/1,
                     /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
                     /*expected_loading_1=*/0, /*expected_loading_2=*/1,
                     /*expected_loading_3=*/0, /*expected_loading_4=*/0,
                     /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
                     /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/1,
                     /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
                     /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
                     /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramLoadEventFiredBackgroundSuffix,
      /*expected_loading_1_or_more=*/0, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/0, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
}

TEST_F(MultiTabLoadingPageLoadMetricsObserverTest, MultiTabLoading5) {
  SimulatePageLoad(/*number_of_tabs_with_inflight_load=*/5,
                   /*number_of_tabs=*/5, Foreground);

  ValidateHistograms(FROM_HERE, internal::kHistogramFirstContentfulPaintSuffix,
                     /*expected_loading_1_or_more=*/1,
                     /*expected_loading_2_or_more=*/1,
                     /*expected_loading_5_or_more=*/1, /*expected_loading_0=*/0,
                     /*expected_loading_1=*/0, /*expected_loading_2=*/0,
                     /*expected_loading_3=*/0, /*expected_loading_4=*/0,
                     /*expected_loading_5=*/1, /*expected_tab_count_0=*/0,
                     /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
                     /*expected_tab_count_4=*/1, /*expected_tab_count_8=*/0,
                     /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
                     /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramLargestContentfulPaintSuffix,
      /*expected_loading_1_or_more=*/1, /*expected_loading_2_or_more=*/1,
      /*expected_loading_5_or_more=*/1, /*expected_loading_0=*/0,
      /*expected_loading_1=*/0, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/1, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/1, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramForegroundToFirstContentfulPaintSuffix,
      /*expected_loading_1_or_more=*/0, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/0, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(FROM_HERE, internal::kHistogramFirstMeaningfulPaintSuffix,
                     /*expected_loading_1_or_more=*/1,
                     /*expected_loading_2_or_more=*/1,
                     /*expected_loading_5_or_more=*/1, /*expected_loading_0=*/0,
                     /*expected_loading_1=*/0, /*expected_loading_2=*/0,
                     /*expected_loading_3=*/0, /*expected_loading_4=*/0,
                     /*expected_loading_5=*/1, /*expected_tab_count_0=*/0,
                     /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
                     /*expected_tab_count_4=*/1, /*expected_tab_count_8=*/0,
                     /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
                     /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramDOMContentLoadedEventFiredSuffix,
      /*expected_loading_1_or_more=*/1, /*expected_loading_2_or_more=*/1,
      /*expected_loading_5_or_more=*/1, /*expected_loading_0=*/0,
      /*expected_loading_1=*/0, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/1, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/1, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramDOMContentLoadedEventFiredBackgroundSuffix,
      /*expected_loading_1_or_more=*/0, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/0, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(FROM_HERE, internal::kHistogramLoadEventFiredSuffix,
                     /*expected_loading_1_or_more=*/1,
                     /*expected_loading_2_or_more=*/1,
                     /*expected_loading_5_or_more=*/1, /*expected_loading_0=*/0,
                     /*expected_loading_1=*/0, /*expected_loading_2=*/0,
                     /*expected_loading_3=*/0, /*expected_loading_4=*/0,
                     /*expected_loading_5=*/1, /*expected_tab_count_0=*/0,
                     /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
                     /*expected_tab_count_4=*/1, /*expected_tab_count_8=*/0,
                     /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
                     /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramLoadEventFiredBackgroundSuffix,
      /*expected_loading_1_or_more=*/0, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/0, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
}

TEST_F(MultiTabLoadingPageLoadMetricsObserverTest, MultiTabBackground) {
  SimulatePageLoad(/*number_of_tabs_with_inflight_load=*/1,
                   /*number_of_tabs=*/1, Background);

  ValidateHistograms(FROM_HERE, internal::kHistogramFirstContentfulPaintSuffix,
                     /*expected_loading_1_or_more=*/0,
                     /*expected_loading_2_or_more=*/0,
                     /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
                     /*expected_loading_1=*/0, /*expected_loading_2=*/0,
                     /*expected_loading_3=*/0, /*expected_loading_4=*/0,
                     /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
                     /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
                     /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
                     /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
                     /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramLargestContentfulPaintSuffix,
      /*expected_loading_1_or_more=*/0, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/0, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramForegroundToFirstContentfulPaintSuffix,
      /*expected_loading_1_or_more=*/1, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/1, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/1, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(FROM_HERE, internal::kHistogramFirstMeaningfulPaintSuffix,
                     /*expected_loading_1_or_more=*/0,
                     /*expected_loading_2_or_more=*/0,
                     /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
                     /*expected_loading_1=*/0, /*expected_loading_2=*/0,
                     /*expected_loading_3=*/0, /*expected_loading_4=*/0,
                     /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
                     /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
                     /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
                     /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
                     /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramDOMContentLoadedEventFiredSuffix,
      /*expected_loading_1_or_more=*/0, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/0, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramDOMContentLoadedEventFiredBackgroundSuffix,
      /*expected_loading_1_or_more=*/1, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/1, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/1, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
  ValidateHistograms(FROM_HERE, internal::kHistogramLoadEventFiredSuffix,
                     /*expected_loading_1_or_more=*/0,
                     /*expected_loading_2_or_more=*/0,
                     /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
                     /*expected_loading_1=*/0, /*expected_loading_2=*/0,
                     /*expected_loading_3=*/0, /*expected_loading_4=*/0,
                     /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
                     /*expected_tab_count_1=*/0, /*expected_tab_count_2=*/0,
                     /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
                     /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
                     /*expected_tab_count_64=*/0);
  ValidateHistograms(
      FROM_HERE, internal::kHistogramLoadEventFiredBackgroundSuffix,
      /*expected_loading_1_or_more=*/1, /*expected_loading_2_or_more=*/0,
      /*expected_loading_5_or_more=*/0, /*expected_loading_0=*/0,
      /*expected_loading_1=*/1, /*expected_loading_2=*/0,
      /*expected_loading_3=*/0, /*expected_loading_4=*/0,
      /*expected_loading_5=*/0, /*expected_tab_count_0=*/0,
      /*expected_tab_count_1=*/1, /*expected_tab_count_2=*/0,
      /*expected_tab_count_4=*/0, /*expected_tab_count_8=*/0,
      /*expected_tab_count_16=*/0, /*expected_tab_count_32=*/0,
      /*expected_tab_count_64=*/0);
}
