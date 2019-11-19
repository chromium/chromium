// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_USE_MEASUREMENT_CHROME_DATA_USE_MEASUREMENT_H_
#define CHROME_BROWSER_DATA_USE_MEASUREMENT_CHROME_DATA_USE_MEASUREMENT_H_

#include <memory>

#include "base/macros.h"
#include "components/data_use_measurement/core/data_use_measurement.h"

class PrefService;

namespace data_use_measurement {

class ChromeDataUseMeasurement : public DataUseMeasurement {
 public:
  static void CreateInstance(PrefService* local_state);
  static ChromeDataUseMeasurement* GetInstance();
  static void DeleteInstance();

  ChromeDataUseMeasurement(
      network::NetworkConnectionTracker* network_connection_tracker,
      PrefService* local_state);

  // Called when requests complete from NetworkService.
  void ReportNetworkServiceDataUse(int32_t network_traffic_annotation_id_hash,
                                   int64_t recv_bytes,
                                   int64_t sent_bytes);
  void ReportUserTrafficDataUse(bool is_tab_visible, int64_t recv_bytes);

  void RecordContentTypeMetric(const std::string& mime_type,
                               bool is_main_frame_resource,
                               bool is_tab_visible,
                               int64_t recv_bytes);

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeDataUseMeasurement);

  void UpdateMetricsUsagePrefs(int64_t total_bytes,
                               bool is_cellular,
                               bool is_metrics_service_usage);

  PrefService* local_state_ = nullptr;
};

}  // namespace data_use_measurement

#endif  // CHROME_BROWSER_DATA_USE_MEASUREMENT_CHROME_DATA_USE_MEASUREMENT_H_
