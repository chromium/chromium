// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_state_manager.h"

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/profiles/profile.h"
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

ActorUiStateManager::ActorUiStateManager(Profile* profile)
    : profile_(profile) {}
ActorUiStateManager::~ActorUiStateManager() = default;

// TODO(crbug.com/424495020): Implement profile/window scoped state changes.
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
      break;
  }
  for (const auto& tab : GetTabs(task_id)) {
    NotifyUiTabController(*tab, ui_tab_state);
  }
}

void ActorUiStateManager::NotifyUiTabController(
    TabInterface& tab,
    const UiTabState& ui_tab_state) {
  CHECK(tab.GetTabFeatures()->actor_ui_tab_controller());
  tab.GetTabFeatures()->actor_ui_tab_controller()->OnUiTabStateChange(
      ui_tab_state);
}

std::vector<tabs::TabInterface*> ActorUiStateManager::GetTabs(TaskId id) {
  if (actor::ActorKeyedService* actor_service =
          actor::ActorKeyedService::Get(profile_)) {
    if (ActorTask* task = actor_service->GetTask(id)) {
      std::vector<tabs::TabInterface*> tabs;
      for (const tabs::TabHandle& handle : task->GetTabs()) {
        if (tabs::TabInterface* tab = handle.Get()) {
          tabs.push_back(tab);
        }
      }
      return tabs;
    }
  }
  return {};
}

void ActorUiStateManager::OnUiEvent(UiEvent event,
                                    UiCompleteCallback callback) {
  // TODO(crbug.com/424495020): Implement this function.
  std::move(callback).Run(MakeOkResult());
}

void ActorUiStateManager::MaybeShowToast() {
  // TODO(crbug.com/428014205): Implement this function.
}

}  // namespace actor::ui
