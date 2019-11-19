// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/default_state.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_animation_types.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_state_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace_controller.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// This specifies how much percent (30%) of a window rect
// must be visible when the window is added to the workspace.
const float kMinimumPercentOnScreenArea = 0.3f;

// When a window that has restore bounds at least as large as a work area is
// unmaximized, inset the bounds slightly so that they are not exactly the same.
// This makes it easier to resize the window.
const int kMaximizedWindowInset = 10;  // DIPs.

constexpr char kSnapWindowSmoothnessHistogramName[] =
    "Ash.Window.AnimationSmoothness.Snap";

gfx::Size GetWindowMaximumSize(aura::Window* window) {
  return window->delegate() ? window->delegate()->GetMaximumSize()
                            : gfx::Size();
}

void MoveToDisplayForRestore(WindowState* window_state) {
  if (!window_state->HasRestoreBounds())
    return;
  const gfx::Rect restore_bounds = window_state->GetRestoreBoundsInScreen();

  // Move only if the restore bounds is outside of
  // the display. There is no information about in which
  // display it should be restored, so this is best guess.
  // TODO(oshima): Restore information should contain the
  // work area information like WindowResizer does for the
  // last window location.
  gfx::Rect display_area = display::Screen::GetScreen()
                               ->GetDisplayNearestWindow(window_state->window())
                               .bounds();

  if (!display_area.Intersects(restore_bounds)) {
    const display::Display& display =
        display::Screen::GetScreen()->GetDisplayMatching(restore_bounds);
    RootWindowController* new_root_controller =
        Shell::Get()->GetRootWindowControllerWithDisplayId(display.id());
    if (new_root_controller->GetRootWindow() !=
        window_state->window()->GetRootWindow()) {
      aura::Window* new_container =
          new_root_controller->GetRootWindow()->GetChildById(
              window_state->window()->parent()->id());
      new_container->AddChild(window_state->window());
    }
  }
}

}  // namespace

class ScopedMeasureBoundsAnimation {
 public:
  ScopedMeasureBoundsAnimation(WindowState* window_state,
                               const std::string& histogram_name)
      : window_state_(window_state) {
    window_state_->set_animation_smoothness_histogram_name(
        base::make_optional(histogram_name));
  }
  ~ScopedMeasureBoundsAnimation() {
    window_state_->set_animation_smoothness_histogram_name(base::nullopt);
  }

  ScopedMeasureBoundsAnimation(const ScopedMeasureBoundsAnimation& other) =
      delete;
  ScopedMeasureBoundsAnimation& operator=(
      const ScopedMeasureBoundsAnimation& rhs) = delete;

 private:
  WindowState* window_state_;
};

DefaultState::DefaultState(WindowStateType initial_state_type)
    : BaseState(initial_state_type), stored_window_state_(nullptr) {}

DefaultState::~DefaultState() = default;

void DefaultState::AttachState(WindowState* window_state,
                               WindowState::State* state_in_previous_mode) {
  DCHECK_EQ(stored_window_state_, window_state);

  // If previous state is unminimized but window state is minimized, sync window
  // state to unminimized.
  if (window_state->IsMinimized() &&
      !IsMinimizedWindowStateType(state_in_previous_mode->GetType())) {
    aura::Window* window = window_state->window();
    window->SetProperty(
        aura::client::kShowStateKey,
        window->GetProperty(aura::client::kPreMinimizedShowStateKey));
  }

  ReenterToCurrentState(window_state, state_in_previous_mode);

  // If the display has changed while in the another mode,
  // we need to let windows know the change.
  display::Display current_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          window_state->window());
  if (stored_display_state_.bounds() != current_display.bounds()) {
    const DisplayMetricsChangedWMEvent event(
        display::DisplayObserver::DISPLAY_METRIC_BOUNDS);
    window_state->OnWMEvent(&event);
  } else if (stored_display_state_.work_area() != current_display.work_area()) {
    const WMEvent event(WM_EVENT_WORKAREA_BOUNDS_CHANGED);
    window_state->OnWMEvent(&event);
  }
}

