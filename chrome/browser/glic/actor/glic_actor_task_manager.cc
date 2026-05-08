// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/actor/glic_actor_task_manager.h"

#include "base/base64.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/to_string.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tab_observation_controller.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/glic/actor/glic_actor_journal_handler.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic_mojom_traits.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/actor.mojom-shared.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/gfx/geometry/point.h"

namespace glic {

namespace {
BASE_FEATURE(kGlicReloadAfterPerformActionsCrash,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGlicRetryFailedObservations, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGlicRequireConversationIdForActorTask,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Observations can sometimes fail due to timeouts or issues stemming from
// high-load scenarios. When we retry, give it a few seconds to increase the
// probability that the system will now be in a state to return a successful
// observation. This should only ever happen very rarely so waiting a few
// seconds should be ok.
const base::FeatureParam<base::TimeDelta> kObservationRetryDelay{
    &kGlicRetryFailedObservations, "delay", base::Seconds(5)};

tabs::TabInterface* GetCrashedTab(actor::ActorTask& task) {
  // TODO(b/464019189): This code only deals with the first crashed tab per
  // Task. If there are multiple tabs that crashed we might want to figure out
  // how to deal with that.
  for (tabs::TabHandle tab_handle : task.GetLastActedTabs()) {
    tabs::TabInterface* tab = tab_handle.Get();
    if (!tab) {
      continue;
    }

    content::WebContents* contents = tab->GetContents();
    if (!contents) {
      continue;
    }
    if (contents->IsCrashed()) {
      return tab;
    }
  }

  return nullptr;
}

}  // namespace

GlicActorClientSession::GlicActorClientSession(GlicActorTaskManager* manager)
    : manager_(*manager),
      journal_handler_(
          std::make_unique<GlicActorJournalHandler>(manager->profile())) {
  // Unretained is safe because the subscription cancels the callback when
  // this is destroyed.
  can_act_on_web_changed_subscription_ =
      actor_policy_checker().AddActOnWebCapabilityChangedCallback(
          base::BindRepeating(&GlicActorClientSession::CanActOnWebChanged,
                              base::Unretained(this)));
}

GlicActorClientSession::~GlicActorClientSession() {
  CancelTask();
}

void GlicActorClientSession::LogBeginAsyncEvent(uint64_t event_async_id,
                                                int32_t task_id,
                                                const std::string& event,
                                                const std::string& details) {
  journal_handler_->LogBeginAsyncEvent(event_async_id, task_id, event, details);
}

void GlicActorClientSession::LogEndAsyncEvent(uint64_t event_async_id,
                                              const std::string& details) {
  journal_handler_->LogEndAsyncEvent(event_async_id, details);
}

void GlicActorClientSession::LogInstantEvent(int32_t task_id,
                                             const std::string& event,
                                             const std::string& details) {
  journal_handler_->LogInstantEvent(task_id, event, details);
}

void GlicActorClientSession::JournalClear() {
  journal_handler_->Clear();
}

void GlicActorClientSession::JournalSnapshot(
    bool clear_journal,
    glic::mojom::WebClientHandler::JournalSnapshotCallback callback) {
  journal_handler_->Snapshot(clear_journal, std::move(callback));
}

void GlicActorClientSession::JournalStart(uint64_t max_bytes,
                                          bool capture_screenshots) {
  journal_handler_->Start(max_bytes, capture_screenshots);
}

void GlicActorClientSession::JournalStop() {
  journal_handler_->Stop();
}

void GlicActorClientSession::JournalRecordFeedback(bool positive,
                                                   const std::string& reason) {
  journal_handler_->RecordFeedback(positive, reason);
}

GlicActorTaskManager::GlicActorTaskManager(
    Profile* profile,
    actor::ActorKeyedService* actor_keyed_service,
    GlicActorPolicyChecker& actor_policy_checker,
    GlicInstanceMetrics* instance_metrics,
    Delegate* delegate)
    : profile_(profile),
      actor_keyed_service_(actor_keyed_service),
      actor_policy_checker_(actor_policy_checker),
      instance_metrics_(instance_metrics),
      delegate_(delegate) {
  CHECK(profile_);
  CHECK(actor_keyed_service_);
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
}

GlicActorTaskManager::~GlicActorTaskManager() = default;

void GlicActorClientSession::CreateTask(
    actor::webui::mojom::TaskOptionsPtr options,
    glic::mojom::WebClientHandler::CreateTaskCallback callback) {
  instance_metrics().OnCreateTask();
  if (!current_task_id_.is_null()) {
    std::move(callback).Run(
        base::unexpected(mojom::CreateTaskErrorReason::kExistingActiveTask));
    return;
  }

  // Conversation ID must be available since a turn is required to create a task
  // and an ID becomes available at first turn. If you hit this in a test you
  // probably need to call RegisterConversation on your GlicInstance.
  // TODO(b/494212836) - The front end currently doesn't guarantee that
  // RegisterConversation is called first. Allow creating a task without a
  // conversationId until that's fixed (the conversationId in ActorTask isn't
  // yet used).
  const std::optional<std::string> conversation_id =
      manager_->delegate_->conversation_id();
  if (!conversation_id.has_value() &&
      base::FeatureList::IsEnabled(kGlicRequireConversationIdForActorTask)) {
    std::move(callback).Run(base::unexpected(
        mojom::CreateTaskErrorReason::kConversationNotRegistered));
    return;
  }

  const GlicActorPolicyChecker::CannotActReason reason_to_log =
      actor_policy_checker().CanActOnWeb()
          ? GlicActorPolicyChecker::CannotActReason::kNone
          : actor_policy_checker().CannotActOnWebReason();
  base::UmaHistogramEnumeration("Actor.Task.CreateFailedReason", reason_to_log);

  if (!actor_policy_checker().CanActOnWeb()) {
    // TODO(bokan): This was moved here to preserve behavior; the failure case
    // was only counting policy blocks which are a Glic-only concept. However,
    // the UMA histogram is in Actor which implies it records all sources of
    // actor tasks. This histogram should probably be migrated to be Glic
    // namespaced.
    actor::RecordActorTaskCreated(/*success=*/false);
    actor_keyed_service().GetJournal().Log(
        GURL(), actor::TaskId(), "GlicActorTaskManager::CreateTask",
        actor::JournalDetailsBuilder()
            .AddError("Actuation capability disabled")
            .Add("reason", base::ToString(reason_to_log))
            .Build());
    std::move(callback).Run(
        base::unexpected(mojom::CreateTaskErrorReason::kBlockedByPolicy));
    return;
  }

  actor::RecordActorTaskCreated(true);

  if (base::FeatureList::IsEnabled(actor::kGlicActorTransientTasks)) {
    if (actor::kGlicActorTransientTasksForceTransient.Get()) {
      if (!options) {
        options = actor::webui::mojom::TaskOptions::New();
      }
      options->duration = actor::webui::mojom::TaskDuration::kTransient;
    }
  } else if (options && options->duration ==
                            actor::webui::mojom::TaskDuration::kTransient) {
    options->duration = actor::webui::mojom::TaskDuration::kDefault;
  }

  current_task_id_ = actor_keyed_service().CreateTaskWithOptions(
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kGlic,
                            conversation_id),
      &actor_policy_checker(), std::move(options),
      manager_->delegate_->GetActorTaskDelegate());
  CHECK(!current_task_id_.is_null());

