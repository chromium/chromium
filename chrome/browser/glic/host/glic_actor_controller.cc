// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_actor_controller.h"

#include <algorithm>
#include <cstddef>
#include <memory>

#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace glic {

using ::actor::ActorKeyedService;
using ::actor::ActorTask;
using ::actor::BuildToolRequest;
using ::actor::BuildToolRequestResult;
using ::actor::TaskId;
using ::actor::ToolRequest;
using ::actor::ToolRequestList;

namespace {

mojom::GetTabContextOptionsPtr ActionableOptions(
    const mojom::GetTabContextOptions& options) {
  // TODO(khushalsagar): Ideally this should be set by the web UI instead of
  // overriding here for actor mode.
  auto actionable_context_options = options.Clone();
  actionable_context_options->annotated_page_content_mode = optimization_guide::
      proto::ANNOTATED_PAGE_CONTENT_MODE_ACTIONABLE_ELEMENTS;
  return actionable_context_options;
}

mojom::ActInFocusedTabResultPtr MakeActErrorResult(
    mojom::ActInFocusedTabErrorReason error_reason) {
  mojom::ActInFocusedTabResultPtr result =
      mojom::ActInFocusedTabResult::NewErrorReason(error_reason);
  UMA_HISTOGRAM_ENUMERATION("Glic.Action.ActInFocusedTabErrorReason",
                            result->get_error_reason());
  return result;
}

void PostTaskForActCallback(
    mojom::WebClientHandler::ActInFocusedTabCallback callback,
    mojom::ActInFocusedTabErrorReason error_reason) {
  mojom::ActInFocusedTabResultPtr result = MakeActErrorResult(error_reason);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

void OnFetchPageContext(
    const GURL& url,
    mojo_base::ProtoWrapperBytes::PassKey proto_pass_key,
    std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry> journal_entry,
    actor::mojom::ActionResultCode result_code,
    mojom::WebClientHandler::ActInFocusedTabCallback callback,
    base::WeakPtr<actor::ExecutionEngine> execution_engine,
    mojom::GetContextResultPtr tab_context_result) {
  if (tab_context_result->is_error_reason()) {
    journal_entry->EndEntry(tab_context_result->get_error_reason());
    mojom::ActInFocusedTabResultPtr result = MakeActErrorResult(
        mojom::ActInFocusedTabErrorReason::kGetContextFailed);
    std::move(callback).Run(std::move(result));
    return;
  }

  if (execution_engine &&
      tab_context_result->get_tab_context()->annotated_page_data &&
      tab_context_result->get_tab_context()
          ->annotated_page_data->annotated_page_content.has_value()) {
    auto byte_span =
        tab_context_result->get_tab_context()
            ->annotated_page_data->annotated_page_content->byte_span(
                proto_pass_key);
    if (byte_span.has_value()) {
      journal_entry->GetJournal().LogAnnotatedPageContent(
          url, journal_entry->GetTaskId(), byte_span.value());
    }

    execution_engine->DidObserveContext(
        tab_context_result->get_tab_context()
            ->annotated_page_data->annotated_page_content.value());
  }

  if (tab_context_result->get_tab_context()->viewport_screenshot) {
    journal_entry->GetJournal().LogScreenshot(
        url, journal_entry->GetTaskId(),
        tab_context_result->get_tab_context()->viewport_screenshot->mime_type,
        tab_context_result->get_tab_context()->viewport_screenshot->data);
  }

  mojom::ActInFocusedTabResultPtr result =
      mojom::ActInFocusedTabResult::NewActInFocusedTabResponse(
          mojom::ActInFocusedTabResponse::New(
              std::move(tab_context_result->get_tab_context()),
              base::to_underlying(result_code)));

  std::move(callback).Run(std::move(result));
}

void LogAddTabError(actor::mojom::ActionResultPtr result) {
  if (!actor::IsOk(*result)) {
    LOG(DFATAL)
        << "Unexpected error when calling AddTab from GlicActorController::Act "
           "(crbug.com/431239173): "
        << actor::ToDebugString(*result);
  }
}

BASE_FEATURE(kGlicProvideObservationOnActionFailure,
             "GlicProvideObservationOnActionFailure",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

// A wrapper class around actor::AggregatedJournal::PendingAsyncEntry for
// dependency purposes.
class GlicActorController::OngoingRequest {
 public:
  std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry> journal_entry_;
};

GlicActorController::GlicActorController(Profile* profile) : profile_(profile) {
  CHECK(profile_);
  actor::ExecutionEngine::RegisterWithProfile(profile_);
}

GlicActorController::~GlicActorController() = default;

void GlicActorController::Act(
    const optimization_guide::proto::BrowserAction& action,
    const mojom::GetTabContextOptions& options,
    mojom::WebClientHandler::ActInFocusedTabCallback callback) {
  auto* actor_service = actor::ActorKeyedService::Get(profile_);
  CHECK(actor_service);

  actor_service->GetJournal().Log(
      GURL(), TaskId(action.task_id()), "GlicActInFocusedTab",
      absl::StrFormat("Proto: %s", actor::ToBase64(action)));

  actor::BuildToolRequestResult result =
      actor::BuildToolRequest(action, /*deprecated_fallback_tab=*/nullptr);

  if (!result.has_value()) {
    actor::ActorKeyedService::Get(profile_)->GetJournal().Log(
        /*url=*/GURL(), actor::TaskId(), "ActImpl",
        absl::StrFormat("Invalid BrowserAction proto[%d]", result.error()));
    PostTaskForActCallback(
        std::move(callback),
        mojom::ActInFocusedTabErrorReason::kInvalidActionProto);
    return;
  }

  ToolRequestList& tool_requests = result.value();

  // TODO(crbug.com/431325114): Once the front end injected CreateTabAction
  // provides a task ID we can remove the GetMostRecentTask branch.
  actor::ActorTask* task =
      action.has_task_id() && action.task_id() != 0
          ? actor_service->GetTask(actor::TaskId(action.task_id()))
          : actor_service->GetMostRecentTask();

  // TODO(crbug.com/431239173): It's not clear what should happen if the current
  // task has no observed tabs in its set yet, and the incoming actions will not
  // add one (e.g. first action in a new task is Wait). This workaround
  // preserves existing behavior, we'll use the currently focused tab. Long term
  // the API should have support to deal with this case (Should we return an
  // empty observation? Should we return an error?).
  if (task && task->GetTabs().empty()) {
    bool will_observe_tab = std::ranges::any_of(
        tool_requests, [](const std::unique_ptr<ToolRequest>& request) {
          return request->AddsTabToObservationSet();
        });

    if (!will_observe_tab) {
      actor_service->GetJournal().Log(
          /*url=*/GURL(), task->id(), "[Warning] No observable tab",
          "Action will end without an observable tab, adding active tab.");

      // Get the most recently active browser for this profile.
      Browser* browser = chrome::FindTabbedBrowser(
          profile_, /*match_original_profiles=*/false);
      // If no browser exists create one.
      if (!browser) {
        browser = Browser::Create(
            Browser::CreateParams(profile_, /*user_gesture=*/false));
      }
      // TODO(crbug.com/431239173): We should remove this call as the UI has
      // no mechanism for reporting errors when launching on this tab.
      task->AddTab(browser->GetActiveTabInterface()->GetHandle(),
                   base::BindOnce(&LogAddTabError));
    }
  }

  ActorKeyedService::PerformActionsCallback action_callback =
      base::BindOnce(&GlicActorController::OnActionFinished, GetWeakPtr(),
                     task->id(), options, std::move(callback));
  actor_service->PerformActions(task ? task->id() : TaskId(),
                                std::move(tool_requests),
                                std::move(action_callback));
}

// TODO(mcnee): Determine if we need additional mechanisms, within the browser,
// to stop a task.
void GlicActorController::StopTask(actor::TaskId task_id) {
  actor::ActorTask* task = GetCurrentTask();
  if (!task) {
    return;
  }
  actor::ActorKeyedService::Get(profile_.get())->StopTask(task->id());
}

void GlicActorController::PauseTask(actor::TaskId task_id) {
  actor::ActorTask* task = GetCurrentTask();
  if (!task) {
    return;
  }
  task->Pause();
}

void GlicActorController::ResumeTask(
    actor::TaskId task_id,
    const mojom::GetTabContextOptions& context_options,
    glic::mojom::WebClientHandler::ResumeActorTaskCallback callback) {
  actor::ActorTask* task = GetCurrentTask();
  if (!task || task->GetState() != actor::ActorTask::State::kPausedByClient) {
    std::move(callback).Run(mojom::GetContextResult::NewErrorReason(
        std::string("task does not exist or was not paused")));
    return;
  }
  task->Resume();
  tabs::TabInterface* tab_of_resumed_task = task->GetTabForObservation();
  if (!tab_of_resumed_task) {
    std::move(callback).Run(glic::mojom::GetContextResult::NewErrorReason(
        std::string("tab does not exist")));
    return;
  }

  glic::FetchPageContext(tab_of_resumed_task,
                         *ActionableOptions(context_options),
                         std::move(callback));
}

void GlicActorController::OnUserInputSubmitted() {
  current_request_ = std::make_unique<OngoingRequest>();
  current_request_->journal_entry_ =
      actor::ActorKeyedService::Get(profile_.get())
          ->GetJournal()
          .CreatePendingAsyncEntry(/*url=*/GURL::EmptyGURL(), actor::TaskId(),
                                   "Request", /*details=*/"User Input");
}

void GlicActorController::OnRequestStarted() {
  auto& journal = actor::ActorKeyedService::Get(profile_.get())->GetJournal();

  if (!current_request_) {
    current_request_ = std::make_unique<OngoingRequest>();
    current_request_->journal_entry_ = journal.CreatePendingAsyncEntry(
        /*url=*/GURL::EmptyGURL(), actor::TaskId(), "Request",
        /*details=*/"Multi-turn");
  } else {
    journal.Log(/*url=*/GURL(), actor::TaskId(), "Request", "Request Started");
  }
}

void GlicActorController::OnResponseStarted() {
  actor::ActorKeyedService::Get(profile_.get())
      ->GetJournal()
      .Log(/*url=*/GURL(), actor::TaskId(), "Request", "Response Started");
}

void GlicActorController::OnResponseStopped() {
  current_request_.reset();
}

void GlicActorController::OnActionFinished(
    actor::TaskId task_id,
    const mojom::GetTabContextOptions& options,
    mojom::WebClientHandler::ActInFocusedTabCallback callback,
    actor::mojom::ActionResultCode result_code,
    std::optional<size_t> index_of_failed_action) const {
  if (!actor::IsOk(result_code) &&
      !ProvideObservationOnActionFailureEnabled()) {
    PostTaskForActCallback(std::move(callback),
                           mojom::ActInFocusedTabErrorReason::kTargetNotFound);
    return;
  }

  actor::ActorTask* task =
      actor::ActorKeyedService::Get(profile_)->GetTask(task_id);
  CHECK(task);

  actor::AggregatedJournal& journal =
      actor::ActorKeyedService::Get(profile_)->GetJournal();
  if (task->GetTabs().size() != 1) {
    journal.Log(GURL::EmptyGURL(), task_id,
                "[Warning] Unexpected number of tabs",
                absl::StrFormat("Expect 1 observable tab but have [%d]",
                                task->GetTabs().size()));
  }

  tabs::TabInterface* tab = task->GetTabForObservation();

  // TODO(https://crbug.com/398271171): Remove when the actor coordinator
  // handles getting a new observation.
  // TODO(https://crbug.com/402086398): Figure out if/how this can be shared
  // with GlicKeyedService::GetContextFromFocusedTab(). It's not clear yet if
  // the same permission checks, etc. should apply here.
  if (tab) {
    const GURL& url = tab->GetContents()->GetLastCommittedURL();
    auto journal_entry =
        journal.CreatePendingAsyncEntry(url, task_id, "FetchPageContext", "");

    FetchPageContext(tab, *ActionableOptions(options),
                     base::BindOnce(OnFetchPageContext, url,
                                    mojo_base::ProtoWrapperBytes::GetPassKey(),
                                    std::move(journal_entry), result_code,
                                    std::move(callback),
                                    task->GetExecutionEngine()->GetWeakPtr()));
  } else {
    journal.Log(GURL::EmptyGURL(), task_id, "FetchPageContext", "Tab is gone");
    PostTaskForActCallback(std::move(callback),
                           mojom::ActInFocusedTabErrorReason::kTargetNotFound);
  }
}

// static
bool GlicActorController::ProvideObservationOnActionFailureEnabled() {
  return base::FeatureList::IsEnabled(kGlicProvideObservationOnActionFailure);
}

base::WeakPtr<const GlicActorController> GlicActorController::GetWeakPtr()
    const {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<GlicActorController> GlicActorController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

actor::ActorTask* GlicActorController::GetCurrentTask() const {
  return actor::ActorKeyedService::Get(profile_)->GetMostRecentTask();
}

}  // namespace glic
