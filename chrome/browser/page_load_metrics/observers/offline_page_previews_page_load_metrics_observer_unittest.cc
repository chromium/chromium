// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/offline_page_previews_page_load_metrics_observer.h"

#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"

namespace previews {

namespace {

const char kDefaultTestUrl1[] = "https://google.com";
const char kDefaultTestUrl2[] = "https://example.com";

class TestOfflinePagePreviewsPageLoadMetricsObserver
    : public OfflinePagePreviewsPageLoadMetricsObserver {
 public:
  explicit TestOfflinePagePreviewsPageLoadMetricsObserver(bool offline_preview)
      : offline_preview_(offline_preview) {}
  ~TestOfflinePagePreviewsPageLoadMetricsObserver() override {}

  bool IsOfflinePreview(content::WebContents* web_contents) const override {
    return offline_preview_;
  }

 private:
  bool offline_preview_;
};

}  // namespace

class OfflinePagePreviewsPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  OfflinePagePreviewsPageLoadMetricsObserverTest()
      : is_offline_preview_(false) {}

  void ResetTest() {
    page_load_metrics::InitPageLoadTimingForTest(&timing_);
    // Reset to the default testing state. Does not reset histogram state.
    timing_.navigation_start = base::Time::FromDoubleT(1);
    timing_.response_start = base::TimeDelta::FromSeconds(2);
    timing_.parse_timing->parse_start = base::TimeDelta::FromSeconds(3);
    timing_.paint_timing->first_contentful_paint =
        base::TimeDelta::FromSeconds(4);
    timing_.paint_timing->first_image_paint = base::TimeDelta::FromSeconds(5);
    timing_.document_timing->load_event_start = base::TimeDelta::FromSeconds(7);
    PopulateRequiredTimingFields(&timing_);
  }

  void RunTest(bool is_offline_preview) {
    is_offline_preview_ = is_offline_preview;
    NavigateAndCommit(GURL(kDefaultTestUrl1));
    tester()->SimulateTimingUpdate(timing_);

    // Navigate again to force OnComplete, which happens when a new navigation
    // occurs.
    NavigateAndCommit(GURL(kDefaultTestUrl2));
  }

  void ValidateHistograms() {
    ValidateTimingHistogramsFor(
        internal::kHistogramOfflinePreviewsDOMContentLoadedEventFired,
        timing_.document_timing->dom_content_loaded_event_start);
    ValidateTimingHistogramsFor(internal::kHistogramOfflinePreviewsFirstLayout,
                                timing_.document_timing->first_layout);
    ValidateTimingHistogramsFor(
        internal::kHistogramOfflinePreviewsLoadEventFired,
        timing_.document_timing->load_event_start);
    ValidateTimingHistogramsFor(
        internal::kHistogramOfflinePreviewsFirstContentfulPaint,
        timing_.paint_timing->first_contentful_paint);
    ValidateTimingHistogramsFor(internal::kHistogramOfflinePreviewsParseStart,
                                timing_.parse_timing->parse_start);
    ValidateHistogramsFor(internal::kHistogramOfflinePreviewsPageEndReason,
                          page_load_metrics::PageEndReason::END_NEW_NAVIGATION);
  }

  void ValidateTimingHistogramsFor(
      const std::string& histogram,
      const base::Optional<base::TimeDelta>& event) {
    ValidateHistogramsFor(histogram, static_cast<base::HistogramBase::Sample>(
                                         event.value().InMilliseconds()));
  }

  void ValidateHistogramsFor(const std::string& histogram,
                             const base::HistogramBase::Sample sample) {
    tester()->histogram_tester().ExpectTotalCount(histogram,
                                                  is_offline_preview_ ? 1 : 0);
    if (!is_offline_preview_)
      return;
    tester()->histogram_tester().ExpectUniqueSample(histogram, sample, 1);
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        base::WrapUnique(new TestOfflinePagePreviewsPageLoadMetricsObserver(
            is_offline_preview_)));
  }

 private:
  page_load_metrics::mojom::PageLoadTiming timing_;
  bool is_offline_preview_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePagePreviewsPageLoadMetricsObserverTest);
};

TEST_F(OfflinePagePreviewsPageLoadMetricsObserverTest, NoPreview) {
  ResetTest();
  RunTest(false);
  ValidateHistograms();
}

TEST_F(OfflinePagePreviewsPageLoadMetricsObserverTest, OfflinePreviews) {
  ResetTest();
  RunTest(true);
  ValidateHistograms();
}

}  // namespace previews
