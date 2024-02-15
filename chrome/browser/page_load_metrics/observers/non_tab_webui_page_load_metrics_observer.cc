// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/non_tab_webui_page_load_metrics_observer.h"

#include "base/strings/strcat.h"
#include "base/trace_event/named_trigger.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"

namespace chrome {

const char kLCPHistogramName[] =
    "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.NonTabWebUI";

const char kFCPHistogramName[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.NonTabWebUI";

std::string GetSuffixedLCPHistogram(const std::string& webui_name) {
  return base::StrCat({kLCPHistogramName, ".", webui_name});
}

std::string GetSuffixedFCPHistogram(const std::string& webui_name) {
  return base::StrCat({kFCPHistogramName, ".", webui_name});
}

NonTabPageLoadMetricsObserver::NonTabPageLoadMetricsObserver(
    const std::string& webui_name)
    : page_load_metrics::PageLoadMetricsObserver(), webui_name_(webui_name) {
  base::trace_event::EmitNamedTrigger("non-tab-webui-creation");
}

void NonTabPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(timing.paint_timing->first_contentful_paint.has_value());

  PAGE_LOAD_HISTOGRAM(kFCPHistogramName,
                      timing.paint_timing->first_contentful_paint.value());
  PAGE_LOAD_HISTOGRAM(GetSuffixedFCPHistogram(webui_name_),
                      timing.paint_timing->first_contentful_paint.value());
}

void NonTabPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  const page_load_metrics::ContentfulPaintTimingInfo&
      main_frame_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .MainFrameLargestContentfulPaint();
  // It's possible to get here and for LCP timing to not be available.
  if (main_frame_largest_contentful_paint.ContainsValidTime()) {
    PAGE_LOAD_HISTOGRAM(kLCPHistogramName,
                        main_frame_largest_contentful_paint.Time().value());
    PAGE_LOAD_HISTOGRAM(GetSuffixedLCPHistogram(webui_name_),
                        main_frame_largest_contentful_paint.Time().value());
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NonTabPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NonTabPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

}  // namespace chrome
