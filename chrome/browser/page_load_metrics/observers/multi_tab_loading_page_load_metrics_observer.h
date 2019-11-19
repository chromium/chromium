// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_MULTI_TAB_LOADING_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_MULTI_TAB_LOADING_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace content {
class NavigationHandle;
}

namespace internal {

// Exposed for tests.
extern const char kHistogramPrefixMultiTabLoading[];
extern const char kHistogramPrefixMultiTabLoading2OrMore[];
extern const char kHistogramPrefixMultiTabLoading5OrMore[];

}  // namespace internal

// Observer responsible for recording core page load metrics while there are
// other loading tabs.
class MultiTabLoadingPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  MultiTabLoadingPageLoadMetricsObserver();
  ~MultiTabLoadingPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy OnStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url,
      bool started_in_foreground) override;
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

 private:
  int num_loading_tabs_when_started_;

  DISALLOW_COPY_AND_ASSIGN(MultiTabLoadingPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_MULTI_TAB_LOADING_PAGE_LOAD_METRICS_OBSERVER_H_
