// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_keyed_service.h"

#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
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
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "ui/base/window_open_disposition.h"

namespace {
void RunLater(base::OnceClosure task) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                              std::move(task));
}

void OnCreateActorTabComplete(
    actor::ActorTask& task,
    actor::ActorKeyedService::CreateActorTabCallback callback,
    actor::AggregatedJournal& journal,
    tabs::TabInterface* tab) {
  if (base::FeatureList::IsEnabled(actor::kActorBindCreatedTabToTask) && tab) {
    task.AddTab(
        tab->GetHandle(),
        base::BindOnce(
            [](actor::ActorKeyedService::CreateActorTabCallback callback,
               tabs::TabHandle handle, actor::TaskId task_id,
               base::WeakPtr<actor::AggregatedJournal> journal,
               actor::mojom::ActionResultPtr result) {
              if (journal) {
                journal->Log(
                    GURL(), task_id, "OnCreateActorTabComplete",
                    actor::JournalDetailsBuilder()
                        .Add("AddTab result", actor::ToDebugString(*result))
                        .Build());
              }
              std::move(callback).Run(handle.Get());
            },
            std::move(callback), tab->GetHandle(), task.id(),
            journal.GetWeakPtr()));
  } else {
    std::move(callback).Run(tab);
  }
}

}  // namespace

namespace actor {

namespace {
BASE_FEATURE(kGlicActorFixPageObservationCrash,
             base::FEATURE_ENABLED_BY_DEFAULT);
}

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
  profile_observation_.Observe(profile_);
}

void ActorKeyedService::OnProfileInitializationComplete(Profile* profile) {
  // `download_notifier_` is set up after profile initialization because
  // `GetDownloadManager()` relies on other download services to be fully
  // initialized which may not be true until the profile is initialized.
  download_notifier_ = std::make_unique<download::AllDownloadItemNotifier>(
      profile_->GetDownloadManager(), this);
}

ActorKeyedService::~ActorKeyedService() = default;

void ActorKeyedService::Shutdown() {
  // Ensure all tasks are stopped here so we don't cause them to stop in the
  // dtor.
  StopAllTasks(ActorTask::StoppedReason::kShutdown);
}

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

void ActorKeyedService::CreateActorTab(TaskId task_id,
                                       bool open_in_background,
                                       tabs::TabHandle initiator_tab_handle,
                                       SessionID initiator_window_id,
                                       CreateActorTabCallback callback) {
  GetJournal().Log(
      GURL(), task_id, "CreateActorTab",
      JournalDetailsBuilder()
          .Add("task_id", task_id)
          .Add("open_in_background", open_in_background)
          .Add("initiator_tab_id", initiator_tab_handle.raw_value())
          .Add("initiator_window_id", initiator_window_id.id())
          .Build());
  ActorTask* task = GetTask(task_id);
  if (!task) {
    GetJournal().Log(GURL(), task_id, "CreateActorTab",
                     JournalDetailsBuilder().AddError("Invalid Task").Build());
  }

  BrowserWindowInterface* window_for_new_tab = nullptr;
  tabs::TabInterface* initiator_tab = initiator_tab_handle.Get();

  // Special case: if the initiator tab is the NTP, no need to create a new
  // tab, reuse it.
  if (initiator_tab && search::IsNTPURL(initiator_tab->GetContents()
                                            ->GetPrimaryMainFrame()
                                            ->GetLastCommittedURL())) {
    GetJournal().Log(
        GURL(), task_id, "CreateActorTab",
        JournalDetailsBuilder().Add("Return", "Initiator is NTP").Build());

    if (!open_in_background) {
      TabStripModel* tab_strip_model =
          initiator_tab->GetBrowserWindowInterface()->GetTabStripModel();
      tab_strip_model->ActivateTabAt(
          tab_strip_model->GetIndexOfTab(initiator_tab));
    }

    OnCreateActorTabComplete(*task, std::move(callback), journal_,
                             initiator_tab);
    return;
  }

  // If the initiating tab is still live, create the new tab in the same window.
  if (initiator_tab) {
    if (initiator_tab->IsInNormalWindow()) {
      window_for_new_tab = initiator_tab->GetBrowserWindowInterface();
      GetJournal().Log(GURL(), task_id, "CreateActorTab",
                       JournalDetailsBuilder()
                           .Add("Using initiator_tab's window",
                                window_for_new_tab->GetSessionID().id())
                           .Build());
    }
  } else {
    // If the tab was closed, open it in the window it was in (at the time of
    // task initiation).
    window_for_new_tab =
        BrowserWindowInterface::FromSessionID(initiator_window_id);
    GetJournal().Log(
        GURL(), task_id, "CreateActorTab",
        JournalDetailsBuilder()
            .Add("Using initiator_window", initiator_window_id.id())
            .Build());
  }

  NavigateParams params(profile_.get(), GURL(url::kAboutBlankURL),
                        ::ui::PAGE_TRANSITION_AUTO_TOPLEVEL);

  if (window_for_new_tab) {
    params.disposition = open_in_background
                             ? WindowOpenDisposition::NEW_BACKGROUND_TAB
                             : WindowOpenDisposition::NEW_FOREGROUND_TAB;
    params.browser = window_for_new_tab;
    params.window_action = NavigateParams::WindowAction::kNoAction;

    if (initiator_tab) {
      int initiator_index =
          window_for_new_tab->GetTabStripModel()->GetIndexOfTab(initiator_tab);
      if (initiator_index != TabStripModel::kNoTab) {
        params.tabstrip_index = initiator_index + 1;
      }
    }
  } else {
    GetJournal().Log(
        GURL(), task_id, "CreateActorTab",
        JournalDetailsBuilder().Add("Creating New Window", "").Build());
    // If window_for_new_tab is still null (e.g. the initiating window was
    // closed) the tab will be created in a new window.
    // TODO(b/454046200): Reconsider what should happen in this case.
    params.disposition = WindowOpenDisposition::NEW_WINDOW;
    params.window_action = NavigateParams::WindowAction::kShowWindow;
  }

  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  if (!handle) {
    GetJournal().Log(
        GURL(), task_id, "CreateActorTab",
        JournalDetailsBuilder().AddError("Failed creating navigation").Build());
    OnCreateActorTabComplete(*task, std::move(callback), journal_, nullptr);
    return;
  }

  content::WebContents* contents = handle->GetWebContents();
  if (!contents) {
    GetJournal().Log(GURL(), task_id, "CreateActorTab",
                     JournalDetailsBuilder()
                         .AddError("Navigation missing WebContents")
                         .Build());
    OnCreateActorTabComplete(*task, std::move(callback), journal_, nullptr);
    return;
  }

  // It might be good to wait for this navigation to finish but given we're
  // navigating to about:blank it probably doesn't matter in practice.
  OnCreateActorTabComplete(*task, std::move(callback), journal_,
                           tabs::TabInterface::GetFromContents(contents));
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
  NotifyTaskStateChanged(task->id(), task->GetState());
  active_tasks_[task_id] = std::move(task);
  return task_id;
}

