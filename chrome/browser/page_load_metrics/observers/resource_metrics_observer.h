// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_RESOURCE_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_RESOURCE_METRICS_OBSERVER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "chrome/browser/page_load_metrics/observers/ad_metrics/frame_data.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"

// Observer that records histograms for individual resources loaded by the page.
class ResourceMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  using ResourceMimeType = FrameData::ResourceMimeType;
  ResourceMetricsObserver();
  ~ResourceMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver
  void OnResourceDataUseObserved(
      content::RenderFrameHost* content,
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  // Records per-resource histograms.
  void RecordResourceHistograms(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource);

  // Records size of resources by mime type.
  void RecordResourceMimeHistograms(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource);

  DISALLOW_COPY_AND_ASSIGN(ResourceMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_RESOURCE_METRICS_OBSERVER_H_
