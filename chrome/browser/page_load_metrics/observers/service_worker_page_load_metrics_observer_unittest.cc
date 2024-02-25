// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/service_worker_page_load_metrics_observer.h"

#include <memory>

#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace {

const char kDefaultTestUrl[] = "https://google.com/";
const char kSearchTestUrl[] = "https://www.google.com/search?q=test";

}  // namespace

class ServiceWorkerPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<ServiceWorkerPageLoadMetricsObserver>());
  }

  void SimulateTimingWithoutPaint() {
    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
    tester()->SimulateTimingUpdate(timing);
  }

  void AssertNoServiceWorkerHistogramsLogged() {
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerFirstPaint, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerFirstContentfulPaint, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kBackgroundHistogramServiceWorkerFirstContentfulPaint, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerFirstContentfulPaintForwardBack, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerFirstContentfulPaintForwardBackNoStore,
        0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::
            kHistogramServiceWorkerFirstContentfulPaintSkippableFetchHandler,
        0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::
            kHistogramServiceWorkerFirstContentfulPaintNonSkippableFetchHandler,
        0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::
            kHistogramServiceWorkerLargestContentfulPaintSkippableFetchHandler,
        0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::
            kHistogramServiceWorkerLargestContentfulPaintNonSkippableFetchHandler,
        0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerParseStartToFirstContentfulPaint, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerDomContentLoaded, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerLoad, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerParseStart, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kBackgroundHistogramServiceWorkerParseStart, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerLargestContentfulPaint, 0);

    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerLargestContentfulPaint, 0);
  }

  void AssertNoSearchHistogramsLogged() {
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerParseStartSearch, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerFirstContentfulPaintSearch, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerParseStartToFirstContentfulPaintSearch,
        0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerDomContentLoadedSearch, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerLoadSearch, 0);
  }

  void AssertNoSearchNoSWHistogramsLogged() {
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramNoServiceWorkerFirstContentfulPaintSearch, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::
            kHistogramNoServiceWorkerParseStartToFirstContentfulPaintSearch,
        0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramNoServiceWorkerDomContentLoadedSearch, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramNoServiceWorkerLoadSearch, 0);
  }

  void InitializeTestPageLoadTiming(
      page_load_metrics::mojom::PageLoadTiming* timing) {
    page_load_metrics::InitPageLoadTimingForTest(timing);
    timing->navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
    timing->interactive_timing->first_input_delay = base::Milliseconds(50);
    timing->interactive_timing->first_input_timestamp = base::Milliseconds(712);
    timing->parse_timing->parse_start = base::Milliseconds(100);
    timing->paint_timing->first_paint = base::Milliseconds(200);
    timing->paint_timing->first_contentful_paint = base::Milliseconds(300);
    timing->document_timing->dom_content_loaded_event_start =
        base::Milliseconds(600);
    timing->document_timing->load_event_start = base::Milliseconds(1000);

    timing->paint_timing->largest_contentful_paint->largest_image_paint =
        base::Milliseconds(4780);
    timing->paint_timing->largest_contentful_paint->largest_image_paint_size =
        100u;

    PopulateRequiredTimingFields(timing);
  }
};

TEST_F(ServiceWorkerPageLoadMetricsObserverTest, NoMetrics) {
  AssertNoServiceWorkerHistogramsLogged();
  AssertNoSearchHistogramsLogged();
  AssertNoSearchNoSWHistogramsLogged();
  const auto& entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_ServiceWorkerControlled::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

TEST_F(ServiceWorkerPageLoadMetricsObserverTest, NoServiceWorker) {
  page_load_metrics::mojom::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);

  AssertNoServiceWorkerHistogramsLogged();
  AssertNoSearchHistogramsLogged();
  AssertNoSearchNoSWHistogramsLogged();
  const auto& entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_ServiceWorkerControlled::kEntryName);
  EXPECT_EQ(0u, entries.size());
  EXPECT_EQ(
      1ul,
      tester()->test_ukm_recorder().GetEntriesByName("DocumentCreated").size());
  EXPECT_EQ(1ul,
            tester()->test_ukm_recorder().GetEntriesByName("Unload").size());
}