void DefaultState::DetachState(WindowState* window_state) {
  stored_window_state_ = window_state;
  stored_bounds_ = window_state->window()->bounds();
  stored_restore_bounds_ = window_state->HasRestoreBounds()
                               ? window_state->GetRestoreBoundsInParent()
                               : gfx::Rect();
  // Remember the display state so that in case of the display change
  // while in the other mode, we can perform necessary action to
  // restore the window state to the proper state for the current
  // display.
  stored_display_state_ = display::Screen::GetScreen()->GetDisplayNearestWindow(
      window_state->window());
}

void DefaultState::HandleWorkspaceEvents(WindowState* window_state,
                                         const WMEvent* event) {
  switch (event->type()) {
    case WM_EVENT_ADDED_TO_WORKSPACE: {
      // When a window is dragged and dropped onto a different
      // root window, the bounds will be updated after they are added
      // to the root window.
      // If a window is opened as maximized or fullscreen, its bounds may be
      // empty, so update the bounds now before checking empty.
      if (window_state->is_dragged() ||
          window_state->allow_set_bounds_direct() ||
          SetMaximizedOrFullscreenBounds(window_state)) {
        return;
      }

      aura::Window* window = window_state->window();
      gfx::Rect bounds = window->bounds();
      // When window is added to a workspace, |bounds| may be not the original
      // not-changed-by-user bounds, for example a resized bounds truncated by
      // available workarea.
      if (window_state->pre_added_to_workspace_window_bounds())
        bounds = *window_state->pre_added_to_workspace_window_bounds();

      // Don't adjust window bounds if the bounds are empty as this
      // happens when a new views::Widget is created.
      if (bounds.IsEmpty())
        return;

      // Only windows of type WINDOW_TYPE_NORMAL need to be adjusted to have
      // minimum visibility, because they are positioned by the user and the
      // user should always be able to interact with them. Other windows are
      // positioned programmatically.
      if (!window_state->IsUserPositionable())
        return;

      // Use entire display instead of workarea. The logic ensures 30%
      // visibility which should be enough to see where the window gets
      // moved.
      gfx::Rect display_area = screen_util::GetDisplayBoundsInParent(window);
      int min_width = bounds.width() * kMinimumPercentOnScreenArea;
      int min_height = bounds.height() * kMinimumPercentOnScreenArea;
      AdjustBoundsToEnsureWindowVisibility(display_area, min_width, min_height,
                                           &bounds);
      window_state->AdjustSnappedBounds(&bounds);
      if (window->bounds() != bounds)
        window_state->SetBoundsConstrained(bounds);
      return;
    }
    case WM_EVENT_DISPLAY_BOUNDS_CHANGED: {
      if (window_state->is_dragged() ||
          window_state->allow_set_bounds_direct() ||
          SetMaximizedOrFullscreenBounds(window_state)) {
        return;
      }
      gfx::Rect work_area_in_parent =
          screen_util::GetDisplayWorkAreaBoundsInParent(window_state->window());
      gfx::Rect bounds = window_state->window()->GetTargetBounds();
      // When display bounds has changed, make sure the entire window is fully
      // visible.
      bounds.AdjustToFit(work_area_in_parent);
      window_state->AdjustSnappedBounds(&bounds);
      if (window_state->window()->GetTargetBounds() != bounds)
        window_state->SetBoundsDirectAnimated(bounds);
      return;
    }
    case WM_EVENT_WORKAREA_BOUNDS_CHANGED: {
      // Don't resize the maximized window when the desktop is covered
      // by fullscreen window. crbug.com/504299.
      // TODO(afakhry): Decide whether we want the active desk's workspace, or
      // the workspace of the desk of `window_state->window()`.
      // For now use the active desk's.
      auto* workspace_controller =
          GetActiveWorkspaceController(window_state->window()->GetRootWindow());
      DCHECK(workspace_controller);
      bool in_fullscreen = workspace_controller->GetWindowState() ==
                           WorkspaceWindowState::kFullscreen;
      if (in_fullscreen && window_state->IsMaximized())
        return;

      if (window_state->is_dragged() ||
          window_state->allow_set_bounds_direct() ||
          SetMaximizedOrFullscreenBounds(window_state)) {
        return;
      }
      gfx::Rect work_area_in_parent =
          screen_util::GetDisplayWorkAreaBoundsInParent(window_state->window());
      gfx::Rect bounds = window_state->window()->GetTargetBounds();
      if (!::wm::GetTransientParent(window_state->window())) {
        AdjustBoundsToEnsureMinimumWindowVisibility(work_area_in_parent,
                                                    &bounds);
      }
      window_state->AdjustSnappedBounds(&bounds);
      if (window_state->window()->GetTargetBounds() != bounds)
        window_state->SetBoundsDirectAnimated(bounds);
      return;
    }
    case WM_EVENT_SYSTEM_UI_AREA_CHANGED:
      break;
    default:
      NOTREACHED() << "Unknown event:" << event->type();
  }
}

