// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/gws_page_load_metrics_observer.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

using page_load_metrics::PageAbortReason;

namespace internal {

const char kHistogramGWSFirstContentfulPaint[] =
    "PageLoad.Clients.GoogleSearch.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramGWSLargestContentfulPaint[] =
    "PageLoad.Clients.GoogleSearch.PaintTiming."
    "NavigationToLargestContentfulPaint";
const char kHistogramGWSParseStart[] =
    "PageLoad.Clients.GoogleSearch.ParseTiming.NavigationToParseStart";

}  // namespace internal

GWSPageLoadMetricsObserver::GWSPageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  return page_load_metrics::IsGoogleSearchResultUrl(navigation_handle->GetURL())
             ? CONTINUE_OBSERVING
             : STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class is interested only in events that are preprocessed and
  // dispatched also to the outermost page at PageLoadTracker. So, this class
  // doesn't need to forward events for FencedFrames.
  return STOP_OBSERVING;
}

void GWSPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    return;
  }

  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSFirstContentfulPaint,
                      timing.paint_timing->first_contentful_paint.value());
}

void GWSPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_start, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSParseStart,
                      timing.parse_timing->parse_start.value());
}

void GWSPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  LogMetricsOnComplete();
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  LogMetricsOnComplete();
  return STOP_OBSERVING;
}

void GWSPageLoadMetricsObserver::LogMetricsOnComplete() {
  const page_load_metrics::ContentfulPaintTimingInfo&
      all_frames_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .MergeMainFrameAndSubframes();
  if (!all_frames_largest_contentful_paint.ContainsValidTime() ||
      !WasStartedInForegroundOptionalEventInForeground(
          all_frames_largest_contentful_paint.Time(), GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSLargestContentfulPaint,
                      all_frames_largest_contentful_paint.Time().value());
}