TEST_F(ServiceWorkerPageLoadMetricsObserverTest, WithServiceWorker) {
  page_load_metrics::mojom::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));

  page_load_metrics::mojom::FrameMetadata metadata;
  metadata.behavior_flags |=
      blink::LoadingBehaviorFlag::kLoadingBehaviorServiceWorkerControlled;
  tester()->SimulateTimingAndMetadataUpdate(timing, metadata);

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerFirstPaint,
      timing.paint_timing->first_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramServiceWorkerFirstContentfulPaint, 0);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaint,
      (timing.paint_timing->first_contentful_paint.value() -
       timing.parse_timing->parse_start.value())
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerDomContentLoaded, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerDomContentLoaded,
      timing.document_timing->dom_content_loaded_event_start.value()
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerLoad, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerLoad,
      timing.document_timing->load_event_start.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerParseStart,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStartForwardBack, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStartForwardBackNoStore, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::
          kHistogramServiceWorkerFirstContentfulPaintSkippableFetchHandler,
      0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::
          kHistogramServiceWorkerLargestContentfulPaintSkippableFetchHandler,
      0);

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramServiceWorkerLargestContentfulPaint),
              testing::ElementsAre(base::Bucket(4780, 1)));

  const auto& entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_ServiceWorkerControlled::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* entry : entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(
        entry, GURL(kDefaultTestUrl));
  }

  AssertNoSearchHistogramsLogged();
  AssertNoSearchNoSWHistogramsLogged();
}

TEST_F(ServiceWorkerPageLoadMetricsObserverTest, WithServiceWorkerBackground) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::FrameMetadata metadata;
  metadata.behavior_flags |=
      blink::LoadingBehaviorFlag::kLoadingBehaviorServiceWorkerControlled;

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingAndMetadataUpdate(timing, metadata);

  // Background the tab, then foreground it.
  web_contents()->WasHidden();
  web_contents()->WasShown();

  InitializeTestPageLoadTiming(&timing);
  tester()->SimulateTimingAndMetadataUpdate(timing, metadata);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstContentfulPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramServiceWorkerFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kBackgroundHistogramServiceWorkerFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerDomContentLoaded, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerLoad, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerLargestContentfulPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramServiceWorkerParseStart, 1);

  const auto& entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_ServiceWorkerControlled::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* entry : entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(
        entry, GURL(kDefaultTestUrl));
  }

  AssertNoSearchHistogramsLogged();
  AssertNoSearchNoSWHistogramsLogged();
}

TEST_F(ServiceWorkerPageLoadMetricsObserverTest, SearchSite) {
  page_load_metrics::mojom::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  NavigateAndCommit(GURL(kSearchTestUrl));
  page_load_metrics::mojom::FrameMetadata metadata;
  metadata.behavior_flags |=
      blink::LoadingBehaviorFlag::kLoadingBehaviorServiceWorkerControlled;
  tester()->SimulateTimingAndMetadataUpdate(timing, metadata);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerParseStart,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStartSearch, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerParseStartSearch,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerFirstPaint,
      timing.paint_timing->first_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstContentfulPaintSearch, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerFirstContentfulPaintSearch,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramServiceWorkerFirstContentfulPaint, 0);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaint,
      (timing.paint_timing->first_contentful_paint.value() -
       timing.parse_timing->parse_start.value())
          .InMilliseconds(),
      1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaintSearch,
      1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaintSearch,
      (timing.paint_timing->first_contentful_paint.value() -
       timing.parse_timing->parse_start.value())
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerDomContentLoaded, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerDomContentLoaded,
      timing.document_timing->dom_content_loaded_event_start.value()
          .InMilliseconds(),
      1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerDomContentLoadedSearch, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerDomContentLoadedSearch,
      timing.document_timing->dom_content_loaded_event_start.value()
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerLoad, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerLoad,
      timing.document_timing->load_event_start.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerLoadSearch, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerLoadSearch,
      timing.document_timing->load_event_start.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerParseStart,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);

  AssertNoSearchNoSWHistogramsLogged();
}

TEST_F(ServiceWorkerPageLoadMetricsObserverTest, SearchNoSWSite) {
  page_load_metrics::mojom::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  NavigateAndCommit(GURL(kSearchTestUrl));
  page_load_metrics::mojom::FrameMetadata metadata;
  tester()->SimulateTimingAndMetadataUpdate(timing, metadata);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNoServiceWorkerFirstContentfulPaintSearch, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramNoServiceWorkerFirstContentfulPaintSearch,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNoServiceWorkerParseStartToFirstContentfulPaintSearch,
      1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramNoServiceWorkerParseStartToFirstContentfulPaintSearch,
      (timing.paint_timing->first_contentful_paint.value() -
       timing.parse_timing->parse_start.value())
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNoServiceWorkerDomContentLoadedSearch, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramNoServiceWorkerDomContentLoadedSearch,
      timing.document_timing->dom_content_loaded_event_start.value()
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNoServiceWorkerLoadSearch, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramNoServiceWorkerLoadSearch,
      timing.document_timing->load_event_start.value().InMilliseconds(), 1);

  AssertNoServiceWorkerHistogramsLogged();
  AssertNoSearchHistogramsLogged();
}

