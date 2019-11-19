// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/data_use_metrics_observer.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_measurement.h"
#include "content/public/browser/navigation_handle.h"

DataUseMetricsObserver::DataUseMetricsObserver() = default;

DataUseMetricsObserver::~DataUseMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataUseMetricsObserver::OnCommit(content::NavigationHandle* navigation_handle,
                                 ukm::SourceId source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataUseMetricsObserver::ShouldObserveMimeType(
    const std::string& mime_type) const {
  // Observe all MIME types. We still only use actual data usage, so strange
  // cases (e.g., data:// URLs) will still record the right amount of data
  // usage.
  return CONTINUE_OBSERVING;
}

void DataUseMetricsObserver::OnResourceDataUseObserved(
    content::RenderFrameHost* rfh,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* chrome_data_use_measurement =
      data_use_measurement::ChromeDataUseMeasurement::GetInstance();
  if (!chrome_data_use_measurement)
    return;

  int64_t received_data_length = 0;
  for (auto const& resource : resources) {
    received_data_length += resource->delta_bytes;
    chrome_data_use_measurement->RecordContentTypeMetric(
        resource->mime_type, resource->is_main_frame_resource,
        GetDelegate().GetVisibilityTracker().currently_in_foreground(),
        resource->delta_bytes);
  }
  if (!received_data_length)
    return;
  chrome_data_use_measurement->ReportUserTrafficDataUse(
      GetDelegate().GetVisibilityTracker().currently_in_foreground(),
      received_data_length);
}
