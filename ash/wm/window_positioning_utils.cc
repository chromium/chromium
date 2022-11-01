// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_positioning_utils.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "ash/display/display_util.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/cxx17_backports.h"
#include "base/notreached.h"
#include "base/numerics/ranges.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tracker.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

int GetSnappedWindowAxisLength(float snap_ratio,
                               int work_area_axis_length,
                               int min_axis_length,
                               bool is_primary_snap) {
  DCHECK_GT(snap_ratio, 0);
  DCHECK_LE(snap_ratio, 1.f);
  min_axis_length = std::min(min_axis_length, work_area_axis_length);
  // The primary snap size is proportional to |snap_ratio|.
  if (is_primary_snap) {
    return base::clamp(static_cast<int>(snap_ratio * work_area_axis_length),
                       min_axis_length, work_area_axis_length);
  }

  // The secondary snap size is proportional to the |snap_ratio|, but
  // we want to make sure there is no gap between the primary and secondary
  // windows when their |snap_ratio|'s sum up to 1. Thus to avoid a gap from
  // integer rounding up issue, we compute the empty-space size and subtracted
  // it from |work_area_axis_length|. An example test is
  // `WindowPositioningUtilsTest.SnapBoundsWithOddNumberedScreenWidth`.
  const int empty_space_axis_length =
      static_cast<int>((1 - snap_ratio) * work_area_axis_length);
  return base::clamp(work_area_axis_length - empty_space_axis_length,
                     min_axis_length, work_area_axis_length);
}

// Return true if the window or one of its ancestor returns true from
// IsLockedToRoot().
bool IsWindowOrAncestorLockedToRoot(const aura::Window* window) {
  return window && (window->GetProperty(kLockedToRootKey) ||
                    IsWindowOrAncestorLockedToRoot(window->parent()));
}

}  // namespace

void AdjustBoundsSmallerThan(const gfx::Size& max_size, gfx::Rect* bounds) {
  bounds->set_width(std::min(bounds->width(), max_size.width()));
  bounds->set_height(std::min(bounds->height(), max_size.height()));
}

void AdjustBoundsToEnsureWindowVisibility(const gfx::Rect& visible_area,
                                          int min_width,
                                          int min_height,
                                          gfx::Rect* bounds) {
  AdjustBoundsSmallerThan(visible_area.size(), bounds);

  min_width = std::min(min_width, visible_area.width());
  min_height = std::min(min_height, visible_area.height());

  if (bounds->right() < visible_area.x() + min_width) {
    bounds->set_x(visible_area.x() + std::min(bounds->width(), min_width) -
                  bounds->width());
  } else if (bounds->x() > visible_area.right() - min_width) {
    bounds->set_x(visible_area.right() - std::min(bounds->width(), min_width));
  }
  if (bounds->bottom() < visible_area.y() + min_height) {
    bounds->set_y(visible_area.y() + std::min(bounds->height(), min_height) -
                  bounds->height());
  } else if (bounds->y() > visible_area.bottom() - min_height) {
    bounds->set_y(visible_area.bottom() -
                  std::min(bounds->height(), min_height));
  }
  if (bounds->y() < visible_area.y())
    bounds->set_y(visible_area.y());
}

void AdjustBoundsToEnsureMinimumWindowVisibility(const gfx::Rect& visible_area,
                                                 gfx::Rect* bounds) {
  AdjustBoundsToEnsureWindowVisibility(visible_area, kMinimumOnScreenArea,
                                       kMinimumOnScreenArea, bounds);
}

gfx::Rect GetSnappedWindowBoundsInParent(aura::Window* window,
                                         SnapViewType type,
                                         float snap_ratio) {
  return GetSnappedWindowBounds(
      screen_util::GetDisplayWorkAreaBoundsInParent(window),
      display::Screen::GetScreen()->GetDisplayNearestWindow(window), window,
      type, snap_ratio);
}

