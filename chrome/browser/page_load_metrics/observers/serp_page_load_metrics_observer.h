// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SERP_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SERP_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace content {
class NavigationHandle;
}

class SerpPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  SerpPageLoadMetricsObserver();
  ~SerpPageLoadMetricsObserver() override;

  SerpPageLoadMetricsObserver(const SerpPageLoadMetricsObserver&) = delete;
  SerpPageLoadMetricsObserver& operator=(const SerpPageLoadMetricsObserver&) =
      delete;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SERP_PAGE_LOAD_METRICS_OBSERVER_H_
