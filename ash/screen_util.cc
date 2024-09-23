// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/screen_util.h"

#include "ash/display/display_configuration_controller.h"
#include "ash/display/mirror_window_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_state.h"
#include "ash/wm/work_area_insets.h"
#include "base/check_op.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace screen_util {

gfx::Rect GetMaximizedWindowBoundsInParent(aura::Window* window) {
  if (Shelf::ForWindow(window)->shelf_widget())
    return GetDisplayWorkAreaBoundsInParent(window);
  return GetDisplayBoundsInParent(window);
}

gfx::Rect GetDisplayBoundsInParent(aura::Window* window) {
  gfx::Rect result =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).bounds();
  ::wm::ConvertRectFromScreen(window->parent(), &result);
  return result;
}

gfx::Rect GetFullscreenWindowBoundsInParent(aura::Window* window) {
  gfx::Rect result = GetDisplayBoundsInParent(window);
  const WorkAreaInsets* const work_area_insets =
      WorkAreaInsets::ForWindow(window->GetRootWindow());
  result.Inset(
      gfx::Insets().set_top(work_area_insets->accessibility_panel_height() +
                            work_area_insets->docked_magnifier_height()));
  return result;
}

gfx::Rect GetDisplayWorkAreaBoundsInParent(aura::Window* window) {
  // If it is application window under `non_lock_screen_containers`, use
  // `in_session_user_work_area_insets`, otherwise, use `user_work_area_insets`.
  const aura::Window* non_lock_screen_containers = Shell::GetContainer(
      window->GetRootWindow(), kShellWindowId_NonLockScreenContainersContainer);
  gfx::Insets insets =
      non_lock_screen_containers->Contains(window)
          ? WorkAreaInsets::ForWindow(window)
                ->in_session_user_work_area_insets()
          : WorkAreaInsets::ForWindow(window)->user_work_area_insets();
  gfx::Rect bounds =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).bounds();
  bounds.Inset(insets);
  ::wm::ConvertRectFromScreen(window->parent(), &bounds);
  return bounds;
}

// TODO(yongshun): Remove or consolidate this function with
// `GetDisplayWorkAreaBoundsInParent`.
gfx::Rect GetDisplayWorkAreaBoundsInParentForLockScreen(aura::Window* window) {
  gfx::Rect bounds = WorkAreaInsets::ForWindow(window)->user_work_area_bounds();
  ::wm::ConvertRectFromScreen(window->parent(), &bounds);
  return bounds;
}

// TODO(yongshun): Remove or consolidate this function with
// `GetDisplayWorkAreaBoundsInParent`.
gfx::Rect GetDisplayWorkAreaBoundsInParentForActiveDeskContainer(
    aura::Window* window) {
  aura::Window* root_window = window->GetRootWindow();
  return GetDisplayWorkAreaBoundsInParent(
      desks_util::GetActiveDeskContainerForRoot(root_window));
}

// TODO(yongshun): Remove or consolidate this function with
// `GetDisplayWorkAreaBoundsInParent`.
gfx::Rect GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
    aura::Window* window) {
  gfx::Rect bounds =
      GetDisplayWorkAreaBoundsInParentForActiveDeskContainer(window);
  ::wm::ConvertRectToScreen(window->GetRootWindow(), &bounds);
  return bounds;
}

gfx::Rect GetDisplayBoundsWithShelf(aura::Window* window) {
  if (!Shell::Get()->display_manager()->IsInUnifiedMode()) {
    return display::Screen::GetScreen()
        ->GetDisplayNearestWindow(window)
        .bounds();
  }

  // In Unified Mode, the display that should contain the shelf depends on the
  // current shelf alignment.
  const display::Display shelf_display =
      Shell::Get()
          ->display_configuration_controller()
          ->GetPrimaryMirroringDisplayForUnifiedDesktop();
  DCHECK_NE(shelf_display.id(), display::kInvalidDisplayId);

  // Transform the bounds back to the unified host's coordinates.
  auto inverse_unified_transform =
      window->GetRootWindow()->GetHost()->GetInverseRootTransform();
  return inverse_unified_transform.MapRect(shelf_display.bounds());
}

gfx::Rect SnapBoundsToDisplayEdge(const gfx::Rect& bounds,
                                  const aura::Window* window) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          const_cast<aura::Window*>(window));

  const float dsf = display.device_scale_factor();
  const gfx::Size display_size_in_pixel = display.GetSizeInPixel();
  const gfx::Size scaled_size_in_pixel =
      gfx::ScaleToFlooredSize(display.size(), dsf);

  // Adjusts |bounds| such that the scaled enclosed bounds are atleast as big as
  // the scaled enclosing unadjusted bounds.
  gfx::Rect snapped_bounds = bounds;
  if (scaled_size_in_pixel.width() < display_size_in_pixel.width() &&
      display.bounds().right() == bounds.right()) {
    snapped_bounds.Inset(gfx::Insets::TLBR(0, 0, 0, -1));
    DCHECK_GE(gfx::ScaleToEnclosedRect(snapped_bounds, dsf).right(),
              gfx::ScaleToEnclosingRect(bounds, dsf).right());
  }
  if (scaled_size_in_pixel.height() < display_size_in_pixel.height() &&
      display.bounds().bottom() == bounds.bottom()) {
    snapped_bounds.Inset(gfx::Insets::TLBR(0, 0, -1, 0));
    DCHECK_GE(gfx::ScaleToEnclosedRect(snapped_bounds, dsf).bottom(),
              gfx::ScaleToEnclosingRect(bounds, dsf).bottom());
  }

  return snapped_bounds;
}

gfx::Rect GetIdealBoundsForMaximizedOrFullscreenOrPinnedState(
    aura::Window* window) {
  auto* window_state = WindowState::Get(window);
  if (window_state->IsMaximized()) {
    auto* shelf = ash::Shelf::ForWindow(window);
    if (shelf->auto_hide_behavior() == ash::ShelfAutoHideBehavior::kAlways) {
      gfx::Rect bounds =
          ash::screen_util::GetFullscreenWindowBoundsInParent(window);
      ::wm::ConvertRectToScreen(window->parent(), &bounds);
      return bounds;
    }
    if (shelf->auto_hide_behavior() ==
        ash::ShelfAutoHideBehavior::kAlwaysHidden) {
      return display::Screen::GetScreen()
          ->GetDisplayNearestWindow(const_cast<aura::Window*>(window))
          .work_area();
    }
    auto work_area =
        ash::WorkAreaInsets::ForWindow(window)->ComputeStableWorkArea();
    return work_area;
  }
  if (window_state->IsFullscreen() || window_state->IsPinned()) {
    gfx::Rect bounds =
        ash::screen_util::GetFullscreenWindowBoundsInParent(window);
    ::wm::ConvertRectToScreen(window->parent(), &bounds);
    return bounds;
  }
  NOTREACHED() << "The window is not maximzied or fullscreen or pinned. state="
               << window_state->GetStateType();
}

}  // namespace screen_util

}  // namespace ash
