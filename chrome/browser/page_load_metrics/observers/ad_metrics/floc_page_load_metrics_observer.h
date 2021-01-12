// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AD_METRICS_FLOC_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AD_METRICS_FLOC_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

// This observer monitors navigation commit and resource usages, which may
// affect whether the navigation's associated history entry is eligible for floc
// computation.
//
// The final eligibility decision may be based on other signals. See the
// FlocEligibilityObserver class for the full criteria.
class FlocPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  FlocPageLoadMetricsObserver();
  ~FlocPageLoadMetricsObserver() override;

  FlocPageLoadMetricsObserver(const FlocPageLoadMetricsObserver&) = delete;
  FlocPageLoadMetricsObserver& operator=(const FlocPageLoadMetricsObserver&) =
      delete;

  // page_load_metrics::PageLoadMetricsObserver
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  void OnResourceDataUseObserved(
      content::RenderFrameHost* rfh,
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources) override;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AD_METRICS_FLOC_PAGE_LOAD_METRICS_OBSERVER_H_
