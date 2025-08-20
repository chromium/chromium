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

static constexpr base::TimeDelta kProfileScopedUiUpdateDebounceDelay =
    base::Milliseconds(500);

class ActorUiStateManagerInterface {
 public:
  // TODO(crbug.com/437161973): Port this over to the dedicated TaskIcon keyed
  // service class and add active/inactive (highlight status) states once the
  // TaskIcon is refactored. We will revisit introducing the old ActorTaskState
  // in the refactor.
  enum class TaskIconUiState {
    // The task icon is not visible.
    kHidden,
    // The task icon is visible with default text.
    kShown,
    // The task icon shows `needs attention` text, if this is set the task
    // icon is expected to be visible.
    kNeedsAttention,
    // The task icon shows `complete tasks` text, if this is set the task
    // icon is expected to be visible.
    kCompleteTasks,
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

  // Shows toast that notifies user the Actor is working in the background.
  // Shows a maximum of kToastShownMax per profile.
  virtual void MaybeShowToast(BrowserWindowInterface* bwi) = 0;

  // Returns the current UI state.
  virtual TaskIconUiState GetTaskIconUiState() const = 0;

#if BUILDFLAG(ENABLE_GLIC)
  // Called on glic window (floaty) state change OR view change.
  virtual void OnGlicUpdateFloatyState(
      glic::GlicWindowController::State floaty_state,
      glic::mojom::CurrentView current_view) = 0;

  // Register for this callback to detect changes to the glic floaty status and
  // TaskIconUiState.
  using TaskIconStateChangeCallback = base::RepeatingCallback<void(
      ActorUiStateManagerInterface::TaskIconUiState,
      glic::GlicWindowController::State,
      glic::mojom::CurrentView)>;
  virtual base::CallbackListSubscription RegisterTaskIconStateChange(
      TaskIconStateChangeCallback callback) = 0;
#endif
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_INTERFACE_H_
