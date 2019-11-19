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
const char kPrerenderSearchFirstContentfulPaint[] =
    "Omnibox.SuggestionUsed.Search.ForegroundToFirstContentfulPaint.Prerender";
const char kPrerenderURLFirstContentfulPaint[] =
    "Omnibox.SuggestionUsed.URL.ForegroundToFirstContentfulPaint.Prerender";

const char kSearchFirstMeaningfulPaint[] =
    "Omnibox.SuggestionUsed.Search.Experimental."
    "NavigationToFirstMeaningfulPaint";
const char kURLFirstMeaningfulPaint[] =
    "Omnibox.SuggestionUsed.URL.Experimental.NavigationToFirstMeaningfulPaint";
const char kPrerenderSearchFirstMeaningfulPaint[] =
    "Omnibox.SuggestionUsed.Search.Experimental."
    "ForegroundToFirstMeaningfulPaint.Prerender";
const char kPrerenderURLFirstMeaningfulPaint[] =
    "Omnibox.SuggestionUsed.URL.Experimental."
    "ForegroundToFirstMeaningfulPaint.Prerender";

const char kPrerenderSearchNavigationToFirstForeground[] =
    "Omnibox.SuggestionUsed.Search.NavigationToFirstForeground.Prerender";
const char kPrerenderURLNavigationToFirstForeground[] =
    "Omnibox.SuggestionUsed.URL.NavigationToFirstForeground.Prerender";

}  // namespace

OmniboxSuggestionUsedMetricsObserver::OmniboxSuggestionUsedMetricsObserver(
    bool is_prerender)
    : is_prerender_(is_prerender) {}

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
  // Since a page is not supposed to paint in the background,
  // when this function gets called, first_foreground_time should be set.
  // We add this check just to be safe.
  if (is_prerender_ && GetDelegate().GetFirstForegroundTime()) {
    base::TimeDelta perceived_fcp =
        std::max(base::TimeDelta(),
                 fcp - GetDelegate().GetFirstForegroundTime().value());
    if (ui::PageTransitionCoreTypeIs(transition_type_,
                                     ui::PAGE_TRANSITION_GENERATED)) {
      PAGE_LOAD_HISTOGRAM(kPrerenderSearchFirstContentfulPaint, perceived_fcp);
      PAGE_LOAD_HISTOGRAM(kPrerenderSearchNavigationToFirstForeground,
                          GetDelegate().GetFirstForegroundTime().value());
    } else if (ui::PageTransitionCoreTypeIs(transition_type_,
                                            ui::PAGE_TRANSITION_TYPED)) {
      PAGE_LOAD_HISTOGRAM(kPrerenderURLFirstContentfulPaint, perceived_fcp);
      PAGE_LOAD_HISTOGRAM(kPrerenderURLNavigationToFirstForeground,
                          GetDelegate().GetFirstForegroundTime().value());
    }
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
  } else if (is_prerender_ && GetDelegate().GetFirstForegroundTime()) {
    base::TimeDelta perceived_fmp =
        std::max(base::TimeDelta(),
                 fmp - GetDelegate().GetFirstForegroundTime().value());
    if (ui::PageTransitionCoreTypeIs(transition_type_,
                                     ui::PAGE_TRANSITION_GENERATED)) {
      PAGE_LOAD_HISTOGRAM(kPrerenderSearchFirstMeaningfulPaint, perceived_fmp);
    } else if (ui::PageTransitionCoreTypeIs(transition_type_,
                                            ui::PAGE_TRANSITION_TYPED)) {
      PAGE_LOAD_HISTOGRAM(kPrerenderURLFirstMeaningfulPaint, perceived_fmp);
    }
  }
}
