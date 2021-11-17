// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/https_latency_sampler.h"

#include "base/memory/ptr_util.h"
#include "base/task/bind_post_task.h"
#include "chrome/browser/ash/net/network_diagnostics/https_latency_routine.h"

namespace reporting {
namespace {

using RoutineResultPtr = chromeos::network_diagnostics::mojom::RoutineResultPtr;

void ConvertMojomRoutineResultToTelemetry(
    const RoutineResultPtr& routine_result,
    HttpsLatencyRoutineData* https_latency_data) {
  using chromeos::network_diagnostics::mojom::RoutineProblems;
  using HttpsLatencyProblemMojom =
      chromeos::network_diagnostics::mojom::HttpsLatencyProblem;
  using RoutineVerdictMojom =
      chromeos::network_diagnostics::mojom::RoutineVerdict;

  switch (routine_result->verdict) {
    case RoutineVerdictMojom::kNoProblem:
      https_latency_data->set_verdict(RoutineVerdict::NO_PROBLEM);
      break;
    case RoutineVerdictMojom::kProblem:
      https_latency_data->set_verdict(RoutineVerdict::PROBLEM);
      break;
    case RoutineVerdictMojom::kNotRun:
      https_latency_data->set_verdict(RoutineVerdict::NOT_RUN);
      break;
  }

  if (!routine_result->problems ||
      routine_result->problems->which() !=
          RoutineProblems::Tag::HTTPS_LATENCY_PROBLEMS ||
      routine_result->problems->get_https_latency_problems().empty()) {
    return;
  }

  const auto& problems = routine_result->problems->get_https_latency_problems();
  // Only one problem is expected for HttpsLatencyRoutine if any.
  switch (problems[0]) {
    case HttpsLatencyProblemMojom::kFailedDnsResolutions:
      https_latency_data->set_problem(
          HttpsLatencyProblem::FAILED_DNS_RESOLUTIONS);
      break;
    case HttpsLatencyProblemMojom::kFailedHttpsRequests:
      https_latency_data->set_problem(
          HttpsLatencyProblem::FAILED_HTTPS_REQUESTS);
      break;
    case HttpsLatencyProblemMojom::kHighLatency:
      https_latency_data->set_problem(HttpsLatencyProblem::HIGH_LATENCY);
      break;
    case HttpsLatencyProblemMojom::kVeryHighLatency:
      https_latency_data->set_problem(HttpsLatencyProblem::VERY_HIGH_LATENCY);
      break;
  }
}
}  // namespace

HttpsLatencySampler::HttpsLatencySampler() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  https_latency_routine_getter_ = base::BindRepeating([]() {
    return std::make_unique<
        chromeos::network_diagnostics::HttpsLatencyRoutine>();
  });
}

HttpsLatencySampler::~HttpsLatencySampler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void HttpsLatencySampler::Collect(MetricCallback callback) {
  CHECK(base::SequencedTaskRunnerHandle::IsSet());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  metric_callbacks_.push(std::move(callback));
  if (is_routine_running_) {
    return;
  }

  https_latency_routine_ = https_latency_routine_getter_.Run();
  chromeos::network_diagnostics::RoutineResultCallback routine_callback =
      base::BindOnce(&HttpsLatencySampler::OnHttpsLatencyRoutineCompleted,
                     weak_ptr_factory_.GetWeakPtr());
  https_latency_routine_->RunRoutine(base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(), std::move(routine_callback)));

  is_routine_running_ = true;
}

void HttpsLatencySampler::SetHttpsLatencyRoutineGetterForTest(
    HttpsLatencyRoutineGetter https_latency_routine_getter) {
  https_latency_routine_getter_ = std::move(https_latency_routine_getter);
}

void HttpsLatencySampler::OnHttpsLatencyRoutineCompleted(
    RoutineResultPtr routine_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  https_latency_routine_.reset();
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