void DefaultState::HandleCompoundEvents(WindowState* window_state,
                                        const WMEvent* event) {
  aura::Window* window = window_state->window();

  switch (event->type()) {
    case WM_EVENT_TOGGLE_MAXIMIZE_CAPTION:
      if (window_state->IsFullscreen()) {
        const WMEvent event(WM_EVENT_TOGGLE_FULLSCREEN);
        window_state->OnWMEvent(&event);
      } else if (window_state->IsMaximized()) {
        window_state->Restore();
      } else if (window_state->IsNormalOrSnapped()) {
        if (window_state->CanMaximize())
          window_state->Maximize();
      }
      return;
    case WM_EVENT_TOGGLE_MAXIMIZE:
      if (window_state->IsFullscreen()) {
        const WMEvent event(WM_EVENT_TOGGLE_FULLSCREEN);
        window_state->OnWMEvent(&event);
      } else if (window_state->IsMaximized()) {
        window_state->Restore();
      } else if (window_state->CanMaximize()) {
        window_state->Maximize();
      }
      return;
    case WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE: {
      gfx::Rect work_area =
          screen_util::GetDisplayWorkAreaBoundsInParent(window);

      // Maximize vertically if:
      // - The window does not have a max height defined.
      // - The window has the normal state type. Snapped windows are excluded
      //   because they are already maximized vertically and reverting to the
      //   restored bounds looks weird.
      if (GetWindowMaximumSize(window).height() != 0 ||
          !window_state->IsNormalStateType()) {
        return;
      }
      if (window_state->HasRestoreBounds() &&
          (window->bounds().height() == work_area.height() &&
           window->bounds().y() == work_area.y())) {
        window_state->SetAndClearRestoreBounds();
      } else {
        window_state->SaveCurrentBoundsForRestore();
        window->SetBounds(gfx::Rect(window->bounds().x(), work_area.y(),
                                    window->bounds().width(),
                                    work_area.height()));
      }
      return;
    }
    case WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE: {
      // Maximize horizontally if:
      // - The window does not have a max width defined.
      // - The window is snapped or has the normal state type.
      if (GetWindowMaximumSize(window).width() != 0)
        return;
      if (!window_state->IsNormalOrSnapped())
        return;
      gfx::Rect work_area =
          screen_util::GetDisplayWorkAreaBoundsInParent(window);
      if (window_state->IsNormalStateType() &&
          window_state->HasRestoreBounds() &&
          (window->bounds().width() == work_area.width() &&
           window->bounds().x() == work_area.x())) {
        window_state->SetAndClearRestoreBounds();
      } else {
        gfx::Rect new_bounds(work_area.x(), window->bounds().y(),
                             work_area.width(), window->bounds().height());

        gfx::Rect restore_bounds = window->GetTargetBounds();
        if (window_state->IsSnapped()) {
          window_state->SetRestoreBoundsInParent(new_bounds);
          window_state->Restore();

          // The restore logic prevents a window from being restored to bounds
          // which match the workspace bounds exactly so it is necessary to set
          // the bounds again below.
        }

        window_state->SetRestoreBoundsInParent(restore_bounds);
        window->SetBounds(new_bounds);
      }
      return;
    }
    case WM_EVENT_TOGGLE_FULLSCREEN:
      ToggleFullScreen(window_state, window_state->delegate());
      return;
    case WM_EVENT_CYCLE_SNAP_LEFT:
    case WM_EVENT_CYCLE_SNAP_RIGHT:
      CycleSnap(window_state, event->type());
      return;
    default:
      NOTREACHED() << "Unknown event:" << event->type();
      break;
  }
}

