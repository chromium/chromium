// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/preview_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// As we don't identify client redirect cases, kPassingVisit may be
// overestimated a little.
enum class PageVisitType {
  kIndependentVisit = 0,
  kOriginVisit = 1,
  kPassingVisit = 2,
  kTerminalVisit = 3,
  kMaxValue = kTerminalVisit,
};

PageVisitType RecordPageVisitType(
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  PageVisitType type;
  if (delegate.IsOriginVisit()) {
    type = delegate.IsTerminalVisit() ? PageVisitType::kIndependentVisit
                                      : PageVisitType::kOriginVisit;
  } else {
    type = delegate.IsTerminalVisit() ? PageVisitType::kTerminalVisit
                                      : PageVisitType::kPassingVisit;
  }
  base::UmaHistogramEnumeration("PageLoad.Experimental.PageVisitType", type);
  return type;
}

}  // namespace

PreviewPageLoadMetricsObserver::ObservePolicy
PreviewPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  if (started_in_foreground) {
    last_time_shown_ = navigation_handle->NavigationStart();
  }
  currently_in_foreground_ = started_in_foreground;
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Track only outermost pages.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return CONTINUE_OBSERVING;
}

PreviewPageLoadMetricsObserver::ObservePolicy
PreviewPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordMetrics();
  return STOP_OBSERVING;
}

PreviewPageLoadMetricsObserver::ObservePolicy
PreviewPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (currently_in_foreground_ && !last_time_shown_.is_null()) {
    total_foreground_duration_ += base::TimeTicks::Now() - last_time_shown_;
  }
  currently_in_foreground_ = false;
  return CONTINUE_OBSERVING;
}

PreviewPageLoadMetricsObserver::ObservePolicy
PreviewPageLoadMetricsObserver::OnShown() {
  currently_in_foreground_ = true;
  last_time_shown_ = base::TimeTicks::Now();
  return CONTINUE_OBSERVING;
}

void PreviewPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordMetrics();
}

void PreviewPageLoadMetricsObserver::RecordMetrics() {
  PageVisitType page_visit_type = RecordPageVisitType(GetDelegate());
  if (currently_in_foreground_ && !last_time_shown_.is_null()) {
    total_foreground_duration_ += base::TimeTicks::Now() - last_time_shown_;
  }
  PAGE_LOAD_LONG_HISTOGRAM(
      "PageLoad.Experimental.TotalForegroundDuration.AllVisit",
      total_foreground_duration_);
  switch (page_visit_type) {
    case PageVisitType::kIndependentVisit:
      PAGE_LOAD_LONG_HISTOGRAM(
          "PageLoad.Experimental.TotalForegroundDuration.IndependentVisit",
          total_foreground_duration_);
      break;
    case PageVisitType::kOriginVisit:
      PAGE_LOAD_LONG_HISTOGRAM(
          "PageLoad.Experimental.TotalForegroundDuration.OriginVisit",
          total_foreground_duration_);
      break;
    case PageVisitType::kPassingVisit:
      PAGE_LOAD_LONG_HISTOGRAM(
          "PageLoad.Experimental.TotalForegroundDuration.PassingVisit",
          total_foreground_duration_);
      break;
    case PageVisitType::kTerminalVisit:
      PAGE_LOAD_LONG_HISTOGRAM(
          "PageLoad.Experimental.TotalForegroundDuration.TerminalVisit",
          total_foreground_duration_);
      break;
  }
}
