// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_keyed_service_proto_wrapper.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/tab_observation_controller.h"
#include "chrome/common/actor.mojom.h"

namespace actor {

ActorKeyedServiceProtoWrapper::ActorKeyedServiceProtoWrapper(
    ActorKeyedService* actor_service) {
  // TODO(crbug.com/565390794): Store actor_service.
  NOTIMPLEMENTED();
}

ActorKeyedServiceProtoWrapper::~ActorKeyedServiceProtoWrapper() = default;

void ActorKeyedServiceProtoWrapper::PerformActions(
    const optimization_guide::proto::Actions& actions,
    PerformActionsCallback callback) {
  // TODO(crbug.com/565390794): Implement proto actions execution and
  // observation.
  NOTIMPLEMENTED();
  std::move(callback).Run(optimization_guide::proto::ActionsResult());
}

// static
optimization_guide::proto::ActionsResult
ActorKeyedServiceProtoWrapper::BuildActionsResult(
    base::TimeTicks start_time,
    const std::vector<ActionResultWithLatencyInfo>& action_results,
    ObservationResult& observation_result) {
  optimization_guide::proto::ActionsResult response;

  mojom::ActionResultCode result_code = mojom::ActionResultCode::kOk;
  std::optional<size_t> index_of_failed_action;
  ExtractErrorResult(action_results, &result_code, index_of_failed_action);

  response.set_action_result(static_cast<int32_t>(result_code));
  if (index_of_failed_action) {
    response.set_index_of_failed_action(*index_of_failed_action);
  }
  CopyScriptToolResults(response, action_results);

  for (const auto& action_result : action_results) {
    if (IsOk(*action_result.result)) {
      response.add_extra_information(action_result.result->message);
    } else {
      // In case of an error, the message is copied to `error_message` instead.
      response.add_extra_information(std::string());
    }
  }

  auto* latency_info = response.mutable_latency_information();
  for (size_t i = 0; i < action_results.size(); ++i) {
    const auto& action_result = action_results.at(i);
    CHECK(action_result.result->execution_end_time);
    {
      auto* latency_step = latency_info->add_latency_steps();
      latency_step->mutable_action()->set_action_index(i);
      latency_step->set_latency_start_ms(
          (action_result.start_time - start_time).InMilliseconds());
      latency_step->set_latency_stop_ms(
          (*action_result.result->execution_end_time - start_time)
              .InMilliseconds());
    }
    // Don't report a page stabilization time if the start and end are the same.
    // Not every tool needs stabilization.
    if (*action_result.result->execution_end_time != action_result.end_time) {
      auto* latency_step = latency_info->add_latency_steps();
      latency_step->mutable_page_stabilization()->set_action_index(i);
      latency_step->set_latency_start_ms(
          (*action_result.result->execution_end_time - start_time)
              .InMilliseconds());
      latency_step->set_latency_stop_ms(
          (action_result.end_time - start_time).InMilliseconds());
    }
    if (!IsOk(*action_result.result)) {
      CHECK_EQ(*index_of_failed_action, i);
      response.set_error_message(action_result.result->message);
    }
  }

  for (auto& obs : observation_result.tab_observations) {
    *response.add_tabs() = std::move(obs);
  }
  for (auto& obs : observation_result.window_observations) {
    *response.add_windows() = std::move(obs);
  }
  for (auto& step : observation_result.latency_steps) {
    *latency_info->add_latency_steps() = std::move(step);
  }

  RecordTabObservationResultHistogram(response);
  RecordObservationOutcomeHistogram(
      response, observation_result.attempted_observation_retry);

  return response;
}

}  // namespace actor