const std::map<TaskId, const ActorTask*> ActorKeyedService::GetActiveTasks()
    const {
  std::map<TaskId, const ActorTask*> active_tasks;
  for (const auto& [id, task] : active_tasks_) {
    CHECK_NE(task->IsCompleted(), true);
    active_tasks[id] = task.get();
  }
  return active_tasks;
}

void ActorKeyedService::ResetForTesting() {
  for (auto it = active_tasks_.begin(); it != active_tasks_.end();) {
    StopTask((it++)->first, ActorTask::StoppedReason::kTaskComplete);
  }
  active_tasks_.clear();
}

TaskId ActorKeyedService::CreateTask() {
  return CreateTaskWithOptions(nullptr, nullptr);
}

TaskId ActorKeyedService::CreateTaskWithOptions(
    webui::mojom::TaskOptionsPtr options,
    base::WeakPtr<ActorTaskDelegate> delegate) {
  TRACE_EVENT0("actor", "ActorKeyedService::CreateTask");
  if (!policy_checker_->can_act_on_web()) {
    base::UmaHistogramBoolean("Actor.Task.Created", false);
    GetJournal().Log(GURL(), TaskId(), "ActorKeyedService::CreateTask",
                     JournalDetailsBuilder()
                         .AddError("Actuation capability disabled")
                         .Build());
    return TaskId();
  }
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

void ActorKeyedService::NotifyTaskStateChanged(TaskId task_id,
                                               ActorTask::State state) {
  tab_state_change_callback_list_.Notify(task_id, state);
}

void ActorKeyedService::OnActOnWebCapabilityChanged(bool can_act_on_web) {
  if (!can_act_on_web) {
    StopAllTasks(ActorTask::StoppedReason::kChromeFailure);
  }
  act_on_web_capability_changed_callback_list_.Notify(can_act_on_web);
}

base::CallbackListSubscription
ActorKeyedService::AddActOnWebCapabilityChangedCallback(
    ActOnWebCapabilityChangedCallback callback) {
  return act_on_web_capability_changed_callback_list_.Add(std::move(callback));
}

void ActorKeyedService::RequestTabObservation(
    tabs::TabInterface& tab,
    TaskId task_id,
    base::OnceCallback<void(TabObservationResult)> callback) {
  TRACE_EVENT0("actor", "ActorKeyedService::RequestTabObservation");
  const GURL& last_committed_url = tab.GetContents()->GetLastCommittedURL();
  auto journal_entry = journal_.CreatePendingAsyncEntry(
      last_committed_url, task_id, MakeBrowserTrackUUID(task_id),
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
            if (base::FeatureList::IsEnabled(
                    kGlicActorFixPageObservationCrash)) {
              if (!result.has_value()) {
                std::move(callback).Run(base::unexpected(result.error()));
                return;
              }
            }

            if (result.has_value() &&
                result.value()->annotated_page_content_result.has_value() &&
                result.value()->screenshot_result.has_value()) {
              auto& fetch_result = **result;
              size_t size = fetch_result.annotated_page_content_result->proto
                                .ByteSizeLong();
              std::vector<uint8_t> buffer(size);
              fetch_result.annotated_page_content_result->proto
                  .SerializeToArray(buffer.data(), size);
              pending_journal_entry->GetJournal().LogAnnotatedPageContent(
                  last_committed_url, pending_journal_entry->GetTaskId(),
                  buffer);

              auto& data = fetch_result.screenshot_result->screenshot_data;
              pending_journal_entry->GetJournal().LogScreenshot(
                  last_committed_url, pending_journal_entry->GetTaskId(),
                  fetch_result.screenshot_result->mime_type,
                  base::as_byte_span(data));
              if (tab) {
                actor::ActorTabData::From(tab.get())->DidObserveContent(
                    fetch_result.annotated_page_content_result->proto);
              }
            }

            std::move(callback).Run(std::move(result).value());
          },
          tab.GetWeakPtr(), std::move(callback), std::move(journal_entry),
          last_committed_url));
}

