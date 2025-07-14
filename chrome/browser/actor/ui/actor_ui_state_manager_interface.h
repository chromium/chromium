// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_INTERFACE_H_

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/ui_event.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/buildflags.h"
#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/widget/glic_window_controller.h"
#endif

namespace actor::ui {
using UiCompleteCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;

// ExpiryPeriod from when the user completes a task and when it should no longer
// show on the ui
// TODO(crbug.com/428014205): This is a placeholder value atm.
static constexpr base::TimeDelta kCompletedTaskExpiryDelay = base::Minutes(3);
static constexpr base::TimeDelta kProfileScopedUiUpdateDebounceDelay =
    base::Milliseconds(500);

class ActorUiStateManagerInterface {
 public:
  // TODO(crbug.com/428014205): Once UX is determined for multiple tasks, states
  // here may change.
  enum class UiState {
    // There are no active agent tasks on this profile.
    kInactive,
    // There are active agent tasks on this profile.
    kActive,
    // There are agent tasks that need attention, this includes agent pause &
    // completed tasks within the kCompletedTaskExpiryDelay.
    kCheckTasks,
  };
  virtual ~ActorUiStateManagerInterface() = default;

  // Called whenever an actor task state changes.
  virtual void OnActorTaskStateChange(TaskId task_id,
                                      ActorTask::State task_state) = 0;

  // Called whenever a ui event occurs.
  virtual void OnUiEvent(AsyncUiEvent event, UiCompleteCallback callback) = 0;
  virtual void OnUiEvent(SyncUiEvent event) = 0;

  // Notifies the ActorUiTabController of a new `ui_tab_state`.
  // Can be stubbed out to do nothing in tests.
  virtual void NotifyUiTabController(tabs::TabInterface& tab,
                                     const UiTabState& ui_tab_state) = 0;

#if BUILDFLAG(ENABLE_GLIC)
  // Called on glic window (floaty) state change.
  virtual void OnGlicUpdateFloatyState(
      glic::GlicWindowController::State floaty_state) = 0;
#endif
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_INTERFACE_H_
