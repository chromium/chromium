// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SERVICE_WORKER_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SERVICE_WORKER_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
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
extern const char kHistogramServiceWorkerParseStartToFirstContentfulPaint[];
extern const char kHistogramServiceWorkerFirstMeaningfulPaint[];
extern const char kHistogramServiceWorkerParseStartToFirstMeaningfulPaint[];
extern const char kHistogramServiceWorkerDomContentLoaded[];
extern const char kHistogramServiceWorkerLoad[];

extern const char kHistogramServiceWorkerParseStartInbox[];
extern const char kHistogramServiceWorkerFirstContentfulPaintInbox[];
extern const char kHistogramServiceWorkerFirstInputDelay[];
extern const char kHistogramServiceWorkerFirstMeaningfulPaintInbox[];
extern const char
    kHistogramServiceWorkerParseStartToFirstMeaningfulPaintInbox[];
extern const char
    kHistogramServiceWorkerParseStartToFirstContentfulPaintInbox[];
extern const char kHistogramServiceWorkerDomContentLoadedInbox[];
extern const char kHistogramServiceWorkerLoadInbox[];

extern const char kHistogramServiceWorkerParseStartSearch[];
extern const char kHistogramServiceWorkerFirstContentfulPaintSearch[];
extern const char kHistogramServiceWorkerFirstMeaningfulPaintSearch[];
extern const char
    kHistogramServiceWorkerParseStartToFirstMeaningfulPaintSearch[];
extern const char
    kHistogramServiceWorkerParseStartToFirstContentfulPaintSearch[];
extern const char kHistogramServiceWorkerDomContentLoadedSearch[];
extern const char kHistogramServiceWorkerLoadSearch[];

extern const char kHistogramNoServiceWorkerFirstContentfulPaintSearch[];
extern const char kHistogramNoServiceWorkerFirstMeaningfulPaintSearch[];
extern const char
    kHistogramNoServiceWorkerParseStartToFirstMeaningfulPaintSearch[];
extern const char
    kHistogramNoServiceWorkerParseStartToFirstContentfulPaintSearch[];
extern const char kHistogramNoServiceWorkerDomContentLoadedSearch[];
extern const char kHistogramNoServiceWorkerLoadSearch[];

extern const char kHistogramServiceWorkerFirstContentfulPaintDocs[];
extern const char kHistogramNoServiceWorkerFirstContentfulPaintDocs[];

}  // namespace internal

class ServiceWorkerPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  ServiceWorkerPageLoadMetricsObserver();
  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  void OnFirstInputInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnParseStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnDomContentLoadedEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadingBehaviorObserved(content::RenderFrameHost* rfh,
                                 int behavior_flags) override;

 private:
  bool IsServiceWorkerControlled();

  ui::PageTransition transition_ = ui::PAGE_TRANSITION_LINK;
  bool was_no_store_main_resource_ = false;
  bool logged_ukm_event_ = false;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SERVICE_WORKER_PAGE_LOAD_METRICS_OBSERVER_H_
