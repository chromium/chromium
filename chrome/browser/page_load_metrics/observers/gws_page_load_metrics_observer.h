// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/google/core/common/google_util.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/navigation_handle_timing.h"

namespace internal {
// Exposed for tests.

extern const char kHistogramGWSNavigationStartToFinalRequestStart[];
extern const char kHistogramGWSNavigationStartToFinalResponseStart[];
extern const char kHistogramGWSNavigationStartToFinalLoaderCallback[];
extern const char kHistogramGWSNavigationStartToFirstRequestStart[];
extern const char kHistogramGWSNavigationStartToFirstResponseStart[];
extern const char kHistogramGWSNavigationStartToFirstLoaderCallback[];

extern const char kHistogramGWSAFTEnd[];
extern const char kHistogramGWSAFTStart[];

extern const char kHistogramGWSFirstContentfulPaint[];
extern const char kHistogramGWSLargestContentfulPaint[];
extern const char kHistogramGWSParseStart[];

extern const char kGwsAFTStartMarkName[];
extern const char kGwsAFTEndMarkName[];

}  // namespace internal

class GWSPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  GWSPageLoadMetricsObserver();

  GWSPageLoadMetricsObserver(const GWSPageLoadMetricsObserver&) = delete;
  GWSPageLoadMetricsObserver& operator=(const GWSPageLoadMetricsObserver&) =
      delete;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;

  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;

  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;

  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnParseStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnCustomUserTimingMarkObserved(
      const std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr>&
          timings) override;

 private:
  void LogMetricsOnComplete();
  void RecordNavigationTimingHistograms();

  content::NavigationHandleTiming navigation_handle_timing_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_PAGE_LOAD_METRICS_OBSERVER_H_