  manager_->SetActuating(true);

  actor_task_state_changed_subscription_ =
      actor_keyed_service().AddTaskStateChangedCallback(base::BindRepeating(
          &GlicActorClientSession::NotifyActorTaskStateChanged,
          base::Unretained(this)));

  std::move(callback).Run(current_task_id_.value());
}

void GlicActorClientSession::PerformActionsFinished(
    mojom::WebClientHandler::PerformActionsCallback callback,
    actor::TaskId task_id,
    base::TimeTicks start_time,
    bool skip_async_observation_information,
    std::optional<page_content_annotations::ScreenshotOptions::
                      ScreenshotCollectionOptions>
        screenshot_collection_options,
    std::vector<actor::ActionResultWithLatencyInfo> action_results) {
  actor::mojom::ActionResultCode result_code =
      actor::mojom::ActionResultCode::kOk;
  std::optional<size_t> index_of_failed_action;
  actor::ExtractErrorResult(action_results, &result_code,
                            index_of_failed_action);
  actor_keyed_service().GetJournal().Log(
      GURL::EmptyGURL(), task_id, "PerformActionsFinished",
      actor::JournalDetailsBuilder()
          .Add("result_code", base::ToString(result_code))
          .Build());

  actor::ActorTask* task = actor_keyed_service().GetTask(task_id);
  // TODO(b/470985724): Reply at the time the task is stopped/canceled instead
  // of here.
  if (!task) {
    optimization_guide::proto::ActionsResult response =
        actor::BuildErrorActionsResult(
            actor::mojom::ActionResultCode::kTaskWentAway, std::nullopt);
    std::move(callback).Run(mojo_base::ProtoWrapper(response));
    return;
  }

  if (result_code == actor::mojom::ActionResultCode::kTaskPaused ||
      result_code == actor::mojom::ActionResultCode::kTaskWentAway) {
    optimization_guide::proto::ActionsResult response =
        actor::BuildErrorActionsResult(result_code, std::nullopt);
    std::move(callback).Run(mojo_base::ProtoWrapper(response));
    return;
  }

  if (base::FeatureList::IsEnabled(actor::kGlicActorTabObservationController)) {
    actor::mojom::ActionResultCode controller_result_code =
        actor::mojom::ActionResultCode::kOk;
    std::optional<size_t> controller_index_of_failed_action;
    actor::ExtractErrorResult(action_results, &controller_result_code,
                              controller_index_of_failed_action);
    auto journal_entry =
        actor_keyed_service().GetJournal().CreatePendingAsyncEntry(
            GURL(), task_id, MakeBrowserTrackUUID(task_id),
            "TabObservationController",
            actor::JournalDetailsBuilder()
                .Add("result_code", base::ToString(controller_result_code))
                .Add("skip_async_observation_information",
                     skip_async_observation_information)
                .Build());

    // base::Unretained(this) is safe because `observation_controllers_` is
    // owned by this class and the controller guarantees that it will not run
    // the callback after its own destruction.
    auto done_callback =
        base::BindOnce(&GlicActorClientSession::OnPerformActionsComplete,
                       base::Unretained(this), std::move(callback), start_time,
                       action_results, std::move(journal_entry));

    auto controller = std::make_unique<actor::TabObservationController>(
        &profile(), task_id, start_time, skip_async_observation_information,
        action_results, std::move(done_callback));

    controller->set_screenshot_collection_options(
        std::move(screenshot_collection_options));
    auto* controller_ptr = controller.get();
    observation_controllers_.push_back(std::move(controller));
    controller_ptr->Start();
    return;
  }

  // TODO(b/471210832): Consider merging tab observation code into the Actor API
  // so that all clients can share logic related to retries, crashed, tabs, and
  // observation fetching mechanics.
  if (base::FeatureList::IsEnabled(kGlicReloadAfterPerformActionsCrash) &&
      !attempted_reload_after_crash_) {
    if (tabs::TabInterface* crashed_tab = GetCrashedTab(*task)) {
      attempted_reload_after_crash_ = true;

      // We call back into PerformActionsFinished once we've reloaded the tab
      // but ensure we respond with kRendererCrashed since the reload/crash is
      // state-destructive.
      auto retry_perform_actions_finished = base::BindOnce(
          &GlicActorClientSession::PerformActionsFinished,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), task_id,
          start_time, skip_async_observation_information,
          std::move(screenshot_collection_options), std::move(action_results));
      ReloadCrashedTab(*crashed_tab, task->id(),
                       std::move(retry_perform_actions_finished));
      return;
    }
  }

