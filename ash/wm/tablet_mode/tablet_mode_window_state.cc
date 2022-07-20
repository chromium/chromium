// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_window_state.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/drag_drop/tab_drag_drop_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_animation_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/ime_util_chromeos.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

using ::chromeos::WindowStateType;

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
                      chromeos::ToWindowStateType(window_state_override));
  window->SetProperty(kRestoreBoundsOverrideKey,
                      new gfx::Rect(bounds_override));
}

// Returns the biggest possible size for a window which is about to be
// maximized.
gfx::Size GetMaximumSizeOfWindow(WindowState* window_state) {
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
                            WindowState* state_object) {
  gfx::Rect work_area_in_parent =
      screen_util::GetDisplayWorkAreaBoundsInParent(state_object->window());
  work_area_in_parent.ClampToCenteredSize(bounds_in_parent.size());
  return work_area_in_parent;
}

// Returns the maximized/full screen and/or centered bounds of a window.
gfx::Rect GetBoundsInTabletMode(WindowState* state_object) {
  aura::Window* window = state_object->window();

  if (state_object->IsFullscreen() || state_object->IsPinned())
    return screen_util::GetFullscreenWindowBoundsInParent(window);

  if (state_object->GetStateType() == WindowStateType::kPrimarySnapped) {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow())
        ->GetSnappedWindowBoundsInParent(SplitViewController::LEFT, window);
  }

  if (state_object->GetStateType() == WindowStateType::kSecondarySnapped) {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow())
        ->GetSnappedWindowBoundsInParent(SplitViewController::RIGHT, window);
  }

  if (chromeos::wm::features::IsFloatWindowEnabled() &&
      state_object->IsFloated()) {
    return Shell::Get()
        ->float_controller()
        ->GetPreferredFloatWindowTabletBounds(window);
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

gfx::Rect GetRestoreBounds(WindowState* window_state) {
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
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  if (window_list.empty())
    return false;

  // Find the window that's currently in tab-dragging process. There is at most
  // one such window.
  aura::Window* dragged_window = nullptr;
  for (auto* window : window_list) {
    if (window_util::IsDraggingTabs(window)) {
      dragged_window = window;
      break;
    }
  }
  if (!dragged_window)
    return false;

  return dragged_window->GetProperty(ash::kTabDraggingSourceWindowKey) ==
         window;
}

// True if |window| is the top window in BuildWindowForCycleList.
bool IsTopWindow(aura::Window* window) {
  DCHECK(window);
  return window == window_util::GetTopWindow();
}

bool IsSnapped(WindowStateType state) {
  return state == WindowStateType::kPrimarySnapped ||
         state == WindowStateType::kSecondarySnapped;
}

// Returns true if the bounds change of |window| is from VK request and can be
// allowed by the current window's state.
bool BoundsChangeIsFromVKAndAllowed(aura::Window* window) {
  WindowStateType state_type = WindowState::Get(window)->GetStateType();
  if (state_type == WindowStateType::kNormal ||
      state_type == WindowStateType::kDefault) {
    return window->GetProperty(wm::kVirtualKeyboardRestoreBoundsKey) != nullptr;
  }

  if (state_type == WindowStateType::kPrimarySnapped ||
      state_type == WindowStateType::kSecondarySnapped) {
    return SplitViewController::Get(window)->BoundsChangeIsFromVKAndAllowed(
        window);
  }
  return false;
}

}  // namespace

// static
void TabletModeWindowState::UpdateWindowPosition(WindowState* window_state,
                                                 bool animate) {
  gfx::Rect bounds_in_parent = GetBoundsInTabletMode(window_state);
  if (bounds_in_parent == window_state->window()->GetTargetBounds())
    return;

  if (animate)
    window_state->SetBoundsDirectAnimated(bounds_in_parent);
  else
    window_state->SetBoundsDirect(bounds_in_parent);
}

