// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool_controller.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/safe_ref.h"
#include "base/state_transitions.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "url/gurl.h"

namespace actor {

using ::optimization_guide::proto::AnnotatedPageContent;

ToolController::ActiveState::ActiveState(
    std::unique_ptr<Tool> tool,
    ResultCallback completion_callback,
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry)
    : tool(std::move(tool)),
      completion_callback(std::move(completion_callback)),
      journal_entry(std::move(journal_entry)) {
  CHECK(this->tool);
  CHECK(!this->completion_callback.is_null());
}
ToolController::ActiveState::~ActiveState() = default;

ToolController::ToolController(ActorTask& task, ToolDelegate& tool_delegate)
    : task_(&task), tool_delegate_(tool_delegate) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
}

ToolController::~ToolController() = default;

void ToolController::SetState(State state) {
  journal().Log(active_state_ ? active_state_->tool->JournalURL() : GURL(),
                task_->id(), "ToolControllerStateChange",
                JournalDetailsBuilder()
                    .Add("current_state", StateToString(state_))
                    .Add("new_state", StateToString(state))
                    .Build());
#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<State>> transitions(
      base::StateTransitions<State>({
          {State::kInit, {State::kCreating}},
          {State::kReady, {State::kCreating}},
          {State::kCreating, {State::kValidating}},
          {State::kValidating, {State::kPostValidate, State::kReady}},
          {State::kPostValidate, {State::kInvokable, State::kReady}},
          {State::kInvokable, {State::kPreInvoke, State::kReady}},
          {State::kPreInvoke, {State::kInvoking, State::kReady}},
          {State::kInvoking, {State::kPostInvoke, State::kReady}},
          {State::kPostInvoke, {State::kReady}},
      }));
  DCHECK_STATE_TRANSITION(transitions, state_, state);
#endif  // DCHECK_IS_ON()
  state_ = state;
}

std::string ToolController::StateToString(State state) {
  switch (state) {
    case State::kInit:
      return "INIT";
    case State::kReady:
      return "READY";
    case State::kCreating:
      return "CREATING";
    case State::kValidating:
      return "VALIDATING";
    case State::kPostValidate:
      return "POST_VALIDATE";
    case State::kInvokable:
      return "INVOKABLE";
    case State::kPreInvoke:
      return "PREINVOKE";
    case State::kInvoking:
      return "INVOKING";
    case State::kPostInvoke:
      return "POSTINVOKE";
  }
}

std::ostream& operator<<(std::ostream& o, const ToolController::State& s) {
  return o << ToolController::StateToString(s);
}

void ToolController::CreateToolAndValidate(
    const ToolRequest& request,
    ResultCallback result_callback) {
  SetState(State::kCreating);
  ToolRequest::CreateToolResult create_result =
      request.CreateTool(task_->id(), *tool_delegate_);
  VLOG(4) << "Creating Tool for " << request.JournalEvent() << " "
          << request.GetURLForJournal();

  if (!IsOk(*create_result.result)) {
    CHECK(!create_result.tool);
    journal().Log(request.GetURLForJournal(), task_->id(),
                  "ToolController CreateToolAndValidate Failed",
                  JournalDetailsBuilder()
                      .AddError(create_result.result->message)
                      .Build());
    PostResponseTask(std::move(result_callback),
                     std::move(create_result.result));
    return;
  }

  observation_page_stability_config_ =
      request.GetObservationPageStabilityConfig();

  std::unique_ptr<Tool>& tool = create_result.tool;
  CHECK(tool);

  auto journal_event = journal().CreatePendingAsyncEntry(
      tool->JournalURL(), task_->id(), MakeBrowserTrackUUID(task_->id()),
      tool->JournalEvent(),
      JournalDetailsBuilder().Add("tool", tool->DebugString()).Build());
  active_state_.emplace(std::move(tool), std::move(result_callback),
                        std::move(journal_event));

  SetState(State::kValidating);
  active_state_->tool->Validate(base::BindOnce(&ToolController::PostValidate,
                                               weak_ptr_factory_.GetWeakPtr()));
}

void ToolController::PostValidate(mojom::ActionResultPtr result) {
  if (!IsOk(*result)) {
    // TODO(b/455139841): Ensure that even if a tool fails validation it gets
    // the change to update the controlled tab set so an observation can be
    // removed. This is band-aid fix but we'll need to rethink how tab-adding
    // works since clients rely on observations always being available.
    active_state_->tool->UpdateTaskBeforeInvoke(
        *task_, base::BindOnce(
                    [](base::WeakPtr<ToolController> tool_controller_this,
                       mojom::ActionResultPtr validate_result,
                       mojom::ActionResultPtr update_task_result_unused) {
                      if (!tool_controller_this) {
                        return;
                      }
                      tool_controller_this->CompleteToolRequest(
                          std::move(validate_result));
                    },
                    weak_ptr_factory_.GetWeakPtr(), std::move(result)));
    return;
  }
  SetState(State::kPostValidate);
  active_state_->tool->UpdateTaskBeforeInvoke(
      *task_, base::BindOnce(&ToolController::PostUpdateTask,
                             weak_ptr_factory_.GetWeakPtr()));
}