void DefaultState::HandleBoundsEvents(WindowState* window_state,
                                      const WMEvent* event) {
  switch (event->type()) {
    case WM_EVENT_SET_BOUNDS: {
      const SetBoundsWMEvent* set_bounds_event =
          static_cast<const SetBoundsWMEvent*>(event);
      SetBounds(window_state, set_bounds_event);
    } break;
    case WM_EVENT_CENTER:
      CenterWindow(window_state);
      break;
    default:
      NOTREACHED() << "Unknown event:" << event->type();
      break;
  }
}

void DefaultState::HandleTransitionEvents(WindowState* window_state,
                                          const WMEvent* event) {
  WindowStateType current_state_type = window_state->GetStateType();
  WindowStateType next_state_type = GetStateForTransitionEvent(event);
  if (event->IsPinEvent()) {
    // If there already is a pinned window, it is not allowed to set it
    // to this window.
    // TODO(hidehiko): If a system modal window is openening, the pinning
    // probably should fail.
    if (Shell::Get()->screen_pinning_controller()->IsPinned()) {
      LOG(ERROR) << "An PIN event will be failed since another window is "
                 << "already in pinned mode.";
      next_state_type = current_state_type;
    }
  }

  if (next_state_type == current_state_type && window_state->IsSnapped()) {
    gfx::Rect snapped_bounds = GetSnappedWindowBoundsInParent(
        window_state->window(), event->type() == WM_EVENT_SNAP_LEFT
                                    ? WindowStateType::kLeftSnapped
                                    : WindowStateType::kRightSnapped);
    window_state->SetBoundsDirectAnimated(snapped_bounds);
    return;
  }

  if (event->type() == WM_EVENT_SNAP_LEFT ||
      event->type() == WM_EVENT_SNAP_RIGHT) {
    window_state->set_bounds_changed_by_user(true);
  }

  EnterToNextState(window_state, next_state_type);
}

// static
bool DefaultState::SetMaximizedOrFullscreenBounds(WindowState* window_state) {
  DCHECK(!window_state->is_dragged());
  DCHECK(!window_state->allow_set_bounds_direct());
  if (window_state->IsMaximized()) {
    window_state->SetBoundsDirect(
        screen_util::GetMaximizedWindowBoundsInParent(window_state->window()));
    return true;
  }
  if (window_state->IsFullscreen() || window_state->IsPinned()) {
    window_state->SetBoundsDirect(
        screen_util::GetFullscreenWindowBoundsInParent(window_state->window()));
    return true;
  }
  return false;
}

// static
void DefaultState::SetBounds(WindowState* window_state,
                             const SetBoundsWMEvent* event) {
  if (window_state->is_dragged() || window_state->allow_set_bounds_direct()) {
    if (event->animate()) {
      window_state->SetBoundsDirectAnimated(event->requested_bounds(),
                                            event->duration());
    } else {
      // TODO(oshima|varkha): Is this still needed? crbug.com/485612.
      window_state->SetBoundsDirect(event->requested_bounds());
    }
  } else if (!SetMaximizedOrFullscreenBounds(window_state)) {
    if (event->animate()) {
      window_state->SetBoundsDirectAnimated(event->requested_bounds(),
                                            event->duration());
    } else {
      window_state->SetBoundsConstrained(event->requested_bounds());
      // Update the restore size if the bounds is updated by PIP itself.
      if (window_state->IsPip() && window_state->HasRestoreBounds()) {
        gfx::Rect restore_bounds = window_state->GetRestoreBoundsInScreen();
        restore_bounds.set_size(
            window_state->window()->GetTargetBounds().size());
        window_state->SetRestoreBoundsInScreen(restore_bounds);
      }
    }
  }
}