TabletModeWindowState::TabletModeWindowState(aura::Window* window,
                                             TabletModeWindowManager* creator,
                                             bool snap,
                                             bool animate_bounds_on_attach,
                                             bool entering_tablet_mode)
    : window_(window),
      creator_(creator),
      animate_bounds_on_attach_(animate_bounds_on_attach) {
  WindowState* state = WindowState::Get(window);
  current_state_type_ = state->GetStateType();
  DCHECK(!snap || SplitViewController::Get(Shell::GetPrimaryRootWindow())
                      ->CanSnapWindow(window));

  // Snapped and floated windows maintain their state; other windows become
  // maximized if possible, centered with a backdrop if not possible.
  state_type_on_attach_ = snap || state->IsFloated()
                              ? current_state_type_
                              : state->GetMaximizedOrCenteredWindowType();
  // TODO(oshima|sammiequon): consider SplitView scenario.
  WindowState::ScopedBoundsChangeAnimation bounds_animation(
      window, entering_tablet_mode && !IsTopWindow(window)
                  ? WindowState::BoundsChangeAnimationType::STEP_END
                  : WindowState::BoundsChangeAnimationType::DEFAULT);
  old_window_bounds_in_screen_ = window->GetBoundsInScreen();
  old_state_.reset(
      state->SetStateObject(std::unique_ptr<State>(this)).release());
}

TabletModeWindowState::~TabletModeWindowState() {
  creator_->WindowStateDestroyed(window_);
}

void TabletModeWindowState::LeaveTabletMode(WindowState* window_state,
                                            bool was_in_overview) {
  // Only do bounds change animation if the window was showing in overview,
  // or the top window or a window showing in splitview before leaving tablet
  // mode, and the window has changed its state. Otherwise, restore its bounds
  // immediately.
  WindowState::BoundsChangeAnimationType animation_type =
      was_in_overview || window_state->IsSnapped() ||
              IsTopWindow(window_state->window())
          ? WindowState::BoundsChangeAnimationType::DEFAULT
          : WindowState::BoundsChangeAnimationType::IMMEDIATE;
  if (old_state_->GetType() == window_state->GetStateType() &&
      !window_state->IsNormalStateType()) {
    animation_type = WindowState::BoundsChangeAnimationType::IMMEDIATE;
  }

  // Note: When we return we will destroy ourselves with the |our_reference|.
  WindowState::ScopedBoundsChangeAnimation bounds_animation(
      window_state->window(), animation_type);
  std::unique_ptr<WindowState::State> our_reference =
      window_state->SetStateObject(std::move(old_state_));
}

