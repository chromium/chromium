// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_actor_controller.h"

#include <memory>

#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/aggregated_journal.h"
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

namespace glic {

namespace {

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
    std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry> journal_entry,
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
    execution_engine->DidObserveContext(
        tab_context_result->get_tab_context()
            ->annotated_page_data->annotated_page_content.value());
  }

  if (tab_context_result->get_tab_context()->viewport_screenshot) {
    journal_entry->GetJournal().LogScreenshot(
        GURL::EmptyGURL(), journal_entry->GetTaskId(),
        tab_context_result->get_tab_context()->viewport_screenshot->mime_type,
        tab_context_result->get_tab_context()->viewport_screenshot->data);
  }

  mojom::ActInFocusedTabResultPtr result =
      mojom::ActInFocusedTabResult::NewActInFocusedTabResponse(
          mojom::ActInFocusedTabResponse::New(
              std::move(tab_context_result->get_tab_context())));

  std::move(callback).Run(std::move(result));
}

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
  // A task is in the process of being started. This means Act() was called
  // twice in a row without waiting for the first one to finish.
  if (starting_task_) {
    PostTaskForActCallback(
        std::move(callback),
        mojom::ActInFocusedTabErrorReason::kFailedToStartTask);
    return;
  }

  // Create a new task if one doesn't exist already.
  if (!actor_task_ ||
      actor_task_->GetState() == actor::ActorTask::State::kFinished) {
    starting_task_ = true;
    optimization_guide::proto::BrowserStartTask start_task;
    start_task.set_tab_id(action.tab_id());
    // Glic doesn't know about tab IDs yet, so we set it in `start_task` but
    // it's always 0. This will cause `StartTask` to create a new tab.
    actor::ActorKeyedService::Get(profile_)->StartTask(
        std::move(start_task),
        base::BindOnce(&GlicActorController::OnTaskStartedForAct, GetWeakPtr(),
                       action, options, std::move(callback)));
    return;
  }

  ActImpl(action, options, std::move(callback));
}

void GlicActorController::OnTaskStartedForAct(
    const optimization_guide::proto::BrowserAction& action,
    const mojom::GetTabContextOptions& options,
    mojom::WebClientHandler::ActInFocusedTabCallback callback,
    optimization_guide::proto::BrowserStartTaskResult result) {
  starting_task_ = false;
  if (result.status() !=
      optimization_guide::proto::BrowserStartTaskResult::SUCCESS) {
    PostTaskForActCallback(
        std::move(callback),
        mojom::ActInFocusedTabErrorReason::kFailedToStartTask);
    return;
  }

  actor_task_ = actor::ActorKeyedService::Get(profile_)->GetTask(
      actor::TaskId(result.task_id()));
  CHECK(actor_task_);

  ActImpl(action, options, std::move(callback));
}

// TODO(mcnee): Determine if we need additional mechanisms, within the browser,
// to stop a task.
void GlicActorController::StopTask(actor::TaskId task_id) {
  if (!GetExecutionEngine() ||
      actor_task_->GetState() == actor::ActorTask::State::kFinished) {
    return;
  }
  actor_task_->Stop();
}

void GlicActorController::PauseTask(actor::TaskId task_id) {
  if (!actor_task_) {
    return;
  }
  actor_task_->Pause();
}

void GlicActorController::ResumeTask(
    actor::TaskId task_id,
    const mojom::GetTabContextOptions& context_options,
    glic::mojom::WebClientHandler::ResumeActorTaskCallback callback) {
  if (!actor_task_ ||
      actor_task_->GetState() != actor::ActorTask::State::kPausedByClient) {
    std::move(callback).Run(mojom::GetContextResult::NewErrorReason(
        std::string("task does not exist or was not paused")));
    return;
  }
  actor_task_->Resume();
  tabs::TabInterface* tab_of_resumed_task =
      GetExecutionEngine()->GetTabOfCurrentTask();
  if (!tab_of_resumed_task) {
    std::move(callback).Run(glic::mojom::GetContextResult::NewErrorReason(
        std::string("tab does not exist")));
    return;
  }

  glic::FetchPageContext(tab_of_resumed_task, context_options,
                         /*include_actionable_data=*/true, std::move(callback));
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

bool GlicActorController::IsExecutionEngineActingOnTab(
    const content::WebContents* wc) const {
  return GetExecutionEngine() && actor_task_ &&
         actor_task_->GetState() != actor::ActorTask::State::kFinished &&
         GetExecutionEngine()->GetTabOfCurrentTask()->GetContents() == wc;
}

actor::ExecutionEngine& GlicActorController::GetExecutionEngineForTesting(
    tabs::TabInterface* tab) {
  if (!actor_task_) {
    auto task = std::make_unique<actor::ActorTask>(
        std::make_unique<actor::ExecutionEngine>(profile_, tab));
    actor_task_ = task.get();
    actor::ActorKeyedService::Get(profile_.get())->AddTask(std::move(task));
  }
  return *actor_task_->GetExecutionEngine();
}

void GlicActorController::ActImpl(
    const optimization_guide::proto::BrowserAction& action,
    const mojom::GetTabContextOptions& options,
    mojom::WebClientHandler::ActInFocusedTabCallback callback) const {
  actor::ExecutionEngine::ActionResultCallback action_callback = base::BindOnce(
      &GlicActorController::OnActionFinished, GetWeakPtr(),
      actor::TaskId(action.task_id()), options, std::move(callback));

  GetExecutionEngine()->Act(action, std::move(action_callback));
}

void GlicActorController::OnActionFinished(
    actor::TaskId task_id,
    const mojom::GetTabContextOptions& options,
    mojom::WebClientHandler::ActInFocusedTabCallback callback,
    actor::mojom::ActionResultPtr result) const {
  if (!actor::IsOk(*result)) {
    PostTaskForActCallback(std::move(callback),
                           mojom::ActInFocusedTabErrorReason::kTargetNotFound);
    return;
  }

  tabs::TabInterface* tab = GetExecutionEngine()->GetTabOfCurrentTask();
  actor::AggregatedJournal& journal =
      actor::ActorKeyedService::Get(profile_)->GetJournal();

  // TODO(https://crbug.com/398271171): Remove when the actor coordinator
  // handles getting a new observation.
  // TODO(https://crbug.com/402086398): Figure out if/how this can be shared
  // with GlicKeyedService::GetContextFromFocusedTab(). It's not clear yet if
  // the same permission checks, etc. should apply here.
  if (tab) {
    auto journal_entry = journal.CreatePendingAsyncEntry(
        tab->GetContents()->GetLastCommittedURL(), task_id, "FetchPageContext",
        "");

    FetchPageContext(
        tab, options, /*include_actionable_data=*/true,
        base::BindOnce(OnFetchPageContext, std::move(journal_entry),
                       std::move(callback),
                       GetExecutionEngine()->GetWeakPtr()));
  } else {
    journal.Log(GURL(), task_id, "FetchPageContext", "Tab is gone");
    PostTaskForActCallback(std::move(callback),
                           mojom::ActInFocusedTabErrorReason::kTargetNotFound);
  }
}

base::WeakPtr<const GlicActorController> GlicActorController::GetWeakPtr()
    const {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<GlicActorController> GlicActorController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

actor::ExecutionEngine* GlicActorController::GetExecutionEngine() const {
  if (!actor_task_) {
    return nullptr;
  }
  return actor_task_->GetExecutionEngine();
}
}  // namespace glic
