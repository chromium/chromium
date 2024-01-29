// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/https_latency_sampler.h"

#include <optional>
#include <utility>

#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/net/network_health/network_health_manager.h"
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

}  // namespace

void HttpsLatencySampler::Delegate::BindDiagnosticsReceiver(
    mojo::PendingReceiver<network_diagnostics_mojom::NetworkDiagnosticsRoutines>
        receiver) {
  ash::network_health::NetworkHealthManager::GetInstance()
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
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const ash::NetworkState* const network_state =
      ash::NetworkHandler::Get()
          ->network_state_handler()
          ->ConnectedNetworkByType(ash::NetworkTypePattern::Default());
  if (!network_state || !network_state->IsOnline()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

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
  network_diagnostics_service_->RunHttpsLatency(
      network_diagnostics_mojom::RoutineCallSource::kMetricsReporting,
      base::BindPostTaskToCurrentDefault(std::move(routine_callback)));

  is_routine_running_ = true;
}

void HttpsLatencySampler::OnHttpsLatencyRoutineCompleted(
    network_diagnostics_mojom::RoutineResultPtr routine_result) {
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
}  // namespace reporting