void TabletModeWindowState::OnWMEvent(WindowState* window_state,
                                      const WMEvent* event) {
  // Ignore events that are sent during the exit transition.
  if (ignore_wm_events_) {
    return;
  }

  switch (event->type()) {
    case WM_EVENT_TOGGLE_FULLSCREEN:
      ToggleFullScreen(window_state, window_state->delegate());
      break;
    case WM_EVENT_FULLSCREEN:
      UpdateWindow(window_state, WindowStateType::kFullscreen,
                   true /* animated */);
      break;
    case WM_EVENT_PIN:
      if (!Shell::Get()->screen_pinning_controller()->IsPinned())
        UpdateWindow(window_state, WindowStateType::kPinned,
                     true /* animated */);
      break;
    case WM_EVENT_PIP:
      if (!window_state->IsPip()) {
        UpdateWindow(window_state, WindowStateType::kPip, true /* animated */);
      }
      break;
    case WM_EVENT_TRUSTED_PIN:
      if (!Shell::Get()->screen_pinning_controller()->IsPinned())
        UpdateWindow(window_state, WindowStateType::kTrustedPinned,
                     true /* animated */);
      break;
    case WM_EVENT_TOGGLE_MAXIMIZE_CAPTION:
    case WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE:
    case WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE:
    case WM_EVENT_TOGGLE_MAXIMIZE:
    case WM_EVENT_CENTER:
    case WM_EVENT_NORMAL:
    case WM_EVENT_MAXIMIZE:
      UpdateWindow(window_state,
                   window_state->GetMaximizedOrCenteredWindowType(),
                   true /* animated */);
      return;
    case WM_EVENT_RESTORE: {
      // We special handle WM_EVENT_RESTORE event here.
      WindowStateType restore_state = window_state->GetRestoreWindowState();
      if (restore_state == WindowStateType::kPrimarySnapped) {
        window_state->set_snap_action_source(
            WindowSnapActionSource::kSnapByWindowStateRestore);
        DoTabletSnap(window_state, WM_EVENT_SNAP_PRIMARY);
      } else if (restore_state == WindowStateType::kSecondarySnapped) {
        window_state->set_snap_action_source(
            WindowSnapActionSource::kSnapByWindowStateRestore);
        DoTabletSnap(window_state, WM_EVENT_SNAP_SECONDARY);
      } else {
        UpdateWindow(window_state, restore_state, /*animate=*/true);
      }
      break;
    }
    case WM_EVENT_FLOAT:
      // Not all windows can be floated.
      if (!FloatController::CanFloatWindowInTablet(window_state->window()))
        return;

      UpdateWindow(window_state, WindowStateType::kFloated,
                   /*=animated=*/true);
      break;
    case WM_EVENT_SNAP_PRIMARY:
    case WM_EVENT_SNAP_SECONDARY:
      DoTabletSnap(window_state, event->type());
      return;
    case WM_EVENT_CYCLE_SNAP_PRIMARY:
      CycleTabletSnap(window_state, SplitViewController::LEFT);
      return;
    case WM_EVENT_CYCLE_SNAP_SECONDARY:
      CycleTabletSnap(window_state, SplitViewController::RIGHT);
      return;
    case WM_EVENT_MINIMIZE:
      UpdateWindow(window_state, WindowStateType::kMinimized,
                   true /* animated */);
      return;
    case WM_EVENT_SHOW_INACTIVE:
    case WM_EVENT_SYSTEM_UI_AREA_CHANGED:
      return;
    case WM_EVENT_SET_BOUNDS: {
      gfx::Rect bounds_in_parent =
          (static_cast<const SetBoundsWMEvent*>(event))->requested_bounds();
      if (bounds_in_parent.IsEmpty())
        return;

      if (window_util::IsDraggingTabs(window_state->window()) ||
          IsTabDraggingSourceWindow(window_state->window()) ||
          TabDragDropDelegate::IsSourceWindowForDrag(window_state->window()) ||
          BoundsChangeIsFromVKAndAllowed(window_state->window())) {
        // If the window is the current tab-dragged window or the current tab-
        // dragged window's source window, we may need to update its bounds
        // during dragging.
        window_state->SetBoundsDirect(bounds_in_parent);
      } else if (current_state_type_ == WindowStateType::kMaximized) {
        // Having a maximized window, it could have been created with an empty
        // size and the caller should get his size upon leaving the maximized
        // mode. As such we set the restore bounds to the requested bounds.
        window_state->SetRestoreBoundsInParent(bounds_in_parent);
      } else if (current_state_type_ != WindowStateType::kMinimized &&
                 current_state_type_ != WindowStateType::kFullscreen &&
                 current_state_type_ != WindowStateType::kPinned &&
                 current_state_type_ != WindowStateType::kTrustedPinned &&
                 current_state_type_ != WindowStateType::kPrimarySnapped &&
                 current_state_type_ != WindowStateType::kSecondarySnapped) {
        // In all other cases (except for minimized windows) we respect the
        // requested bounds and center it to a fully visible area on the screen.
        bounds_in_parent = GetCenteredBounds(bounds_in_parent, window_state);
        if (bounds_in_parent != window_state->window()->bounds()) {
          const SetBoundsWMEvent* bounds_event =
              static_cast<const SetBoundsWMEvent*>(event);
          if (window_state->window()->IsVisible() && bounds_event->animate())
            window_state->SetBoundsDirectAnimated(bounds_in_parent);
          else
            window_state->SetBoundsDirect(bounds_in_parent);
        }
      }
      break;
    }
    case WM_EVENT_ADDED_TO_WORKSPACE:
      if (current_state_type_ != WindowStateType::kMaximized &&
          current_state_type_ != WindowStateType::kFullscreen &&
          current_state_type_ != WindowStateType::kMinimized) {
        // If an already snapped window gets added to the workspace it should
        // not be maximized, rather retain its previous state.
        const WindowStateType new_state =
            IsSnapped(current_state_type_)
                ? window_state->GetStateType()
                : window_state->GetMaximizedOrCenteredWindowType();
        UpdateWindow(window_state, new_state, /*animated=*/true);
      }
      break;
    case WM_EVENT_WORKAREA_BOUNDS_CHANGED:
      if (current_state_type_ != WindowStateType::kMinimized)
        UpdateBounds(window_state, true /* animated */);
      break;
    case WM_EVENT_DISPLAY_BOUNDS_CHANGED:
      // Don't animate on a screen rotation - just snap to new size.
      if (current_state_type_ != WindowStateType::kMinimized)
        UpdateBounds(window_state, false /* animated */);
      break;
  }
}