void DefaultState::EnterToNextState(WindowState* window_state,
                                    WindowStateType next_state_type) {
  // Do nothing if  we're already in the same state.
  if (state_type_ == next_state_type)
    return;

  WindowStateType previous_state_type = state_type_;
  state_type_ = next_state_type;

  window_state->UpdateWindowPropertiesFromStateType();
  window_state->NotifyPreStateTypeChange(previous_state_type);

  // Don't update the window if the window is detached from parent.
  // This can happen during dragging.
  // TODO(oshima): This was added for DOCKED windows. Investigate if
  // we still need this.
  if (window_state->window()->parent()) {
    if (!window_state->HasRestoreBounds() &&
        (previous_state_type == WindowStateType::kDefault ||
         previous_state_type == WindowStateType::kNormal) &&
        !window_state->IsMinimized() && !window_state->IsNormalStateType()) {
      window_state->SaveCurrentBoundsForRestore();
    }

    // When restoring from a minimized state, we want to restore to the
    // previous bounds. However, we want to maintain the restore bounds.
    // (The restore bounds are set if a user maximized the window in one
    // axis by double clicking the window border for example).
    gfx::Rect restore_bounds_in_screen;
    if (previous_state_type == WindowStateType::kMinimized &&
        window_state->IsNormalStateType() && window_state->HasRestoreBounds() &&
        !window_state->unminimize_to_restore_bounds()) {
      restore_bounds_in_screen = window_state->GetRestoreBoundsInScreen();
      window_state->SaveCurrentBoundsForRestore();
    }

    if (window_state->IsMaximizedOrFullscreenOrPinned())
      MoveToDisplayForRestore(window_state);

    UpdateBoundsFromState(window_state, previous_state_type);
    UpdateMinimizedState(window_state, previous_state_type);

    // Normal state should have no restore bounds unless it's
    // unminimized.
    if (!restore_bounds_in_screen.IsEmpty())
      window_state->SetRestoreBoundsInScreen(restore_bounds_in_screen);
    else if (window_state->IsNormalStateType())
      window_state->ClearRestoreBounds();
  }
  window_state->NotifyPostStateTypeChange(previous_state_type);

  if (next_state_type == WindowStateType::kPinned ||
      previous_state_type == WindowStateType::kPinned ||
      next_state_type == WindowStateType::kTrustedPinned ||
      previous_state_type == WindowStateType::kTrustedPinned) {
    Shell::Get()->screen_pinning_controller()->SetPinnedWindow(
        window_state->window());
  }
}

void DefaultState::ReenterToCurrentState(
    WindowState* window_state,
    WindowState::State* state_in_previous_mode) {
  WindowStateType previous_state_type = state_in_previous_mode->GetType();

  // A state change should not move a window into or out of full screen or
  // pinned since these are "special mode" the user wanted to be in and
  // should be respected as such.
  if (previous_state_type == WindowStateType::kFullscreen ||
      previous_state_type == WindowStateType::kPinned ||
      previous_state_type == WindowStateType::kTrustedPinned) {
    state_type_ = previous_state_type;
  } else if (state_type_ == WindowStateType::kFullscreen ||
             state_type_ == WindowStateType::kPinned ||
             state_type_ == WindowStateType::kTrustedPinned) {
    state_type_ = previous_state_type;
  }

  window_state->UpdateWindowPropertiesFromStateType();
  window_state->NotifyPreStateTypeChange(previous_state_type);

  if ((state_type_ == WindowStateType::kNormal ||
       state_type_ == WindowStateType::kDefault) &&
      !stored_bounds_.IsEmpty()) {
    // Use the restore mechanism to set the bounds for
    // the window in normal state. This also covers unminimize case.
    window_state->SetRestoreBoundsInParent(stored_bounds_);
  }

  UpdateBoundsFromState(window_state, state_in_previous_mode->GetType());
  UpdateMinimizedState(window_state, state_in_previous_mode->GetType());

  // Then restore the restore bounds to their previous value.
  if (!stored_restore_bounds_.IsEmpty())
    window_state->SetRestoreBoundsInParent(stored_restore_bounds_);
  else
    window_state->ClearRestoreBounds();

  window_state->NotifyPostStateTypeChange(previous_state_type);
}

