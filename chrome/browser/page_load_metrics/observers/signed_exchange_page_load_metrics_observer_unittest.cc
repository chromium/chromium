// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/signed_exchange_page_load_metrics_observer.h"

#include <memory>

#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"

namespace {

const char kDefaultTestUrl[] = "https://example.com/";

}  // namespace

class SignedExchangePageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<SignedExchangePageLoadMetricsObserver>());
  }

  void NavigateAndCommitSignedExchange(const GURL& url,
                                       bool was_fetched_via_cache) {
    std::unique_ptr<content::NavigationSimulator> navigation =
        content::NavigationSimulator::CreateBrowserInitiated(url,
                                                             web_contents());
    navigation->Start();
    navigation->SetWasFetchedViaCache(was_fetched_via_cache);
    navigation->SetIsSignedExchangeInnerResponse(true);
    navigation->Commit();
  }

  void AssertNoSignedExchangeHistogramsLoggedFor(const std::string& prefix) {
    base::HistogramTester::CountsMap empty_counts_map =
        tester()->histogram_tester().GetTotalCountsForPrefix(prefix);
    for (const auto& it : empty_counts_map) {
      base::HistogramBase::Count count = it.second;
      EXPECT_EQ(0, count) << "Histogram \"" << it.first
                          << "\" should be empty.";
    }
  }

  void AssertNoSignedExchangeHistogramsLogged() {
    AssertNoSignedExchangeHistogramsLoggedFor(
        internal::kHistogramSignedExchangePrefix);
  }

  void InitializeTestPageLoadTiming(
      page_load_metrics::mojom::PageLoadTiming* timing) {
    page_load_metrics::InitPageLoadTimingForTest(timing);
    timing->navigation_start = base::Time::FromDoubleT(1);
    timing->interactive_timing->first_input_delay =
        base::TimeDelta::FromMilliseconds(50);
    timing->interactive_timing->first_input_timestamp =
        base::TimeDelta::FromMilliseconds(712);
    timing->parse_timing->parse_start = base::TimeDelta::FromMilliseconds(100);
    timing->paint_timing->first_paint = base::TimeDelta::FromMilliseconds(200);
    timing->paint_timing->first_contentful_paint =
        base::TimeDelta::FromMilliseconds(300);
    timing->paint_timing->first_meaningful_paint =
        base::TimeDelta::FromMilliseconds(700);
    timing->document_timing->dom_content_loaded_event_start =
        base::TimeDelta::FromMilliseconds(600);
    timing->document_timing->load_event_start =
        base::TimeDelta::FromMilliseconds(1000);
    PopulateRequiredTimingFields(timing);
  }
};

TEST_F(SignedExchangePageLoadMetricsObserverTest, NoMetrics) {
  AssertNoSignedExchangeHistogramsLogged();
  AssertNoSignedExchangeHistogramsLoggedFor(
      internal::kHistogramCachedSignedExchangePrefix);
  AssertNoSignedExchangeHistogramsLoggedFor(
      internal::kHistogramNotCachedSignedExchangePrefix);
}

TEST_F(SignedExchangePageLoadMetricsObserverTest, NoSignedExchange) {
  page_load_metrics::mojom::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);

  AssertNoSignedExchangeHistogramsLogged();
  AssertNoSignedExchangeHistogramsLoggedFor(
      internal::kHistogramCachedSignedExchangePrefix);
  AssertNoSignedExchangeHistogramsLoggedFor(
      internal::kHistogramNotCachedSignedExchangePrefix);
}

