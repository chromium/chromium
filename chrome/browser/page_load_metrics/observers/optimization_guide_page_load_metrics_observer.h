// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_OPTIMIZATION_GUIDE_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_OPTIMIZATION_GUIDE_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

// Observer responsible for capturing and recording page load metrics for the
// Optimization Guide.
class OptimizationGuidePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  OptimizationGuidePageLoadMetricsObserver();

  OptimizationGuidePageLoadMetricsObserver(
      const OptimizationGuidePageLoadMetricsObserver&) = delete;
  OptimizationGuidePageLoadMetricsObserver& operator=(
      const OptimizationGuidePageLoadMetricsObserver&) = delete;

  ~OptimizationGuidePageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  // OptimizationGuideWebContentsObserver to pass any captured page load metrics
  // needed for the Optimization Guide. Not owned.
  raw_ptr<OptimizationGuideWebContentsObserver>
      optimization_guide_web_contents_observer_ = nullptr;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_OPTIMIZATION_GUIDE_PAGE_LOAD_METRICS_OBSERVER_H_