gfx::Rect GetDefaultSnappedWindowBoundsInParent(aura::Window* window,
                                                SnapViewType type) {
  return GetSnappedWindowBoundsInParent(window, type, kDefaultSnapRatio);
}

gfx::Rect GetSnappedWindowBounds(const gfx::Rect& work_area,
                                 const display::Display display,
                                 aura::Window* window,
                                 SnapViewType type,
                                 float snap_ratio) {
  chromeos::OrientationType orientation = GetSnapDisplayOrientation(display);
  enum class SnapPosition { kLeft, kRight, kBottom, kTop, kInvalid };
  SnapPosition position = SnapPosition::kInvalid;
  const bool is_primary_snap = type == SnapViewType::kPrimary;
  bool is_horizontal = true;

  // Find the actual position of window should be snapped to based on
  // |orientation| and |type|
  switch (orientation) {
    case chromeos::OrientationType::kLandscapePrimary:
      position = is_primary_snap ? SnapPosition::kLeft : SnapPosition::kRight;
      break;
    case chromeos::OrientationType::kLandscapeSecondary:
      position = is_primary_snap ? SnapPosition::kRight : SnapPosition::kLeft;
      break;
    case chromeos::OrientationType::kPortraitPrimary:
      position = is_primary_snap ? SnapPosition::kTop : SnapPosition::kBottom;
      is_horizontal = false;
      break;
    case chromeos::OrientationType::kPortraitSecondary:
      position = is_primary_snap ? SnapPosition::kBottom : SnapPosition::kTop;
      is_horizontal = false;
      break;
    default:
      position = SnapPosition::kInvalid;
      NOTREACHED();
      break;
  }

  // Compute size of the side of the window bound that should be proportional
  // |WindowState::snap_ratio_| to that of the work area, i.e. width for
  // horizontal layout and height for vertical layout.
  gfx::Rect snap_bounds = gfx::Rect(work_area);
  const int work_area_axis_length =
      is_horizontal ? work_area.width() : work_area.height();
  int min_size = 0;
  if (window->delegate()) {
    const gfx::Size minimum_size = window->delegate()->GetMinimumSize();
    min_size = is_horizontal ? minimum_size.width() : minimum_size.height();
  }

  int axis_length = GetSnappedWindowAxisLength(
      snap_ratio, work_area_axis_length, min_size, is_primary_snap);
  const gfx::Size* preferred_size =
      window->GetProperty(kUnresizableSnappedSizeKey);
  if (preferred_size && !WindowState::Get(window)->CanResize()) {
    DCHECK(preferred_size->width() == 0 || preferred_size->height() == 0);
    if (is_horizontal && preferred_size->width() > 0)
      axis_length = preferred_size->width();
    if (!is_horizontal && preferred_size->height() > 0)
      axis_length = preferred_size->height();
  }

  // Set the size of such side and the window position based on a given snap
  // position.
  switch (position) {
    case SnapPosition::kLeft:
      snap_bounds.set_width(axis_length);
      break;
    case SnapPosition::kRight:
      snap_bounds.set_width(axis_length);
      // Snap to the right.
      snap_bounds.set_x(work_area.right() - axis_length);
      break;
    case SnapPosition::kTop:
      snap_bounds.set_height(axis_length);
      break;
    case SnapPosition::kBottom:
      snap_bounds.set_height(axis_length);
      // Snap to the bottom.
      snap_bounds.set_y(work_area.bottom() - axis_length);
      break;
    case SnapPosition::kInvalid:
      NOTREACHED();
      break;
  }
  return snap_bounds;
}

