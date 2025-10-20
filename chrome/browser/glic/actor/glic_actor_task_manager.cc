// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/actor/glic_actor_task_manager.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "mojo/public/cpp/base/proto_wrapper.h"

namespace glic {

GlicActorTaskManager::GlicActorTaskManager(
    Profile* profile,
    actor::ActorKeyedService* actor_keyed_service)
    : profile_(profile), actor_keyed_service_(actor_keyed_service) {
  CHECK(profile_);
  CHECK(actor_keyed_service_);
}

GlicActorTaskManager::~GlicActorTaskManager() = default;

void GlicActorTaskManager::CreateTask(
    base::WeakPtr<actor::ActorTaskDelegate> delegate,
    actor::webui::mojom::TaskOptionsPtr options,
    mojom::WebClientHandler::CreateTaskCallback callback) {
  if (!base::FeatureList::IsEnabled(features::kGlicActor)) {
    std::move(callback).Run(
        base::unexpected(mojom::CreateTaskErrorReason::kTaskSystemUnavailable));
    return;
  }
  actor::TaskId task_id = actor_keyed_service_->CreateTaskWithOptions(
      std::move(options), std::move(delegate));
  std::move(callback).Run(task_id.value());
}

void GlicActorTaskManager::PerformActionsFinished(
    mojom::WebClientHandler::PerformActionsCallback callback,
    actor::TaskId task_id,
    base::TimeTicks start_time,
    bool skip_async_observation_information,
    actor::mojom::ActionResultCode result_code,
    std::optional<size_t> index_of_failed_action,
    std::vector<actor::ActionResultWithLatencyInfo> action_results) {
  actor::ActorTask* task = actor_keyed_service_->GetTask(task_id);

  // Task is checked when calling PerformActions and it doesn't go away.
  CHECK(task);

  // The callback doesn't need any weak semantics since all it does is wrap the
  // result and pass it to the mojo callback. If `this` is destroyed the mojo
  // connection is closed so this will be a no-op but the callback doesn't touch
  // any freed memory.
  auto result_callback = base::BindOnce(
      [](mojom::WebClientHandler::PerformActionsCallback callback,
         std::unique_ptr<optimization_guide::proto::ActionsResult> result,
         std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry>
             journal_entry) {
        CHECK(result);
        std::move(callback).Run(mojo_base::ProtoWrapper(*result));
      },
      std::move(callback));

  actor::BuildActionsResultWithObservations(
      *profile_, start_time, result_code, index_of_failed_action,
      std::move(action_results), *task, skip_async_observation_information,
      std::move(result_callback));
}

void GlicActorTaskManager::PerformActions(
    const std::vector<uint8_t>& actions_proto,
    mojom::WebClientHandler::PerformActionsCallback callback) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  // TODO(bokan): Refactor the actor code in this class into an actor-specific
  // wrapper for proto-to-actor conversion.
  optimization_guide::proto::Actions actions;
  if (!actions.ParseFromArray(actions_proto.data(), actions_proto.size())) {
    std::move(callback).Run(
        base::unexpected(mojom::PerformActionsErrorReason::kInvalidProto));
    return;
  }

  actor_keyed_service_->GetJournal().Log(
      GURL(), actor::TaskId(actions.task_id()),
      actor::mojom::JournalTrack::kActor, "GlicPerformActions",
      actor::JournalDetailsBuilder()
          .Add("proto", actor::ToBase64(actions))
          .Build());

  if (!actions.has_task_id()) {
    std::move(callback).Run(
        base::unexpected(mojom::PerformActionsErrorReason::kMissingTaskId));
    return;
  }

  actor::TaskId task_id(actions.task_id());
  if (!actor_keyed_service_->GetTask(task_id)) {
    actor_keyed_service_->GetJournal().Log(GURL::EmptyGURL(), task_id,
                                           actor::mojom::JournalTrack::kActor,
                                           "Act Failed",
                                           actor::JournalDetailsBuilder()
                                               .AddError("No such task")
                                               .Add("id", task_id.value())
                                               .Build());

    optimization_guide::proto::ActionsResult response =
        actor::BuildErrorActionsResult(
            actor::mojom::ActionResultCode::kTaskWentAway, std::nullopt);
    std::move(callback).Run(mojo_base::ProtoWrapper(response));
    return;
  }

  actor::BuildToolRequestResult requests = actor::BuildToolRequest(actions);
  if (!requests.has_value()) {
    actor_keyed_service_->GetJournal().Log(
        GURL::EmptyGURL(), task_id, actor::mojom::JournalTrack::kActor,
        "Act Failed",
        actor::JournalDetailsBuilder()
            .AddError("Failed to convert proto::Actions to ToolRequest")
            .Add("failed_action_index", requests.error())
            .Build());
    optimization_guide::proto::ActionsResult response =
        actor::BuildErrorActionsResult(
            actor::mojom::ActionResultCode::kArgumentsInvalid,
            requests.error());
    std::move(callback).Run(mojo_base::ProtoWrapper(response));
    return;
  }
  bool skip_async_observation_information =
      actions.has_skip_async_observation_collection() &&
      actions.skip_async_observation_collection();
  actor_keyed_service_->PerformActions(
      task_id, std::move(requests.value()), actor::ActorTaskMetadata(actions),
      base::BindOnce(&GlicActorTaskManager::PerformActionsFinished,
                     GetWeakPtr(), std::move(callback), task_id, start_time,
                     skip_async_observation_information));
}