void DefaultState::UpdateBoundsFromState(WindowState* window_state,
                                         WindowStateType previous_state_type) {
  aura::Window* window = window_state->window();
  gfx::Rect bounds_in_parent;
  switch (state_type_) {
    case WindowStateType::kLeftSnapped:
    case WindowStateType::kRightSnapped:
      bounds_in_parent =
          GetSnappedWindowBoundsInParent(window_state->window(), state_type_);
      break;

    case WindowStateType::kDefault:
    case WindowStateType::kNormal: {
      gfx::Rect work_area_in_parent =
          screen_util::GetDisplayWorkAreaBoundsInParent(window);
      if (window_state->HasRestoreBounds()) {
        bounds_in_parent = window_state->GetRestoreBoundsInParent();
        // Check if the |window|'s restored size is bigger than the working area
        // This may happen if a window was resized to maximized bounds or if the
        // display resolution changed while the window was maximized.
        if (previous_state_type == WindowStateType::kMaximized &&
            bounds_in_parent.width() >= work_area_in_parent.width() &&
            bounds_in_parent.height() >= work_area_in_parent.height()) {
          bounds_in_parent = work_area_in_parent;
          bounds_in_parent.Inset(kMaximizedWindowInset, kMaximizedWindowInset,
                                 kMaximizedWindowInset, kMaximizedWindowInset);
        }
      } else {
        bounds_in_parent = window->bounds();
      }
      // Make sure that part of the window is always visible.
      if (!window_state->is_dragged()) {
        // Avoid doing this while the window is being dragged as its root
        // window hasn't been updated yet in the case of dragging to another
        // display. crbug.com/666836.
        AdjustBoundsToEnsureMinimumWindowVisibility(work_area_in_parent,
                                                    &bounds_in_parent);
      }
      break;
    }
    case WindowStateType::kMaximized:
      bounds_in_parent = screen_util::GetMaximizedWindowBoundsInParent(window);
      break;

    case WindowStateType::kFullscreen:
    case WindowStateType::kPinned:
    case WindowStateType::kTrustedPinned:
      bounds_in_parent = screen_util::GetFullscreenWindowBoundsInParent(window);
      break;

    case WindowStateType::kMinimized:
      break;
    case WindowStateType::kInactive:
    case WindowStateType::kAutoPositioned:
    case WindowStateType::kPip:
      return;
  }

  if (window_state->IsMinimized())
    return;

  if (IsMinimizedWindowStateType(previous_state_type) ||
      window_state->IsFullscreen() || window_state->IsPinned() ||
      window_state->bounds_animation_type() ==
          WindowState::BoundsChangeAnimationType::IMMEDIATE) {
    window_state->SetBoundsDirect(bounds_in_parent);
  } else if (window_state->IsMaximized() ||
             IsMaximizedOrFullscreenOrPinnedWindowStateType(
                 previous_state_type)) {
    window_state->SetBoundsDirectCrossFade(bounds_in_parent);
  } else if (window_state->is_dragged()) {
    // SetBoundsDirectAnimated does not work when the window gets reparented.
    // TODO(oshima): Consider fixing it and re-enable the animation.
    window_state->SetBoundsDirect(bounds_in_parent);
  } else {
    // Record smoothness of the snapping animation if the size of the window
    // changes.
    if (window_state->IsSnapped() &&
        bounds_in_parent.size() != window->bounds().size()) {
      ScopedMeasureBoundsAnimation scoped(window_state,
                                          kSnapWindowSmoothnessHistogramName);
      window_state->SetBoundsDirectAnimated(bounds_in_parent);
    } else {
      window_state->SetBoundsDirectAnimated(bounds_in_parent);
    }
  }
}

}  // namespace ash
