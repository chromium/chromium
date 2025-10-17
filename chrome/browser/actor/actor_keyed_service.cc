// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_keyed_service.h"

#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/types/pass_key.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_policy_checker.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/task_id.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace {
void RunLater(base::OnceClosure task) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                              std::move(task));
}

}  // namespace

namespace actor {

std::optional<page_content_annotations::PaintPreviewOptions>
CreateOptionalPaintPreviewOptions() {
  if (!base::FeatureList::IsEnabled(kGlicTabScreenshotPaintPreviewBackend)) {
    return std::nullopt;
  }
  page_content_annotations::PaintPreviewOptions paint_preview_options;
  paint_preview_options.max_per_capture_bytes =
      kScreenshotMaxPerCaptureBytes.Get();
  paint_preview_options.iframe_redaction_scope =
      kScreenshotIframeRedaction.Get();
  return paint_preview_options;
}

using ui::ActorUiStateManagerInterface;

ActorKeyedService::ActorKeyedService(Profile* profile) : profile_(profile) {
  actor_ui_state_manager_ = std::make_unique<ui::ActorUiStateManager>(*this);
  policy_checker_ = std::make_unique<ActorPolicyChecker>(*this);
}

ActorKeyedService::~ActorKeyedService() = default;

// static
ActorKeyedService* ActorKeyedService::Get(content::BrowserContext* context) {
  return ActorKeyedServiceFactory::GetActorKeyedService(context);
}

void ActorKeyedService::SetActorUiStateManagerForTesting(
    std::unique_ptr<ui::ActorUiStateManagerInterface> ausm) {
  actor_ui_state_manager_ = std::move(ausm);
}

const ActorTask* ActorKeyedService::GetActingActorTaskForWebContents(
    content::WebContents* web_contents) {
  if (auto* tab_interface =
          tabs::TabModel::MaybeGetFromContents(web_contents)) {
    // There should only be one active task per tab.
    for (const auto& [task_id, actor_task] : GetActiveTasks()) {
      if (actor_task->IsActingOnTab(tab_interface->GetHandle())) {
        return actor_task;
      }
    }
  }

  return nullptr;
}

base::WeakPtr<ActorKeyedService> ActorKeyedService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

TaskId ActorKeyedService::AddActiveTask(std::unique_ptr<ActorTask> task) {
  TRACE_EVENT0("actor", "ActorKeyedService::AddActiveTask");
  TaskId task_id = next_task_id_.GenerateNextId();
  task->SetId(base::PassKey<ActorKeyedService>(), task_id);
  task->GetExecutionEngine()->SetOwner(task.get());
  // Notify of task creation now that the task id is set.
  NotifyTaskStateChanged(*task);
  active_tasks_[task_id] = std::move(task);
  return task_id;
}

const std::map<TaskId, const ActorTask*> ActorKeyedService::GetActiveTasks()
    const {
  std::map<TaskId, const ActorTask*> active_tasks;
  for (const auto& [id, task] : active_tasks_) {
    CHECK_NE(task->IsStopped(), true);
    active_tasks[id] = task.get();
  }
  return active_tasks;
}

const std::map<TaskId, const ActorTask*> ActorKeyedService::GetInactiveTasks()
    const {
  std::map<TaskId, const ActorTask*> inactive_tasks;
  for (const auto& [id, task] : inactive_tasks_) {
    inactive_tasks[id] = task.get();
  }
  return inactive_tasks;
}

void ActorKeyedService::ResetForTesting() {
  for (auto it = active_tasks_.begin(); it != active_tasks_.end();) {
    StopTask((it++)->first, /*success=*/true);
  }
  active_tasks_.clear();
  inactive_tasks_.clear();
}

TaskId ActorKeyedService::CreateTask() {
  return CreateTaskWithOptions(nullptr, nullptr);
}

TaskId ActorKeyedService::CreateTaskWithOptions(
    webui::mojom::TaskOptionsPtr options,
    base::WeakPtr<ActorTaskDelegate> delegate) {
  TRACE_EVENT0("actor", "ActorKeyedService::CreateTask");
  base::UmaHistogramBoolean("Actor.Task.Created", true);
  auto execution_engine = std::make_unique<ExecutionEngine>(profile_.get());
  auto actor_task = std::make_unique<ActorTask>(
      profile_.get(), std::move(execution_engine),
      ui::NewUiEventDispatcher(GetActorUiStateManager()), std::move(options),
      std::move(delegate));
  return AddActiveTask(std::move(actor_task));
}

base::CallbackListSubscription ActorKeyedService::AddTaskStateChangedCallback(
    TaskStateChangedCallback callback) {
  return tab_state_change_callback_list_.Add(std::move(callback));
}

void ActorKeyedService::NotifyTaskStateChanged(const ActorTask& task) {
  tab_state_change_callback_list_.Notify(task);
}

base::CallbackListSubscription
ActorKeyedService::AddRequestToShowCredentialSelectionDialogSubscriberCallback(
    RequestToShowCredentialSelectionDialogSubscriberCallback callback) {
  return request_to_show_credential_selection_dialog_callback_list_.Add(
      std::move(callback));
}

void ActorKeyedService::NotifyRequestToShowCredentialSelectionDialog(
    TaskId task_id,
    const base::flat_map<std::string, gfx::Image>& icons,
    const std::vector<actor_login::Credential>& credentials) {
  request_to_show_credential_selection_dialog_callback_list_.Notify(
      task_id, icons, credentials,
      base::BindRepeating(&ActorKeyedService::OnCredentialSelected,
                          weak_ptr_factory_.GetWeakPtr(), task_id));
}

void ActorKeyedService::OnCredentialSelected(
    TaskId request_task_id,
    webui::mojom::SelectCredentialDialogResponsePtr response) {
  TRACE_EVENT0("actor", "ActorKeyedService::OnCredentialSelected");
  // TODO(crbug.com/440147814): Update the `UserGrantedPermissionDuration`
  // if the user changes the permission.
  TaskId response_task_id(response->task_id);
  if (response_task_id != request_task_id) {
    // TODO(crbug.com/441500534): We should also add error handling in
    // glic_api_host.ts.
    VLOG(1) << "SelectCredentialDialogResponse has a different task id "
            << response_task_id << " than requested " << request_task_id;
    // If the task ID mismatches, generate an empty response with the correct
    // task ID and error value.
    response->task_id = request_task_id.value();
    response->selected_credential_id = std::nullopt;
    // TODO(crbug.com/427817882): Explicit error reason (kMismatchedTaskId).
    response->error_reason = std::nullopt;
  }
  if (auto* task = GetTask(request_task_id)) {
    task->GetExecutionEngine()->OnCredentialSelected(std::move(response));
  } else {
    VLOG(1) << "Task not found for task id: " << request_task_id;
  }
}

base::CallbackListSubscription
ActorKeyedService::AddRequestToShowUserConfirmationDialogSubscriberCallback(
    RequestToShowUserConfirmationDialogSubscriberCallback callback) {
  return request_to_show_user_confirmation_dialog_callback_list_.Add(
      std::move(callback));
}

void ActorKeyedService::NotifyRequestToShowUserConfirmationDialog(
    TaskId task_id,
    const std::optional<url::Origin>& navigation_origin,
    const std::optional<int32_t> download_id) {
  request_to_show_user_confirmation_dialog_callback_list_.Notify(
      navigation_origin, download_id,
      base::BindRepeating(&ActorKeyedService::OnUserConfirmationDialogDecision,
                          weak_ptr_factory_.GetWeakPtr(), task_id));
}

void ActorKeyedService::OnUserConfirmationDialogDecision(
    TaskId request_task_id,
    webui::mojom::UserConfirmationDialogResponsePtr response) {
  if (auto* task = GetTask(request_task_id)) {
    task->GetExecutionEngine()->OnUserConfirmation(std::move(response));
  } else {
    VLOG(1) << "Task not found for task id: " << request_task_id;
  }
}

void ActorKeyedService::OnActuationCapabilityChanged(
    bool has_actuation_capability) {
  if (!has_actuation_capability) {
    FailAllTasks();
  }
  // TODO(crbug.com/450525715): Depends on the shape of the Chrome API to signal
  // the HostCapability (Set vs Observable), we might need to inform the web
  // client about the capability change.
}

void ActorKeyedService::RequestTabObservation(
    tabs::TabInterface& tab,
    TaskId task_id,
    base::OnceCallback<void(TabObservationResult)> callback) {
  TRACE_EVENT0("actor", "ActorKeyedService::RequestTabObservation");
  const GURL& last_committed_url = tab.GetContents()->GetLastCommittedURL();
  auto journal_entry = journal_.CreatePendingAsyncEntry(
      last_committed_url, task_id, mojom::JournalTrack::kActor,
      "RequestTabObservation", {});
  page_content_annotations::FetchPageContextOptions options;

  options.screenshot_options =
      kFullPageScreenshot.Get()
          // It's safe to dereference the optional here because
          // kFullPageScreenshot being true implies
          // kGlicTabScreenshotPaintPreviewBackend is enabled.
          ? page_content_annotations::ScreenshotOptions::FullPage(
                CreateOptionalPaintPreviewOptions().value())
          : page_content_annotations::ScreenshotOptions::ViewportOnly(
                CreateOptionalPaintPreviewOptions());

  options.annotated_page_content_options =
      optimization_guide::ActionableAIPageContentOptions(
          /* on_critical_path =*/true);
  // The maximum number of meta tags to extract from the page. This is a fairly
  // generous limit that should be sufficient for the metadata we expect to see.
  // 32 is the value specified in the TabObservation proto comment.
  options.annotated_page_content_options->max_meta_elements = 32;
  page_content_annotations::FetchPageContext(
      *tab.GetContents(), options,
      CreateActorJournalFetchPageProgressListener(journal_.GetSafeRef(),
                                                  last_committed_url, task_id),
      base::BindOnce(
          [](base::WeakPtr<tabs::TabInterface> tab,
             base::OnceCallback<void(TabObservationResult)> callback,
             std::unique_ptr<AggregatedJournal::PendingAsyncEntry>
                 pending_journal_entry,
             const GURL& last_committed_url,
             page_content_annotations::FetchPageContextResultCallbackArg
                 result) {
            if (!result.has_value()) {
              std::move(callback).Run(base::unexpected(absl::StrFormat(
                  "Failed Observation: code[%s] message[%s]",
                  page_content_annotations::ToString(result.error().error_code),
                  result.error().message)));
              return;
            }

            // Context for actor observations should always have an APC and a
            // screenshot, return failure if either is missing.
            auto& fetch_result = **result;
            bool has_apc =
                fetch_result.annotated_page_content_result.has_value();
            bool has_screenshot = fetch_result.screenshot_result.has_value();
            if (!has_apc || !has_screenshot) {
              std::move(callback).Run(base::unexpected(absl::StrFormat(
                  "Failed Observation: APC[%g] screenshot[%s]", has_apc,
                  fetch_result.screenshot_result.has_value()
                      ? std::string("OK")
                      : fetch_result.screenshot_result.error())));
              return;
            }

            size_t size = fetch_result.annotated_page_content_result->proto
                              .ByteSizeLong();
            std::vector<uint8_t> buffer(size);
            fetch_result.annotated_page_content_result->proto.SerializeToArray(
                buffer.data(), size);
            pending_journal_entry->GetJournal().LogAnnotatedPageContent(
                last_committed_url, pending_journal_entry->GetTaskId(), buffer);

            auto& data = fetch_result.screenshot_result->screenshot_data;
            pending_journal_entry->GetJournal().LogScreenshot(
                last_committed_url, pending_journal_entry->GetTaskId(),
                fetch_result.screenshot_result->mime_type, base::as_byte_span(data));
            if (tab) {
              actor::ActorTabData::From(tab.get())->DidObserveContent(
                  fetch_result.annotated_page_content_result->proto);
            }

            std::move(callback).Run(std::move(result).value());
          },
          tab.GetWeakPtr(), std::move(callback), std::move(journal_entry),
          last_committed_url));
}

void ActorKeyedService::PerformActions(
    TaskId task_id,
    std::vector<std::unique_ptr<ToolRequest>>&& actions,
    ActorTaskMetadata task_metadata,
    PerformActionsCallback callback) {
  TRACE_EVENT0("actor", "ActorKeyedService::PerformActions");
  std::vector<ActionResultWithLatencyInfo> empty_results;
  auto* task = GetTask(task_id);
  if (!task) {
    VLOG(1) << "PerformActions failed: Task not found.";
    RunLater(base::BindOnce(std::move(callback),
                            mojom::ActionResultCode::kTaskWentAway,
                            std::nullopt, std::move(empty_results)));
    return;
  }

  if (actions.empty()) {
    VLOG(1) << "PerformActions failed: No actions provided.";
    RunLater(base::BindOnce(std::move(callback),
                            mojom::ActionResultCode::kEmptyActionSequence,
                            std::nullopt, std::move(empty_results)));
    return;
  }

  task->GetExecutionEngine()->AddWritableMainframeOrigins(
      task_metadata.added_writable_mainframe_origins());
  task->Act(
      std::move(actions),
      base::BindOnce(&ActorKeyedService::OnActionsFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ActorKeyedService::OnActionsFinished(
    PerformActionsCallback callback,
    mojom::ActionResultPtr result,
    std::optional<size_t> index_of_failed_action,
    std::vector<ActionResultWithLatencyInfo> action_results) {
  TRACE_EVENT0("actor", "ActorKeyedService::OnActionsFinished");
  // If the result if Ok then we must not have a failed action.
  CHECK(!IsOk(*result) || !index_of_failed_action);
  RunLater(base::BindOnce(std::move(callback), result->code,
                          index_of_failed_action, std::move(action_results)));
}

void ActorKeyedService::FailAllTasks() {
  std::vector<TaskId> tasks_to_stop =
      FindTaskIdsInActive([](const ActorTask& task) { return true; });
  for (const auto& task_id : tasks_to_stop) {
    StopTask(task_id, /*success=*/false);
  }
}

void ActorKeyedService::StopTask(TaskId task_id, bool success) {
  TRACE_EVENT0("actor", "ActorKeyedService::StopTask");
  auto task = active_tasks_.extract(task_id);
  if (!task.empty()) {
    auto ret = inactive_tasks_.insert(std::move(task));
    ret.position->second->Stop(success);
  }
}

ActorTask* ActorKeyedService::GetTask(TaskId task_id) {
  auto task = active_tasks_.find(task_id);
  if (task != active_tasks_.end()) {
    return task->second.get();
  }
  task = inactive_tasks_.find(task_id);
  if (task != inactive_tasks_.end()) {
    return task->second.get();
  }
  return nullptr;
}

ActorUiStateManagerInterface* ActorKeyedService::GetActorUiStateManager() {
  return actor_ui_state_manager_.get();
}

ActorPolicyChecker& ActorKeyedService::GetPolicyChecker() {
  return *policy_checker_;
}

bool ActorKeyedService::IsActiveOnTab(const tabs::TabInterface& tab) const {
  tabs::TabHandle handle = tab.GetHandle();
  for (auto [task_id, task] : GetActiveTasks()) {
    if (task->IsActingOnTab(handle)) {
      return true;
    }
  }

  return false;
}

TaskId ActorKeyedService::GetTaskFromTab(const tabs::TabInterface& tab) const {
  tabs::TabHandle handle = tab.GetHandle();
  for (auto [task_id, task] : GetActiveTasks()) {
    if (task->HasTab(handle)) {
      return task_id;
    }
  }

  return TaskId();
}

Profile* ActorKeyedService::GetProfile() {
  return profile_;
}

std::vector<TaskId> ActorKeyedService::FindTaskIdsInActive(
    base::FunctionRef<bool(const ActorTask&)> predicate) const {
  std::vector<TaskId> result;
  for (const auto& [id, task] : active_tasks_) {
    if (predicate(*task)) {
      result.push_back(id);
    }
  }
  return result;
}

std::vector<TaskId> ActorKeyedService::FindTaskIdsInInactive(
    base::FunctionRef<bool(const ActorTask&)> predicate) const {
  std::vector<TaskId> result;
  for (const auto& [id, task] : inactive_tasks_) {
    if (predicate(*task)) {
      result.push_back(id);
    }
  }
  return result;
}

}  // namespace actor
