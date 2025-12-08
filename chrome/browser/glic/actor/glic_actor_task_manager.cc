// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/actor/glic_actor_task_manager.h"

#include "base/base64.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/actor.mojom-data-view.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "mojo/public/cpp/base/proto_wrapper.h"

namespace glic {

namespace {
BASE_FEATURE(kGlicReloadAfterPerformActionsCrash,
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace

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

  CancelTask();

  current_task_id_ = actor_keyed_service_->CreateTaskWithOptions(
      std::move(options), std::move(delegate));
  std::move(callback).Run(current_task_id_.value());
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

  actor_keyed_service_->GetJournal().Log(
      GURL::EmptyGURL(), task_id, "PerformActionsFinished",
      actor::JournalDetailsBuilder()
          .Add("result_code", base::ToString(result_code))
          .Build());

  // Task has disappeared, clear the current task id.
  if (!task) {
    ResetTaskState();
    optimization_guide::proto::ActionsResult response =
        actor::BuildErrorActionsResult(
            actor::mojom::ActionResultCode::kTaskWentAway, std::nullopt);
    std::move(callback).Run(mojo_base::ProtoWrapper(response));
    return;
  }

  // If the task went away it must have been handled in the !task branch above.
  DCHECK_NE(result_code, actor::mojom::ActionResultCode::kTaskWentAway);

  if (result_code == actor::mojom::ActionResultCode::kTaskPaused) {
    optimization_guide::proto::ActionsResult response =
        actor::BuildErrorActionsResult(
            actor::mojom::ActionResultCode::kTaskPaused, std::nullopt);
    std::move(callback).Run(mojo_base::ProtoWrapper(response));
    return;
  }

  if (!attempted_reload_ &&
      base::FeatureList::IsEnabled(kGlicReloadAfterPerformActionsCrash) &&
      result_code == actor::mojom::ActionResultCode::kRendererCrashed) {
    // We call back into PerformActionsFinished once we've reloaded the tab.
    auto peform_actions_done = base::BindOnce(
        &GlicActorTaskManager::PerformActionsFinished,
        weak_ptr_factory_.GetWeakPtr(), std::move(callback), task_id,
        start_time, skip_async_observation_information, result_code,
        index_of_failed_action, std::move(action_results));
    ReloadTab(*task, std::move(peform_actions_done));
  } else {
    // The callback doesn't need any weak semantics since all it does is wrap
    // the result and pass it to the mojo callback. If `this` is destroyed the
    // mojo connection is closed so this will be a no-op but the callback
    // doesn't touch any freed memory.
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
}

void GlicActorTaskManager::ReloadTab(actor::ActorTask& task,
                                     base::OnceClosure callback) {
  CHECK(!attempted_reload_);
  // TODO(b/464019189): This code only deals with a single tab crashing. If
  // they are multiple tabs that crashed we might want to figure out how to
  // deal with that.
  for (tabs::TabHandle tab_handle : task.GetLastActedTabs()) {
    tabs::TabInterface* tab = tab_handle.Get();
    if (!tab) {
      continue;
    }

    if (content::WebContents* contents = tab->GetContents()) {
      if (contents->IsCrashed()) {
        attempted_reload_ = true;
        reload_observer_ = std::make_unique<actor::ObservationDelayController>(
            task.id(), actor_keyed_service_->GetJournal());
        contents->GetController().Reload(content::ReloadType::NORMAL, true);
        reload_observer_->Wait(
            *tab, base::BindOnce(&GlicActorTaskManager::ReloadObserverDone,
                                 base::Unretained(this), std::move(callback)));
        return;
      }
    }
  }

  if (callback) {
    std::move(callback).Run();
  }
}

void GlicActorTaskManager::PerformActions(
    const std::vector<uint8_t>& actions_proto,
    mojom::WebClientHandler::PerformActionsCallback callback) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  // TODO(bokan): Refactor the actor code in this class into an actor-specific
  // wrapper for proto-to-actor conversion.
  optimization_guide::proto::Actions actions;
  if (!actions.ParseFromArray(actions_proto.data(), actions_proto.size())) {
    // TODO(bokan): include the base64 proto in the error
    actor_keyed_service_->GetJournal().Log(
        GURL(), actor::TaskId(), "GlicPerformActions",
        actor::JournalDetailsBuilder().AddError("Invalid Proto").Build());
    std::move(callback).Run(
        base::unexpected(mojom::PerformActionsErrorReason::kInvalidProto));
    return;
  }

  actor_keyed_service_->GetJournal().Log(
      GURL(), actor::TaskId(actions.task_id()), "GlicPerformActions",
      actor::JournalDetailsBuilder()
          .Add("proto", actor::ToBase64(actions))
          .Build());

  if (!actions.has_task_id()) {
    actor_keyed_service_->GetJournal().Log(
        GURL(), actor::TaskId(actions.task_id()), "GlicPerformActions",
        actor::JournalDetailsBuilder().AddError("Missing Task Id").Build());
    std::move(callback).Run(
        base::unexpected(mojom::PerformActionsErrorReason::kMissingTaskId));
    return;
  }

  actor::TaskId task_id(actions.task_id());
  if (!actor_keyed_service_->GetTask(task_id)) {
    actor_keyed_service_->GetJournal().Log(GURL::EmptyGURL(), task_id,
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
        GURL::EmptyGURL(), task_id, "Act Failed",
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
  if (current_task_id_ == task_id) {
    ResetTaskState();
  }

  actor::ActorTask* task = actor_keyed_service_->GetTask(task_id);
  if (!task || task->IsCompleted()) {
    actor_keyed_service_->GetJournal().Log(
        GURL::EmptyGURL(), task_id, "Failed to stop task",
        actor::JournalDetailsBuilder()
            .AddError(task ? "Task already stopped" : "No such task")
            .Add("id", task_id.value())
            .Build());
    return;
  }

  actor::ActorTask::StoppedReason reason;
  switch (stop_reason) {
    case glic::mojom::ActorTaskStopReason::kTaskComplete:
      reason = actor::ActorTask::StoppedReason::kTaskComplete;
      break;
    case glic::mojom::ActorTaskStopReason::kStoppedByUser:
      reason = actor::ActorTask::StoppedReason::kStoppedByUser;
      break;
    case glic::mojom::ActorTaskStopReason::kModelError:
      reason = actor::ActorTask::StoppedReason::kModelError;
      break;
    case glic::mojom::ActorTaskStopReason::kUserStartedNewChat:
      reason = actor::ActorTask::StoppedReason::kUserStartedNewChat;
      break;
    case glic::mojom::ActorTaskStopReason::kUserLoadedPreviousChat:
      reason = actor::ActorTask::StoppedReason::kUserLoadedPreviousChat;
      break;
  }

  actor_keyed_service_->StopTask(task->id(), reason);
}

void GlicActorTaskManager::MaybeShowDeactivationToastUi() {
  BrowserWindowInterface* const last_active_bwi =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  actor_keyed_service_->GetActorUiStateManager()->MaybeShowToast(
      last_active_bwi);
}

void GlicActorTaskManager::PauseActorTask(
    actor::TaskId task_id,
    mojom::ActorTaskPauseReason pause_reason,
    tabs::TabInterface::Handle tab_handle) {
  actor::ActorTask* task = actor_keyed_service_->GetTask(task_id);
  if (!task || task->IsCompleted() || task->IsUnderUserControl()) {
    actor_keyed_service_->GetJournal().Log(
        GURL::EmptyGURL(), task_id, "Failed to pause task",
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
  if (!task || !task->IsUnderUserControl()) {
    std::string error_message = task ? "Task is not paused" : "No such task";
    actor_keyed_service_->GetJournal().Log(GURL::EmptyGURL(), task_id,
                                           "Failed to resume task",
                                           actor::JournalDetailsBuilder()
                                               .AddError(error_message)
                                               .Add("id", task_id.value())
                                               .Build());
    std::move(callback).Run(mojom::GetContextResultWithActionResultCode::New(
        mojom::GetContextResult::NewErrorReason(error_message), std::nullopt));
    return;
  }

  task->Resume();

  actor::mojom::ActionResultCode resume_response_code =
      actor::mojom::ActionResultCode::kOk;
  if (actor::ExecutionEngine* execution_engine = task->GetExecutionEngine()) {
    resume_response_code = execution_engine->user_take_over_result().value_or(
        actor::mojom::ActionResultCode::kOk);
    // Reset the takeover result
    execution_engine->set_user_take_over_result(std::nullopt);
  }

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
                                           "Failed to resume task",
                                           actor::JournalDetailsBuilder()
                                               .AddError(error_message)
                                               .Add("id", task_id.value())
                                               .Build());
    std::move(callback).Run(mojom::GetContextResultWithActionResultCode::New(
        mojom::GetContextResult::NewErrorReason(error_message), std::nullopt));
    return;
  }

  auto observation_callback = base::BindOnce(
      [](glic::mojom::WebClientHandler::ResumeActorTaskCallback reply_callback,
         glic::mojom::TabDataPtr tab_data,
         actor::mojom::ActionResultCode resume_response_code,
         actor::ActorKeyedService::TabObservationResult result) {
        if (!result.has_value()) {
          std::move(reply_callback)
              .Run(mojom::GetContextResultWithActionResultCode::New(
                  mojom::GetContextResult::NewErrorReason(result.error()),
                  std::nullopt));
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

        glic::mojom::GetContextResultPtr tab_context_ptr =
            glic::mojom::GetContextResult::NewTabContext(
                std::move(glic_tab_context));

        std::move(reply_callback)
            .Run(mojom::GetContextResultWithActionResultCode::New(
                std::move(tab_context_ptr),
                static_cast<int32_t>(resume_response_code)));
      },
      std::move(callback), CreateTabData(tab_of_resumed_task->GetContents()),
      resume_response_code);

  actor_keyed_service_->RequestTabObservation(*tab_of_resumed_task, task_id,
                                              std::move(observation_callback));
}

bool GlicActorTaskManager::IsActuating() const {
  return !!current_task_id_;
}

void GlicActorTaskManager::InterruptActorTask(actor::TaskId task_id) {
  actor::ActorTask* task = actor_keyed_service_->GetTask(task_id);
  if (!task) {
    actor_keyed_service_->GetJournal().Log(GURL::EmptyGURL(), task_id,
                                           "Failed to interrupt task",
                                           actor::JournalDetailsBuilder()
                                               .AddError("No such task")
                                               .Add("id", task_id.value())
                                               .Build());
    return;
  }
  task->Interrupt();
}

void GlicActorTaskManager::UninterruptActorTask(actor::TaskId task_id) {
  actor::ActorTask* task = actor_keyed_service_->GetTask(task_id);
  if (!task) {
    actor_keyed_service_->GetJournal().Log(GURL::EmptyGURL(), task_id,
                                           "Failed to uninterrupt task",
                                           actor::JournalDetailsBuilder()
                                               .AddError("No such task")
                                               .Add("id", task_id.value())
                                               .Build());
    return;
  }
  actor::ActorTask::State next_state = actor::ActorTask::State::kReflecting;
  if (task->GetExecutionEngine() &&
      task->GetExecutionEngine()->HasActionSequence()) {
    // TODO(mcnee): Explicitly stash the old state instead of inferring it.
    next_state = actor::ActorTask::State::kActing;
  }
  task->Uninterrupt(next_state);
}

void GlicActorTaskManager::CreateActorTab(
    actor::TaskId task_id,
    bool open_in_background,
    const std::optional<int32_t>& initiator_tab_id,
    const std::optional<int32_t>& initiator_window_id,
    glic::mojom::WebClientHandler::CreateActorTabCallback callback) {
  tabs::TabHandle initiator_tab_handle =
      initiator_tab_id.has_value() ? tabs::TabHandle(*initiator_tab_id)
                                   : tabs::TabHandle::Null();
  SessionID initiator_window_session_id =
      initiator_window_id.has_value()
          ? SessionID::FromSerializedValue(*initiator_window_id)
          : SessionID::InvalidValue();

  actor_keyed_service_->CreateActorTab(
      task_id, open_in_background, initiator_tab_handle,
      initiator_window_session_id,
      base::BindOnce(&GlicActorTaskManager::CreateActorTabFinished,
                     GetWeakPtr(), std::move(callback)));
}

void GlicActorTaskManager::CreateActorTabFinished(
    glic::mojom::WebClientHandler::CreateActorTabCallback callback,
    tabs::TabInterface* new_tab) {
  std::move(callback).Run(
      CreateTabData(new_tab ? new_tab->GetContents() : nullptr));
}

void GlicActorTaskManager::ReloadObserverDone(base::OnceClosure callback) {
  reload_observer_.reset();
  std::move(callback).Run();
}

void GlicActorTaskManager::CancelTask() {
  if (current_task_id_) {
    StopActorTask(current_task_id_,
                  glic::mojom::ActorTaskStopReason::kStoppedByUser);
  }
}

void GlicActorTaskManager::ResetTaskState() {
  current_task_id_ = actor::TaskId();
  attempted_reload_ = false;
  reload_observer_.reset();
}

base::WeakPtr<GlicActorTaskManager> GlicActorTaskManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace glic