  actor::BuildActionsResultWithObservations(
      profile(), start_time, std::move(action_results), *task,
      skip_async_observation_information, screenshot_collection_options,
      base::BindOnce(&GlicActorClientSession::DidFinishBuildObservation,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void GlicActorClientSession::OnPerformActionsComplete(
    mojom::WebClientHandler::PerformActionsCallback callback,
    base::TimeTicks start_time,
    std::vector<actor::ActionResultWithLatencyInfo> action_results,
    std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry> journal_entry,
    actor::TabObservationController* controller_ptr,
    std::unique_ptr<actor::ObservationResult> result) {
  CHECK(
      base::FeatureList::IsEnabled(actor::kGlicActorTabObservationController));
  CHECK(result);
  std::erase_if(observation_controllers_, [&](const auto& controller) {
    return controller.get() == controller_ptr;
  });

  optimization_guide::proto::ActionsResult response;

  actor::mojom::ActionResultCode result_code =
      actor::mojom::ActionResultCode::kOk;
  std::optional<size_t> index_of_failed_action;
  actor::ExtractErrorResult(action_results, &result_code,
                            index_of_failed_action);

  response.set_action_result(static_cast<int32_t>(result_code));
  if (index_of_failed_action) {
    response.set_index_of_failed_action(*index_of_failed_action);
  }

  actor::CopyScriptToolResults(response, action_results);

  auto* latency_info = response.mutable_latency_information();
  for (size_t i = 0; i < action_results.size(); ++i) {
    auto& action_result = action_results.at(i);
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
    // Don't report a page stabilization time if the start and end
    // are the same. Not every tool needs stabilization.
    if (*action_result.result->execution_end_time != action_result.end_time) {
      auto* latency_step = latency_info->add_latency_steps();
      latency_step->mutable_page_stabilization()->set_action_index(i);
      latency_step->set_latency_start_ms(
          (*action_result.result->execution_end_time - start_time)
              .InMilliseconds());
      latency_step->set_latency_stop_ms(
          (action_result.end_time - start_time).InMilliseconds());
    }
    if (!actor::IsOk(*action_result.result)) {
      CHECK_EQ(*index_of_failed_action, i);
      response.set_error_message(action_result.result->message);
    }
  }

  for (auto& obs : result->tab_observations) {
    *response.add_tabs() = std::move(obs);
  }
  for (auto& obs : result->window_observations) {
    *response.add_windows() = std::move(obs);
  }
  for (auto& step : result->latency_steps) {
    *latency_info->add_latency_steps() = std::move(step);
  }

  actor::RecordTabObservationResultHistogram(response);
  actor::RecordObservationOutcomeHistogram(response,
                                           result->attempted_observation_retry);

  if (journal_entry) {
    journal_entry->EndEntry({});
  }

  std::move(callback).Run(mojo_base::ProtoWrapper(response));
}

void GlicActorClientSession::DidFinishBuildObservation(
    mojom::WebClientHandler::PerformActionsCallback callback,
    base::TimeTicks start_time,
    std::vector<actor::ActionResultWithLatencyInfo> action_results,
    actor::TaskId task_id,
    bool skip_async_observation_information,
    std::optional<page_content_annotations::ScreenshotOptions::
                      ScreenshotCollectionOptions>
        screenshot_collection_options,
    std::unique_ptr<optimization_guide::proto::ActionsResult> result,
    std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry>
        journal_entry) {
  CHECK(
      !base::FeatureList::IsEnabled(actor::kGlicActorTabObservationController));
  CHECK(result);
  actor::RecordTabObservationResultHistogram(*result);

  if (base::FeatureList::IsEnabled(kGlicRetryFailedObservations) &&
      !attempted_observation_retry_) {
    using optimization_guide::proto::TabObservation;

    // If any of the tab observations failed, retry observation.
    for (const TabObservation& tab_observation : result->tabs()) {
      CHECK(tab_observation.has_result());
      if (tab_observation.result() != TabObservation::TAB_OBSERVATION_OK) {
        attempted_observation_retry_ = true;

        actor_keyed_service().GetJournal().Log(
            GURL::EmptyGURL(), task_id, "Retrying failed observation",
            actor::JournalDetailsBuilder()
                .Add("tab_id", tab_observation.id())
                .AddError(base::ToString(tab_observation.result()))
                .Build());

        auto retry_perform_actions_finished = base::BindOnce(
            &GlicActorClientSession::PerformActionsFinished,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback), task_id,
            start_time, skip_async_observation_information,
            std::move(screenshot_collection_options),
            std::move(action_results));

        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE, std::move(retry_perform_actions_finished),
            kObservationRetryDelay.Get());
        return;
      }
    }
  }

  actor::RecordObservationOutcomeHistogram(*result,
                                           attempted_observation_retry_);

  std::move(callback).Run(mojo_base::ProtoWrapper(*result));
}

void GlicActorClientSession::ReloadCrashedTab(tabs::TabInterface& crashed_tab,
                                              actor::TaskId task_id,
                                              base::OnceClosure callback) {
  // TODO(b/464019189): This code only deals with the first crashed tab per
  // Task. If they are multiple tabs that crashed we might want to figure out
  // how to deal with that.
  content::WebContents* contents = crashed_tab.GetContents();
  if (!contents) {
    std::move(callback).Run();
    return;
  }
  CHECK(contents->IsCrashed());

  actor_keyed_service().GetJournal().Log(
      contents->GetLastCommittedURL(), task_id,
      "GlicActorTaskManager::ReloadCrashedTab", /*details=*/{});
  reload_observer_ = std::make_unique<actor::ObservationDelayController>(
      task_id, actor_keyed_service().GetJournal());
  // TODO(b/471205189): Should `check_for_repost` be true here since a user
  // isn't in control?
  contents->GetController().Reload(content::ReloadType::NORMAL, true);
  reload_observer_->Wait(
      crashed_tab,
      base::BindOnce(&GlicActorClientSession::ReloadObserverDone,
                     base::Unretained(this), crashed_tab.GetHandle(),
                     std::move(callback)));
}

void GlicActorClientSession::PerformActions(
    const std::vector<uint8_t>& actions_proto,
    mojom::WebClientHandler::PerformActionsCallback callback) {
  instance_metrics().OnPerformActions();
  base::TimeTicks start_time = base::TimeTicks::Now();
  // TODO(bokan): Refactor the actor code in this class into an actor-specific
  // wrapper for proto-to-actor conversion.
  optimization_guide::proto::Actions actions;
  if (!actions.ParseFromArray(actions_proto.data(), actions_proto.size())) {
    // TODO(bokan): include the base64 proto in the error
    actor_keyed_service().GetJournal().Log(
        GURL(), actor::TaskId(), "GlicPerformActions",
        actor::JournalDetailsBuilder().AddError("Invalid Proto").Build());
    std::move(callback).Run(
        base::unexpected(mojom::PerformActionsErrorReason::kInvalidProto));
    return;
  }

  actor_keyed_service().GetJournal().Log(
      GURL(), actor::TaskId(actions.task_id()), "GlicPerformActions",
      actor::JournalDetailsBuilder()
          .Add("proto", actor::ToBase64(actions))
          .Build());

  if (!actions.has_task_id()) {
    actor_keyed_service().GetJournal().Log(
        GURL(), actor::TaskId(actions.task_id()), "GlicPerformActions",
        actor::JournalDetailsBuilder().AddError("Missing Task Id").Build());
    std::move(callback).Run(
        base::unexpected(mojom::PerformActionsErrorReason::kMissingTaskId));
    return;
  }

  actor::TaskId task_id(actions.task_id());
  if (!actor_keyed_service().GetTask(task_id)) {
    actor_keyed_service().GetJournal().Log(GURL::EmptyGURL(), task_id,
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
    actor_keyed_service().GetJournal().Log(
        GURL::EmptyGURL(), task_id, "Act Failed",
        actor::JournalDetailsBuilder()
            .AddError("Failed to convert proto::Actions to ToolRequest")
            .Add("failed_action_index", requests.error().first)
            .Add("error_code", static_cast<int>(requests.error().second))
            .Build());
    optimization_guide::proto::ActionsResult response =
        actor::BuildErrorActionsResult(requests.error().second,
                                       requests.error().first);
    std::move(callback).Run(mojo_base::ProtoWrapper(response));
    return;
  }
  bool skip_async_observation_information =
      actions.has_skip_async_observation_collection() &&
      actions.skip_async_observation_collection();

  attempted_observation_retry_ = false;
  actor_keyed_service().PerformActions(
      task_id, std::move(requests.value()), actor::ActorTaskMetadata(actions),
      base::BindOnce(&GlicActorClientSession::PerformActionsFinished,
                     GetWeakPtr(), std::move(callback), task_id, start_time,
                     skip_async_observation_information,
                     actor::GetScreenshotCollectionOptions(actions)));
}

void GlicActorClientSession::CancelActions(
    actor::TaskId task_id,
    mojom::WebClientHandler::CancelActionsCallback callback) {
  actor::ActorTask* task = actor_keyed_service().GetTask(task_id);
  if (!task) {
    std::move(callback).Run(mojom::CancelActionsResult::kTaskNotFound);
    return;
  }

  bool success = task->CancelOngoingActions(
      actor::mojom::ActionResultCode::kActionsCancelled);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                success ? mojom::CancelActionsResult::kSuccess
                                        : mojom::CancelActionsResult::kFailed));
}

