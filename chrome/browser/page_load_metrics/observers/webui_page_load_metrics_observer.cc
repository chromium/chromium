// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/webui_page_load_metrics_observer.h"

#include "base/logging.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/url_constants.h"

namespace {
constexpr char kWebUINavigationToFCPHistogramName[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.WebUI";

constexpr char kWebUINavigationToLCPHistogramName[] =
    "PageLoad.PaintTiming.NavigationToLargestContentfulPaint.WebUI";
}  // namespace

WebUIPageLoadMetricsObserver::WebUIPageLoadMetricsObserver() = default;

WebUIPageLoadMetricsObserver::~WebUIPageLoadMetricsObserver() = default;

void WebUIPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(timing.paint_timing->first_contentful_paint.has_value());
  PAGE_LOAD_HISTOGRAM(kWebUINavigationToFCPHistogramName,
                      timing.paint_timing->first_contentful_paint.value());
  PAGE_LOAD_HISTOGRAM(base::StrCat({kWebUINavigationToFCPHistogramName, ".",
                                    GetDelegate().GetUrl().host()}),
                      timing.paint_timing->first_contentful_paint.value());
}

void WebUIPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSessionEndHistograms();
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
WebUIPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!GetDelegate().DidCommit()) {
    return STOP_OBSERVING;
  }

  // If we successfully recorded the histogram, stop observing.
  // Otherwise, continue observing in case LCP becomes available later.
  return RecordSessionEndHistograms() ? STOP_OBSERVING : CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
WebUIPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
WebUIPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
WebUIPageLoadMetricsObserver::ShouldObserveScheme(const GURL& url) const {
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(content::kChromeUIUntrustedScheme)) {
    return CONTINUE_OBSERVING;
  }
  return STOP_OBSERVING;
}

bool WebUIPageLoadMetricsObserver::RecordSessionEndHistograms() {
  const page_load_metrics::ContentfulPaintTimingInfo&
      main_frame_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .MainFrameLargestContentfulPaint();

  // It's possible to get here and for LCP timing to not be available.
  if (!main_frame_largest_contentful_paint.ContainsValidTime()) {
    return false;
  }

  PAGE_LOAD_HISTOGRAM(kWebUINavigationToLCPHistogramName,
                      main_frame_largest_contentful_paint.Time().value());
  PAGE_LOAD_HISTOGRAM(base::StrCat({kWebUINavigationToLCPHistogramName, ".",
                                    GetDelegate().GetUrl().host()}),
                      main_frame_largest_contentful_paint.Time().value());

  return true;
}