TEST_F(SignedExchangePageLoadMetricsObserverTest, WithSignedExchange) {
  page_load_metrics::mojom::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  NavigateAndCommitSignedExchange(GURL(kDefaultTestUrl), false);
  tester()->SimulateTimingUpdate(timing);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramSignedExchangeFirstInputDelay, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramSignedExchangeFirstInputDelay,
      timing.interactive_timing->first_input_delay.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramSignedExchangeFirstPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramSignedExchangeFirstPaint,
      timing.paint_timing->first_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramSignedExchangeFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramSignedExchangeFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramSignedExchangeParseStartToFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramSignedExchangeParseStartToFirstContentfulPaint,
      (timing.paint_timing->first_contentful_paint.value() -
       timing.parse_timing->parse_start.value())
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramSignedExchangeDomContentLoaded, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramSignedExchangeDomContentLoaded,
      timing.document_timing->dom_content_loaded_event_start.value()
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramSignedExchangeLoad, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramSignedExchangeLoad,
      timing.document_timing->load_event_start.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramSignedExchangeParseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramSignedExchangeParseStart,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNotCachedSignedExchangeParseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramNotCachedSignedExchangeParseStart,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNotCachedSignedExchangeFirstInputDelay, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramNotCachedSignedExchangeFirstInputDelay,
      timing.interactive_timing->first_input_delay.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNotCachedSignedExchangeFirstPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramNotCachedSignedExchangeFirstPaint,
      timing.paint_timing->first_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNotCachedSignedExchangeFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramNotCachedSignedExchangeFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::
          kHistogramNotCachedSignedExchangeParseStartToFirstContentfulPaint,
      1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::
          kHistogramNotCachedSignedExchangeParseStartToFirstContentfulPaint,
      (timing.paint_timing->first_contentful_paint.value() -
       timing.parse_timing->parse_start.value())
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNotCachedSignedExchangeDomContentLoaded, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramNotCachedSignedExchangeDomContentLoaded,
      timing.document_timing->dom_content_loaded_event_start.value()
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNotCachedSignedExchangeLoad, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramNotCachedSignedExchangeLoad,
      timing.document_timing->load_event_start.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNotCachedSignedExchangeParseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramNotCachedSignedExchangeParseStart,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);

  AssertNoSignedExchangeHistogramsLoggedFor(
      internal::kHistogramCachedSignedExchangePrefix);
}

TEST_F(SignedExchangePageLoadMetricsObserverTest, WithCachedSignedExchange) {
  page_load_metrics::mojom::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  NavigateAndCommitSignedExchange(GURL(kDefaultTestUrl), true);
  tester()->SimulateTimingUpdate(timing);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramSignedExchangeFirstInputDelay, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramSignedExchangeFirstInputDelay,
      timing.interactive_timing->first_input_delay.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramSignedExchangeFirstPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramSignedExchangeFirstPaint,
      timing.paint_timing->first_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramSignedExchangeFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramSignedExchangeFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramSignedExchangeParseStartToFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramSignedExchangeParseStartToFirstContentfulPaint,
      (timing.paint_timing->first_contentful_paint.value() -
       timing.parse_timing->parse_start.value())
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramSignedExchangeDomContentLoaded, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramSignedExchangeDomContentLoaded,
      timing.document_timing->dom_content_loaded_event_start.value()
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramSignedExchangeLoad, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramSignedExchangeLoad,
      timing.document_timing->load_event_start.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramCachedSignedExchangeParseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramCachedSignedExchangeParseStart,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramCachedSignedExchangeFirstInputDelay, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramCachedSignedExchangeFirstInputDelay,
      timing.interactive_timing->first_input_delay.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramCachedSignedExchangeFirstPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramCachedSignedExchangeFirstPaint,
      timing.paint_timing->first_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramCachedSignedExchangeFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramCachedSignedExchangeFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramCachedSignedExchangeParseStartToFirstContentfulPaint,
      1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramCachedSignedExchangeParseStartToFirstContentfulPaint,
      (timing.paint_timing->first_contentful_paint.value() -
       timing.parse_timing->parse_start.value())
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramCachedSignedExchangeDomContentLoaded, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramCachedSignedExchangeDomContentLoaded,
      timing.document_timing->dom_content_loaded_event_start.value()
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramCachedSignedExchangeLoad, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramCachedSignedExchangeLoad,
      timing.document_timing->load_event_start.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramCachedSignedExchangeParseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramCachedSignedExchangeParseStart,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);

  AssertNoSignedExchangeHistogramsLoggedFor(
      internal::kHistogramNotCachedSignedExchangePrefix);
}

TEST_F(SignedExchangePageLoadMetricsObserverTest,
       WithSignedExchangeBackground) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommitSignedExchange(GURL(kDefaultTestUrl), true);
  tester()->SimulateTimingUpdate(timing);

  // Background the tab, then foreground it.
  web_contents()->WasHidden();
  web_contents()->WasShown();

  InitializeTestPageLoadTiming(&timing);
  tester()->SimulateTimingUpdate(timing);

  AssertNoSignedExchangeHistogramsLogged();
  AssertNoSignedExchangeHistogramsLoggedFor(
      internal::kHistogramCachedSignedExchangePrefix);
  AssertNoSignedExchangeHistogramsLoggedFor(
      internal::kHistogramNotCachedSignedExchangePrefix);
}