chromeos::OrientationType GetSnapDisplayOrientation(
    const display::Display& display) {
  // This function is used by `GetSnappedWindowBounds()` for clamshell mode
  // only. Tablet mode uses a different function
  // `SplitViewController::GetSnappedWindowBoundsInScreen()`1.
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  DCHECK(!tablet_mode_controller || !tablet_mode_controller->InTabletMode());

  const display::Display::Rotation& rotation =
      Shell::Get()
          ->display_manager()
          ->GetDisplayInfo(display.id())
          .GetActiveRotation();

  return RotationToOrientation(chromeos::GetDisplayNaturalOrientation(display),
                               rotation);
}

void CenterWindow(aura::Window* window) {
  WMEvent event(WM_EVENT_CENTER);
  WindowState::Get(window)->OnWMEvent(&event);
}

void SetBoundsInScreen(aura::Window* window,
                       const gfx::Rect& bounds_in_screen,
                       const display::Display& display) {
  // Don't move a window to other root window if:
  // a) the window is a transient window. It moves when its
  //    transient parent moves.
  // b) if the window or its ancestor has IsLockedToRoot(). It's intentionally
  //    kept in the same root window even if the bounds is outside of the
  //    display.
  if (!::wm::GetTransientParent(window) &&
      !IsWindowOrAncestorLockedToRoot(window)) {
    RootWindowController* dst_root_window_controller =
        Shell::GetRootWindowControllerWithDisplayId(display.id());
    DCHECK(dst_root_window_controller);
    aura::Window* dst_root = dst_root_window_controller->GetRootWindow();
    DCHECK(dst_root);
    aura::Window* dst_container = nullptr;
    if (dst_root != window->GetRootWindow()) {
      int container_id = window->parent()->GetId();
      // All containers that use screen coordinates must have valid window ids.
      DCHECK_GE(container_id, 0);
      // Don't move modal background.
      if (!SystemModalContainerLayoutManager::IsModalBackground(window))
        dst_container = dst_root->GetChildById(container_id);
    }

    if (dst_container && window->parent() != dst_container) {
      aura::Window* focused = window_util::GetFocusedWindow();
      aura::Window* active = window_util::GetActiveWindow();

      aura::WindowTracker tracker;
      if (focused)
        tracker.Add(focused);
      if (active && focused != active)
        tracker.Add(active);

      // Client controlled window will have its own logic on client side
      // to adjust bounds.
      // TODO(oshima): Use WM_EVENT_SET_BOUNDS with target display id.
      auto* window_state = WindowState::Get(window);
      if (!window_state || !window_state->allow_set_bounds_direct()) {
        gfx::Point origin = bounds_in_screen.origin();
        const gfx::Point display_origin = display.bounds().origin();
        origin.Offset(-display_origin.x(), -display_origin.y());
        gfx::Rect new_bounds = gfx::Rect(origin, bounds_in_screen.size());
        // Set new bounds now so that the container's layout manager can adjust
        // the bounds if necessary.
        if (window_state)
          window_state->set_is_moving_to_another_display(true);
        window->SetBounds(new_bounds);
      }
      dst_container->AddChild(window);

      if (window_state)
        window_state->set_is_moving_to_another_display(false);

      // Restore focused/active window.
      if (focused && tracker.Contains(focused)) {
        aura::client::GetFocusClient(focused)->FocusWindow(focused);
        Shell::SetRootWindowForNewWindows(focused->GetRootWindow());
      } else if (active && tracker.Contains(active)) {
        wm::ActivateWindow(active);
      }
      // TODO(oshima): We should not have to update the bounds again
      // below in theory, but we currently do need as there is a code
      // that assumes that the bounds will never be overridden by the
      // layout mananger. We should have more explicit control how
      // constraints are applied by the layout manager.
    }
  }
  gfx::Point origin(bounds_in_screen.origin());
  const gfx::Point display_origin = display::Screen::GetScreen()
                                        ->GetDisplayNearestWindow(window)
                                        .bounds()
                                        .origin();
  origin.Offset(-display_origin.x(), -display_origin.y());
  window->SetBounds(gfx::Rect(origin, bounds_in_screen.size()));
}

}  // namespace ash