TEST_F(ServiceWorkerPageLoadMetricsObserverTest,
       WithServiceWorker_ForwardBack) {
  page_load_metrics::mojom::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  // Back navigations to a page that was reloaded report a main transition type
  // of PAGE_TRANSITION_RELOAD with a PAGE_TRANSITION_FORWARD_BACK
  // modifier. This test verifies that when we encounter such a page, we log it
  // as a forward/back navigation.
  tester()->NavigateWithPageTransitionAndCommit(
      GURL(kDefaultTestUrl),
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_RELOAD |
                                ui::PAGE_TRANSITION_FORWARD_BACK));
  page_load_metrics::mojom::FrameMetadata metadata;
  metadata.behavior_flags |=
      blink::LoadingBehaviorFlag::kLoadingBehaviorServiceWorkerControlled;
  tester()->SimulateTimingAndMetadataUpdate(timing, metadata);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerFirstPaint,
      timing.paint_timing->first_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstContentfulPaintForwardBack, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerFirstContentfulPaintForwardBack,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerParseStart,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStartForwardBack, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerParseStartForwardBack,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramServiceWorkerLargestContentfulPaint),
              testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_F(ServiceWorkerPageLoadMetricsObserverTest,
       WithServiceWorker_SkippableFetchHandler) {
  page_load_metrics::mojom::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));

  page_load_metrics::mojom::FrameMetadata metadata;
  metadata.behavior_flags |=
      blink::LoadingBehaviorFlag::kLoadingBehaviorServiceWorkerControlled;
  metadata.behavior_flags |= blink::LoadingBehaviorFlag::
      kLoadingBehaviorServiceWorkerFetchHandlerSkippable;
  tester()->SimulateTimingAndMetadataUpdate(timing, metadata);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerFirstPaint,
      timing.paint_timing->first_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::
          kHistogramServiceWorkerFirstContentfulPaintSkippableFetchHandler,
      1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::
          kHistogramServiceWorkerFirstContentfulPaintSkippableFetchHandler,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::
          kHistogramServiceWorkerFirstContentfulPaintNonSkippableFetchHandler,
      0);

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramServiceWorkerLargestContentfulPaint),
              testing::ElementsAre(base::Bucket(4780, 1)));
  EXPECT_THAT(
      tester()->histogram_tester().GetAllSamples(
          internal::
              kHistogramServiceWorkerLargestContentfulPaintSkippableFetchHandler),
      testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_F(ServiceWorkerPageLoadMetricsObserverTest,
       WithServiceWorker_NonSkippableFetchHandler) {
  page_load_metrics::mojom::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));

  page_load_metrics::mojom::FrameMetadata metadata;
  metadata.behavior_flags |=
      blink::LoadingBehaviorFlag::kLoadingBehaviorServiceWorkerControlled;
  tester()->SimulateTimingAndMetadataUpdate(timing, metadata);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerFirstPaint,
      timing.paint_timing->first_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::
          kHistogramServiceWorkerFirstContentfulPaintSkippableFetchHandler,
      0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::
          kHistogramServiceWorkerFirstContentfulPaintNonSkippableFetchHandler,
      1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::
          kHistogramServiceWorkerFirstContentfulPaintNonSkippableFetchHandler,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramServiceWorkerLargestContentfulPaint),
              testing::ElementsAre(base::Bucket(4780, 1)));
  EXPECT_THAT(
      tester()->histogram_tester().GetAllSamples(
          internal::
              kHistogramServiceWorkerLargestContentfulPaintNonSkippableFetchHandler),
      testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_F(ServiceWorkerPageLoadMetricsObserverTest,
       FlushMetricsOnAppEnterBackground) {
  page_load_metrics::mojom::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));

  page_load_metrics::mojom::FrameMetadata metadata;
  metadata.behavior_flags |=
      blink::LoadingBehaviorFlag::kLoadingBehaviorServiceWorkerControlled;
  tester()->SimulateTimingAndMetadataUpdate(timing, metadata);

  // Most timings have been recorded. Just test FCP for simplicity. LCP has not
  // yet been recorded.
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerLargestContentfulPaint, 0);

  // This flushes LCP.
  tester()->SimulateAppEnterBackground();
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramServiceWorkerLargestContentfulPaint),
              testing::ElementsAre(base::Bucket(4780, 1)));

  // Navigate again, forcing completion callbacks to be called.
  tester()->NavigateToUntrackedUrl();

  // LCP will not be recorded again, since FlushMetricsOnAppEnterBackground()
  // returned STOP_OBSERVING.
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramServiceWorkerLargestContentfulPaint),
              testing::ElementsAre(base::Bucket(4780, 1)));
}
