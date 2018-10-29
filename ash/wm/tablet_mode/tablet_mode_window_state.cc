// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_window_state.h"

#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_animation_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// Sets the restore bounds and show state overrides. These values take
// precedence over the restore bounds and restore show state (if set).
// If |bounds_override| is empty the values are cleared.
void SetWindowRestoreOverrides(aura::Window* window,
                               const gfx::Rect& bounds_override,
                               ui::WindowShowState window_state_override) {
  if (bounds_override.IsEmpty()) {
    window->ClearProperty(kRestoreWindowStateTypeOverrideKey);
    window->ClearProperty(kRestoreBoundsOverrideKey);
    return;
  }
  window->SetProperty(kRestoreWindowStateTypeOverrideKey,
                      ToWindowStateType(window_state_override));
  window->SetProperty(kRestoreBoundsOverrideKey,
                      new gfx::Rect(bounds_override));
}

// Returns the biggest possible size for a window which is about to be
// maximized.
gfx::Size GetMaximumSizeOfWindow(wm::WindowState* window_state) {
  DCHECK(window_state->CanMaximize() || window_state->CanResize());

  gfx::Size workspace_size =
      screen_util::GetMaximizedWindowBoundsInParent(window_state->window())
          .size();

  gfx::Size size = window_state->window()->delegate()
                       ? window_state->window()->delegate()->GetMaximumSize()
                       : gfx::Size();
  if (size.IsEmpty())
    return workspace_size;

  size.SetToMin(workspace_size);
  return size;
}

// Returns the centered bounds of the given bounds in the work area.
gfx::Rect GetCenteredBounds(const gfx::Rect& bounds_in_parent,
                            wm::WindowState* state_object) {
  gfx::Rect work_area_in_parent =
      screen_util::GetDisplayWorkAreaBoundsInParent(state_object->window());
  work_area_in_parent.ClampToCenteredSize(bounds_in_parent.size());
  return work_area_in_parent;
}

// Returns true if the window can snap in tablet mode.
bool CanSnap(wm::WindowState* window_state) {
  // If split view mode is not allowed in tablet mode, do not allow snapping
  // windows.
  if (!SplitViewController::ShouldAllowSplitView())
    return false;
  return window_state->CanSnap();
}

// Returns the maximized/full screen and/or centered bounds of a window.
gfx::Rect GetBoundsInMaximizedMode(wm::WindowState* state_object) {
  if (state_object->IsFullscreen() || state_object->IsPinned())
    return screen_util::GetDisplayBoundsInParent(state_object->window());

  if (state_object->GetStateType() == mojom::WindowStateType::LEFT_SNAPPED) {
    return Shell::Get()
        ->split_view_controller()
        ->GetSnappedWindowBoundsInParent(state_object->window(),
                                         SplitViewController::LEFT);
  }

  if (state_object->GetStateType() == mojom::WindowStateType::RIGHT_SNAPPED) {
    return Shell::Get()
        ->split_view_controller()
        ->GetSnappedWindowBoundsInParent(state_object->window(),
                                         SplitViewController::RIGHT);
  }

  gfx::Rect bounds_in_parent;
  // Make the window as big as possible.

  // Do no maximize the transient children, which should be stacked
  // above the transient parent.
  if ((state_object->CanMaximize() || state_object->CanResize()) &&
      ::wm::GetTransientParent(state_object->window()) == nullptr) {
    bounds_in_parent.set_size(GetMaximumSizeOfWindow(state_object));
  } else {
    // We prefer the user given window dimensions over the current windows
    // dimensions since they are likely to be the result from some other state
    // object logic.
    if (state_object->HasRestoreBounds())
      bounds_in_parent = state_object->GetRestoreBoundsInParent();
    else
      bounds_in_parent = state_object->window()->bounds();
  }
  return GetCenteredBounds(bounds_in_parent, state_object);
}

gfx::Rect GetRestoreBounds(wm::WindowState* window_state) {
  if (window_state->IsMinimized() || window_state->IsMaximized() ||
      window_state->IsFullscreen()) {
    gfx::Rect restore_bounds = window_state->GetRestoreBoundsInScreen();
    if (!restore_bounds.IsEmpty())
      return restore_bounds;
  }
  return window_state->window()->GetBoundsInScreen();
}

// Returns true if |window| is the source window of the current tab-dragging
// window.
bool IsTabDraggingSourceWindow(aura::Window* window) {
  if (!window)
    return false;

  MruWindowTracker::WindowList window_list =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList();
  if (window_list.empty())
    return false;

  // Find the window that's currently in tab-dragging process. There is at most
  // one such window.
  aura::Window* dragged_window = nullptr;
  for (auto* window : window_list) {
    if (wm::IsDraggingTabs(window)) {
      dragged_window = window;
      break;
    }
  }
  if (!dragged_window)
    return false;

  return dragged_window->GetProperty(ash::kTabDraggingSourceWindowKey) ==
         window;
}

}  // namespace