WindowStateType TabletModeWindowState::GetType() const {
  return current_state_type_;
}

void TabletModeWindowState::AttachState(WindowState* window_state,
                                        WindowState::State* previous_state) {
  current_state_type_ = previous_state->GetType();

  gfx::Rect restore_bounds = GetRestoreBounds(window_state);
  if (!restore_bounds.IsEmpty()) {
    // We do not want to do a session restore to our window states. Therefore
    // we tell the window to use the current default states instead.
    SetWindowRestoreOverrides(window_state->window(), restore_bounds,
                              window_state->GetShowState());
  }

  if (current_state_type_ != WindowStateType::kMaximized &&
      current_state_type_ != WindowStateType::kMinimized &&
      current_state_type_ != WindowStateType::kFullscreen &&
      current_state_type_ != WindowStateType::kPinned &&
      current_state_type_ != WindowStateType::kFloated &&
      current_state_type_ != WindowStateType::kTrustedPinned) {
    UpdateWindow(window_state, state_type_on_attach_,
                 animate_bounds_on_attach_);
  }
}

void TabletModeWindowState::DetachState(WindowState* window_state) {
  // From now on, we can use the default session restore mechanism again.
  SetWindowRestoreOverrides(window_state->window(), gfx::Rect(),
                            ui::SHOW_STATE_NORMAL);
}

void TabletModeWindowState::UpdateWindow(WindowState* window_state,
                                         WindowStateType target_state,
                                         bool animated) {
  DCHECK(target_state == WindowStateType::kMinimized ||
         target_state == WindowStateType::kMaximized ||
         target_state == WindowStateType::kPinned ||
         target_state == WindowStateType::kTrustedPinned ||
         (target_state == WindowStateType::kNormal &&
          (!window_state->CanMaximize() ||
           !!wm::GetTransientParent(window_state->window()))) ||
         target_state == WindowStateType::kFullscreen ||
         target_state == WindowStateType::kPrimarySnapped ||
         target_state == WindowStateType::kSecondarySnapped ||
         target_state == WindowStateType::kFloated);

  if (current_state_type_ == target_state) {
    if (target_state == WindowStateType::kMinimized)
      return;
    // If the state type did not change, update it accordingly.
    UpdateBounds(window_state, animated);
    return;
  }

  const WindowStateType old_state_type = current_state_type_;
  current_state_type_ = target_state;
  window_state->UpdateWindowPropertiesFromStateType();
  window_state->NotifyPreStateTypeChange(old_state_type);

  if (target_state == WindowStateType::kFloated)
    Shell::Get()->float_controller()->Float(window_state->window());

  // Unfloat floated window when exiting float state to another state.
  if (old_state_type == WindowStateType::kFloated)
    Shell::Get()->float_controller()->Unfloat(window_state->window());

  if (target_state == WindowStateType::kMinimized) {
    wm::SetWindowVisibilityAnimationType(
        window_state->window(), WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE);
    window_state->window()->Hide();
    if (window_state->IsActive())
      window_state->Deactivate();
  } else {
    UpdateBounds(window_state, animated);
  }

  if ((window_state->window()->layer()->GetTargetVisibility() ||
       old_state_type == WindowStateType::kMinimized) &&
      !window_state->window()->layer()->visible()) {
    // The layer may be hidden if the window was previously minimized. Make
    // sure it's visible.
    window_state->window()->Show();
  }

  window_state->NotifyPostStateTypeChange(old_state_type);

  if (old_state_type == WindowStateType::kPinned ||
      target_state == WindowStateType::kPinned ||
      old_state_type == WindowStateType::kTrustedPinned ||
      target_state == WindowStateType::kTrustedPinned) {
    Shell::Get()->screen_pinning_controller()->SetPinnedWindow(
        window_state->window());
  }
}

WindowStateType TabletModeWindowState::GetSnappedWindowStateType(
    WindowState* window_state,
    WindowStateType target_state) {
  DCHECK(target_state == WindowStateType::kPrimarySnapped ||
         target_state == WindowStateType::kSecondarySnapped);
  return SplitViewController::Get(Shell::GetPrimaryRootWindow())
                 ->CanSnapWindow(window_state->window())
             ? target_state
             : window_state->GetMaximizedOrCenteredWindowType();
}