void GlicActorTaskManager::StopActorTask(
    actor::TaskId task_id,
    mojom::ActorTaskStopReason stop_reason) {
  actor::ActorTask* task = actor_keyed_service_->GetTask(task_id);
  if (!task || task->IsStopped()) {
    actor_keyed_service_->GetJournal().Log(
        GURL::EmptyGURL(), task_id, actor::mojom::JournalTrack::kActor,
        "Failed to stop task",
        actor::JournalDetailsBuilder()
            .AddError(task ? "Task already stopped" : "No such task")
            .Add("id", task_id.value())
            .Build());
    return;
  }

  const bool success = stop_reason == mojom::ActorTaskStopReason::kTaskComplete;

  actor_keyed_service_->StopTask(task->id(), success);
}

void GlicActorTaskManager::PauseActorTask(
    actor::TaskId task_id,
    mojom::ActorTaskPauseReason pause_reason,
    tabs::TabInterface::Handle tab_handle) {
  actor::ActorTask* task = actor_keyed_service_->GetTask(task_id);
  if (!task || task->IsStopped() || task->IsPaused()) {
    actor_keyed_service_->GetJournal().Log(
        GURL::EmptyGURL(), task_id, actor::mojom::JournalTrack::kActor,
        "Failed to pause task",
        actor::JournalDetailsBuilder()
            .AddError(task ? "Task is not running" : "No such task")
            .Add("id", task_id.value())
            .Build());
    return;
  }

  if (tab_handle != tabs::TabHandle::Null()) {
    // Pausing the task on a tab means we're actuating on it.
    task->AddTab(tab_handle, base::DoNothing());
  }

  const bool from_actor =
      pause_reason == mojom::ActorTaskPauseReason::kPausedByModel;

  task->Pause(from_actor);
}

void GlicActorTaskManager::ResumeActorTask(
    actor::TaskId task_id,
    const mojom::GetTabContextOptions& context_options,
    glic::mojom::WebClientHandler::ResumeActorTaskCallback callback) {
  actor::ActorTask* task = actor_keyed_service_->GetTask(task_id);
  if (!task || !task->IsPaused()) {
    std::string error_message = task ? "Task is not paused" : "No such task";
    actor_keyed_service_->GetJournal().Log(GURL::EmptyGURL(), task_id,
                                           actor::mojom::JournalTrack::kActor,
                                           "Failed to resume task",
                                           actor::JournalDetailsBuilder()
                                               .AddError(error_message)
                                               .Add("id", task_id.value())
                                               .Build());
    std::move(callback).Run(
        mojom::GetContextResult::NewErrorReason(error_message));
    return;
  }

  task->Resume();

  // TODO(crbug.com/420669167): GetLastActedTabs should only ever have 1 tab in
  // it for now but once we support multi-tab we'll need to grab observations
  // for all relevant tabs.
  DCHECK_GT(task->GetLastActedTabs().size(), 0ul);
  DCHECK_LT(task->GetLastActedTabs().size(), 2ul);
  tabs::TabInterface* tab_of_resumed_task = nullptr;
  for (tabs::TabHandle tab_handle : task->GetLastActedTabs()) {
    if (tabs::TabInterface* tab = tab_handle.Get()) {
      tab_of_resumed_task = tab;
      break;
    }
  }
  if (!tab_of_resumed_task) {
    std::string error_message = "No tab for observation";
    actor_keyed_service_->GetJournal().Log(GURL::EmptyGURL(), task_id,
                                           actor::mojom::JournalTrack::kActor,
                                           "Failed to resume task",
                                           actor::JournalDetailsBuilder()
                                               .AddError(error_message)
                                               .Add("id", task_id.value())
                                               .Build());
    std::move(callback).Run(
        glic::mojom::GetContextResult::NewErrorReason(error_message));
    return;
  }

  auto observation_callback = base::BindOnce(
      [](glic::mojom::WebClientHandler::ResumeActorTaskCallback reply_callback,
         glic::mojom::TabDataPtr tab_data,
         actor::ActorKeyedService::TabObservationResult result) {
        if (!result.has_value()) {
          std::move(reply_callback)
              .Run(glic::mojom::GetContextResult::NewErrorReason(
                  result.error()));
          return;
        }

        page_content_annotations::FetchPageContextResult& page_context =
            *result.value();

        // RequestTabObservation guarantees a successful request has both
        // screenshot and APC.
        CHECK(page_context.screenshot_result.has_value());
        CHECK(page_context.annotated_page_content_result.has_value());

        auto glic_tab_context = mojom::TabContext::New();

        glic_tab_context->tab_data = std::move(tab_data);

        glic_tab_context->viewport_screenshot = glic::mojom::Screenshot::New(
            page_context.screenshot_result->dimensions.width(),
            page_context.screenshot_result->dimensions.height(),
            std::move(page_context.screenshot_result->screenshot_data),
            page_context.screenshot_result->mime_type,
            // TODO(b/380495633): Finalize and implement image annotations.
            glic::mojom::ImageOriginAnnotations::New());

        glic_tab_context->annotated_page_data = mojom::AnnotatedPageData::New();
        glic_tab_context->annotated_page_data->annotated_page_content =
            mojo_base::ProtoWrapper(
                page_context.annotated_page_content_result->proto);
        glic_tab_context->annotated_page_data->metadata =
            std::move(page_context.annotated_page_content_result->metadata);

        glic::mojom::GetContextResultPtr glic_result =
            glic::mojom::GetContextResult::NewTabContext(
                std::move(glic_tab_context));
        std::move(reply_callback).Run(std::move(glic_result));
      },
      std::move(callback), CreateTabData(tab_of_resumed_task->GetContents()));

  actor_keyed_service_->RequestTabObservation(*tab_of_resumed_task, task_id,
                                              std::move(observation_callback));
}

base::WeakPtr<GlicActorTaskManager> GlicActorTaskManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace glic
