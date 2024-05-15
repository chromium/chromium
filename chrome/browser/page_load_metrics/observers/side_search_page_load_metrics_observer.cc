// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/side_search_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/side_search/side_search_side_contents_helper.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"

namespace internal {

const char kSideSearchFirstContentfulPaint[] =
    "PageLoad.Clients.SideSearch.SidePanel.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kSideSearchFirstMeaningfulPaint[] =
    "PageLoad.Clients.SideSearch.SidePanel.Experimental.PaintTiming."
    "NavigationToFirstMeaningfulPaint";
const char kSideSearchInteractiveInputDelay[] =
    "PageLoad.Clients.SideSearch.SidePanel.InteractiveTiming.FirstInputDelay4";
const char kSideSearchLargestContentfulPaint[] =
    "PageLoad.Clients.SideSearch.SidePanel.PaintTiming."
    "NavigationToLargestContentfulPaint2";
const char kSideSearchMaxCumulativeShiftScore[] =
    "PageLoad.Clients.SideSearch.SidePanel.LayoutInstability."
    "MaxCumulativeShiftScore."
    "SessionWindow.Gap1000ms.Max5000ms2";

}  // namespace internal

// static
std::unique_ptr<SideSearchPageLoadMetricsObserver>
SideSearchPageLoadMetricsObserver::CreateIfNeeded(
    content::WebContents* web_contents) {
  // The side panel WebContents hosting the SRP for Side Search will have a
  // SideSearchSideContentsHelper if the feature is enabled. If this is present
  // create the page load metrics observer.
  if (!SideSearchSideContentsHelper::FromWebContents(web_contents))
    return nullptr;

  return std::make_unique<SideSearchPageLoadMetricsObserver>();
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SideSearchPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class is interested only in events that are preprocessed and
  // dispatched also to the outermost page at PageLoadTracker. So, this class
  // doesn't need to forward events for FencedFrames.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SideSearchPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // There is no prerendering trigger for contents in the side panel.

  // Note that here is not reachable because this class is only registered for
  // the WebContents of the side panel. Especially, the navigation is not
  // prerendering. See
  // chrome/browser/page_load_metrics/page_load_metrics_initialize.cc
  NOTREACHED_IN_MIGRATION();
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SideSearchPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSessionEndHistograms();
  return STOP_OBSERVING;
}

void SideSearchPageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_timestamp, GetDelegate())) {
    INPUT_DELAY_HISTOGRAM(internal::kSideSearchInteractiveInputDelay,
                          timing.interactive_timing->first_input_delay.value());
  }
}

void SideSearchPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint.value(), GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kSideSearchFirstContentfulPaint,
                        timing.paint_timing->first_contentful_paint.value());
  }
}

void SideSearchPageLoadMetricsObserver::
    OnFirstMeaningfulPaintInMainFrameDocument(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint.value(), GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kSideSearchFirstMeaningfulPaint,
                        timing.paint_timing->first_meaningful_paint.value());
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SideSearchPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSessionEndHistograms();
  return STOP_OBSERVING;
}

void SideSearchPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSessionEndHistograms();
}

void SideSearchPageLoadMetricsObserver::RecordSessionEndHistograms() {
  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();
  if (largest_contentful_paint.ContainsValidTime() &&
      page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          largest_contentful_paint.Time(), GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kSideSearchLargestContentfulPaint,
                        largest_contentful_paint.Time().value());
  }

  const page_load_metrics::NormalizedCLSData& normalized_cls_data =
      GetDelegate().GetNormalizedCLSData(
          page_load_metrics::PageLoadMetricsObserverDelegate::BfcacheStrategy::
              ACCUMULATE);
  if (!normalized_cls_data.data_tainted) {
    page_load_metrics::UmaMaxCumulativeShiftScoreHistogram10000x(
        internal::kSideSearchMaxCumulativeShiftScore, normalized_cls_data);
  }
}
