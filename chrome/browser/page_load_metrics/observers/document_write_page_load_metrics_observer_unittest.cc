// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/document_write_page_load_metrics_observer.h"

#include <memory>

#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"

class DocumentWritePageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<DocumentWritePageLoadMetricsObserver>());
  }

  void AssertNoBlockHistogramsLogged() {
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
  }
};

TEST_F(DocumentWritePageLoadMetricsObserverTest, NoMetrics) {
  AssertNoBlockHistogramsLogged();
}

TEST_F(DocumentWritePageLoadMetricsObserverTest, PossibleBlock) {
  base::TimeDelta contentful_paint = base::Milliseconds(1);
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_contentful_paint = contentful_paint;
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.parse_timing->parse_stop = base::Milliseconds(100);
  timing.parse_timing->parse_blocked_on_script_load_duration =
      base::Milliseconds(5);
  timing.parse_timing
      ->parse_blocked_on_script_load_from_document_write_duration =
      base::Milliseconds(5);
  timing.parse_timing->parse_blocked_on_script_execution_duration =
      base::Milliseconds(3);
  timing.parse_timing
      ->parse_blocked_on_script_execution_from_document_write_duration =
      base::Milliseconds(3);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::FrameMetadata metadata;
  metadata.behavior_flags |=
      blink::LoadingBehaviorFlag::kLoadingBehaviorDocumentWriteBlock;
  NavigateAndCommit(GURL("https://www.google.com/"));
  tester()->SimulateTimingAndMetadataUpdate(timing, metadata);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint,
      contentful_paint.InMilliseconds(), 1);

  NavigateAndCommit(GURL("https://www.example.com/"));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint,
      contentful_paint.InMilliseconds(), 1);
}

TEST_F(DocumentWritePageLoadMetricsObserverTest, NoPossibleBlock) {
  base::TimeDelta contentful_paint = base::Milliseconds(1);
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_contentful_paint = contentful_paint;
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::FrameMetadata metadata;
  NavigateAndCommit(GURL("https://www.google.com/"));
  tester()->SimulateTimingAndMetadataUpdate(timing, metadata);

  NavigateAndCommit(GURL("https://www.example.com/"));
  AssertNoBlockHistogramsLogged();
}
