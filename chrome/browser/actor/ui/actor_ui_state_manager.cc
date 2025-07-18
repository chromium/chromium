// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_state_manager.h"

#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/actor/variant_visitor.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/actor/action_result.h"
#include "components/tabs/public/tab_interface.h"

namespace actor::ui {
namespace {
using tabs::TabInterface;

// TODO(crbug.com/424495020): Hardcoded states; Move this out to it's own file
// to be shared with tab controller.
const UiTabState& GetAgentControlledUiTabState() {
  static const UiTabState kAgentState = {
      .actor_overlay = ActorOverlayState(/*is_active=*/true),
      .handoff_button = {
          .is_active = true,
          .controller = HandoffButtonState::ControlOwnership::kAgent}};
  return kAgentState;
}

const UiTabState& GetPausedUiTabState() {
  static const UiTabState kPausedState = {
      .actor_overlay = ActorOverlayState(/*is_active=*/false),
      .handoff_button = {
          .is_active = true,
          .controller = HandoffButtonState::ControlOwnership::kClient}};
  return kPausedState;
}

const UiTabState& GetCompletedUiTabState() {
  static const UiTabState kCompletedState = {
      .actor_overlay = ActorOverlayState(/*is_active=*/false),
      .handoff_button = {
          .is_active = false,
          .controller = HandoffButtonState::ControlOwnership::kClient}};
  return kCompletedState;
}

struct TabUiUpdate {
  raw_ptr<TabInterface> tab;
  UiTabState ui_tab_state;
};

auto GetNewUiStateFn(ActorUiStateManager& manager) {
  return Visitor{
      [&manager](const StartingToActOnTab& e) -> TabUiUpdate {
        auto* tab = e.tab_handle.Get();
        if (auto* tab_controller = manager.GetUiTabController(tab)) {
          tab_controller->SetActiveTaskId(e.task_id);
        }
        return TabUiUpdate{tab, GetAgentControlledUiTabState()};
      },
      [&manager](const StoppedActingOnTab& e) -> TabUiUpdate {
        auto* tab = e.tab_handle.Get();
        if (auto* tab_controller = manager.GetUiTabController(tab)) {
          tab_controller->ClearActiveTaskId();
        }
        return TabUiUpdate{tab, GetCompletedUiTabState()};
      },
      [](const MouseClick& e) -> TabUiUpdate {
        UiTabState ui_tab_state = GetAgentControlledUiTabState();
        ui_tab_state.actor_overlay.mouse_down = true;
        return TabUiUpdate{e.tab_handle.Get(), ui_tab_state};
      },
      [](const MouseMove& e) -> TabUiUpdate {
        UiTabState ui_tab_state = GetAgentControlledUiTabState();
        ui_tab_state.actor_overlay.mouse_target = e.target;
        return TabUiUpdate{e.tab_handle.Get(), ui_tab_state};
      }};
}

// TODO(crbug.com/424495020): Bool may be converted to a map of ui
// components:bool depending on what controller returns.
void OnUiChangeComplete(UiCompleteCallback complete_callback, bool result) {
  std::move(complete_callback).Run(result ? MakeOkResult() : MakeErrorResult());
}

void LogUiChangeError(bool result) {
  if (!result) {
    LOG(DFATAL)
        << "Unexpected error when trying to update actor ui components.";
  }
}

}  // namespace

ActorUiStateManager::ActorUiStateManager(ActorKeyedService& actor_service)
    : actor_service_(actor_service) {}
ActorUiStateManager::~ActorUiStateManager() = default;

// TODO(crbug.com/424495020): If the tab doesn't exist we will silently
// fail/not send a callback in the interim until these tasks are able to
// accept a callback.
void ActorUiStateManager::OnActorTaskStateChange(
    TaskId task_id,
    ActorTask::State new_task_state) {
  // TODO(crbug.com/424495020): Look into converting this switch into a
  // map/catalog.
  // Notify tab-scoped UI components.
  UiTabState ui_tab_state;
  switch (new_task_state) {
    case ActorTask::State::kCreated:
      LOG(FATAL)
          << "Task state should never be set to kCreated from another state.";
    case ActorTask::State::kActing:
    case ActorTask::State::kReflecting:
      ui_tab_state = GetAgentControlledUiTabState();
      break;
    case ActorTask::State::kPausedByClient:
      ui_tab_state = GetPausedUiTabState();
      break;
    case ActorTask::State::kFinished:
      ui_tab_state = GetCompletedUiTabState();
      completed_tasks_expiry_timer_.Start(
          FROM_HERE, kCompletedTaskExpiryDelay,
          base::BindOnce(&ActorUiStateManager::MaybeUpdateProfileScopedUiState,
                         weak_factory_.GetWeakPtr()));
      break;
  }
  for (const auto& tab : GetTabs(task_id)) {
    if (auto* tab_controller = GetUiTabController(tab)) {
      tab_controller->OnUiTabStateChange(ui_tab_state,
                                         base::BindOnce(&LogUiChangeError));
    }
  }

  // Update profile scoped state change.
  update_profile_scoped_ui_debounce_timer_.Start(
      FROM_HERE, kProfileScopedUiUpdateDebounceDelay,
      base::BindOnce(&ActorUiStateManager::MaybeUpdateProfileScopedUiState,
                     weak_factory_.GetWeakPtr()));
}

ActorUiTabControllerInterface* ActorUiStateManager::GetUiTabController(
    tabs::TabInterface* tab) {
  if (!tab) {
    LOG(ERROR) << "Tab does not exist.";
    return nullptr;
  }
  auto* tab_controller = tab->GetTabFeatures()->actor_ui_tab_controller();
  DCHECK(tab_controller)
      << "TabController should always exist for a valid tab.";
  return tab_controller;
}

std::vector<tabs::TabInterface*> ActorUiStateManager::GetTabs(TaskId id) {
  if (ActorTask* task = actor_service_->GetTask(id)) {
    std::vector<tabs::TabInterface*> tabs;
    for (const tabs::TabHandle& handle : task->GetTabs()) {
      if (tabs::TabInterface* tab = handle.Get()) {
        tabs.push_back(tab);
      }
    }
    return tabs;
  }
  return {};
}

// TODO(crbug.com/424495020): In the future when a UiEvent can modify multiple
// scoped ui components, we can look into using BarrierClosure.
void ActorUiStateManager::OnUiEvent(AsyncUiEvent event,
                                    UiCompleteCallback callback) {
  const TabUiUpdate update = std::visit(GetNewUiStateFn(*this), event);
  if (auto* tab_controller = GetUiTabController(update.tab)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ActorUiTabControllerInterface::OnUiTabStateChange,
            tab_controller->GetWeakPtr(), update.ui_tab_state,
            base::BindOnce(&OnUiChangeComplete, std::move(callback))));
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       MakeResult(mojom::ActionResultCode::kTabWentAway)));
  }
}

