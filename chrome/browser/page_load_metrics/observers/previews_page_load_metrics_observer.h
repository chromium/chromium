// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PREVIEWS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PREVIEWS_PAGE_LOAD_METRICS_OBSERVER_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/previews/core/previews_experiments.h"

namespace content {
class NavigationHandle;
}

namespace previews {

// Observer responsible for recording page load metrics for some types of
// previews (such as NoScript and ResourceLoadingHints previews).
class PreviewsPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  PreviewsPageLoadMetricsObserver();
  ~PreviewsPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnResourceDataUseObserved(
      content::RenderFrameHost* rfh,
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources) override;

 protected:
  // Virtual for testing. Writes the savings to the data saver feature.
  virtual void WriteToSavings(const GURL& url, int64_t byte_savings);

 private:
  void RecordTimingMetrics(
      const page_load_metrics::mojom::PageLoadTiming& timing);

  // Records UMA of page size when the observer is about to be deleted.
  void RecordPageSizeUMA() const;

  content::BrowserContext* browser_context_;

  // The previews type active for the page load (available at commit-time).
  previews::PreviewsType previews_type_ = previews::PreviewsType::UNSPECIFIED;

  // The total number of bytes from OnDataUseObserved().
  int64_t total_network_bytes_ = 0;

  // The percent of bytes used by load event that should be considered savings.
  // This is often larger than 100 as it corresponds to bytes that were not
  // downloaded.
  int data_savings_inflation_percent_ = 0;

  int64_t num_network_resources_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PreviewsPageLoadMetricsObserver);
};

}  // namespace previews

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PREVIEWS_PAGE_LOAD_METRICS_OBSERVER_H_
