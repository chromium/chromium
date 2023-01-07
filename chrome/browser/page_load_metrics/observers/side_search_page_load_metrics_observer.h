// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SIDE_SEARCH_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SIDE_SEARCH_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace internal {

extern const char kSideSearchFirstContentfulPaint[];
extern const char kSideSearchFirstMeaningfulPaint[];
extern const char kSideSearchInteractiveInputDelay[];
extern const char kSideSearchLargestContentfulPaint[];
extern const char kSideSearchMaxCumulativeShiftScore[];

}  // namespace internal

class SideSearchPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  static std::unique_ptr<SideSearchPageLoadMetricsObserver> CreateIfNeeded(
      content::WebContents* web_contents);

  SideSearchPageLoadMetricsObserver() = default;
  SideSearchPageLoadMetricsObserver(const SideSearchPageLoadMetricsObserver&) =
      delete;
  SideSearchPageLoadMetricsObserver& operator=(
      const SideSearchPageLoadMetricsObserver&) = delete;
  ~SideSearchPageLoadMetricsObserver() override = default;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstInputInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  void RecordSessionEndHistograms();
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SIDE_SEARCH_PAGE_LOAD_METRICS_OBSERVER_H_
