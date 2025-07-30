// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_INTERFACE_H_

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/actor/ui/ui_event.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/buildflags.h"
#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/widget/glic_window_controller.h"
#endif

namespace actor::ui {
using UiCompleteCallback =
    base::OnceCallback<void(::actor::mojom::ActionResultPtr)>;

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
    // There are no active Actor tasks on this profile.
    kInactive,
    // There are active Actor tasks on this profile.
    kActive,
    // There are Actor tasks that need attention, this includes Actor pause &
    // completed tasks within the kCompletedTaskExpiryDelay.
    kCheckTasks,
  };
  virtual ~ActorUiStateManagerInterface() = default;

  // Handles a UiEvent that may be processed asynchronously.
  virtual void OnUiEvent(AsyncUiEvent event, UiCompleteCallback callback) = 0;
  // Handles a UiEvent that must be processed synchronously.
  virtual void OnUiEvent(SyncUiEvent event) = 0;

  // Gets the relevant UiTabController if the `tab`
  // exists. Can be stubbed out to do nothing in tests.
  virtual ActorUiTabControllerInterface* GetUiTabController(
      tabs::TabInterface* tab) = 0;

#if BUILDFLAG(ENABLE_GLIC)
  // Called on glic window (floaty) state change. Receives new state and the
  // last active window before the floaty became active.
  virtual void OnGlicUpdateFloatyState(
      glic::GlicWindowController::State floaty_state,
      BrowserWindowInterface* bwi) = 0;

  // Register for this callback to detect changes to the glic floaty status and
  // UiState.
  using FloatyTaskStateChangeCallback =
      base::RepeatingCallback<void(ActorUiStateManagerInterface::UiState,
                                   glic::GlicWindowController::State)>;
  virtual base::CallbackListSubscription RegisterFloatyTaskStateChange(
      FloatyTaskStateChangeCallback callback) = 0;
#endif
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_INTERFACE_H_
