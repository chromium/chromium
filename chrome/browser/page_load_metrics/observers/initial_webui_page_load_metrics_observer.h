// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_INITIAL_WEBUI_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_INITIAL_WEBUI_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "url/gurl.h"

class WaapUIMetricsService;

namespace content {
class NavigationHandle;
}  // namespace content

// Observer for initial WebUI page loads.
// See
// https://docs.google.com/document/d/13nVm0v4hKFfTjbsE0n7loh3seBdRmqyLXByZqjlpc8Q/edit?tab=t.0
class InitialWebUIPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  InitialWebUIPageLoadMetricsObserver();

  // Not movable or copyable.
  InitialWebUIPageLoadMetricsObserver(
      const InitialWebUIPageLoadMetricsObserver&) = delete;
  InitialWebUIPageLoadMetricsObserver& operator=(
      const InitialWebUIPageLoadMetricsObserver&) = delete;

  ~InitialWebUIPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  const char* GetObserverName() const override;
  void OnFirstPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;

 private:
  // Returns the service for the current profile.
  // The service is guaranteed to be non-null.
  WaapUIMetricsService* service() const;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_INITIAL_WEBUI_PAGE_LOAD_METRICS_OBSERVER_H_
