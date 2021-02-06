// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_SAVER_SITE_BREAKDOWN_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_SAVER_SITE_BREAKDOWN_METRICS_OBSERVER_H_

#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom-forward.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class BrowserContext;
class NavigationHandle;
}  // namespace content

// Observer responsible for recording data usage per site to the data reduction
// proxy database.
class DataSaverSiteBreakdownMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  DataSaverSiteBreakdownMetricsObserver();
  ~DataSaverSiteBreakdownMetricsObserver() override;

 private:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;

  void OnResourceDataUseObserved(
      content::RenderFrameHost* rfh,
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources) override;
  void OnNewDeferredResourceCounts(
      const page_load_metrics::mojom::DeferredResourceCounts&
          new_deferred_resource_data) override;
  ObservePolicy ShouldObserveMimeType(
      const std::string& mime_type) const override;

  std::string committed_host_;
  std::string committed_origin_;

  // The browser context this navigation is operating in.
  content::BrowserContext* browser_context_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(DataSaverSiteBreakdownMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_SAVER_SITE_BREAKDOWN_METRICS_OBSERVER_H_
