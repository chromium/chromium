// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/omnibox_suggestion_used_page_load_metrics_observer.h"

#include <algorithm>

#include "components/page_load_metrics/browser/page_load_metrics_util.h"

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
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
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
