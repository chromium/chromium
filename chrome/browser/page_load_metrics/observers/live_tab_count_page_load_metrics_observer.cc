// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/live_tab_count_page_load_metrics_observer.h"

#include <string>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/metrics/tab_count_metrics.h"
#include "chrome/browser/page_load_metrics/observers/histogram_suffixes.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/tab_count_metrics/tab_count_metrics.h"

#define LIVE_TAB_COUNT_PAGE_LOAD_HISTOGRAM(prefix, tab_count_bucket, sample, \
                                           histogram_min, histogram_max,     \
                                           histogram_buckets)                \
  STATIC_HISTOGRAM_POINTER_GROUP(                                            \
      tab_count_metrics::HistogramName(                                      \
          histogram_prefix, /* live_tabs_only = */ true, tab_count_bucket),  \
      static_cast<int>(bucket),                                              \
      static_cast<int>(tab_count_metrics::kNumTabCountBuckets),              \
      AddTimeMillisecondsGranularity(sample),                                \
      base::Histogram::FactoryTimeGet(                                       \
          tab_count_metrics::HistogramName(                                  \
              prefix, /* live_tabs_only = */ true, tab_count_bucket),        \
          histogram_min, histogram_max, histogram_buckets,                   \
          base::HistogramBase::kUmaTargetedHistogramFlag))

// These are the same histogram parameters used for the core page load paint
// timing metrics (see PAGE_LOAD_HISTOGRAM).
#define LIVE_TAB_COUNT_PAINT_PAGE_LOAD_HISTOGRAM(prefix, bucket, sample)    \
  LIVE_TAB_COUNT_PAGE_LOAD_HISTOGRAM(prefix, bucket, sample,                \
                                     base::TimeDelta::FromMilliseconds(10), \
                                     base::TimeDelta::FromMinutes(10), 100)

// These are the same histogram parameters used for the core page load metric
// of first input dleay (see CorePageLoadMetricsObserver::OnFirstInputInPage).
#define LIVE_TAB_COUNT_INPUT_PAGE_LOAD_HISTOGRAM(prefix, bucket, sample)    \
  LIVE_TAB_COUNT_PAGE_LOAD_HISTOGRAM(prefix, bucket, sample,                \
                                     base::TimeDelta::FromMilliseconds(10), \
                                     base::TimeDelta::FromSeconds(60), 50)

namespace internal {

// Histograms created with the prefix will have suffixes appended corresponding
// to live tab count bucket, e.g. ".ByLiveTabCount.1Tab". So, we don't include
// LiveTabCount in the histogram prefix as it would be redundant.
const char kHistogramPrefixLiveTabCount[] = "PageLoad.";

}  // namespace internal

LiveTabCountPageLoadMetricsObserver::LiveTabCountPageLoadMetricsObserver() {}

LiveTabCountPageLoadMetricsObserver::~LiveTabCountPageLoadMetricsObserver() {}

void LiveTabCountPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    const std::string histogram_prefix(
        std::string(internal::kHistogramPrefixLiveTabCount)
            .append(internal::kHistogramFirstContentfulPaintSuffix));
    const size_t bucket =
        tab_count_metrics::BucketForTabCount(GetLiveTabCount());
    LIVE_TAB_COUNT_PAINT_PAGE_LOAD_HISTOGRAM(
        histogram_prefix, bucket,
        timing.paint_timing->first_contentful_paint.value());
  }
}

void LiveTabCountPageLoadMetricsObserver::
    OnFirstMeaningfulPaintInMainFrameDocument(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, GetDelegate())) {
    const std::string histogram_prefix(
        std::string(internal::kHistogramPrefixLiveTabCount)
            .append(internal::kHistogramFirstMeaningfulPaintSuffix));
    const size_t bucket =
        tab_count_metrics::BucketForTabCount(GetLiveTabCount());
    LIVE_TAB_COUNT_PAINT_PAGE_LOAD_HISTOGRAM(
        histogram_prefix, bucket,
        timing.paint_timing->first_meaningful_paint.value());
  }
}

void LiveTabCountPageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_timestamp, GetDelegate())) {
    const std::string histogram_prefix(
        std::string(internal::kHistogramPrefixLiveTabCount)
            .append(internal::kHistogramFirstInputDelaySuffix));
    const size_t bucket =
        tab_count_metrics::BucketForTabCount(GetLiveTabCount());
    LIVE_TAB_COUNT_INPUT_PAGE_LOAD_HISTOGRAM(
        histogram_prefix, bucket,
        timing.interactive_timing->first_input_delay.value());
  }
}

size_t LiveTabCountPageLoadMetricsObserver::GetLiveTabCount() const {
  return tab_count_metrics::LiveTabCount();
}
