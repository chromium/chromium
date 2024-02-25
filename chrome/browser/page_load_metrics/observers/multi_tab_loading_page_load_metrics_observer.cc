// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/multi_tab_loading_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/page_load_metrics/observers/histogram_suffixes.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace {

void PageLoadHistogram(const std::string& name, base::TimeDelta sample) {
  base::UmaHistogramCustomTimes(name, sample, base::Milliseconds(10),
                                base::Minutes(10), 100);
}

}  // namespace

namespace internal {

const char kHistogramPrefixMultiTabLoading[] =
    "PageLoad.Clients.MultiTabLoading.";
const char kHistogramPrefixMultiTabLoading1OrMore[] =
    "PageLoad.Clients.MultiTabLoading.1OrMore.";
const char kHistogramPrefixMultiTabLoading2OrMore[] =
    "PageLoad.Clients.MultiTabLoading.2OrMore.";
const char kHistogramPrefixMultiTabLoading5OrMore[] =
    "PageLoad.Clients.MultiTabLoading.5OrMore.";
const char kHistogramPrefixMultiTabLoading0[] =
    "PageLoad.Clients.MultiTabLoading.With_0_OtherLoading.";
const char kHistogramPrefixMultiTabLoading1[] =
    "PageLoad.Clients.MultiTabLoading.With_1_OtherLoading.";
const char kHistogramPrefixMultiTabLoading2[] =
    "PageLoad.Clients.MultiTabLoading.With_2_OtherLoading.";
const char kHistogramPrefixMultiTabLoading3[] =
    "PageLoad.Clients.MultiTabLoading.With_3_OtherLoading.";
const char kHistogramPrefixMultiTabLoading4[] =
    "PageLoad.Clients.MultiTabLoading.With_4_OtherLoading.";
const char kHistogramPrefixMultiTabLoading5[] =
    "PageLoad.Clients.MultiTabLoading.With_5_OtherLoading.";
const char kHistogramPrefixMultiTab0[] =
    "PageLoad.Clients.MultiTabLoading.WithTabCount_0.";
const char kHistogramPrefixMultiTab1[] =
    "PageLoad.Clients.MultiTabLoading.WithTabCount_1.";
const char kHistogramPrefixMultiTab2[] =
    "PageLoad.Clients.MultiTabLoading.WithTabCount_2_or_3.";
const char kHistogramPrefixMultiTab4[] =
    "PageLoad.Clients.MultiTabLoading.WithTabCount_4_to_7.";
const char kHistogramPrefixMultiTab8[] =
    "PageLoad.Clients.MultiTabLoading.WithTabCount_8_to_15.";
const char kHistogramPrefixMultiTab16[] =
    "PageLoad.Clients.MultiTabLoading.WithTabCount_16_to_31.";
const char kHistogramPrefixMultiTab32[] =
    "PageLoad.Clients.MultiTabLoading.WithTabCount_32_to_63.";
const char kHistogramPrefixMultiTab64[] =
    "PageLoad.Clients.MultiTabLoading.WithTabCount_64_or_more.";

}  // namespace internal

MultiTabLoadingPageLoadMetricsObserver::
    MultiTabLoadingPageLoadMetricsObserver() = default;

MultiTabLoadingPageLoadMetricsObserver::
    ~MultiTabLoadingPageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
MultiTabLoadingPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  num_loading_tabs_when_started_ =
      NumberOfTabsWithInflightLoad(navigation_handle);
  num_of_tabs_when_started_ = NumberOfTabs(navigation_handle);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
MultiTabLoadingPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class doesn't use subframe information. No need to forward.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
MultiTabLoadingPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class will be deprecated. No need to support.
  return STOP_OBSERVING;
}

#define RECORD_HISTOGRAM(condition, name)                               \
  do {                                                                  \
    if (condition) {                                                    \
      PageLoadHistogram(                                                \
          std::string(internal::kHistogramPrefix##name).append(suffix), \
          sample);                                                      \
    }                                                                   \
  } while (false)

void MultiTabLoadingPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    RecordHistograms(internal::kHistogramFirstContentfulPaintSuffix,
                     timing.paint_timing->first_contentful_paint.value());
  }

  if (page_load_metrics::WasStartedInBackgroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    RecordHistograms(internal::kHistogramForegroundToFirstContentfulPaintSuffix,
                     timing.paint_timing->first_contentful_paint.value() -
                         GetDelegate().GetTimeToFirstForeground().value());
  }
}

void MultiTabLoadingPageLoadMetricsObserver::
    OnFirstMeaningfulPaintInMainFrameDocument(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, GetDelegate())) {
    RecordHistograms(internal::kHistogramFirstMeaningfulPaintSuffix,
                     timing.paint_timing->first_meaningful_paint.value());
  }
}

void MultiTabLoadingPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->dom_content_loaded_event_start,
          GetDelegate())) {
    RecordHistograms(
        internal::kHistogramDOMContentLoadedEventFiredSuffix,
        timing.document_timing->dom_content_loaded_event_start.value());
  } else {
    RecordHistograms(
        internal::kHistogramDOMContentLoadedEventFiredBackgroundSuffix,
        timing.document_timing->dom_content_loaded_event_start.value());
  }
}

void MultiTabLoadingPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, GetDelegate())) {
    RecordHistograms(internal::kHistogramLoadEventFiredSuffix,
                     timing.document_timing->load_event_start.value());
  } else {
    RecordHistograms(internal::kHistogramLoadEventFiredBackgroundSuffix,
                     timing.document_timing->load_event_start.value());
  }
}

void MultiTabLoadingPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordTimingHistograms();
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
MultiTabLoadingPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // This follows UmaPageLoadMetricsObserver.
  if (GetDelegate().DidCommit()) {
    RecordTimingHistograms();
  }
  return STOP_OBSERVING;
}

void MultiTabLoadingPageLoadMetricsObserver::RecordTimingHistograms() {
  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();
  if (largest_contentful_paint.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          largest_contentful_paint.Time(), GetDelegate())) {
    RecordHistograms(internal::kHistogramLargestContentfulPaintSuffix,
                     largest_contentful_paint.Time().value());
  }
}

void MultiTabLoadingPageLoadMetricsObserver::RecordHistograms(
    const char* suffix,
    base::TimeDelta sample) {
  int num_of_loading = num_loading_tabs_when_started_;
  int num_of_tabs = num_of_tabs_when_started_;
  RECORD_HISTOGRAM(num_of_loading >= 1, MultiTabLoading);
  RECORD_HISTOGRAM(num_of_loading >= 1, MultiTabLoading1OrMore);
  RECORD_HISTOGRAM(num_of_loading >= 2, MultiTabLoading2OrMore);
  RECORD_HISTOGRAM(num_of_loading >= 5, MultiTabLoading5OrMore);
  RECORD_HISTOGRAM(num_of_loading == 0, MultiTabLoading0);
  RECORD_HISTOGRAM(num_of_loading == 1, MultiTabLoading1);
  RECORD_HISTOGRAM(num_of_loading == 2, MultiTabLoading2);
  RECORD_HISTOGRAM(num_of_loading == 3, MultiTabLoading3);
  RECORD_HISTOGRAM(num_of_loading == 4, MultiTabLoading4);
  RECORD_HISTOGRAM(num_of_loading == 5, MultiTabLoading5);
  RECORD_HISTOGRAM(num_of_tabs == 0, MultiTab0);
  RECORD_HISTOGRAM(num_of_tabs == 1, MultiTab1);
  RECORD_HISTOGRAM(num_of_tabs == 2 || num_of_tabs == 3, MultiTab2);
  RECORD_HISTOGRAM(num_of_tabs >= 4 && num_of_tabs < 8, MultiTab4);
  RECORD_HISTOGRAM(num_of_tabs >= 8 && num_of_tabs < 16, MultiTab8);
  RECORD_HISTOGRAM(num_of_tabs >= 16 && num_of_tabs < 32, MultiTab16);
  RECORD_HISTOGRAM(num_of_tabs >= 32 && num_of_tabs < 64, MultiTab32);
  RECORD_HISTOGRAM(num_of_tabs >= 64, MultiTab64);
}

#if BUILDFLAG(IS_ANDROID)

int MultiTabLoadingPageLoadMetricsObserver::NumberOfTabsWithInflightLoad(
    content::NavigationHandle* navigation_handle) {
  content::WebContents* this_contents = navigation_handle->GetWebContents();
  int num_loading = 0;
  for (const TabModel* model : TabModelList::models()) {
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

int MultiTabLoadingPageLoadMetricsObserver::NumberOfTabs(
    content::NavigationHandle* navigation_handle) {
  int num_of_tabs = 0;
  for (const TabModel* model : TabModelList::models()) {
    num_of_tabs += model->GetTabCount();
  }
  return num_of_tabs;
}

#else  // BUILDFLAG(IS_ANDROID)

int MultiTabLoadingPageLoadMetricsObserver::NumberOfTabsWithInflightLoad(
    content::NavigationHandle* navigation_handle) {
  content::WebContents* this_contents = navigation_handle->GetWebContents();
  int num_loading = 0;
  for (Browser* browser : *BrowserList::GetInstance()) {
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

int MultiTabLoadingPageLoadMetricsObserver::NumberOfTabs(
    content::NavigationHandle* navigation_handle) {
  int num_of_tabs = 0;
  for (Browser* browser : *BrowserList::GetInstance()) {
    num_of_tabs += browser->tab_strip_model()->count();
  }
  return num_of_tabs;
}

#endif  // BUILDFLAG(IS_ANDROID)