//  static
std::optional<std::string> ActorKeyedService::ExtractErrorMessageIfFailed(
    const TabObservationResult& result) {
  if (!result.has_value()) {
    return absl::StrFormat(
        "Failed Observation: code[%s] message[%s]",
        page_content_annotations::ToString(result.error().error_code),
        result.error().message);
  }

  page_content_annotations::FetchPageContextResult& fetch_result = **result;

  // Context for actor observations should always have an APC and a screenshot,
  // return failure if either is missing.
  bool has_apc = fetch_result.annotated_page_content_result.has_value();
  bool has_screenshot = fetch_result.screenshot_result.has_value();
  if (!has_apc || !has_screenshot) {
    return absl::StrFormat(
        "Fetch Error: APC[%s] screenshot[%s]",
        has_apc ? std::string("OK")
                : fetch_result.annotated_page_content_result.error(),
        has_screenshot ? std::string("OK")
                       : fetch_result.screenshot_result.error());
  }

  return std::nullopt;
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
    GetJournal().Log(GURL(), TaskId(), "ActorKeyedService::PerformActions",
                     JournalDetailsBuilder()
                         .Add("task_id", task_id)
                         .AddError("Invalid Task")
                         .Build());
    RunLater(base::BindOnce(std::move(callback),
                            mojom::ActionResultCode::kTaskWentAway,
                            std::nullopt, std::move(empty_results)));
    return;
  }

  if (actions.empty()) {
    GetJournal().Log(
        GURL(), TaskId(), "ActorKeyedService::PerformActions",
        JournalDetailsBuilder().AddError("Empty Actions List").Build());
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

void ActorKeyedService::StopAllTasks(ActorTask::StoppedReason stop_reason) {
  std::vector<TaskId> tasks_to_stop =
      FindTaskIdsInActive([](const ActorTask& task) { return true; });
  GetJournal().Log(GURL(), TaskId(), "ActorKeyedService::StopAllTasks", {});
  for (const auto& task_id : tasks_to_stop) {
    StopTask(task_id, stop_reason);
  }
}

void ActorKeyedService::StopTask(TaskId task_id,
                                 ActorTask::StoppedReason stop_reason) {
  TRACE_EVENT0("actor", "ActorKeyedService::StopTask");
  GetJournal().Log(GURL(), TaskId(), "ActorKeyedService::StopTask",
                   JournalDetailsBuilder()
                       .Add("task_id", task_id)
                       .Add("stop_reason", stop_reason)
                       .Build());

  auto task = active_tasks_.extract(task_id);
  if (!task.empty()) {
    task.mapped()->Stop(stop_reason);
  }
}

ActorTask* ActorKeyedService::GetTask(TaskId task_id) {
  auto task = active_tasks_.find(task_id);
  if (task != active_tasks_.end()) {
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

void ActorKeyedService::OnDownloadCreated(content::DownloadManager* manager,
                                          download::DownloadItem* item) {
  if (content::WebContents* web_contents =
          content::DownloadItemUtils::GetWebContents(item)) {
    if (GetActingActorTaskForWebContents(web_contents)) {
      base::UmaHistogramBoolean("Actor.Download.DirectDownloadTriggered", true);
    }
  }
}

}  // namespace actor
