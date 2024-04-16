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

#define HISTOGRAM_PREFIX "PageLoad.Clients.GoogleSearch."

const char kHistogramGWSNavigationStartToFinalRequestStart[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToFinalRequestStart";
const char kHistogramGWSNavigationStartToFinalResponseStart[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToFinalResponseStart";
const char kHistogramGWSNavigationStartToFinalLoaderCallback[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToFinalLoaderCallback";
const char kHistogramGWSNavigationStartToFirstRequestStart[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToFirstRequestStart";
const char kHistogramGWSNavigationStartToFirstResponseStart[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToFirstResponseStart";
const char kHistogramGWSNavigationStartToFirstLoaderCallback[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToFirstLoaderCallback";
const char kHistogramGWSFirstContentfulPaint[] =
    HISTOGRAM_PREFIX "PaintTiming.NavigationToFirstContentfulPaint";
const char kHistogramGWSLargestContentfulPaint[] =
    HISTOGRAM_PREFIX "PaintTiming.NavigationToLargestContentfulPaint";
const char kHistogramGWSParseStart[] =
    HISTOGRAM_PREFIX "ParseTiming.NavigationToParseStart";

}  // namespace internal

GWSPageLoadMetricsObserver::GWSPageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  if (!page_load_metrics::IsGoogleSearchResultUrl(
          navigation_handle->GetURL())) {
    return STOP_OBSERVING;
  }

  navigation_handle_timing_ = navigation_handle->GetNavigationHandleTiming();
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // TODO(crbug.com/40222513): Handle Prerendering cases.
  return STOP_OBSERVING;
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
  RecordNavigationTimingHistograms();
  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSLargestContentfulPaint,
                      all_frames_largest_contentful_paint.Time().value());
}

void GWSPageLoadMetricsObserver::RecordNavigationTimingHistograms() {
  const base::TimeTicks navigation_start_time =
      GetDelegate().GetNavigationStart();
  const content::NavigationHandleTiming& timing = navigation_handle_timing_;

  // Record metrics for navigation only when all relevant milestones are
  // recorded and in the expected order. It is allowed that they have the same
  // value for some cases (e.g., internal redirection for HSTS).
  if (navigation_start_time.is_null() ||
      timing.first_request_start_time.is_null() ||
      timing.first_response_start_time.is_null() ||
      timing.first_loader_callback_time.is_null() ||
      timing.final_request_start_time.is_null() ||
      timing.final_response_start_time.is_null() ||
      timing.final_loader_callback_time.is_null() ||
      timing.navigation_commit_sent_time.is_null()) {
    return;
  }

  // Record the elapsed time from the navigation start milestone.
  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSNavigationStartToFirstRequestStart,
                      timing.first_request_start_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramGWSNavigationStartToFirstResponseStart,
      timing.first_response_start_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramGWSNavigationStartToFirstLoaderCallback,
      timing.first_loader_callback_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSNavigationStartToFinalRequestStart,
                      timing.final_request_start_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramGWSNavigationStartToFinalResponseStart,
      timing.final_response_start_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramGWSNavigationStartToFinalLoaderCallback,
      timing.final_loader_callback_time - navigation_start_time);
}
