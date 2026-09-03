// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/navigation_initiator_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/page_load_metrics/chrome_initiator_location.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/page_load_metrics/google/browser/google_url_util.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

namespace {

void RecordInitiatorMetrics(content::NavigationHandle& navigation_handle) {
  bool is_srp =
      page_load_metrics::IsGoogleSearchResultUrl(navigation_handle.GetURL());
  auto* navigation_handle_user_data =
      page_load_metrics::NavigationHandleUserData::GetForNavigationHandle(
          navigation_handle);
  const ChromeInitiatorLocation initiator_location = [&]() {
    if (ui::PageTransitionCoreTypeIs(navigation_handle.GetPageTransition(),
                                     ui::PAGE_TRANSITION_RELOAD)) {
      return ChromeInitiatorLocation::kReload;
    }
    if ((navigation_handle.GetPageTransition() &
         ui::PAGE_TRANSITION_FORWARD_BACK) ||
        navigation_handle.IsServedFromBackForwardCache()) {
      int history_offset = navigation_handle.GetNavigationEntryOffset();
      CHECK_NE(history_offset, 0);
      if (history_offset < 0) {
        return ChromeInitiatorLocation::kBackward;
      } else if (history_offset > 0) {
        return ChromeInitiatorLocation::kForward;
      }
    }
    if (navigation_handle_user_data) {
      return GetChromeInitiatorLocation(
          navigation_handle_user_data->navigation_type());
    }
    if (navigation_handle.IsRendererInitiated() &&
        navigation_handle.HasUserGesture() &&
        ui::PageTransitionCoreTypeIs(navigation_handle.GetPageTransition(),
                                     ui::PAGE_TRANSITION_LINK)) {
      return ChromeInitiatorLocation::kLinkClick;
    }
    return ChromeInitiatorLocation::kOther;
  }();

  base::UmaHistogramEnumeration("Navigation.InitiatorType.All",
                                initiator_location);
  if (is_srp) {
    base::UmaHistogramEnumeration("Navigation.InitiatorType.SRP",
                                  initiator_location);
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
