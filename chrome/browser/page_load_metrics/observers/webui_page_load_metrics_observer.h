// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_WEBUI_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_WEBUI_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

// Records Page Load Metrics for all WebUI pages (chrome:// and
// chrome-untrusted:// schemes). This observer measures FCP and LCP metrics
// for any WebUI page.
class WebUIPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  WebUIPageLoadMetricsObserver();

  WebUIPageLoadMetricsObserver(const WebUIPageLoadMetricsObserver&) = delete;
  WebUIPageLoadMetricsObserver& operator=(const WebUIPageLoadMetricsObserver&) =
      delete;

  ~WebUIPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy ShouldObserveScheme(const GURL& url) const override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  // Returns true if the histogram was recorded, false otherwise.
  bool RecordSessionEndHistograms();
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_WEBUI_PAGE_LOAD_METRICS_OBSERVER_H_
