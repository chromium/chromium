// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/scheme_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SchemePageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return started_in_foreground ? CONTINUE_OBSERVING : STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SchemePageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This observer is interested in comparing performance among HTTP and HTTPS.
  // Including prerendering cases can be another factor to differentiate
  // performance, and it will be a noise for the original goal.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SchemePageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  // Capture committed transition type.
  transition_ = navigation_handle->GetPageTransition();
  if (navigation_handle->GetURL().scheme() == url::kHttpScheme ||
      navigation_handle->GetURL().scheme() == url::kHttpsScheme) {
    return CONTINUE_OBSERVING;
  }
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SchemePageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class is interested in events that are dispatched only for the primary
  // page or preprocessed by PageLoadTracker to be per-outermost page. So, no
  // need to forward events at the observer layer.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SchemePageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  return STOP_OBSERVING;
}

void SchemePageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (GetDelegate().GetUrl().scheme() == url::kHttpScheme) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTP.ParseTiming.NavigationToParseStart",
        timing.parse_timing->parse_start.value());
  } else if (GetDelegate().GetUrl().scheme() == url::kHttpsScheme) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTPS.ParseTiming.NavigationToParseStart",
        timing.parse_timing->parse_start.value());
  }
}

void SchemePageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK(GetDelegate().GetUrl().scheme() == url::kHttpScheme ||
         GetDelegate().GetUrl().scheme() == url::kHttpsScheme);

  base::TimeDelta fcp = timing.paint_timing->first_contentful_paint.value();
  base::TimeDelta parse_start_to_fcp =
      fcp - timing.parse_timing->parse_start.value();

  if (GetDelegate().GetUrl().scheme() == url::kHttpScheme) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTP.PaintTiming."
        "NavigationToFirstContentfulPaint",
        fcp);
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTP.PaintTiming."
        "ParseStartToFirstContentfulPaint",
        parse_start_to_fcp);
  } else {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTPS.PaintTiming."
        "NavigationToFirstContentfulPaint",
        fcp);
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTPS.PaintTiming."
        "ParseStartToFirstContentfulPaint",
        parse_start_to_fcp);
  }
}

void SchemePageLoadMetricsObserver::OnFirstMeaningfulPaintInMainFrameDocument(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (GetDelegate().GetUrl().scheme() == url::kHttpScheme) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTP.Experimental.PaintTiming."
        "NavigationToFirstMeaningfulPaint",
        timing.paint_timing->first_meaningful_paint.value());
  } else if (GetDelegate().GetUrl().scheme() == url::kHttpsScheme) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTPS.Experimental.PaintTiming."
        "NavigationToFirstMeaningfulPaint",
        timing.paint_timing->first_meaningful_paint.value());
  }
}