void GlicActorClientSession::StopActorTask(
    actor::TaskId task_id,
    mojom::ActorTaskStopReason stop_reason) {
  instance_metrics().OnStopActorTask();
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

  StopTaskImpl(task_id, reason);
}

void GlicActorTaskManager::MaybeShowDeactivationToastUi() {
  // If the ui is deactivated on a tab that is not actuating, don't show the
  // toast.
  if (!IsActuating()) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  BrowserWindowInterface* const last_active_bwi =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  actor_keyed_service_->GetActorUiStateManager()->MaybeShowToast(
      last_active_bwi);
#endif
}

void GlicActorClientSession::PauseActorTask(
    actor::TaskId task_id,
    mojom::ActorTaskPauseReason pause_reason,
    tabs::TabInterface::Handle tab_handle) {
  instance_metrics().OnPauseActorTask();
  actor::ActorTask* task = actor_keyed_service().GetTask(task_id);
  if (!task || task->IsCompleted() || task->IsUnderUserControl()) {
    actor_keyed_service().GetJournal().Log(
        GURL::EmptyGURL(), task_id, "Failed to pause task",
        actor::JournalDetailsBuilder()
            .AddError(task ? "Task is not running" : "No such task")
            .Add("id", task_id.value())
            .Build());
    return;
  }

  if (tab_handle != tabs::TabHandle::Null()) {
    // Pausing the task on a tab means we're actuating on it.
    task->AddTab(tab_handle, /*stop_task_on_detach=*/true, base::DoNothing());
  }

  const bool from_actor =
      pause_reason == mojom::ActorTaskPauseReason::kPausedByModel;

  task->Pause(from_actor);
}

