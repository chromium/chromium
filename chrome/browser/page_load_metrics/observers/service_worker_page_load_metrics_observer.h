// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SERVICE_WORKER_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SERVICE_WORKER_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace internal {

// Expose metrics for tests.
extern const char kHistogramServiceWorkerParseStart[];
extern const char kBackgroundHistogramServiceWorkerParseStart[];
extern const char kHistogramServiceWorkerParseStartForwardBack[];
extern const char kHistogramServiceWorkerParseStartForwardBackNoStore[];
extern const char kHistogramServiceWorkerFirstPaint[];
extern const char kHistogramServiceWorkerFirstContentfulPaint[];
extern const char kBackgroundHistogramServiceWorkerFirstContentfulPaint[];
extern const char kHistogramServiceWorkerFirstContentfulPaintForwardBack[];
extern const char
    kHistogramServiceWorkerFirstContentfulPaintForwardBackNoStore[];
extern const char
    kHistogramServiceWorkerFirstContentfulPaintSkippableFetchHandler[];
extern const char
    kHistogramServiceWorkerFirstContentfulPaintNonSkippableFetchHandler[];
extern const char kHistogramServiceWorkerParseStartToFirstContentfulPaint[];
extern const char kHistogramServiceWorkerDomContentLoaded[];
extern const char kHistogramServiceWorkerLoad[];
extern const char kHistogramServiceWorkerLargestContentfulPaint[];
extern const char
    kHistogramServiceWorkerLargestContentfulPaintSkippableFetchHandler[];
extern const char
    kHistogramServiceWorkerLargestContentfulPaintNonSkippableFetchHandler[];
extern const char kHistogramServiceWorkerParseStartSearch[];
extern const char kHistogramServiceWorkerFirstContentfulPaintSearch[];
extern const char
    kHistogramServiceWorkerParseStartToFirstContentfulPaintSearch[];
extern const char kHistogramServiceWorkerDomContentLoadedSearch[];
extern const char kHistogramServiceWorkerLoadSearch[];

extern const char kHistogramNoServiceWorkerFirstContentfulPaintSearch[];
extern const char
    kHistogramNoServiceWorkerParseStartToFirstContentfulPaintSearch[];
extern const char kHistogramNoServiceWorkerDomContentLoadedSearch[];
extern const char kHistogramNoServiceWorkerLoadSearch[];

extern const char kHistogramServiceWorkerFirstContentfulPaintDocs[];
extern const char kHistogramNoServiceWorkerFirstContentfulPaintDocs[];

extern const char kHistogramServiceWorkerSubresourceTotalRouterEvaluationTime[];

}  // namespace internal

class ServiceWorkerPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  ServiceWorkerPageLoadMetricsObserver();

  ServiceWorkerPageLoadMetricsObserver(
      const ServiceWorkerPageLoadMetricsObserver&) = delete;
  ServiceWorkerPageLoadMetricsObserver& operator=(
      const ServiceWorkerPageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnFirstInputInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnParseStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnDomContentLoadedEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadingBehaviorObserved(content::RenderFrameHost* rfh,
                                 int behavior_flags) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy
  FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  void RecordTimingHistograms();
  bool IsServiceWorkerControlled();
  bool IsServiceWorkerFetchHandlerSkippable();
  bool IsServiceWorkerEligibleForRaceNetworkRequest();
  void RecordSubresourceLoad();

  ui::PageTransition transition_ = ui::PAGE_TRANSITION_LINK;
  bool was_no_store_main_resource_ = false;
  bool logged_ukm_event_ = false;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SERVICE_WORKER_PAGE_LOAD_METRICS_OBSERVER_H_
