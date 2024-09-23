// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/gws_page_load_metrics_observer.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/page_load_metrics/observers/histogram_suffixes.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/page_load_metrics/google/browser/gws_abandoned_page_load_metrics_observer.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace {

constexpr char kGoogleSearchResultsUrl[] = "https://www.google.com/search?q=d";

}  // namespace

class GWSPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  GWSPageLoadMetricsObserverTest()
      // Tests in this suite need a mock clock, because they care about which
      // histogram buckets the times of various events land inside. Using the
      // real clock would introduce flakes depending on how long the test takes
      // to execute. See https://issues.chromium.org/issues/327150423
      : page_load_metrics::PageLoadMetricsObserverTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // page_load_metrics::PageLoadMetricsObserverTestHarness:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    auto observer = std::make_unique<GWSPageLoadMetricsObserver>();
    // Set the PLMO navigation to the first navigation to ensure that we get
    // constant UMA names.
    observer->SetIsFirstNavigationForTesting(true);
    observer->SetNewTabPageForTesting(true);
    observer_ = observer.get();
    tracker->AddObserver(std::move(observer));
  }

  void SimulateTimingWithoutPaint() {
    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
    tester()->SimulateTimingUpdate(timing);
  }

  void SimulateTimingWithFirstPaint() {
    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.parse_timing->parse_start = base::Milliseconds(0);
    timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
    timing.paint_timing->first_paint = base::Milliseconds(0);
    PopulateRequiredTimingFields(&timing);
    tester()->SimulateTimingUpdate(timing);
  }

  std::string AddHistogramSuffix(const std::string& metric_name) {
    return metric_name + internal::kSuffixFirstNavigation +
           internal::kSuffixFromNewTabPage;
  }

 protected:
  raw_ptr<GWSPageLoadMetricsObserver, DanglingUntriaged> observer_ = nullptr;
};

TEST_F(GWSPageLoadMetricsObserverTest, Search) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.connect_start = base::Milliseconds(1);
  timing.domain_lookup_timing->domain_lookup_start = base::Milliseconds(1);
  timing.domain_lookup_timing->domain_lookup_end = base::Milliseconds(1);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(10);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(100);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  PopulateRequiredTimingFields(&timing);

  // Wait until the browser init is complete.
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();

  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstRequestStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFirstRequestStart, 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstResponseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFirstResponseStart, 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstLoaderCallback, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFirstLoaderCallback, 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalRequestStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFinalRequestStart, 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalResponseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFinalResponseStart, 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalLoaderCallback, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFinalLoaderCallback, 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToOnComplete, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSParseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSParseStart, 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSConnectStart), 1);
  tester()->histogram_tester().ExpectBucketCount(
      AddHistogramSuffix(internal::kHistogramGWSConnectStart), 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupStart), 1);
  tester()->histogram_tester().ExpectBucketCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupStart), 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupEnd), 1);
  tester()->histogram_tester().ExpectBucketCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupEnd), 1, 1);
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
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.connect_start = base::Milliseconds(1);
  timing.domain_lookup_timing->domain_lookup_start = base::Milliseconds(1);
  timing.domain_lookup_timing->domain_lookup_end = base::Milliseconds(1);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(10);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(100);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  PopulateRequiredTimingFields(&timing);

  // Wait until the browser init is complete.
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();

  NavigateAndCommit(GURL("https://www.google.com/foo&q=test"));

  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstRequestStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstResponseStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstLoaderCallback, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalRequestStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalResponseStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalLoaderCallback, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToOnComplete, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSParseStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSConnectStart), 0);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupStart), 0);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupEnd), 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSFirstContentfulPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSLargestContentfulPaint, 0);
}

TEST_F(GWSPageLoadMetricsObserverTest, SearchBackground) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.parse_timing->parse_start = base::Seconds(60);
  timing.connect_start = base::Milliseconds(1);
  timing.domain_lookup_timing->domain_lookup_start = base::Milliseconds(1);
  timing.domain_lookup_timing->domain_lookup_end = base::Milliseconds(1);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_contentful_paint = base::Seconds(60);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Seconds(60);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  PopulateRequiredTimingFields(&timing);

  // Wait until the browser init is complete.
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();

  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  web_contents()->WasHidden();
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstRequestStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstResponseStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstLoaderCallback, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalRequestStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalResponseStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalLoaderCallback, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToOnComplete, 1);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSConnectStart), 0);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupStart), 0);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupEnd), 0);
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
  timing.connect_start = base::Milliseconds(1);
  timing.domain_lookup_timing->domain_lookup_start = base::Milliseconds(1);
  timing.domain_lookup_timing->domain_lookup_end = base::Milliseconds(1);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_contentful_paint = base::Microseconds(1);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Microseconds(1);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  PopulateRequiredTimingFields(&timing);

  // Wait until the browser init is complete.
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();

  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  // Sleep to make sure the backgrounded time is > than the paint time, even
  // for low resolution timers.
  task_environment()->FastForwardBy(base::Milliseconds(50));
  web_contents()->WasHidden();
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstRequestStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFirstRequestStart, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstResponseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFirstResponseStart, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstLoaderCallback, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFirstLoaderCallback, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalRequestStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFinalRequestStart, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalResponseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFinalResponseStart, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalLoaderCallback, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFinalLoaderCallback, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToOnComplete, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSParseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSParseStart, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSConnectStart), 1);
  tester()->histogram_tester().ExpectBucketCount(
      AddHistogramSuffix(internal::kHistogramGWSConnectStart), 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupStart), 1);
  tester()->histogram_tester().ExpectBucketCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupStart), 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupEnd), 1);
  tester()->histogram_tester().ExpectBucketCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupEnd), 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSFirstContentfulPaint, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSLargestContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSLargestContentfulPaint, 0, 1);
}

TEST_F(GWSPageLoadMetricsObserverTest, CustomUserTimingMark) {
  // No user timing mark. Expecting AFT events are not recorded.
  page_load_metrics::mojom::CustomUserTimingMark timing;

  // Wait until the browser init is complete.
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();

  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  tester()->SimulateCustomUserTimingUpdate(timing.Clone());
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramGWSAFTStart,
                                                0);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramGWSAFTEnd,
                                                0);

  // Simulate AFT events. This is recorded with expected event name.
  auto timing2 = timing.Clone();
  timing2->mark_name = internal::kGwsAFTStartMarkName;
  timing2->start_time = base::Milliseconds(100);

  auto timing3 = timing.Clone();
  timing3->mark_name = internal::kGwsAFTEndMarkName;
  timing3->start_time = base::Milliseconds(500);

  tester()->SimulateCustomUserTimingUpdate(timing2.Clone());
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramGWSAFTStart,
                                                1);

  tester()->SimulateCustomUserTimingUpdate(timing2.Clone());
  tester()->SimulateCustomUserTimingUpdate(timing3.Clone());
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramGWSAFTStart,
                                                2);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramGWSAFTEnd,
                                                1);
}
