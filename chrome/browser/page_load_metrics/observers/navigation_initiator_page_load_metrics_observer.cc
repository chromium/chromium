// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/navigation_initiator_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/page_load_metrics/chrome_initiator_location.h"
#include "components/google/core/common/google_util.h"
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
  const ChromeInitiatorLocation initiator_location = [&]() {
    // Back/forward navigation and BFCache restore must be checked before reload
    // because back/forward navigations to an entry that was previously reloaded
    // have a transition type of `PAGE_TRANSITION_RELOAD |
    // PAGE_TRANSITION_FORWARD_BACK`. `PageTransitionCoreTypeIs()` strips
    // qualifiers like `PAGE_TRANSITION_FORWARD_BACK`, so checking for reload
    // first would misclassify back/forward navigations as `kReload`.
    if ((transition & ui::PAGE_TRANSITION_FORWARD_BACK) ||
        navigation_handle.IsServedFromBackForwardCache()) {
      int history_offset = navigation_handle.GetNavigationEntryOffset();
      if (history_offset < 0) {
        return ChromeInitiatorLocation::kBackward;
      } else if (history_offset > 0) {
        return ChromeInitiatorLocation::kForward;
      }
      // `history_offset` can be 0 when a reload navigation is served from
      // BFCache (crbug.com/420769973). Fall through to the subsequent checks so
      // that it is classified as `kReload`.
      // TODO(crbug.com/420769973): Fix this behavior, and avoid the
      // fall-through.
    }
    if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD)) {
      return ChromeInitiatorLocation::kReload;
    }
    // Note: The lookup of the initiator must be done here, not at the
    // beginning of this lambda, to keep the precedence of
    // `ui::PageTransition` above.
    if (std::optional<ChromeInitiatorLocation> attached =
            GetAttachedChromeInitiatorLocation(navigation_handle)) {
      return *attached;
    }
    if (navigation_handle.IsRendererInitiated() &&
        navigation_handle.HasUserGesture()) {
      if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK)) {
        return ChromeInitiatorLocation::kLinkClick;
      }

      if (ui::PageTransitionCoreTypeIs(transition,
                                       ui::PAGE_TRANSITION_FORM_SUBMIT)) {
        return ChromeInitiatorLocation::kFormSubmission;
      }
    }
    return ChromeInitiatorLocation::kOther;
  }();

  base::UmaHistogramEnumeration("Navigation.InitiatorType.All",
                                initiator_location);
  if (is_srp) {
    base::UmaHistogramEnumeration("Navigation.InitiatorType.SRP",
                                  initiator_location);
  }

  if (initiator_location == ChromeInitiatorLocation::kOther) {
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
