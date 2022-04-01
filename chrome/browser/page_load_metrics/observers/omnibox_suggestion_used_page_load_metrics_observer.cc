// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/omnibox_suggestion_used_page_load_metrics_observer.h"

#include <algorithm>

#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"

namespace {

const char kSearchFirstContentfulPaint[] =
    "Omnibox.SuggestionUsed.Search.NavigationToFirstContentfulPaint";
const char kURLFirstContentfulPaint[] =
    "Omnibox.SuggestionUsed.URL.NavigationToFirstContentfulPaint";

const char kSearchFirstMeaningfulPaint[] =
    "Omnibox.SuggestionUsed.Search.Experimental."
    "NavigationToFirstMeaningfulPaint";
const char kURLFirstMeaningfulPaint[] =
    "Omnibox.SuggestionUsed.URL.Experimental.NavigationToFirstMeaningfulPaint";

const char kSearchLargestContentfulPaint2[] =
    "Omnibox.SuggestionUsed.Search.NavigationToLargestContentfulPaint2";
const char kURLLargestContentfulPaint2[] =
    "Omnibox.SuggestionUsed.URL.NavigationToLargestContentfulPaint2";
}  // namespace

OmniboxSuggestionUsedMetricsObserver::OmniboxSuggestionUsedMetricsObserver() =
    default;

OmniboxSuggestionUsedMetricsObserver::~OmniboxSuggestionUsedMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
OmniboxSuggestionUsedMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
OmniboxSuggestionUsedMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  transition_type_ = navigation_handle->GetPageTransition();
  return (transition_type_ & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) != 0
             ? CONTINUE_OBSERVING
             : STOP_OBSERVING;
}

void OmniboxSuggestionUsedMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  base::TimeDelta fcp = timing.paint_timing->first_contentful_paint.value();

  if (GetDelegate().StartedInForeground()) {
    if (ui::PageTransitionCoreTypeIs(transition_type_,
                                     ui::PAGE_TRANSITION_GENERATED)) {
      PAGE_LOAD_HISTOGRAM(kSearchFirstContentfulPaint, fcp);
    } else if (ui::PageTransitionCoreTypeIs(transition_type_,
                                            ui::PAGE_TRANSITION_TYPED)) {
      PAGE_LOAD_HISTOGRAM(kURLFirstContentfulPaint, fcp);
    }
    return;
  }
}

void OmniboxSuggestionUsedMetricsObserver::
    OnFirstMeaningfulPaintInMainFrameDocument(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  base::TimeDelta fmp = timing.paint_timing->first_meaningful_paint.value();

  if (GetDelegate().StartedInForeground()) {
    if (ui::PageTransitionCoreTypeIs(transition_type_,
                                     ui::PAGE_TRANSITION_GENERATED)) {
      PAGE_LOAD_HISTOGRAM(kSearchFirstMeaningfulPaint, fmp);
    } else if (ui::PageTransitionCoreTypeIs(transition_type_,
                                            ui::PAGE_TRANSITION_TYPED)) {
      PAGE_LOAD_HISTOGRAM(kURLFirstMeaningfulPaint, fmp);
    }
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
OmniboxSuggestionUsedMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (GetDelegate().DidCommit())
    RecordSessionEndHistograms(timing);
  return STOP_OBSERVING;
}

void OmniboxSuggestionUsedMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSessionEndHistograms(timing);
}

void OmniboxSuggestionUsedMetricsObserver::RecordSessionEndHistograms(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK(GetDelegate().DidCommit());

  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();
  if (largest_contentful_paint.ContainsValidTime() &&
      page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          largest_contentful_paint.Time(), GetDelegate())) {
    if (ui::PageTransitionCoreTypeIs(transition_type_,
                                     ui::PAGE_TRANSITION_GENERATED)) {
      PAGE_LOAD_HISTOGRAM(kSearchLargestContentfulPaint2,
                          largest_contentful_paint.Time().value());
    } else if (ui::PageTransitionCoreTypeIs(transition_type_,
                                            ui::PAGE_TRANSITION_TYPED)) {
      PAGE_LOAD_HISTOGRAM(kURLLargestContentfulPaint2,
                          largest_contentful_paint.Time().value());
    }
  }
}
