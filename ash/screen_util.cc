// Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "ash/wm/work_area_insets.h"
#include "base/logging.h"
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
  result.Inset(0,
               work_area_insets->accessibility_panel_height() +
                   work_area_insets->docked_magnifier_height(),
               0, 0);
  return result;
}

gfx::Rect GetDisplayWorkAreaBoundsInParent(aura::Window* window) {
  gfx::Rect result =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).work_area();
  ::wm::ConvertRectFromScreen(window->parent(), &result);
  return result;
}

gfx::Rect GetDisplayWorkAreaBoundsInParentForLockScreen(aura::Window* window) {
  gfx::Rect bounds = WorkAreaInsets::ForWindow(window)->user_work_area_bounds();
  ::wm::ConvertRectFromScreen(window->parent(), &bounds);
  return bounds;
}

gfx::Rect GetDisplayWorkAreaBoundsInParentForActiveDeskContainer(
    aura::Window* window) {
  aura::Window* root_window = window->GetRootWindow();
  return GetDisplayWorkAreaBoundsInParent(
      desks_util::GetActiveDeskContainerForRoot(root_window));
}

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
  gfx::RectF shelf_display_screen_bounds(shelf_display.bounds());

  // Transform the bounds back to the unified host's coordinates.
  auto inverse_unified_transform =
      window->GetRootWindow()->GetHost()->GetInverseRootTransform();
  inverse_unified_transform.TransformRect(&shelf_display_screen_bounds);

  return gfx::ToEnclosingRect(shelf_display_screen_bounds);
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
    snapped_bounds.Inset(0, 0, -1, 0);
    DCHECK_GE(gfx::ScaleToEnclosedRect(snapped_bounds, dsf).right(),
              gfx::ScaleToEnclosingRect(bounds, dsf).right());
  }
  if (scaled_size_in_pixel.height() < display_size_in_pixel.height() &&
      display.bounds().bottom() == bounds.bottom()) {
    snapped_bounds.Inset(0, 0, 0, -1);
    DCHECK_GE(gfx::ScaleToEnclosedRect(snapped_bounds, dsf).bottom(),
              gfx::ScaleToEnclosingRect(bounds, dsf).bottom());
  }

  return snapped_bounds;
}

}  // namespace screen_util

}  // namespace ash
