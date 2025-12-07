// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_HP_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_HP_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace internal {
// Exposed for tests.
extern const char kHistogramGWSHpParseStart[];

extern const char kHistogramGWSHpConnectStart[];
extern const char kHistogramGWSHpDomainLookupStart[];
extern const char kHistogramGWSHpDomainLookupEnd[];

}  // namespace internal

class GWSHpPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  GWSHpPageLoadMetricsObserver();

  GWSHpPageLoadMetricsObserver(const GWSHpPageLoadMetricsObserver&) = delete;
  GWSHpPageLoadMetricsObserver& operator=(const GWSHpPageLoadMetricsObserver&) =
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

  void OnParseStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnConnectStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnDomainLookupStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnDomainLookupEnd(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  std::string AddHistogramSuffix(const std::string& histogram_name);

  bool is_first_navigation_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_HP_PAGE_LOAD_METRICS_OBSERVER_H_
