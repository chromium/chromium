// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/scheme_page_load_metrics_observer.h"

#include <memory>

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"

class SchemePageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    std::unique_ptr<SchemePageLoadMetricsObserver> observer =
        std::make_unique<SchemePageLoadMetricsObserver>();
    observer_ = observer.get();
    tracker->AddObserver(std::move(observer));
  }

  void InitializeTestPageLoadTiming(
      page_load_metrics::mojom::PageLoadTiming* timing) {
    page_load_metrics::InitPageLoadTimingForTest(timing);
    timing->navigation_start = base::Time::FromDoubleT(1);
    timing->parse_timing->parse_start = base::TimeDelta::FromMilliseconds(100);
    timing->paint_timing->first_paint = base::TimeDelta::FromMilliseconds(200);
    timing->paint_timing->first_contentful_paint =
        base::TimeDelta::FromMilliseconds(300);
    timing->paint_timing->first_meaningful_paint =
        base::TimeDelta::FromMilliseconds(400);
    timing->document_timing->dom_content_loaded_event_start =
        base::TimeDelta::FromMilliseconds(600);
    timing->document_timing->load_event_start =
        base::TimeDelta::FromMilliseconds(1000);
    timing->interactive_timing->interactive =
        base::TimeDelta::FromMilliseconds(1200);
    PopulateRequiredTimingFields(timing);
  }

  void SimulateNavigation(
      std::string scheme,
      ui::PageTransition transition = ui::PAGE_TRANSITION_LINK) {
    NavigateAndCommit(GURL(scheme.append("://google.com")), transition);

    page_load_metrics::mojom::PageLoadTiming timing;
    InitializeTestPageLoadTiming(&timing);
    tester()->SimulateTimingUpdate(timing);

    // Navigate again to force OnComplete, which happens when a new navigation
    // occurs.
    NavigateAndCommit(GURL(scheme.append("://example.com")));
  }

  // Excludes understat metrics.
  int CountTotalProtocolMetricsRecorded(const std::string& protocol) {
    int count = 0;
    base::HistogramTester::CountsMap counts_map =
        tester()->histogram_tester().GetTotalCountsForPrefix(
            "PageLoad.Clients.Scheme.");
    for (const auto& entry : counts_map)
      count += entry.second;

    int understat_count = 0;
    base::HistogramTester::CountsMap understat_counts_map =
        tester()->histogram_tester().GetTotalCountsForPrefix(
            "PageLoad.Clients.Scheme." + base::ToUpperASCII(protocol) +
            ".PaintTiming.UnderStat");
    for (const auto& entry : understat_counts_map)
      understat_count += entry.second;

    return count - understat_count;
  }

  // Returns the value of the sample present in |histogram_name|. Should be
  // called only if |histogram_name| contains exactly 1 sample.
  int32_t GetRecordedMetricValue(const std::string& histogram_name) const {
    tester()->histogram_tester().ExpectTotalCount(histogram_name, 1);

    std::vector<base::Bucket> buckets =
        tester()->histogram_tester().GetAllSamples(histogram_name);
    for (const auto& bucket : buckets) {
      if (bucket.count == 1) {
        return bucket.min;
      }
    }
    NOTREACHED();
    return 0;
  }

  void CheckHistograms(int expected_count,
                       const std::string& protocol,
                       bool new_navigation = true) {
    EXPECT_EQ(expected_count, CountTotalProtocolMetricsRecorded(protocol));
    if (expected_count == 0)
      return;

    std::string prefix = "PageLoad.Clients.Scheme.";
    prefix += base::ToUpperASCII(protocol);

    std::string fcp_histogram_name(
        prefix + ".PaintTiming.NavigationToFirstContentfulPaint");
    std::string fcp_understat_histogram_name(prefix + ".PaintTiming.UnderStat");
    std::string fcp_understat_new_nav_histogram_name(
        fcp_understat_histogram_name + ".UserInitiated.NewNavigation");

    tester()->histogram_tester().ExpectTotalCount(
        prefix + ".ParseTiming.NavigationToParseStart", 1);
    tester()->histogram_tester().ExpectTotalCount(fcp_histogram_name, 1);
    tester()->histogram_tester().ExpectTotalCount(
        prefix + ".PaintTiming.ParseStartToFirstContentfulPaint", 1);
    tester()->histogram_tester().ExpectUniqueSample(
        prefix + ".PaintTiming.ParseStartToFirstContentfulPaint",
        static_cast<base::HistogramBase::Sample>(200), 1);
    tester()->histogram_tester().ExpectTotalCount(
        prefix + ".Experimental.PaintTiming.NavigationToFirstMeaningfulPaint",
        1);

    tester()->histogram_tester().ExpectBucketCount(fcp_understat_histogram_name,
                                                   0, 1);
    if (new_navigation) {
      tester()->histogram_tester().ExpectBucketCount(
          fcp_understat_new_nav_histogram_name, 0, 1);
    } else {
      tester()->histogram_tester().ExpectTotalCount(
          fcp_understat_new_nav_histogram_name, 0);
    }

    // Must remain synchronized with the array of the same name in
    // scheme_page_load_metrics_observer.cc.
    static constexpr const int kUnderStatRecordingIntervalsSeconds[] = {1, 2, 5,
                                                                        8, 10};

    base::TimeDelta recorded_fcp_value = base::TimeDelta::FromMilliseconds(
        GetRecordedMetricValue(fcp_histogram_name));

    for (size_t index = 0;
         index < base::size(kUnderStatRecordingIntervalsSeconds); ++index) {
      base::TimeDelta threshold(base::TimeDelta::FromSeconds(
          kUnderStatRecordingIntervalsSeconds[index]));
      if (recorded_fcp_value <= threshold) {
        tester()->histogram_tester().ExpectBucketCount(
            fcp_understat_histogram_name, index + 1, 1);
        if (new_navigation) {
          tester()->histogram_tester().ExpectBucketCount(
              fcp_understat_new_nav_histogram_name, index + 1, 1);
        }
      }
    }

    // Overflow bucket should be empty. This also ensures that
    // kUnderStatRecordingIntervalsSeconds above is synchronized with the array
    // of the same name in scheme_page_load_metrics_observer.cc.
    tester()->histogram_tester().ExpectBucketCount(
        fcp_understat_histogram_name,
        base::size(kUnderStatRecordingIntervalsSeconds) + 1, 0);
  }

  SchemePageLoadMetricsObserver* observer_;
};

TEST_F(SchemePageLoadMetricsObserverTest, HTTPNavigation) {
  SimulateNavigation(url::kHttpScheme);
  CheckHistograms(6, url::kHttpScheme);
}

TEST_F(SchemePageLoadMetricsObserverTest, HTTPSNavigation) {
  SimulateNavigation(url::kHttpsScheme);
  CheckHistograms(6, url::kHttpsScheme);
}

// Make sure no metrics are recorded for an unobserved scheme.
TEST_F(SchemePageLoadMetricsObserverTest, AboutNavigation) {
  SimulateNavigation(url::kAboutScheme);
  CheckHistograms(0, "");
}

TEST_F(SchemePageLoadMetricsObserverTest, HTTPForwardBackNavigation) {
  SimulateNavigation(url::kHttpScheme, ui::PAGE_TRANSITION_FORWARD_BACK);
  CheckHistograms(6, url::kHttpScheme, false /* new_navigation */);
}

TEST_F(SchemePageLoadMetricsObserverTest, HTTPSReloadNavigation) {
  SimulateNavigation(url::kHttpsScheme, ui::PAGE_TRANSITION_RELOAD);
  CheckHistograms(6, url::kHttpsScheme, false /* new_navigation */);
}
