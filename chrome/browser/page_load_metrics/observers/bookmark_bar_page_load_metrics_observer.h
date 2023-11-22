// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_BOOKMARK_BAR_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_BOOKMARK_BAR_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

// BookmarkBarMetricsObserver is responsible for recording performance related
// metrics, such as NavigationOrActivationToFirstContentfulPaint and
// NavigationOrActivationToLargestContentfulPaint for BookmarkBar navigations.
class BookmarkBarMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  BookmarkBarMetricsObserver();

  BookmarkBarMetricsObserver(const BookmarkBarMetricsObserver&) = delete;
  BookmarkBarMetricsObserver& operator=(const BookmarkBarMetricsObserver&) =
      delete;

  ~BookmarkBarMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  void RecordSessionEndHistograms(
      const page_load_metrics::mojom::PageLoadTiming& timing);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_BOOKMARK_BAR_PAGE_LOAD_METRICS_OBSERVER_H_
