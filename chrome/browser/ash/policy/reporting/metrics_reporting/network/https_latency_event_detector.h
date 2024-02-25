// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_HTTPS_LATENCY_EVENT_DETECTOR_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_HTTPS_LATENCY_EVENT_DETECTOR_H_

#include <optional>

#include "components/reporting/metrics/periodic_event_collector.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

// Detect HTTPS latency events given the previous and current collected HTTPS
// latency data, an event is detected if  either the HTTPS latency
// `RoutineVerdict` changes, the `HttpsLatencyProblem` changes, or an
// `HttpsLatencyProblem` is detected and there was no previously collected
// HTTPS latency data.
class HttpsLatencyEventDetector : public PeriodicEventCollector::EventDetector {
 public:
  HttpsLatencyEventDetector() = default;

  HttpsLatencyEventDetector(const HttpsLatencyEventDetector&) = delete;
  HttpsLatencyEventDetector& operator=(const HttpsLatencyEventDetector&) =
      delete;

  ~HttpsLatencyEventDetector() override = default;

  std::optional<MetricEventType> DetectEvent(
      std::optional<MetricData> previous_metric_data,
      const MetricData& current_metric_data) override;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_HTTPS_LATENCY_EVENT_DETECTOR_H_
