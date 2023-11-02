// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LIVE_TAB_COUNT_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LIVE_TAB_COUNT_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace internal {

// Exposed for tests.
extern const char kHistogramPrefixLiveTabCount[];

}  // namespace internal

// Observer responsible for recording core page load metrics bucketed by the
// number of live tabs.
class LiveTabCountPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  LiveTabCountPageLoadMetricsObserver();

  LiveTabCountPageLoadMetricsObserver(
      const LiveTabCountPageLoadMetricsObserver&) = delete;
  LiveTabCountPageLoadMetricsObserver& operator=(
      const LiveTabCountPageLoadMetricsObserver&) = delete;

  ~LiveTabCountPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstInputInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 protected:
  // Returns the number of live tabs, including the one that we're observing.
  // This is virtual and protected so we can control the live tab count from
  // unit tests.
  virtual size_t GetLiveTabCount() const;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LIVE_TAB_COUNT_PAGE_LOAD_METRICS_OBSERVER_H_
