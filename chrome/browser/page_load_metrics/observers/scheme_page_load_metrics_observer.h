// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SCHEME_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SCHEME_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "net/http/http_response_info.h"
#include "services/metrics/public/cpp/ukm_source.h"

class SchemePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  SchemePageLoadMetricsObserver() = default;

  SchemePageLoadMetricsObserver(const SchemePageLoadMetricsObserver&) = delete;
  SchemePageLoadMetricsObserver& operator=(
      const SchemePageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnParseStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  // The ui transition for the committed navigation.
  ui::PageTransition transition_ = ui::PAGE_TRANSITION_FIRST;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SCHEME_PAGE_LOAD_METRICS_OBSERVER_H_
