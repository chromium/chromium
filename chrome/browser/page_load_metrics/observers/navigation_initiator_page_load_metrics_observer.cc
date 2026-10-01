// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/navigation_initiator_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "components/google/core/common/google_util.h"
#include "components/page_load_metrics/browser/navigation_initiator.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/page_transition_types.h"

namespace {

void RecordInitiatorMetrics(content::NavigationHandle& navigation_handle) {
  const ui::PageTransition transition = navigation_handle.GetPageTransition();
  // Note: This must be consistent with the SRP judgement of
  // `PreloadServingMetricsPageLoadMetricsObserver` so that
  // `Navigation.InitiatorType.SRP` and `PreloadServingMetrics.*.SRP` are
  // comparable.
  bool is_srp = google_util::IsGoogleSearchUrl(navigation_handle.GetURL());

  const page_load_metrics::NavigationInitiator initiator =
      page_load_metrics::GetNavigationInitiator(navigation_handle);

  // Note: `UmaHistogramExactLinear()`, not `UmaHistogramEnumeration()`, as
  // `page_load_metrics::NavigationInitiator` is an open enum whose values are
  // defined across the layers.
  base::UmaHistogramExactLinear(
      "Navigation.InitiatorType.All", initiator.id(),
      page_load_metrics::NavigationInitiator::kIdExclusiveMax);

  if (is_srp) {
    base::UmaHistogramExactLinear(
        "Navigation.InitiatorType.SRP", initiator.id(),
        page_load_metrics::NavigationInitiator::kIdExclusiveMax);
  }

  if (initiator == page_load_metrics::navigation_initiator::kOther) {
    base::UmaHistogramSparse("Navigation.UnknownInitiator.PageTransition.All",
                             transition);
    if (is_srp) {
      base::UmaHistogramSparse("Navigation.UnknownInitiator.PageTransition.SRP",
                               transition);
    }
  }
}

}  // namespace

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NavigationInitiatorPageLoadMetricsObserver::OnEnterBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  return CONTINUE_OBSERVING;
}

void NavigationInitiatorPageLoadMetricsObserver::OnRestoreFromBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    content::NavigationHandle* navigation_handle) {
  RecordInitiatorMetrics(*navigation_handle);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NavigationInitiatorPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NavigationInitiatorPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return CONTINUE_OBSERVING;
}

void NavigationInitiatorPageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  RecordInitiatorMetrics(*navigation_handle);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NavigationInitiatorPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  CHECK(navigation_handle);

  if (navigation_handle->IsInPrerenderedMainFrame()) {
    return CONTINUE_OBSERVING;
  }

  CHECK(navigation_handle->IsInPrimaryMainFrame());

  RecordInitiatorMetrics(*navigation_handle);

  return CONTINUE_OBSERVING;
}
