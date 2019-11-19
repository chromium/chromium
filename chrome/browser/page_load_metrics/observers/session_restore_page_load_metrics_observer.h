// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SESSION_RESTORE_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SESSION_RESTORE_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace internal {

extern const char kHistogramSessionRestoreForegroundTabFirstPaint[];
extern const char kHistogramSessionRestoreForegroundTabFirstContentfulPaint[];
extern const char kHistogramSessionRestoreForegroundTabFirstMeaningfulPaint[];

}  // namespace internal

// Record page load metrics of foreground tabs during session restore. This
// observer observes foreground tabs created by session restores only. It will
// stop observing if the tab gets hidden, reloaded, or navigated away.
class SessionRestorePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  SessionRestorePageLoadMetricsObserver();

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  void OnFirstPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionRestorePageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SESSION_RESTORE_PAGE_LOAD_METRICS_OBSERVER_H_
