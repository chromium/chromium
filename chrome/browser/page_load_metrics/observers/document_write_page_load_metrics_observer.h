// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DOCUMENT_WRITE_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DOCUMENT_WRITE_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace internal {

// Expose metrics for tests.
extern const char kHistogramDocWriteParseStartToFirstContentfulPaint[];
extern const char kHistogramDocWriteBlockParseStartToFirstContentfulPaint[];

}  // namespace internal

class DocumentWritePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  DocumentWritePageLoadMetricsObserver() = default;

  DocumentWritePageLoadMetricsObserver(
      const DocumentWritePageLoadMetricsObserver&) = delete;
  DocumentWritePageLoadMetricsObserver& operator=(
      const DocumentWritePageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;

  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void OnParseStop(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  enum DocumentWriteLoadingBehavior {
    LOADING_BEHAVIOR_BLOCK,
    LOADING_BEHAVIOR_RELOAD,
    LOADING_BEHAVIOR_SAME_SITE_DIFF_SCHEME,
    LOADING_BEHAVIOR_MAX
  };

 private:
  void LogDocumentWriteBlockFirstContentfulPaint(
      const page_load_metrics::mojom::PageLoadTiming& timing);

  void LogDocumentWriteBlockParseStop(
      const page_load_metrics::mojom::PageLoadTiming& timing);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DOCUMENT_WRITE_PAGE_LOAD_METRICS_OBSERVER_H_
