// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/omnibox_suggestion_used_page_load_metrics_observer.h"

#include <algorithm>

#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/page_transition_types.h"

namespace {

const char kSearchInputToNavigationStart[] =
    "Omnibox.SuggestionUsed.Search.InputToNavigationStart2";
const char kURLInputToNavigationStart[] =
    "Omnibox.SuggestionUsed.URL.InputToNavigationStart2";

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
    "Omnibox.SuggestionUsed.Search.NavigationToLargestContentfulPaint2.1";
const char kURLLargestContentfulPaint2[] =
    "Omnibox.SuggestionUsed.URL.NavigationToLargestContentfulPaint2.1";

const char kSearchLargestContentfulPaint2Above2s[] =
    "Omnibox.SuggestionUsed.Search.NavigationToLargestContentfulPaint"
    "2.1Above2s";
}  // namespace

OmniboxSuggestionUsedMetricsObserver::OmniboxSuggestionUsedMetricsObserver() =
    default;

OmniboxSuggestionUsedMetricsObserver::~OmniboxSuggestionUsedMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
OmniboxSuggestionUsedMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // We don't record metrics for prerendering in this class. Instead, we record
  // variant metrics in PrerenderPageLoadMetricsObserver. For example,
  // PageLoad.Clients.Prerender.PaintTiming.ActivationToFirstContentfulPaint.Embedder_DirectURLInput
  // instead of
  // Omnibox.SuggestionUsed.URL.NavigationToFirstContentfulPaint.
  //
  // For more details, see
  // https://docs.google.com/document/d/10FNXtDdWdEA79VTI45VbmHjkFm0OYujZ8wT-etbah4I/edit?resourcekey=0-_w_JjHBzSLWWaniuNxXqFQ
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
OmniboxSuggestionUsedMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class is interested only in the primary page.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
OmniboxSuggestionUsedMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  transition_type_ = navigation_handle->GetPageTransition();
  if (!ui::PageTransitionIsNewNavigation(transition_type_)) {
    return STOP_OBSERVING;
  }
  return (transition_type_ & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) != 0
             ? CONTINUE_OBSERVING
             : STOP_OBSERVING;
}

void OmniboxSuggestionUsedMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(timing.paint_timing->first_contentful_paint);
  base::TimeDelta fcp = timing.paint_timing->first_contentful_paint.value();

  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    if (ui::PageTransitionCoreTypeIs(transition_type_,
                                     ui::PAGE_TRANSITION_GENERATED)) {
      if (timing.input_to_navigation_start) {
        // Use `PAGE_LOAD_SHORT_HISTOGRAM()`, and not `PAGE_LOAD_HISTOGRAM()`,
        // as this has many (30-55%) of events <10ms, and almost no events
        // >1minute.
        PAGE_LOAD_SHORT_HISTOGRAM(kSearchInputToNavigationStart,
                                  timing.input_to_navigation_start.value());
      }
      PAGE_LOAD_HISTOGRAM(kSearchFirstContentfulPaint, fcp);
    } else if (ui::PageTransitionCoreTypeIs(transition_type_,
                                            ui::PAGE_TRANSITION_TYPED)) {
      if (timing.input_to_navigation_start) {
        // Use `PAGE_LOAD_SHORT_HISTOGRAM()`, and not `PAGE_LOAD_HISTOGRAM()`,
        // as this has many (30-55%) of events <10ms, and almost no events
        // >1minute.
        PAGE_LOAD_SHORT_HISTOGRAM(kURLInputToNavigationStart,
                                  timing.input_to_navigation_start.value());
      }
      PAGE_LOAD_HISTOGRAM(kURLFirstContentfulPaint, fcp);
    }
    return;
  }
}

void OmniboxSuggestionUsedMetricsObserver::
    OnFirstMeaningfulPaintInMainFrameDocument(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  base::TimeDelta fmp = timing.paint_timing->first_meaningful_paint.value();

  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, GetDelegate())) {
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
  if (GetDelegate().DidCommit()) {
    RecordSessionEndHistograms(timing);
  }
  return STOP_OBSERVING;
}

void OmniboxSuggestionUsedMetricsObserver::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSearchLCP2Above2s();
}

void OmniboxSuggestionUsedMetricsObserver::RecordSearchLCP2Above2s() {
  if (lcp2_above_2s_recorded_) {
    return;
  }
  const page_load_metrics::ContentfulPaintTimingInfo& paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();
  if (!paint.ContainsValidTime() || paint.Time()->InMilliseconds() < 2000) {
    return;
  }
  if (!ui::PageTransitionCoreTypeIs(transition_type_,
                                    ui::PAGE_TRANSITION_GENERATED)) {
    // Not a Search load.
    return;
  }

  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          paint.Time(), GetDelegate())) {
    // Page was started in background or was backgrounded before reaching this
    // LCP value.
    return;
  }
  PAGE_LOAD_HISTOGRAM(kSearchLargestContentfulPaint2Above2s,
                      paint.Time().value());
  lcp2_above_2s_recorded_ = true;
}

void OmniboxSuggestionUsedMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSessionEndHistograms(timing);
}

void OmniboxSuggestionUsedMetricsObserver::RecordSessionEndHistograms(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK(GetDelegate().DidCommit());

  RecordSearchLCP2Above2s();

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
