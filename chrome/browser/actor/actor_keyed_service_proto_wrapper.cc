// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_keyed_service_proto_wrapper.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/to_string.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/tab_observation_controller.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/common/actor.mojom.h"
#include "components/actor/core/journal_details_builder.h"
#include "url/gurl.h"

namespace actor {

ActorKeyedServiceProtoWrapper::ActorKeyedServiceProtoWrapper(
    ActorKeyedService* actor_service)
    : actor_service_(actor_service) {
  CHECK(actor_service_);
}

ActorKeyedServiceProtoWrapper::~ActorKeyedServiceProtoWrapper() = default;

void ActorKeyedServiceProtoWrapper::PerformActions(
    const optimization_guide::proto::Actions& actions,
    PerformActionsCallback callback) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  TaskId task_id(actions.task_id());
  if (!actor_service_->GetTask(task_id)) {
    actor_service_->GetJournal().Log(GURL::EmptyGURL(), task_id, "Act Failed",
                                     JournalDetailsBuilder()
                                         .AddError("No such task")
                                         .Add("id", task_id.value())
                                         .Build());
    std::move(callback).Run(BuildErrorActionsResult(
        mojom::ActionResultCode::kTaskWentAway, std::nullopt));
    return;
  }

  BuildToolRequestResult requests = BuildToolRequest(actions);
  if (!requests.has_value()) {
    actor_service_->GetJournal().Log(
        GURL::EmptyGURL(), task_id, "Act Failed",
        JournalDetailsBuilder()
            .AddError("Failed to convert proto::Actions to ToolRequest")
            .Add("failed_action_index", requests.error().first)
            .Add("error_code", static_cast<int>(requests.error().second))
            .Build());
    std::move(callback).Run(BuildErrorActionsResult(requests.error().second,
                                                    requests.error().first));
    return;
  }

  bool skip_async_observation_information =
      actions.has_skip_async_observation_collection() &&
      actions.skip_async_observation_collection();

  actor_service_->PerformActions(
      task_id, std::move(requests.value()), ActorTaskMetadata(actions),
      base::BindOnce(&ActorKeyedServiceProtoWrapper::OnPerformActionsFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     task_id, start_time, skip_async_observation_information,
                     GetScreenshotCollectionOptions(actions)));
}

void ActorKeyedServiceProtoWrapper::OnPerformActionsFinished(
    PerformActionsCallback callback,
    TaskId task_id,
    base::TimeTicks start_time,
    bool skip_async_observation_information,
    std::optional<page_content_annotations::ScreenshotOptions::
                      ScreenshotCollectionOptions>
        screenshot_collection_options,
    std::vector<ActionResultWithLatencyInfo> action_results,
    TabObservationStrategy observation_strategy) {
  mojom::ActionResultCode result_code = mojom::ActionResultCode::kOk;
  std::optional<size_t> index_of_failed_action;
  ExtractErrorResult(action_results, &result_code, index_of_failed_action);
  actor_service_->GetJournal().Log(
      GURL::EmptyGURL(), task_id, "PerformActionsFinished",
      JournalDetailsBuilder()
          .Add("result_code", base::ToString(result_code))
          .Build());

  // TODO(b/470985724): Reply at the time the task is stopped/canceled instead
  // of here.
  if (!actor_service_->GetTask(task_id)) {
    std::move(callback).Run(BuildErrorActionsResult(
        mojom::ActionResultCode::kTaskWentAway, std::nullopt));
    return;
  }

  if (result_code == mojom::ActionResultCode::kTaskPaused ||
      result_code == mojom::ActionResultCode::kTaskWentAway) {
    std::move(callback).Run(BuildErrorActionsResult(result_code, std::nullopt));
    return;
  }

  auto journal_entry = actor_service_->GetJournal().CreatePendingAsyncEntry(
      GURL(), task_id, MakeBrowserTrackUUID(task_id),
      "TabObservationController",
      JournalDetailsBuilder()
          .Add("result_code", base::ToString(result_code))
          .Add("skip_async_observation_information",
               skip_async_observation_information)
          .Build());

  // base::Unretained(this) is safe because `observation_controllers_` is owned
  // by this class and the controller guarantees that it will not run the
  // callback after its own destruction.
  auto done_callback =
      base::BindOnce(&ActorKeyedServiceProtoWrapper::OnTabObservationComplete,
                     base::Unretained(this), std::move(callback), start_time,
                     action_results, std::move(journal_entry));

  auto controller = std::make_unique<TabObservationController>(
      actor_service_->GetProfile(), task_id, start_time,
      skip_async_observation_information, std::move(action_results),
      std::move(observation_strategy), std::move(done_callback));
  controller->set_screenshot_collection_options(
      std::move(screenshot_collection_options));
  TabObservationController* controller_ptr = controller.get();
  observation_controllers_.push_back(std::move(controller));
  controller_ptr->Start();
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

void ActorKeyedServiceProtoWrapper::OnTabObservationComplete(
    PerformActionsCallback callback,
    base::TimeTicks start_time,
    std::vector<ActionResultWithLatencyInfo> action_results,
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry,
    TabObservationController* controller_ptr,
    std::unique_ptr<ObservationResult> result) {
  CHECK(result);
  std::erase_if(observation_controllers_, [&](const auto& controller) {
    return controller.get() == controller_ptr;
  });

  optimization_guide::proto::ActionsResult response =
      BuildActionsResult(start_time, action_results, *result);

  if (journal_entry) {
    journal_entry->EndEntry({});
  }

  std::move(callback).Run(std::move(response));
}

}  // namespace actor