void GlicActorClientSession::ResumeActorTask(
    actor::TaskId task_id,
    const mojom::GetTabContextOptions& context_options,
    glic::mojom::WebClientHandler::ResumeActorTaskCallback callback) {
  instance_metrics().OnResumeActorTask();
  actor::ActorTask* task = actor_keyed_service().GetTask(task_id);
  if (!task || !task->IsUnderUserControl()) {
    std::string error_message = task ? "Task is not paused" : "No such task";
    actor_keyed_service().GetJournal().Log(GURL::EmptyGURL(), task_id,
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
  actor::ExecutionEngine& execution_engine = task->GetExecutionEngine();
  resume_response_code = execution_engine.user_take_over_result().value_or(
      actor::mojom::ActionResultCode::kOk);
  // Reset the takeover result
  execution_engine.set_user_take_over_result(std::nullopt);

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
    actor_keyed_service().GetJournal().Log(GURL::EmptyGURL(), task_id,
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
        std::optional<std::string> error_message =
            actor::ActorKeyedService::ExtractErrorMessageIfFailed(result);
        if (error_message) {
          std::move(reply_callback)
              .Run(mojom::GetContextResultWithActionResultCode::New(
                  mojom::GetContextResult::NewErrorReason(*error_message),
                  std::nullopt));
          return;
        }

        page_content_annotations::FetchPageContextResult& page_context =
            *result.value();

        // An empty result from ExtractErrorMessageIfFailed guarantees a
        // successful request has both screenshot and APC.
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
      std::move(callback), CreateTabData(tab_of_resumed_task),
      resume_response_code);

  actor_keyed_service().RequestTabObservation(
      *tab_of_resumed_task, task_id,
      context_options.screenshot_collection_options,
      std::move(observation_callback));
}

bool GlicActorClientSession::IsActuating() const {
  return !!current_task_id_;
}

bool GlicActorTaskManager::IsActuating() const {
  return session_ && session_->IsActuating();
}

base::CallbackListSubscription
GlicActorTaskManager::AddActuatingChangedCallback(
    base::RepeatingCallback<void(bool)> callback) {
  return actuating_changed_callbacks_.Add(std::move(callback));
}

void GlicActorClientSession::InterruptActorTask(actor::TaskId task_id) {
  instance_metrics().InterruptActorTask();
  actor::ActorTask* task = actor_keyed_service().GetTask(task_id);
  if (!task) {
    actor_keyed_service().GetJournal().Log(GURL::EmptyGURL(), task_id,
                                           "Failed to interrupt task",
                                           actor::JournalDetailsBuilder()
                                               .AddError("No such task")
                                               .Add("id", task_id.value())
                                               .Build());
    return;
  }
  task->Interrupt();
}

void GlicActorClientSession::UninterruptActorTask(actor::TaskId task_id) {
  instance_metrics().UninterruptActorTask();
  actor::ActorTask* task = actor_keyed_service().GetTask(task_id);
  if (!task) {
    actor_keyed_service().GetJournal().Log(GURL::EmptyGURL(), task_id,
                                           "Failed to uninterrupt task",
                                           actor::JournalDetailsBuilder()
                                               .AddError("No such task")
                                               .Add("id", task_id.value())
                                               .Build());
    return;
  }
  actor::ActorTask::State next_state = actor::ActorTask::State::kReflecting;
  if (task->GetExecutionEngine().HasActionSequence()) {
    next_state = actor::ActorTask::State::kActing;
  }
  task->Uninterrupt(next_state);
}

void GlicActorClientSession::CreateActorTab(
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

  actor_keyed_service().CreateActorTab(
      task_id, open_in_background, initiator_tab_handle,
      initiator_window_session_id,
      base::BindOnce(&GlicActorClientSession::CreateActorTabFinished,
                     GetWeakPtr(), std::move(callback)));
}

void GlicActorClientSession::CreateActorTabFinished(
    glic::mojom::WebClientHandler::CreateActorTabCallback callback,
    tabs::TabInterface* new_tab) {
  std::move(callback).Run(CreateTabData(new_tab));
}

void GlicActorClientSession::ReloadObserverDone(
    tabs::TabHandle tab_handle,
    base::OnceClosure callback,
    actor::ObservationDelayController::Result result) {
  if (current_task_id_ &&
      result == actor::ObservationDelayController::Result::kPageNavigated) {
    tabs::TabInterface* tab = tab_handle.Get();
    if (tab) {
      size_t last_navigation_count = reload_observer_->NavigationCount();
      reload_observer_ = std::make_unique<actor::ObservationDelayController>(
          current_task_id_, actor_keyed_service().GetJournal());
      reload_observer_->SetNavigationCount(last_navigation_count + 1);
      reload_observer_->Wait(
          *tab, base::BindOnce(&GlicActorClientSession::ReloadObserverDone,
                               base::Unretained(this), tab_handle,
                               std::move(callback)));
      return;
    }
  }
  reload_observer_.reset();
  std::move(callback).Run();
}

void GlicActorClientSession::CancelTask() {
  if (current_task_id_) {
    StopTaskImpl(current_task_id_,
                 actor::ActorTask::StoppedReason::kStoppedByUser);
  }
}

void GlicActorTaskManager::CancelTask() {
  if (!session_) {
    return;
  }
  session_->CancelTask();
}

void GlicActorClientSession::CanActOnWebChanged(bool can_act_on_web) {
  if (!can_act_on_web && current_task_id_) {
    StopTaskImpl(current_task_id_,
                 actor::ActorTask::StoppedReason::kChromeFailure);
  }
}

void GlicActorClientSession::NotifyActorTaskStateChanged(
    actor::ActorTask& task) {
  CHECK(!task.id().is_null());
  if (current_task_id_ != task.id()) {
    return;
  }

  if (task.IsCompleted()) {
    current_task_id_ = actor::TaskId();
    attempted_reload_after_crash_ = false;
    reload_observer_.reset();
    actor_task_state_changed_subscription_.reset();
    manager_->SetActuating(false);
  }
}

void GlicActorClientSession::StopTaskImpl(
    actor::TaskId task_id,
    actor::ActorTask::StoppedReason reason) {
  actor::ActorTask* task = actor_keyed_service().GetTask(task_id);
  if (!task || task->IsCompleted()) {
    actor_keyed_service().GetJournal().Log(
        GURL::EmptyGURL(), task_id, "Failed to stop task",
        actor::JournalDetailsBuilder()
            .AddError(task ? "Task already stopped" : "No such task")
            .Add("id", task_id.value())
            .Build());
    return;
  }

  actor_keyed_service().StopTask(task->id(), reason);
}

actor::ActorKeyedService& GlicActorClientSession::actor_keyed_service() const {
  return *manager_->actor_keyed_service_;
}

GlicActorPolicyChecker& GlicActorClientSession::actor_policy_checker() const {
  return *manager_->actor_policy_checker_;
}

GlicInstanceMetrics& GlicActorClientSession::instance_metrics() const {
  return *manager_->instance_metrics_;
}

Profile& GlicActorClientSession::profile() const {
  return *manager_->profile_;
}

base::WeakPtr<GlicActorTaskManager> GlicActorTaskManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

GlicActorClientSession* GlicActorTaskManager::GetClientSessionForTesting() {
  return session_.get();
}

base::WeakPtr<GlicActorClientSession> GlicActorClientSession::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

GlicActorClientSession* GlicActorTaskManager::BindSession() {
  session_ = std::make_unique<GlicActorClientSession>(this);
  return session_.get();
}

void GlicActorTaskManager::UnbindSession() {
  session_.reset();
  SetActuating(false);
}

void GlicActorTaskManager::SetActuating(bool actuating) {
  if (actuating_ == actuating) {
    return;
  }
  actuating_ = actuating;
  actuating_changed_callbacks_.Notify(actuating_);
}

}  // namespace glic
