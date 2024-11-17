// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NON_TAB_WEBUI_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NON_TAB_WEBUI_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

extern const char kNonTabWebUIRequestToFCPHistogramName[];
extern const char kNonTabWebUIRequestToLCPHistogramName[];

// Records Page Load Metrics for non-tab chrome:// pages such as side-panel
// content and webUI based bubbles. This covers any webUI that goes through
// `WebUIContentsWrapperT`
class NonTabPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  explicit NonTabPageLoadMetricsObserver(const std::string& webui_name);

  // page_load_metrics::PageLoadMetricsObserver:
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  page_load_metrics::PageLoadMetricsObserver::ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;

  page_load_metrics::PageLoadMetricsObserver::ObservePolicy OnPrerenderStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;

  ObservePolicy ShouldObserveScheme(const GURL& url) const override;

 private:
  std::string webui_name_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NON_TAB_WEBUI_PAGE_LOAD_METRICS_OBSERVER_H_
