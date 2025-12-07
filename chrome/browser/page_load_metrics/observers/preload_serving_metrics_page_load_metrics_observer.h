// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PRELOAD_SERVING_METRICS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PRELOAD_SERVING_METRICS_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/preload_serving_metrics_capsule.h"

// Records FirstContentfulPaint for `PreloadServingMetrics`
//
// See `PreloadServingMetrics` for more details.
class PreloadServingMetricsPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  PreloadServingMetricsPageLoadMetricsObserver();
  ~PreloadServingMetricsPageLoadMetricsObserver() override;

  // Not movable nor copyable.
  PreloadServingMetricsPageLoadMetricsObserver(
      PreloadServingMetricsPageLoadMetricsObserver&& other) = delete;
  PreloadServingMetricsPageLoadMetricsObserver& operator=(
      PreloadServingMetricsPageLoadMetricsObserver&& other) = delete;
  PreloadServingMetricsPageLoadMetricsObserver(
      const PreloadServingMetricsPageLoadMetricsObserver&) = delete;
  PreloadServingMetricsPageLoadMetricsObserver& operator=(
      const PreloadServingMetricsPageLoadMetricsObserver&) = delete;

 private:
  // PageLoadMetricsObserver implementation:
  const char* GetObserverName() const override;
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  PageLoadMetricsObserver::ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void MaybeRecord();

  std::unique_ptr<content::PreloadServingMetricsCapsule>
      preload_serving_metrics_capsule_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PRELOAD_SERVING_METRICS_PAGE_LOAD_METRICS_OBSERVER_H_
