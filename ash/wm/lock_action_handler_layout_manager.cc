// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_action_handler_layout_manager.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/mojom/tray_action.mojom.h"
#include "ash/shell.h"
#include "ash/wm/lock_window_state.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/functional/bind.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

// Whether child windows should be shown depending on lock screen note action
// state and lock screen action background state.
// This should not be used for lock screen background windows.
bool ShowChildWindows(mojom::TrayActionState action_state,
                      LockScreenActionBackgroundState background_state) {
  return (action_state == mojom::TrayActionState::kActive ||
          action_state == mojom::TrayActionState::kLaunching) &&
         (background_state == LockScreenActionBackgroundState::kShown ||
          background_state == LockScreenActionBackgroundState::kHidden);
}

}  // namespace

LockActionHandlerLayoutManager::LockActionHandlerLayoutManager(
    aura::Window* window,
    LockScreenActionBackgroundController* action_background_controller)
    : LockLayoutManager(window),
      action_background_controller_(action_background_controller) {
  TrayAction* tray_action = Shell::Get()->tray_action();
  tray_action_observation_.Observe(tray_action);
  action_background_observation_.Observe(action_background_controller_.get());
}

LockActionHandlerLayoutManager::~LockActionHandlerLayoutManager() = default;

void LockActionHandlerLayoutManager::OnWindowAddedToLayout(
    aura::Window* child) {
  wm::SetWindowVisibilityAnimationTransition(child, wm::ANIMATE_NONE);

  // The lock action background should be shown behind the shelf (which is
  // transparent on the lock screen), unlike lock action handler windows.
  const bool shelf_excluded =
      !action_background_controller_->IsBackgroundWindow(child);
  WindowState* window_state =
      LockWindowState::SetLockWindowState(child, shelf_excluded);
  WMEvent event(WM_EVENT_ADDED_TO_WORKSPACE);
  window_state->OnWMEvent(&event);
}

void LockActionHandlerLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
  if (action_background_controller_->IsBackgroundWindow(child)) {
    window()->StackChildAtBottom(child);
    return;
  }

  // Windows should be shown only in active state.
  if (visible &&
      !ShowChildWindows(Shell::Get()->tray_action()->GetLockScreenNoteState(),
                        action_background_controller_->state())) {
    child->Hide();
  }
}

void LockActionHandlerLayoutManager::OnLockScreenNoteStateChanged(
    mojom::TrayActionState state) {
  // Update the background controller state first.
  bool background_changed = false;
  switch (state) {
    case mojom::TrayActionState::kNotAvailable:
      background_changed =
          action_background_controller_->HideBackgroundImmediately();
      break;
    case mojom::TrayActionState::kAvailable:
      background_changed = action_background_controller_->HideBackground();
      break;
    case mojom::TrayActionState::kLaunching:
    case mojom::TrayActionState::kActive:
      background_changed = action_background_controller_->ShowBackground();
      break;
  }

  // Given that background state changes invoke the background controller
  // observers (one of which is |this|), and |UpdateChildren| is called as part
  // of handling background state changes, the child windows state has alreday
  // been updated if |background_changed| is true - no need to do it again.
  if (background_changed)
    return;

  UpdateChildren(state, action_background_controller_->state());
}

void LockActionHandlerLayoutManager::OnLockScreenActionBackgroundStateChanged(
    LockScreenActionBackgroundState state) {
  UpdateChildren(Shell::Get()->tray_action()->GetLockScreenNoteState(), state);
}

void LockActionHandlerLayoutManager::UpdateChildren(
    mojom::TrayActionState action_state,
    LockScreenActionBackgroundState background_state) {
  // Update children state:
  // * a child can be visible only in active state
  // * on transition to active state:
  //     * show hidden windows, so children that were added when action was not
  //       in active state are shown
  //     * activate a container child to ensure the container gets focus when
  //       moving from background state.
  bool show_children = ShowChildWindows(action_state, background_state);
  aura::Window* child_to_activate = nullptr;
  for (aura::Window* child : window()->children()) {
    if (action_background_controller_->IsBackgroundWindow(child))
      continue;
    if (show_children) {
      child->Show();
      child_to_activate = child;
    } else {
      child->Hide();
    }
  }

  if (child_to_activate)
    wm::ActivateWindow(child_to_activate);
}

}  // namespace ash
