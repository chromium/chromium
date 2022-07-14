// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/https_latency_sampler.h"

#include <utility>

#include "base/task/bind_post_task.h"
#include "chrome/browser/ash/net/network_health/network_health_service.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {
namespace {

namespace network_diagnostics_mojom = ::chromeos::network_diagnostics::mojom;

void ConvertMojomRoutineResultToTelemetry(
    const network_diagnostics_mojom::RoutineResultPtr& routine_result,
    HttpsLatencyRoutineData* https_latency_data) {
  CHECK(!routine_result.is_null());

  if (!routine_result->result_value.is_null() &&
      routine_result->result_value->is_https_latency_result_value()) {
    https_latency_data->set_latency_ms(
        routine_result->result_value->get_https_latency_result_value()
            ->latency.InMilliseconds());
  }

  switch (routine_result->verdict) {
    case network_diagnostics_mojom::RoutineVerdict::kNoProblem:
      https_latency_data->set_verdict(RoutineVerdict::NO_PROBLEM);
      break;
    case network_diagnostics_mojom::RoutineVerdict::kProblem:
      https_latency_data->set_verdict(RoutineVerdict::PROBLEM);
      break;
    case network_diagnostics_mojom::RoutineVerdict::kNotRun:
      https_latency_data->set_verdict(RoutineVerdict::NOT_RUN);
      break;
  }

  if (!routine_result->problems ||
      !routine_result->problems->is_https_latency_problems() ||
      routine_result->problems->get_https_latency_problems().empty()) {
    return;
  }

  const auto& problems = routine_result->problems->get_https_latency_problems();
  // Only one problem is expected for HttpsLatencyRoutine if any.
  switch (problems[0]) {
    case network_diagnostics_mojom::HttpsLatencyProblem::kFailedDnsResolutions:
      https_latency_data->set_problem(
          HttpsLatencyProblem::FAILED_DNS_RESOLUTIONS);
      break;
    case network_diagnostics_mojom::HttpsLatencyProblem::kFailedHttpsRequests:
      https_latency_data->set_problem(
          HttpsLatencyProblem::FAILED_HTTPS_REQUESTS);
      break;
    case network_diagnostics_mojom::HttpsLatencyProblem::kHighLatency:
      https_latency_data->set_problem(HttpsLatencyProblem::HIGH_LATENCY);
      break;
    case network_diagnostics_mojom::HttpsLatencyProblem::kVeryHighLatency:
      https_latency_data->set_problem(HttpsLatencyProblem::VERY_HIGH_LATENCY);
      break;
  }
}

bool IsDeviceOnline() {
  ::ash::NetworkStateHandler::NetworkStateList network_state_list;
  ::ash::NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
      ::ash::NetworkTypePattern::Default(),
      /*configured_only=*/true,
      /*visible_only=*/false,
      /*limit=*/0,  // no limit to number of results
      &network_state_list);
  for (const auto* network : network_state_list) {
    if (network->IsOnline()) {
      return true;
    }
  }
  return false;
}
}  // namespace

void HttpsLatencySampler::Delegate::BindDiagnosticsReceiver(
    mojo::PendingReceiver<
        ::chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
        receiver) {
  chromeos::network_health::NetworkHealthService::GetInstance()
      ->BindDiagnosticsReceiver(std::move(receiver));
}

HttpsLatencySampler::HttpsLatencySampler(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

HttpsLatencySampler::~HttpsLatencySampler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void HttpsLatencySampler::MaybeCollect(OptionalMetricCallback callback) {
  CHECK(base::SequencedTaskRunnerHandle::IsSet());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  metric_callbacks_.push(std::move(callback));
  if (is_routine_running_) {
    return;
  }

  if (!network_diagnostics_service_.is_bound()) {
    delegate_->BindDiagnosticsReceiver(
        network_diagnostics_service_.BindNewPipeAndPassReceiver());
  }
  auto routine_callback =
      base::BindOnce(&HttpsLatencySampler::OnHttpsLatencyRoutineCompleted,
                     weak_ptr_factory_.GetWeakPtr());
  network_diagnostics_service_->RunHttpsLatency(base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(), std::move(routine_callback)));

  is_routine_running_ = true;
}

void HttpsLatencySampler::OnHttpsLatencyRoutineCompleted(
    ::chromeos::network_diagnostics::mojom::RoutineResultPtr routine_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_routine_running_ = false;

  MetricData metric_data;
  auto* https_latency_data = metric_data.mutable_telemetry_data()
                                 ->mutable_networks_telemetry()
                                 ->mutable_https_latency_data();
  ConvertMojomRoutineResultToTelemetry(routine_result, https_latency_data);

  while (!metric_callbacks_.empty()) {
    std::move(metric_callbacks_.front()).Run(metric_data);
    metric_callbacks_.pop();
  }
}

absl::optional<MetricEventType> HttpsLatencyEventDetector::DetectEvent(
    const MetricData& previous_metric_data,
    const MetricData& current_metric_data) {
  const auto& previous_https_latency_data =
      previous_metric_data.telemetry_data()
          .networks_telemetry()
          .https_latency_data();
  const auto& current_https_latency_data = current_metric_data.telemetry_data()
                                               .networks_telemetry()
                                               .https_latency_data();

  if (current_https_latency_data.has_problem() &&
      (current_https_latency_data.problem() ==
           HttpsLatencyProblem::FAILED_HTTPS_REQUESTS ||
       current_https_latency_data.problem() ==
           HttpsLatencyProblem::FAILED_DNS_RESOLUTIONS) &&
      !IsDeviceOnline()) {
    return absl::nullopt;
  }

  if ((!previous_https_latency_data.has_verdict() &&
       current_https_latency_data.verdict() == RoutineVerdict::PROBLEM) ||
      (previous_https_latency_data.has_verdict() &&
       current_https_latency_data.verdict() !=
           previous_https_latency_data.verdict()) ||
      current_https_latency_data.problem() !=
          previous_https_latency_data.problem()) {
    return MetricEventType::NETWORK_HTTPS_LATENCY_CHANGE;
  }

  return absl::nullopt;
}
}  // namespace reporting
