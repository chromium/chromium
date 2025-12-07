// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/preload_serving_metrics_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"

PreloadServingMetricsPageLoadMetricsObserver::
    PreloadServingMetricsPageLoadMetricsObserver() = default;

PreloadServingMetricsPageLoadMetricsObserver::
    ~PreloadServingMetricsPageLoadMetricsObserver() = default;

const char* PreloadServingMetricsPageLoadMetricsObserver::GetObserverName()
    const {
  static const char kName[] = "PreloadServingMetricsPageLoadMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreloadServingMetricsPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreloadServingMetricsPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreloadServingMetricsPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreloadServingMetricsPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle) {
    return STOP_OBSERVING;
  }

  if (navigation_handle->IsInPrerenderedMainFrame()) {
    // Wait prerender activation.
    return CONTINUE_OBSERVING;
  }

  // Take `PreloadServingMetrics` of non prerender navigation.
  preload_serving_metrics_capsule_ =
      content::PreloadServingMetricsCapsule::TakeFromNavigationHandle(
          *navigation_handle);
  CHECK(preload_serving_metrics_capsule_);

  return CONTINUE_OBSERVING;
}

void PreloadServingMetricsPageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  // Take `PreloadServingMetrics` of prerender activation navigation.
  preload_serving_metrics_capsule_ =
      content::PreloadServingMetricsCapsule::TakeFromNavigationHandle(
          *navigation_handle);
  CHECK(preload_serving_metrics_capsule_);
}

void PreloadServingMetricsPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // `OnFirstContentfulPaintInPage()` is called after `OnCommit()` and
  // `DidActivatePrerenderedPage()` if prerender.
  CHECK(preload_serving_metrics_capsule_);

  base::TimeDelta corrected =
      page_load_metrics::CorrectEventAsNavigationOrActivationOrigined(
          GetDelegate(), timing.paint_timing->first_contentful_paint.value());
  preload_serving_metrics_capsule_->RecordFirstContentfulPaint(
      std::move(corrected));
}

void PreloadServingMetricsPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  MaybeRecord();
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreloadServingMetricsPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  MaybeRecord();
  return STOP_OBSERVING;
}

void PreloadServingMetricsPageLoadMetricsObserver::MaybeRecord() {
  // Record if the navigation is non prerender and committed; or if the
  // navigations are prerender initial/activation navigation and activated.

  if (!preload_serving_metrics_capsule_) {
    return;
  }

  preload_serving_metrics_capsule_
      ->RecordMetricsForNonPrerenderNavigationCommitted();
}
