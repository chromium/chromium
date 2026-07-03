// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NAVIGATION_INITIATOR_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NAVIGATION_INITIATOR_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

// TODO(crbug.com/517725655):
// components/page_load_metrics/browser/navigation_handle_user_data.h is planned
// to be moved to //chrome. This observer is placed in //chrome to avoid
// blocking that refactoring.
//
// Observer that records page load metrics based on the navigation's initiator.
class NavigationInitiatorPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  NavigationInitiatorPageLoadMetricsObserver() = default;
  ~NavigationInitiatorPageLoadMetricsObserver() override = default;

  // page_load_metrics::PageLoadMetricsObserver:
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NAVIGATION_INITIATOR_PAGE_LOAD_METRICS_OBSERVER_H_
