// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/https_latency_event_detector.h"

#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

std::optional<MetricEventType> HttpsLatencyEventDetector::DetectEvent(
    std::optional<MetricData> previous_metric_data,
    const MetricData& current_metric_data) {
  const auto& current_https_latency_data = current_metric_data.telemetry_data()
                                               .networks_telemetry()
                                               .https_latency_data();
  const ash::NetworkState* const network_state =
      ash::NetworkHandler::Get()
          ->network_state_handler()
          ->ConnectedNetworkByType(ash::NetworkTypePattern::Default());
  if (current_https_latency_data.has_problem() &&
      (current_https_latency_data.problem() ==
           HttpsLatencyProblem::FAILED_HTTPS_REQUESTS ||
       current_https_latency_data.problem() ==
           HttpsLatencyProblem::FAILED_DNS_RESOLUTIONS) &&
      (!network_state || !network_state->IsOnline())) {
    return std::nullopt;
  }

  if (!previous_metric_data.has_value()) {
    if (current_https_latency_data.verdict() == RoutineVerdict::PROBLEM) {
      return MetricEventType::NETWORK_HTTPS_LATENCY_CHANGE;
    }
    return std::nullopt;
  }

  const auto& previous_https_latency_data =
      previous_metric_data->telemetry_data()
          .networks_telemetry()
          .https_latency_data();
  if ((!previous_https_latency_data.has_verdict() &&
       current_https_latency_data.verdict() == RoutineVerdict::PROBLEM) ||
      (previous_https_latency_data.has_verdict() &&
       current_https_latency_data.verdict() !=
           previous_https_latency_data.verdict()) ||
      current_https_latency_data.problem() !=
          previous_https_latency_data.problem()) {
    return MetricEventType::NETWORK_HTTPS_LATENCY_CHANGE;
  }

  return std::nullopt;
}
}  // namespace reporting
