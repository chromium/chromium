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
  void MaybeShowToast(BrowserWindowInterface* bwi) override;

// TODO(crbug.com/424495020): Post-task icon refactor, look into removing these
// functions from AUSM.
#if BUILDFLAG(ENABLE_GLIC)
  void OnGlicUpdateFloatyState(glic::GlicWindowController::State floaty_state,
                               glic::mojom::CurrentView current_view) override;

  base::CallbackListSubscription RegisterTaskIconStateChange(
      TaskIconStateChangeCallback callback) override;
#endif

  // Returns the tabs associated with a given task id.
  std::vector<tabs::TabInterface*> GetTabs(TaskId id);

  // Returns the current task icon ui state.
  TaskIconUiState GetTaskIconUiState() const override;

 protected:
  TaskIconUiState task_icon_state_ = TaskIconUiState::kHidden;

 private:
  void MaybeNotifyProfileScopedUiComponents();
  void OnActorTaskStateChange(TaskId task_id, ActorTask::State new_task_state);

  // Returns completed tasks within the Completed Task Expiry Delay of the
  // `current_time`.
  std::vector<TaskId> GetCompletedTasks(base::Time current_time) const;


  base::OneShotTimer update_profile_scoped_ui_debounce_timer_;
  base::OneShotTimer completed_tasks_expiry_timer_;

  const raw_ref<ActorKeyedService> actor_service_;

// TODO(crbug.com/437161973): Refactor this repeating callback into 2 separate
// callbacks within the standalone task icon controller.
#if BUILDFLAG(ENABLE_GLIC)
  // Determines whether `suppress_task_icon_text_` is updated based on passed in
  // parameters.
  void UpdateTaskIconSuppressionOnFloatyStateChange(
      glic::GlicWindowController::State floaty_state,
      glic::mojom::CurrentView current_view);

  base::RepeatingCallbackList<void(
      ActorUiStateManagerInterface::TaskIconUiState,
      glic::GlicWindowController::State,
      glic::mojom::CurrentView)>
      task_icon_change_callback_list_;
  // Determines whether or not to suppress the task icon text.
  bool ShouldSuppressTaskIconText(
      glic::GlicWindowController::State floaty_state,
      glic::mojom::CurrentView view);

  // Cached value whether the task icon should be suppressed or not.
  bool suppress_task_icon_text_ = false;
#endif

  base::WeakPtrFactory<ActorUiStateManager> weak_factory_{this};
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_