void TabletModeWindowState::UpdateBounds(WindowState* window_state,
                                         bool animated) {
  // Do not update window's bounds if it's in tab-dragging process. The bounds
  // will be updated later when the drag ends.
  if (window_util::IsDraggingTabs(window_state->window()))
    return;

  // Do not update minimized windows bounds until it was unminimized.
  if (current_state_type_ == WindowStateType::kMinimized)
    return;

  gfx::Rect bounds_in_parent = GetBoundsInTabletMode(window_state);
  // If we have a target bounds rectangle, we center it and set it
  // accordingly.
  if (!bounds_in_parent.IsEmpty() &&
      bounds_in_parent != window_state->window()->bounds()) {
    if (!window_state->window()->IsVisible() || !animated) {
      window_state->SetBoundsDirect(bounds_in_parent);
    } else {
      if (window_state->bounds_animation_type() ==
          WindowState::BoundsChangeAnimationType::STEP_END) {
        // Just use the normal bounds animation with ZERO tween with long enough
        // duration for STEP_END. The animation will be stopped when the to
        // window's animation ends.
        window_state->SetBoundsDirectAnimated(
            bounds_in_parent, base::Seconds(1), gfx::Tween::ZERO);
        return;
      }
      // Use cross fade in some cases to avoid flashing and/or for better
      // performance.
      if (window_state->IsMaximized() || window_state->IsFloated())
        window_state->SetBoundsDirectCrossFade(bounds_in_parent);
      else
        window_state->SetBoundsDirectAnimated(bounds_in_parent);
    }
  }
}

void TabletModeWindowState::CycleTabletSnap(
    WindowState* window_state,
    SplitViewController::SnapPosition snap_position) {
  aura::Window* window = window_state->window();
  SplitViewController* split_view_controller = SplitViewController::Get(window);
  // If |window| is already snapped in |snap_position|, then unsnap |window|.
  if (window == split_view_controller->GetSnappedWindow(snap_position)) {
    UpdateWindow(window_state, window_state->GetMaximizedOrCenteredWindowType(),
                 /*animated=*/true);
    window_state->ReadOutWindowCycleSnapAction(
        IDS_WM_RESTORE_SNAPPED_WINDOW_ON_SHORTCUT);
    return;
  }
  // If |window| can snap in split view, then snap |window| in |snap_position|.
  if (split_view_controller->CanSnapWindow(window)) {
    split_view_controller->SnapWindow(window, snap_position);
    window_state->ReadOutWindowCycleSnapAction(
        snap_position == SplitViewController::LEFT
            ? IDS_WM_SNAP_WINDOW_TO_LEFT_ON_SHORTCUT
            : IDS_WM_SNAP_WINDOW_TO_RIGHT_ON_SHORTCUT);
    return;
  }
  // Otherwise, show the cannot snap toast.
  ShowAppCannotSnapToast();
}

void TabletModeWindowState::DoTabletSnap(WindowState* window_state,
                                         WMEventType snap_event_type) {
  DCHECK(snap_event_type == WM_EVENT_SNAP_PRIMARY ||
         snap_event_type == WM_EVENT_SNAP_SECONDARY);

  aura::Window* window = window_state->window();
  SplitViewController* split_view_controller = SplitViewController::Get(window);
  if (!split_view_controller->CanSnapWindow(window)) {
    ShowAppCannotSnapToast();
    return;
  }

  window_state->set_bounds_changed_by_user(true);
  chromeos::WindowStateType new_state_type =
      snap_event_type == WM_EVENT_SNAP_PRIMARY
          ? WindowStateType::kPrimarySnapped
          : WindowStateType::kSecondarySnapped;
  window_state->RecordAndResetWindowSnapActionSource(
      window_state->GetStateType(), new_state_type);

  // A snap WMEvent will put the window in tablet split view.
  split_view_controller->OnWindowSnapWMEvent(window, snap_event_type);

  // Change window state and bounds to the snapped window state and bounds.
  UpdateWindow(window_state, new_state_type, /*animated=*/false);
}

}  // namespace ash