// static
void TabletModeWindowState::UpdateWindowPosition(wm::WindowState* window_state,
                                                 bool animate) {
  gfx::Rect bounds_in_parent = GetBoundsInMaximizedMode(window_state);
  if (bounds_in_parent == window_state->window()->GetTargetBounds())
    return;

  if (animate)
    window_state->SetBoundsDirectAnimated(bounds_in_parent);
  else
    window_state->SetBoundsDirect(bounds_in_parent);
}

TabletModeWindowState::TabletModeWindowState(aura::Window* window,
                                             TabletModeWindowManager* creator)
    : window_(window),
      creator_(creator),
      current_state_type_(wm::GetWindowState(window)->GetStateType()) {
  old_state_.reset(wm::GetWindowState(window)
                       ->SetStateObject(std::unique_ptr<State>(this))
                       .release());
}

TabletModeWindowState::~TabletModeWindowState() {
  creator_->WindowStateDestroyed(window_);
}

void TabletModeWindowState::LeaveTabletMode(wm::WindowState* window_state) {
  // Note: When we return we will destroy ourselves with the |our_reference|.
  std::unique_ptr<wm::WindowState::State> our_reference =
      window_state->SetStateObject(std::move(old_state_));
}

void TabletModeWindowState::SetDeferBoundsUpdates(bool defer_bounds_updates) {
  if (defer_bounds_updates_ == defer_bounds_updates)
    return;

  defer_bounds_updates_ = defer_bounds_updates;
  if (!defer_bounds_updates_)
    UpdateBounds(wm::GetWindowState(window_), true /* animated */);
}

