// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_MULTI_TAB_LOADING_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_MULTI_TAB_LOADING_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace content {
class NavigationHandle;
}

namespace internal {

// Exposed for tests.
extern const char kHistogramPrefixMultiTabLoading[];
extern const char kHistogramPrefixMultiTabLoading1OrMore[];
extern const char kHistogramPrefixMultiTabLoading2OrMore[];
extern const char kHistogramPrefixMultiTabLoading5OrMore[];
extern const char kHistogramPrefixMultiTabLoading0[];
extern const char kHistogramPrefixMultiTabLoading1[];
extern const char kHistogramPrefixMultiTabLoading2[];
extern const char kHistogramPrefixMultiTabLoading3[];
extern const char kHistogramPrefixMultiTabLoading4[];
extern const char kHistogramPrefixMultiTabLoading5[];
extern const char kHistogramPrefixMultiTab0[];
extern const char kHistogramPrefixMultiTab1[];
extern const char kHistogramPrefixMultiTab2[];
extern const char kHistogramPrefixMultiTab4[];
extern const char kHistogramPrefixMultiTab8[];
extern const char kHistogramPrefixMultiTab16[];
extern const char kHistogramPrefixMultiTab32[];
extern const char kHistogramPrefixMultiTab64[];

}  // namespace internal

// Observer responsible for recording core page load metrics while there are
// other loading tabs.
class MultiTabLoadingPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  MultiTabLoadingPageLoadMetricsObserver();

  MultiTabLoadingPageLoadMetricsObserver(
      const MultiTabLoadingPageLoadMetricsObserver&) = delete;
  MultiTabLoadingPageLoadMetricsObserver& operator=(
      const MultiTabLoadingPageLoadMetricsObserver&) = delete;

  ~MultiTabLoadingPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy OnStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url,
      bool started_in_foreground) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy OnPrerenderStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnDomContentLoadedEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 protected:
  // Overridden in testing. Returns the number of loading tabs, excluding
  // current tab.
  virtual int NumberOfTabsWithInflightLoad(
      content::NavigationHandle* navigation_handle);

  // Overridden in testing. Returns the number of tabs.
  virtual int NumberOfTabs(content::NavigationHandle* navigation_handle);

 private:
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy
  FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void RecordTimingHistograms();
  void RecordHistograms(const char* suffix, base::TimeDelta sample);

  int num_loading_tabs_when_started_;
  int num_of_tabs_when_started_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_MULTI_TAB_LOADING_PAGE_LOAD_METRICS_OBSERVER_H_
