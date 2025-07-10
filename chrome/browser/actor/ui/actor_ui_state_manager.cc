// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_state_manager.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/execution_engine.h"
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
      .agent_overlay = AgentOverlayState(/*is_active=*/true),
      .handoff_button = {
          .is_active = true,
          .controller = HandoffButtonState::ControlOwnership::kAgent}};
  return kAgentState;
}

const UiTabState& GetPausedUiTabState() {
  static const UiTabState kPausedState = {
      .agent_overlay = AgentOverlayState(/*is_active=*/false),
      .handoff_button = {
          .is_active = true,
          .controller = HandoffButtonState::ControlOwnership::kClient}};
  return kPausedState;
}

const UiTabState& GetCompletedUiTabState() {
  static const UiTabState kCompletedState = {
      .agent_overlay = AgentOverlayState(/*is_active=*/false),
      .handoff_button = {.is_active = false}};
  return kCompletedState;
}

}  // namespace

ActorUiStateManager::ActorUiStateManager(ActorKeyedService& actor_service)
    : actor_service_(actor_service) {}
ActorUiStateManager::~ActorUiStateManager() = default;

void ActorUiStateManager::OnActorTaskStateChange(TaskId task_id,
                                                 ActorTask::State task_state) {
  // TODO(crbug.com/424495020): Look into converting this switch into a
  // map/catalog.
  // Notify tab-scoped UI components.
  UiTabState ui_tab_state;
  switch (task_state) {
    case ActorTask::State::kCreated:
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
    NotifyUiTabController(*tab, ui_tab_state);
  }

  // Update profile scoped state change.
  update_profile_scoped_ui_debounce_timer_.Start(
      FROM_HERE, kProfileScopedUiUpdateDebounceDelay,
      base::BindOnce(&ActorUiStateManager::MaybeUpdateProfileScopedUiState,
                     weak_factory_.GetWeakPtr()));
}

void ActorUiStateManager::NotifyUiTabController(
    TabInterface& tab,
    const UiTabState& ui_tab_state) {
  CHECK(tab.GetTabFeatures()->actor_ui_tab_controller());
  tab.GetTabFeatures()->actor_ui_tab_controller()->OnUiTabStateChange(
      ui_tab_state);
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

void ActorUiStateManager::OnUiEvent(UiEvent event,
                                    UiCompleteCallback callback) {
  // TODO(crbug.com/424495020): Return a callback from the Ui state once
  // successful.
  std::visit(Visitor{
                 [this](const StartTask& e) {
                   this->MaybeUpdateProfileScopedUiState();
                 },
                 [](const StartingToActOnTab& e) {},
                 [](const StoppedActingOnTab& e) {},
                 [](const MouseClick& e) {},
                 [](const MouseMove& e) {},
             },
             event);
  std::move(callback).Run(MakeOkResult());
}

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
