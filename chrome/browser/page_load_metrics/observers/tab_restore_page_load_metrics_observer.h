// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_TAB_RESTORE_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_TAB_RESTORE_PAGE_LOAD_METRICS_OBSERVER_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace content {
class NavigationHandle;
}

// Observer responsible for recording core page load metrics relevant to
// restored tabs.
class TabRestorePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  TabRestorePageLoadMetricsObserver();
  ~TabRestorePageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy OnStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url,
      bool started_in_foreground) override;
  void OnResourceDataUseObserved(
      content::RenderFrameHost* rfh,
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy
  FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 protected:
  // Whether the navigation handle is a tab restore.
  // Overridden in testing.
  virtual bool IsTabRestore(content::NavigationHandle* navigation_handle);

 private:
  // Records histograms for byte information.
  void RecordByteHistograms();

  // The number of body (not header) prefilter bytes consumed by requests for
  // the page.
  int64_t cache_bytes_;
  int64_t network_bytes_;

  DISALLOW_COPY_AND_ASSIGN(TabRestorePageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_TAB_RESTORE_PAGE_LOAD_METRICS_OBSERVER_H_
