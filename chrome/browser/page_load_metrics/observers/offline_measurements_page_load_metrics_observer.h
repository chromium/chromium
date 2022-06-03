// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_OFFLINE_MEASUREMENTS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_OFFLINE_MEASUREMENTS_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

// Observer that triggers the logging of any persisted metrics from the
// |OfflineMeasurementsBackgroundTask| to both UKM and UMA.
class OfflineMeasurementsPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  static std::unique_ptr<OfflineMeasurementsPageLoadMetricsObserver>
  CreateIfNeeded();

  OfflineMeasurementsPageLoadMetricsObserver() = default;
  ~OfflineMeasurementsPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_OFFLINE_MEASUREMENTS_PAGE_LOAD_METRICS_OBSERVER_H_
