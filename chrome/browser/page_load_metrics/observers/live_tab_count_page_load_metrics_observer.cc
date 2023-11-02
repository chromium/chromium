// Copyright 2018 The Chromium Authors
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

#define LIVE_TAB_COUNT_HISTOGRAM(prefix, bucket, sample, histogram_min,     \
                                 histogram_max, histogram_buckets)          \
  STATIC_HISTOGRAM_POINTER_GROUP(                                           \
      tab_count_metrics::HistogramName(prefix, /* live_tabs_only = */ true, \
                                       bucket),                             \
      static_cast<int>(bucket),                                             \
      static_cast<int>(tab_count_metrics::kNumTabCountBuckets),             \
      AddTimeMillisecondsGranularity(sample),                               \
      base::Histogram::FactoryTimeGet(                                      \
          tab_count_metrics::HistogramName(                                 \
              prefix, /* live_tabs_only = */ true, bucket),                 \
          histogram_min, histogram_max, histogram_buckets,                  \
          base::HistogramBase::kUmaTargetedHistogramFlag))

// These are the same histogram parameters used for the core page load paint
// timing metrics (see PAGE_LOAD_HISTOGRAM).
#define LIVE_TAB_COUNT_PAINT_PAGE_LOAD_HISTOGRAM(prefix, bucket, sample)   \
  LIVE_TAB_COUNT_HISTOGRAM(prefix, bucket, sample, base::Milliseconds(10), \
                           base::Minutes(10), 100)

namespace internal {

// Histograms created with the prefix will have suffixes appended corresponding
// to live tab count bucket, e.g. ".ByLiveTabCount.1Tab". So, we don't include
// LiveTabCount in the histogram prefix as it would be redundant.
const char kHistogramPrefixLiveTabCount[] = "PageLoad.";

}  // namespace internal

LiveTabCountPageLoadMetricsObserver::LiveTabCountPageLoadMetricsObserver() {}

LiveTabCountPageLoadMetricsObserver::~LiveTabCountPageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
LiveTabCountPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class doesn't use information on subframes and inner pages. No need to
  // forward.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
LiveTabCountPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return CONTINUE_OBSERVING;
}

void LiveTabCountPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  const base::TimeDelta event =
      timing.paint_timing->first_contentful_paint.value();

  if (!page_load_metrics::EventOccurredBeforeNonPrerenderingBackgroundStart(
          GetDelegate(), timing, event)) {
    return;
  }

  base::TimeDelta corrected =
      page_load_metrics::CorrectEventAsNavigationOrActivationOrigined(
          GetDelegate(), timing, event);
  const size_t bucket = tab_count_metrics::BucketForTabCount(GetLiveTabCount());
  LIVE_TAB_COUNT_PAINT_PAGE_LOAD_HISTOGRAM(
      std::string(internal::kHistogramPrefixLiveTabCount)
          .append(internal::kHistogramFirstContentfulPaintSuffix),
      bucket, corrected);
}

void LiveTabCountPageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  const base::TimeDelta event =
      timing.interactive_timing->first_input_delay.value();

  if (!page_load_metrics::EventOccurredBeforeNonPrerenderingBackgroundStart(
          GetDelegate(), timing, event)) {
    return;
  }

  // These are the same histogram parameters used for the non-suffixed
  // FirstInputDelay histogram (see
  // UmaPageLoadMetricsObserver::OnFirstInputInPage).
  const size_t bucket = tab_count_metrics::BucketForTabCount(GetLiveTabCount());
  LIVE_TAB_COUNT_HISTOGRAM(
      std::string(internal::kHistogramPrefixLiveTabCount)
          .append(internal::kHistogramFirstInputDelaySuffix),
      bucket, event, base::Milliseconds(1), base::Seconds(60), 50);
}

size_t LiveTabCountPageLoadMetricsObserver::GetLiveTabCount() const {
  return tab_count_metrics::LiveTabCount();
}