void TabletModeWindowState::OnWMEvent(wm::WindowState* window_state,
                                      const wm::WMEvent* event) {
  // Ignore events that are sent during the exit transition.
  if (ignore_wm_events_) {
    return;
  }

  switch (event->type()) {
    case wm::WM_EVENT_TOGGLE_FULLSCREEN:
      ToggleFullScreen(window_state, window_state->delegate());
      break;
    case wm::WM_EVENT_FULLSCREEN:
      UpdateWindow(window_state, mojom::WindowStateType::FULLSCREEN,
                   true /* animated */);
      break;
    case wm::WM_EVENT_PIN:
      if (!Shell::Get()->screen_pinning_controller()->IsPinned())
        UpdateWindow(window_state, mojom::WindowStateType::PINNED,
                     true /* animated */);
      break;
    case wm::WM_EVENT_TRUSTED_PIN:
      if (!Shell::Get()->screen_pinning_controller()->IsPinned())
        UpdateWindow(window_state, mojom::WindowStateType::TRUSTED_PINNED,
                     true /* animated */);
      break;
    case wm::WM_EVENT_TOGGLE_MAXIMIZE_CAPTION:
    case wm::WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE:
    case wm::WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE:
    case wm::WM_EVENT_TOGGLE_MAXIMIZE:
    case wm::WM_EVENT_CYCLE_SNAP_LEFT:
    case wm::WM_EVENT_CYCLE_SNAP_RIGHT:
    case wm::WM_EVENT_CENTER:
    case wm::WM_EVENT_NORMAL:
    case wm::WM_EVENT_MAXIMIZE:
      UpdateWindow(window_state, GetMaximizedOrCenteredWindowType(window_state),
                   true /* animated */);
      return;
    case wm::WM_EVENT_SNAP_LEFT:
      // Set bounds_changed_by_user to true to avoid WindowPositioner to auto
      // place the window.
      window_state->set_bounds_changed_by_user(true);
      UpdateWindow(window_state,
                   GetSnappedWindowStateType(
                       window_state, mojom::WindowStateType::LEFT_SNAPPED),
                   false /* animated */);
      return;
    case wm::WM_EVENT_SNAP_RIGHT:
      // Set bounds_changed_by_user to true to avoid WindowPositioner to auto
      // place the window.
      window_state->set_bounds_changed_by_user(true);
      UpdateWindow(window_state,
                   GetSnappedWindowStateType(
                       window_state, mojom::WindowStateType::RIGHT_SNAPPED),
                   false /* animated */);
      return;
    case wm::WM_EVENT_MINIMIZE:
      UpdateWindow(window_state, mojom::WindowStateType::MINIMIZED,
                   true /* animated */);
      return;
    case wm::WM_EVENT_SHOW_INACTIVE:
    case wm::WM_EVENT_SYSTEM_UI_AREA_CHANGED:
      return;
    case wm::WM_EVENT_SET_BOUNDS: {
      gfx::Rect bounds_in_parent =
          (static_cast<const wm::SetBoundsEvent*>(event))->requested_bounds();
      if (bounds_in_parent.IsEmpty())
        return;

      if (wm::IsDraggingTabs(window_state->window()) ||
          IsTabDraggingSourceWindow(window_state->window())) {
        // If the window is the current tab-dragged window or the current tab-
        // dragged window's source window, we may need to update its bounds
        // during dragging.
        window_state->SetBoundsDirect(bounds_in_parent);
      } else if (current_state_type_ == mojom::WindowStateType::MAXIMIZED) {
        // Having a maximized window, it could have been created with an empty
        // size and the caller should get his size upon leaving the maximized
        // mode. As such we set the restore bounds to the requested bounds.
        window_state->SetRestoreBoundsInParent(bounds_in_parent);
      } else if (current_state_type_ != mojom::WindowStateType::MINIMIZED &&
                 current_state_type_ != mojom::WindowStateType::FULLSCREEN &&
                 current_state_type_ != mojom::WindowStateType::PINNED &&
                 current_state_type_ !=
                     mojom::WindowStateType::TRUSTED_PINNED &&
                 current_state_type_ != mojom::WindowStateType::LEFT_SNAPPED &&
                 current_state_type_ != mojom::WindowStateType::RIGHT_SNAPPED) {
        // In all other cases (except for minimized windows) we respect the
        // requested bounds and center it to a fully visible area on the screen.
        bounds_in_parent = GetCenteredBounds(bounds_in_parent, window_state);
        if (bounds_in_parent != window_state->window()->bounds()) {
          const wm::SetBoundsEvent* bounds_event =
              static_cast<const wm::SetBoundsEvent*>(event);
          if (window_state->window()->IsVisible() && bounds_event->animate())
            window_state->SetBoundsDirectAnimated(bounds_in_parent);
          else
            window_state->SetBoundsDirect(bounds_in_parent);
        }
      }
      break;
    }
    case wm::WM_EVENT_ADDED_TO_WORKSPACE:
      if (current_state_type_ != mojom::WindowStateType::MAXIMIZED &&
          current_state_type_ != mojom::WindowStateType::FULLSCREEN &&
          current_state_type_ != mojom::WindowStateType::MINIMIZED) {
        mojom::WindowStateType new_state =
            GetMaximizedOrCenteredWindowType(window_state);
        UpdateWindow(window_state, new_state, true /* animated */);
      }
      break;
    case wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED:
      if (current_state_type_ != mojom::WindowStateType::MINIMIZED)
        UpdateBounds(window_state, true /* animated */);
      break;
    case wm::WM_EVENT_DISPLAY_BOUNDS_CHANGED:
      // Don't animate on a screen rotation - just snap to new size.
      if (current_state_type_ != mojom::WindowStateType::MINIMIZED)
        UpdateBounds(window_state, false /* animated */);
      break;
  }
}

mojom::WindowStateType TabletModeWindowState::GetType() const {
  return current_state_type_;
}

void TabletModeWindowState::AttachState(
    wm::WindowState* window_state,
    wm::WindowState::State* previous_state) {
  current_state_type_ = previous_state->GetType();

  gfx::Rect restore_bounds = GetRestoreBounds(window_state);
  if (!restore_bounds.IsEmpty()) {
    // We do not want to do a session restore to our window states. Therefore
    // we tell the window to use the current default states instead.
    SetWindowRestoreOverrides(window_state->window(), restore_bounds,
                              window_state->GetShowState());
  }

  // Initialize the state to a good preset.
  if (current_state_type_ != mojom::WindowStateType::MAXIMIZED &&
      current_state_type_ != mojom::WindowStateType::MINIMIZED &&
      current_state_type_ != mojom::WindowStateType::FULLSCREEN &&
      current_state_type_ != mojom::WindowStateType::PINNED &&
      current_state_type_ != mojom::WindowStateType::TRUSTED_PINNED) {
    UpdateWindow(window_state, GetMaximizedOrCenteredWindowType(window_state),
                 true /* animated */);
  }
}

void TabletModeWindowState::DetachState(wm::WindowState* window_state) {
  // From now on, we can use the default session restore mechanism again.
  SetWindowRestoreOverrides(window_state->window(), gfx::Rect(),
                            ui::SHOW_STATE_NORMAL);
}

