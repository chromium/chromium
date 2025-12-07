// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/new_tab_page_initiated_page_load_metrics_observer.h"

#include <algorithm>

#include "chrome/browser/search/search.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace {

const char kNewTabPagePrerenderNavigationToActivation[] =
    "NewTabPage.PrerenderNavigationToActivation";
const char kNewTabPageNavigationOrActivationToFirstContentfulPaint[] =
    "NewTabPage.NavigationOrActivationToFirstContentfulPaint";
const char kNewTabPageNavigationOrActivationToFirstMeaningfulPaint[] =
    "NewTabPage.NavigationOrActivationToFirstMeaningfulPaint";
const char kNewTabPageNavigationOrActivationToLargestContentfulPaint[] =
    "NewTabPage.NavigationOrActivationToLargestContentfulPaint";
}  // namespace

NewTabPageInitiatedPageLoadMetricsObserver::NewTabPageInitiatedPageLoadMetricsObserver() =
    default;

NewTabPageInitiatedPageLoadMetricsObserver::~NewTabPageInitiatedPageLoadMetricsObserver() =
    default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NewTabPageInitiatedPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NewTabPageInitiatedPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NewTabPageInitiatedPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class is interested only in the primary page.
  return STOP_OBSERVING;
}

void NewTabPageInitiatedPageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  // `navigation_handle` here is for the activation navigation, while
  // `GetDelegate().GetNavigationStart()` is the start time of initial prerender
  // navigation.
  base::TimeDelta navigation_to_activation =
      navigation_handle->NavigationStart() - GetDelegate().GetNavigationStart();
  PAGE_LOAD_HISTOGRAM(kNewTabPagePrerenderNavigationToActivation,
                      navigation_to_activation);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NewTabPageInitiatedPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  // NavigationHandleUserData is set to be
  // page_load_metrics::NavigationHandleUserData::InitiatorLocation::kNewTabPage
  // for all NewTabPage triggered prerender and non-prerender navigation. The
  // value is checked here to keep on monitoring only NewTabPage triggered
  // cases.
  auto* navigation_userdata =
      page_load_metrics::NavigationHandleUserData::GetForNavigationHandle(
          *navigation_handle);
  if (navigation_userdata && navigation_userdata->navigation_type() ==
                                 page_load_metrics::NavigationHandleUserData::
                                     InitiatorLocation::kNewTabPage) {
    return CONTINUE_OBSERVING;
  }

  return STOP_OBSERVING;
}

void NewTabPageInitiatedPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(timing.paint_timing->first_contentful_paint);
  base::TimeDelta fcp = timing.paint_timing->first_contentful_paint.value();

  if (page_load_metrics::WasActivatedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    base::TimeDelta activation_to_fcp =
        page_load_metrics::CorrectEventAsNavigationOrActivationOrigined(
            GetDelegate(), timing.paint_timing->first_contentful_paint.value());

    PAGE_LOAD_HISTOGRAM(kNewTabPageNavigationOrActivationToFirstContentfulPaint,
                        activation_to_fcp);
  } else if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
                 timing.paint_timing->first_contentful_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(kNewTabPageNavigationOrActivationToFirstContentfulPaint,
                        fcp);
  }
}

void NewTabPageInitiatedPageLoadMetricsObserver::
    OnFirstMeaningfulPaintInMainFrameDocument(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasActivatedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, GetDelegate())) {
    base::TimeDelta activation_to_meaningful_fcp =
        page_load_metrics::CorrectEventAsNavigationOrActivationOrigined(
            GetDelegate(), timing.paint_timing->first_meaningful_paint.value());

    PAGE_LOAD_HISTOGRAM(kNewTabPageNavigationOrActivationToFirstMeaningfulPaint,
                        activation_to_meaningful_fcp);
  } else if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
                 timing.paint_timing->first_meaningful_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(kNewTabPageNavigationOrActivationToFirstMeaningfulPaint,
                        timing.paint_timing->first_meaningful_paint.value());
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NewTabPageInitiatedPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (GetDelegate().DidCommit()) {
    RecordSessionEndHistograms(timing);
  }
  return STOP_OBSERVING;
}

void NewTabPageInitiatedPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSessionEndHistograms(timing);
}

void NewTabPageInitiatedPageLoadMetricsObserver::RecordSessionEndHistograms(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(GetDelegate().DidCommit());
  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();

  if (largest_contentful_paint.ContainsValidTime() &&
      page_load_metrics::WasActivatedInForegroundOptionalEventInForeground(
          largest_contentful_paint.Time(), GetDelegate())) {
    base::TimeDelta activation_to_meaningful_fcp =
        page_load_metrics::CorrectEventAsNavigationOrActivationOrigined(
            GetDelegate(), largest_contentful_paint.Time().value());
    PAGE_LOAD_HISTOGRAM(
        kNewTabPageNavigationOrActivationToLargestContentfulPaint,
        activation_to_meaningful_fcp);
  } else if (largest_contentful_paint.ContainsValidTime() &&
             WasStartedInForegroundOptionalEventInForeground(
                 largest_contentful_paint.Time(), GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        kNewTabPageNavigationOrActivationToLargestContentfulPaint,
        largest_contentful_paint.Time().value());
  }
}
