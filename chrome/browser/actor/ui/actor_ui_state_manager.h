// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_

#include "base/timer/timer.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/widget/glic_window_controller.h"
#endif

namespace tabs {
class TabInterface;
}

namespace actor::ui {
class ActorUiStateManager : public ActorUiStateManagerInterface {
 public:
  explicit ActorUiStateManager(ActorKeyedService& actor_service);
  ~ActorUiStateManager() override;

  // ActorUiStateManagerInterface:
  void OnUiEvent(AsyncUiEvent event, UiCompleteCallback callback) override;
  void OnUiEvent(SyncUiEvent event) override;
  ActorUiTabControllerInterface* GetUiTabController(
      tabs::TabInterface* tab) override;

// TODO(crbug.com/424495020): Post-task icon refactor, look into removing these
// functions from AUSM.
#if BUILDFLAG(ENABLE_GLIC)
  void OnGlicUpdateFloatyState(glic::GlicWindowController::State floaty_state,
                               BrowserWindowInterface* bwi) override;

  base::CallbackListSubscription RegisterFloatyTaskStateChange(
      FloatyTaskStateChangeCallback callback) override;
#endif

  // Returns the tabs associated with a given task id.
  std::vector<tabs::TabInterface*> GetTabs(TaskId id);

  // Returns the current profile scoped ui state.
  UiState GetUiState() const;

 protected:
  UiState state_ = UiState::kInactive;

 private:
  void MaybeUpdateProfileScopedUiState();
  void OnActorTaskStateChange(TaskId task_id, ActorTask::State new_task_state);

  // Returns completed tasks within the kCompletedTaskExpiryDelay of the
  // `current_time`.
  std::vector<TaskId> GetCompletedTasks(base::Time current_time) const;

  // Shows toast that notifies user the Actor is working in the background.
  // Shows a maximum of kToastShownMax per profile.
  void MaybeShowToast(BrowserWindowInterface* bwi);

  base::OneShotTimer update_profile_scoped_ui_debounce_timer_;
  base::OneShotTimer completed_tasks_expiry_timer_;

  const raw_ref<ActorKeyedService> actor_service_;

#if BUILDFLAG(ENABLE_GLIC)
  using FloatyTaskStateChangeCallbackList =
      base::RepeatingCallbackList<void(ActorUiStateManagerInterface::UiState,
                                       glic::GlicWindowController::State)>;
  FloatyTaskStateChangeCallbackList floaty_task_state_change_callback_list_;
#endif

  base::WeakPtrFactory<ActorUiStateManager> weak_factory_{this};
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_