void TabletModeWindowState::UpdateWindow(wm::WindowState* window_state,
                                         mojom::WindowStateType target_state,
                                         bool animated) {
  DCHECK(target_state == mojom::WindowStateType::MINIMIZED ||
         target_state == mojom::WindowStateType::MAXIMIZED ||
         target_state == mojom::WindowStateType::PINNED ||
         target_state == mojom::WindowStateType::TRUSTED_PINNED ||
         (target_state == mojom::WindowStateType::NORMAL &&
          (!window_state->CanMaximize() ||
           !!::wm::GetTransientParent(window_state->window()))) ||
         target_state == mojom::WindowStateType::FULLSCREEN ||
         target_state == mojom::WindowStateType::LEFT_SNAPPED ||
         target_state == mojom::WindowStateType::RIGHT_SNAPPED);

  if (current_state_type_ == target_state) {
    if (target_state == mojom::WindowStateType::MINIMIZED)
      return;
    // If the state type did not change, update it accordingly.
    UpdateBounds(window_state, animated);
    return;
  }

  const mojom::WindowStateType old_state_type = current_state_type_;
  current_state_type_ = target_state;
  window_state->UpdateWindowPropertiesFromStateType();
  window_state->NotifyPreStateTypeChange(old_state_type);

  if (target_state == mojom::WindowStateType::MINIMIZED) {
    ::wm::SetWindowVisibilityAnimationType(
        window_state->window(), wm::WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE);
    window_state->window()->Hide();
    if (window_state->IsActive())
      window_state->Deactivate();
  } else {
    UpdateBounds(window_state, animated);
  }

  if ((window_state->window()->layer()->GetTargetVisibility() ||
       old_state_type == mojom::WindowStateType::MINIMIZED) &&
      !window_state->window()->layer()->visible()) {
    // The layer may be hidden if the window was previously minimized. Make
    // sure it's visible.
    window_state->window()->Show();
  }

  window_state->NotifyPostStateTypeChange(old_state_type);

  if (old_state_type == mojom::WindowStateType::PINNED ||
      target_state == mojom::WindowStateType::PINNED ||
      old_state_type == mojom::WindowStateType::TRUSTED_PINNED ||
      target_state == mojom::WindowStateType::TRUSTED_PINNED) {
    Shell::Get()->screen_pinning_controller()->SetPinnedWindow(
        window_state->window());
  }
}

mojom::WindowStateType TabletModeWindowState::GetMaximizedOrCenteredWindowType(
    wm::WindowState* window_state) {
  return (window_state->CanMaximize() &&
          ::wm::GetTransientParent(window_state->window()) == nullptr)
             ? mojom::WindowStateType::MAXIMIZED
             : mojom::WindowStateType::NORMAL;
}

mojom::WindowStateType TabletModeWindowState::GetSnappedWindowStateType(
    wm::WindowState* window_state,
    mojom::WindowStateType target_state) {
  DCHECK(target_state == mojom::WindowStateType::LEFT_SNAPPED ||
         target_state == mojom::WindowStateType::RIGHT_SNAPPED);
  return CanSnap(window_state) ? target_state
                               : GetMaximizedOrCenteredWindowType(window_state);
}

void TabletModeWindowState::UpdateBounds(wm::WindowState* window_state,
                                         bool animated) {
  if (defer_bounds_updates_)
    return;

  // Do not update window's bounds if it's in tab-dragging process. The bounds
  // will be updated later when the drag ends.
  if (wm::IsDraggingTabs(window_state->window()))
    return;

  // Do not update minimized windows bounds until it was unminimized.
  if (current_state_type_ == mojom::WindowStateType::MINIMIZED)
    return;

  gfx::Rect bounds_in_parent = GetBoundsInMaximizedMode(window_state);
  // If we have a target bounds rectangle, we center it and set it
  // accordingly.
  if (!bounds_in_parent.IsEmpty() &&
      bounds_in_parent != window_state->window()->bounds()) {
    if (!window_state->window()->IsVisible() || !animated) {
      window_state->SetBoundsDirect(bounds_in_parent);
    } else {
      if (use_zero_animation_type_) {
        window_state->SetBoundsDirectCrossFade(bounds_in_parent,
                                               gfx::Tween::ZERO);
        return;
      }
      // If we animate (to) tablet mode, we want to use the cross fade to
      // avoid flashing.
      if (window_state->IsMaximized())
        window_state->SetBoundsDirectCrossFade(bounds_in_parent);
      else if (window_state->IsSnapped())
        window_state->SetBoundsDirect(bounds_in_parent);
      else
        window_state->SetBoundsDirectAnimated(bounds_in_parent);
    }
  }
}

}  // namespace ash