void ActorUiStateManager::OnUiEvent(SyncUiEvent event) {
  std::visit(Visitor{[this](const StartTask& e) {
                       this->MaybeUpdateProfileScopedUiState();
                     },
                     [this](const TaskStateChanged& e) {
                       this->OnActorTaskStateChange(e.task_id, e.state);
                     }},
             event);
}

#if BUILDFLAG(ENABLE_GLIC)
void ActorUiStateManager::OnGlicUpdateFloatyState(
    glic::GlicWindowController::State floaty_state) {
  switch (floaty_state) {
    case glic::GlicWindowController::State::kClosed:
      MaybeShowToast();
      break;
    case glic::GlicWindowController::State::kOpen:
    case glic::GlicWindowController::State::kWaitingForGlicToLoad:
      break;
  }
}
#endif

void ActorUiStateManager::MaybeShowToast() {
  // TODO(crbug.com/428014205): Implement this function.
}

void ActorUiStateManager::MaybeUpdateProfileScopedUiState() {
  const auto& active_tasks = actor_service_->GetActiveTasks();
  const bool has_paused_task = std::any_of(
      active_tasks.begin(), active_tasks.end(), [](const auto& task_pair) {
        return task_pair.second->GetState() ==
               ActorTask::State::kPausedByClient;
      });

  UiState new_state;
  if (!GetCompletedTasks(base::Time::Now()).empty() || has_paused_task) {
    new_state = ActorUiStateManager::UiState::kCheckTasks;
  } else if (!active_tasks.empty()) {
    new_state = ActorUiStateManager::UiState::kActive;
  } else {
    new_state = ActorUiStateManager::UiState::kInactive;
  }

  if (state_ != new_state) {
    state_ = new_state;
    // TODO(crbug.com/424495020): Create window controller and send new state
    // via BrowserList::GetInstance()->ForEachCurrentAndNewBrowser...
    // then wait for a callback.
  }
}

std::vector<TaskId> ActorUiStateManager::GetCompletedTasks(
    base::Time current_time) const {
  std::vector<TaskId> completed_tasks;
  for (const auto& [task_id, task] : actor_service_->GetInactiveTasks()) {
    if (task->GetState() == ActorTask::State::kFinished &&
        (current_time - task->GetEndTime() < kCompletedTaskExpiryDelay)) {
      completed_tasks.push_back(task_id);
    }
  }
  return completed_tasks;
}

ActorUiStateManager::UiState ActorUiStateManager::GetUiState() const {
  return state_;
}

}  // namespace actor::ui
