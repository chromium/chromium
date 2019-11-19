// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/multi_tab_loading_page_load_metrics_observer.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/page_load_metrics/observers/histogram_suffixes.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace internal {

const char kHistogramPrefixMultiTabLoading[] =
    "PageLoad.Clients.MultiTabLoading.";
const char kHistogramPrefixMultiTabLoading2OrMore[] =
    "PageLoad.Clients.MultiTabLoading.2OrMore.";
const char kHistogramPrefixMultiTabLoading5OrMore[] =
    "PageLoad.Clients.MultiTabLoading.5OrMore.";

}  // namespace internal

MultiTabLoadingPageLoadMetricsObserver::
    MultiTabLoadingPageLoadMetricsObserver() {}

MultiTabLoadingPageLoadMetricsObserver::
    ~MultiTabLoadingPageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
MultiTabLoadingPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  num_loading_tabs_when_started_ =
      NumberOfTabsWithInflightLoad(navigation_handle);
  return num_loading_tabs_when_started_ > 0 ? CONTINUE_OBSERVING
                                            : STOP_OBSERVING;
}

#define RECORD_HISTOGRAMS(suffix, sample)                                      \
  do {                                                                         \
    base::TimeDelta sample_value(sample);                                      \
    PAGE_LOAD_HISTOGRAM(                                                       \
        std::string(internal::kHistogramPrefixMultiTabLoading).append(suffix), \
        sample_value);                                                         \
    if (num_loading_tabs_when_started_ >= 2) {                                 \
      PAGE_LOAD_HISTOGRAM(                                                     \
          std::string(internal::kHistogramPrefixMultiTabLoading2OrMore)        \
              .append(suffix),                                                 \
          sample_value);                                                       \
    }                                                                          \
    if (num_loading_tabs_when_started_ >= 5) {                                 \
      PAGE_LOAD_HISTOGRAM(                                                     \
          std::string(internal::kHistogramPrefixMultiTabLoading5OrMore)        \
              .append(suffix),                                                 \
          sample_value);                                                       \
    }                                                                          \
  } while (false)

void MultiTabLoadingPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    RECORD_HISTOGRAMS(internal::kHistogramFirstContentfulPaintSuffix,
                      timing.paint_timing->first_contentful_paint.value());
  }

  if (page_load_metrics::WasStartedInBackgroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    RECORD_HISTOGRAMS(
        internal::kHistogramForegroundToFirstContentfulPaintSuffix,
        timing.paint_timing->first_contentful_paint.value() -
            GetDelegate().GetFirstForegroundTime().value());
  }
}

void MultiTabLoadingPageLoadMetricsObserver::
    OnFirstMeaningfulPaintInMainFrameDocument(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, GetDelegate())) {
    RECORD_HISTOGRAMS(internal::kHistogramFirstMeaningfulPaintSuffix,
                      timing.paint_timing->first_meaningful_paint.value());
  }
  if (page_load_metrics::WasStartedInBackgroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, GetDelegate())) {
    RECORD_HISTOGRAMS(
        internal::kHistogramForegroundToFirstMeaningfulPaintSuffix,
        timing.paint_timing->first_meaningful_paint.value() -
            GetDelegate().GetFirstForegroundTime().value());
  }
}

void MultiTabLoadingPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->dom_content_loaded_event_start,
          GetDelegate())) {
    RECORD_HISTOGRAMS(
        internal::kHistogramDOMContentLoadedEventFiredSuffix,
        timing.document_timing->dom_content_loaded_event_start.value());
  } else {
    RECORD_HISTOGRAMS(
        internal::kHistogramDOMContentLoadedEventFiredBackgroundSuffix,
        timing.document_timing->dom_content_loaded_event_start.value());
  }
}

void MultiTabLoadingPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, GetDelegate())) {
    RECORD_HISTOGRAMS(internal::kHistogramLoadEventFiredSuffix,
                      timing.document_timing->load_event_start.value());
  } else {
    RECORD_HISTOGRAMS(internal::kHistogramLoadEventFiredBackgroundSuffix,
                      timing.document_timing->load_event_start.value());
  }
}

#if defined(OS_ANDROID)

int MultiTabLoadingPageLoadMetricsObserver::NumberOfTabsWithInflightLoad(
    content::NavigationHandle* navigation_handle) {
  content::WebContents* this_contents = navigation_handle->GetWebContents();
  int num_loading = 0;
  for (TabModelList::const_iterator it = TabModelList::begin();
       it != TabModelList::end(); ++it) {
    TabModel* model = *it;
    // Note: |this_contents| may not appear in |model|.
    for (int i = 0; i < model->GetTabCount(); ++i) {
      content::WebContents* other_contents = model->GetWebContentsAt(i);
      if (other_contents && other_contents != this_contents &&
          other_contents->IsLoading()) {
        num_loading++;
      }
    }
  }
  return num_loading;
}

#else  // defined(OS_ANDROID)

int MultiTabLoadingPageLoadMetricsObserver::NumberOfTabsWithInflightLoad(
    content::NavigationHandle* navigation_handle) {
  content::WebContents* this_contents = navigation_handle->GetWebContents();
  int num_loading = 0;
  for (auto* browser : *BrowserList::GetInstance()) {
    TabStripModel* model = browser->tab_strip_model();
    // Note: |this_contents| may not appear in |model|, e.g. for a new
    // background tab navigation.
    for (int i = 0; i < model->count(); ++i) {
      content::WebContents* other_contents = model->GetWebContentsAt(i);
      if (other_contents != this_contents && other_contents->IsLoading()) {
        num_loading++;
      }
    }
  }
  return num_loading;
}

#endif  // defined(OS_ANDROID)