void ToolController::PostUpdateTask(mojom::ActionResultPtr result) {
  CHECK(active_state_);

  if (!IsOk(*result)) {
    CompleteToolRequest(std::move(result));
    return;
  }
  SetState(State::kInvokable);
  PostResponseTask(std::move(active_state_->completion_callback),
                   MakeOkResult());
}

void ToolController::Invoke(ResultCallback result_callback) {
  SetState(State::kPreInvoke);
  active_state_->completion_callback = std::move(result_callback);

  Tool& tool = *active_state_->tool;

  const optimization_guide::proto::AnnotatedPageContent*
      last_observed_page_content = nullptr;

  // Not all tools operate on a tab.
  if (tabs::TabInterface* tab = tool.GetTargetTab().Get()) {
    if (auto* tab_data = ActorTabData::From(tab)) {
      last_observed_page_content = tab_data->GetLastObservedPageContent();
    }
  }

  mojom::ActionResultPtr toctou_result =
      tool.TimeOfUseValidation(last_observed_page_content);
  if (!IsOk(*toctou_result)) {
    journal().Log(tool.JournalURL(), task_->id(), "TOCTOU Check Failed",
                  JournalDetailsBuilder()
                      .AddError(ToDebugString(*toctou_result))
                      .Build());
    CompleteToolRequest(std::move(toctou_result));
    return;
  }

  // TODO(crbug.com/389739308): Ensure the acting tab remains valid (i.e.
  // alive and focused), return error otherwise.

  SetState(State::kInvoking);
  observation_delayer_ =
      tool.GetObservationDelayer(observation_page_stability_config_);
  tool.Invoke(base::BindOnce(&ToolController::DidFinishToolInvoke,
                             weak_ptr_factory_.GetWeakPtr()));
}

void ToolController::Cancel() {
  // Only cancel callbacks and states if the tool has been created.
  if (state_ != State::kInit && state_ != State::kReady) {
    weak_ptr_factory_.InvalidateWeakPtrs();
    observation_delayer_.reset();
    if (active_state_) {
      active_state_->tool->Cancel();
    }
    active_state_.reset();
    SetState(State::kReady);
  }
}

void ToolController::DidFinishToolInvoke(mojom::ActionResultPtr result) {
  CHECK(active_state_);

  // Renderers will mark end of execution time.
  if (!result->execution_end_time) {
    result->execution_end_time = base::TimeTicks::Now();
  }

  if (!RequiresPageStabilization(*result) || !observation_delayer_) {
    PostInvokeTool(std::move(result));
    return;
  }

  WaitForObservation(std::move(result));
}

void ToolController::WaitForObservation(mojom::ActionResultPtr result) {
  if (tabs::TabInterface* target_tab =
          active_state_->tool->GetTargetTab().Get()) {
    observation_delayer_->Wait(
        *target_tab,
        base::BindOnce(&ToolController::ObservationDelayComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(result)));
  } else {
    journal().Log(active_state_->tool->JournalURL(), task_->id(),
                  "ToolController DidFinishToolInvoke",
                  JournalDetailsBuilder()
                      .AddError("Tab is gone when tool finishes successfully")
                      .Build());
    PostInvokeTool(std::move(result));
  }
}

void ToolController::ObservationDelayComplete(
    mojom::ActionResultPtr action_result,
    ObservationDelayController::Result observation_result) {
  switch (observation_result) {
    case ObservationDelayController::Result::kOk:
      PostInvokeTool(std::move(action_result));
      break;
    case ObservationDelayController::Result::kPageNavigated: {
      if (tabs::TabInterface* tab = active_state_->tool->GetTargetTab().Get()) {
        size_t last_navigation_count = observation_delayer_->NavigationCount();
        // The page navigated, restart the observation.
        journal().Log(active_state_->tool->JournalURL(), task_->id(),
                      "ToolController Restarting Observation", {});
        observation_delayer_ = std::make_unique<ObservationDelayController>(
            *tab->GetContents()->GetPrimaryMainFrame(), task_->id(), journal(),
            observation_page_stability_config_);
        observation_delayer_->SetNavigationCount(last_navigation_count + 1);
        WaitForObservation(std::move(action_result));
      } else {
        PostInvokeTool(std::move(action_result));
      }
      break;
    }
  }
}

void ToolController::PostInvokeTool(mojom::ActionResultPtr result) {
  if (!IsOk(*result)) {
    CompleteToolRequest(std::move(result));
    return;
  }

  SetState(State::kPostInvoke);
  active_state_->tool->UpdateTaskAfterInvoke(
      *task_, std::move(result),
      base::BindOnce(&ToolController::CompleteToolRequest,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ToolController::CompleteToolRequest(mojom::ActionResultPtr result) {
  CHECK(active_state_);

  SetState(State::kReady);
  observation_delayer_.reset();
  if (IsOk(*result)) {
    active_state_->journal_entry->EndEntry(
        JournalDetailsBuilder().Add("result", "success").Build());
  } else {
    active_state_->journal_entry->EndEntry(
        JournalDetailsBuilder().AddError(ToDebugString(*result)).Build());
  }
  PostResponseTask(std::move(active_state_->completion_callback),
                   std::move(result));
  active_state_.reset();
}

}  // namespace actor
