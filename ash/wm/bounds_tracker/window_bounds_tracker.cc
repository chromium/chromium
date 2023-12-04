// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/bounds_tracker/window_bounds_tracker.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Adjusts the `window_bounds` on different source and target screen
// orientations. Keep the window's physical position while doing the adjustment.
// Given the `inout_source_work_area`, it will be adjusted such that its new
// orientation matches the orientation of the target display if needed. This
// allows `inout_source_work_area` to be used further when
// `AdjustBoundsForWorkArea` is called.
//
//
// Source: landscape --> Target: portrait
// Pretend to rotate the source display by 90 degrees to change it to portrait
// orientation. Adjust the window's bounds inside it with this rotation. Then
// mapping will be between two portrait displays.
//
//  1----------------2   2----------3
//  |         +======|   | |      | |
//  |         |      |   | |      | |
//  |         |      |   | +======+ |
//  |         +======|   |          |
//  4----------------3   |          |
//                       |          |
//                       |          |
//                       1----------4
//
//
// Source: portrait --> Target: landscape
// Pretend to rotate the source display by 270 degrees to change it to landscape
// orientation. Adjust the window's bounds inside it with this rotation. Then
// mapping will be between two landscape displays.
//
//  1----------2   4----------------1
//  |   |      |   |          |     |
//  |   |      |   |          +=====|
//  |===+      |   |                |
//  |          |   |                |
//  |          |   3----------------2
//  |          |
//  |          |
//  4----------3
//
// Note: It does not matter to rotate 90 or 270 degrees while mapping from
// landscape to portrait. We just pick one to rotate the source display to
// portrait orientation as well. Then do the opposite rotation when mapping from
// portrait to landscape orientation.
gfx::Rect AdjustBoundsForRotation(const gfx::Rect& window_bounds,
                                  const display::Display& source_display,
                                  const display::Display& target_display,
                                  gfx::Rect& inout_source_work_area) {
  const int64_t source_display_id = source_display.id();
  const int64_t target_display_id = target_display.id();
  // TODO: Taking care of the rotation in the same display.
  CHECK_NE(source_display_id, target_display_id);
  const bool is_source_landscape = chromeos::IsLandscapeOrientation(
      chromeos::GetDisplayCurrentOrientation(source_display));
  const bool is_target_landscape = chromeos::IsLandscapeOrientation(
      chromeos::GetDisplayCurrentOrientation(target_display));
  if (is_source_landscape == is_target_landscape) {
    return window_bounds;
  }

  const gfx::Size source_display_size = source_display.size();
  // Adjust the source work area on pretend rotation for the further steps of
  // calculation.
  gfx::Size work_area_size = source_display_size;
  const gfx::Insets source_insets = source_display.GetWorkAreaInsets();
  work_area_size.Transpose();
  work_area_size.Enlarge(-source_insets.width(), -source_insets.height());
  inout_source_work_area.set_size(work_area_size);

  const gfx::Point rotated_origin =
      is_source_landscape
          ? gfx::Point(window_bounds.y(), source_display_size.width() -
                                              window_bounds.width() -
                                              window_bounds.x())
          : gfx::Point(source_display_size.height() - window_bounds.height() -
                           window_bounds.y(),
                       window_bounds.x());

  // TODO: Taking care of the window's minimum size while swapping the width and
  // height.
  const gfx::Size rotated_size(window_bounds.height(), window_bounds.width());
  return gfx::Rect(rotated_origin, rotated_size);
}

// Adjusts the given window's `inout_bounds` to account for changes in the
// work area between `source_work_area` and `target_work_area`. The adjustment
// ensures that the distance of the window's center point from the center of
// `target_work_area` is equal to the distance of the window's center point
// from the center of `source_work_area` multiplied by a *factor*.

// This factor is ratio between the target and source work area sizes, i.e.:

// factor_x = target_work_area.width() / source_work_area.width();
// factor_y = target_work_area.height() / source_work_area.height();

// Note: `source_work_area` must have already been adjusted to match
// the orientation of `target_work_area`, i.e. by calling
// `AdjustBoundsForRotation()` before this.
void AdjustBoundsForWorkArea(const gfx::Rect& source_work_area,
                             const gfx::Rect& target_work_area,
                             gfx::Rect& inout_bounds) {
  const bool is_source_landscape =
      source_work_area.width() > source_work_area.height();
  const bool is_target_landscape =
      target_work_area.width() > target_work_area.height();
  CHECK_EQ(is_source_landscape, is_target_landscape);

  const gfx::Point target_work_area_center = target_work_area.CenterPoint();
  const gfx::Point source_work_area_center = source_work_area.CenterPoint();
  const gfx::Point source_window_center = inout_bounds.CenterPoint();

  gfx::Vector2dF offset = source_window_center - source_work_area_center;
  offset.Scale(
      static_cast<float>(target_work_area.width()) / source_work_area.width(),
      static_cast<float>(target_work_area.height()) /
          source_work_area.height());
  const gfx::Point new_window_center =
      target_work_area_center + gfx::ToRoundedVector2d(offset);
  inout_bounds.set_origin(
      new_window_center -
      gfx::Vector2d(inout_bounds.width() / 2, inout_bounds.height() / 2));
}

}  // namespace

WindowBoundsTracker::WindowBoundsTracker() {
  Shell::Get()->activation_client()->AddObserver(this);
}

WindowBoundsTracker::~WindowBoundsTracker() {
  bounds_database_.clear();
  window_observations_.RemoveAllObservations();
  Shell::Get()->activation_client()->RemoveObserver(this);
}

void WindowBoundsTracker::OnWindowDestroying(aura::Window* window) {
  RemoveWindowFromBoundsDatabase(window);
}

void WindowBoundsTracker::OnWindowAddedToRootWindow(aura::Window* window) {
  // Set `window` to the remapping bounds calculated and stored to
  // `bounds_database_` inside `OnWindowRemovingFromRootWindow`. If there is no
  // remapping or restoring bounds can be found for `window`, which means it has
  // never been moved to another display without user-assigned bounds.
  const auto iter = bounds_database_.find(window);
  if (iter == bounds_database_.end()) {
    return;
  }
  const auto& window_bounds_map = iter->second;
  CHECK(!window_bounds_map.empty());
  display::Display target_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  const WindowDisplayInfo target_window_display_info(
      target_display.id(), target_display.rotation(),
      target_display.GetLocalWorkArea());
  const auto bounds_iter = window_bounds_map.find(target_window_display_info);
  CHECK(bounds_iter != window_bounds_map.end());

  window->SetBounds(bounds_iter->second);
}

void WindowBoundsTracker::OnWindowRemovingFromRootWindow(
    aura::Window* window,
    aura::Window* new_root) {
  // Check whether we should remap or restore `window` on its root window
  // changes. Only needed if 1) the window was moved between displays through
  // the shortcut `kMoveActiveWindowBetweenDisplays` 2) removing the window's
  // host display and the window will be moved to the current primary display.
  // As in these two scenarios, the window is moving to another display without
  // user assigned bounds.
  const bool is_moving_window_between_displays =
      window == moving_window_between_displays_;
  const bool should_remap_or_restore =
      is_moving_window_between_displays ||
      RootWindowController::ForWindow(window->GetRootWindow())
          ->is_shutting_down();
  if (!should_remap_or_restore) {
    return;
  }

  RemapOrRestore(
      window,
      display::Screen::GetScreen()->GetDisplayNearestWindow(new_root).id());
  // Reset `moving_window_between_displays_` after finishing the remap or
  // restore on it.
  if (is_moving_window_between_displays) {
    moving_window_between_displays_ = nullptr;
  }
}

void WindowBoundsTracker::OnWindowActivated(ActivationReason reason,
                                            aura::Window* gained_active,
                                            aura::Window* lost_active) {
  if (WindowState::Get(gained_active) &&
      !window_observations_.IsObservingSource(gained_active)) {
    window_observations_.AddObservation(gained_active);
  }
}

void WindowBoundsTracker::AddWindowDisplayIdOnDisplayRemoval(
    aura::Window* window) {
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  CHECK(display.is_valid());
  window_to_display_map_[window] = display.id();
}

void WindowBoundsTracker::MaybeRestoreWindowsOnDisplayAdded() {
  auto* display_manager = Shell::Get()->display_manager();
  auto iter = window_to_display_map_.begin();
  while (iter != window_to_display_map_.end()) {
    const auto candidate_old_display_id = iter->second;
    if (display_manager->IsDisplayIdValid(candidate_old_display_id)) {
      auto* window = iter->first;
      // TODO(b/314160218): Do not store the bounds if it is not user-assigned.
      // Store the window's bounds in the source display before moving it to the
      // target display.
      const display::Display source_display =
          display::Screen::GetScreen()->GetDisplayNearestWindow(window);
      UpdateBoundsDatabaseOfWindow(
          window,
          WindowDisplayInfo(source_display.id(), source_display.rotation(),
                            source_display.GetLocalWorkArea()),
          window->bounds());
      window_util::MoveWindowToDisplay(window, candidate_old_display_id);
      iter = window_to_display_map_.erase(iter);
    } else {
      ++iter;
    }
  }
}

// -----------------------------------------------------------------------------
// WindowBoundsTracker::WindowDisplayInfo:

WindowBoundsTracker::WindowDisplayInfo::WindowDisplayInfo(
    int64_t given_display_id,
    display::Display::Rotation given_rotation,
    const gfx::Rect& given_local_work_area)
    : display_id(given_display_id),
      rotation(given_rotation),
      local_work_area(given_local_work_area) {}

bool WindowBoundsTracker::WindowDisplayInfo::operator<(
    const WindowDisplayInfo& rhs) const {
  return std::tie(display_id, local_work_area, rotation) <
         std::tie(rhs.display_id, rhs.local_work_area, rhs.rotation);
}

// -----------------------------------------------------------------------------
// WindowBoundsTracker:

void WindowBoundsTracker::RemapOrRestore(aura::Window* window,
                                         int64_t target_display_id) {
  WindowState* window_state = WindowState::Get(window);
  const gfx::Rect bounds_in_parent = window->bounds();
  // TODO: Taking care of the windows in other window states.
  if (!window_state->IsNormalStateType()) {
    return;
  }

  auto* screen = display::Screen::GetScreen();
  const display::Display source_display =
      screen->GetDisplayNearestWindow(window);
  const int64_t source_display_id = source_display.id();
  gfx::Rect source_work_area = source_display.GetLocalWorkArea();

  const auto& window_bounds_map = UpdateBoundsDatabaseOfWindow(
      window,
      WindowDisplayInfo(source_display_id, source_display.rotation(),
                        source_work_area),
      window->bounds());
  CHECK(!window_bounds_map.empty());

  display::Display target_display;
  screen->GetDisplayWithDisplayId(target_display_id, &target_display);
  CHECK(target_display.is_valid());
  const gfx::Rect target_work_area = target_display.GetLocalWorkArea();
  const WindowDisplayInfo target_window_display_info(
      target_display_id, target_display.rotation(), target_work_area);
  const auto iter = window_bounds_map.find(target_window_display_info);

  if (iter != window_bounds_map.end()) {
    return;
  }

  // Otherwise, calculating the remapping bounds.

  // Step 1: Anchor point redesign, aka, keep the window's physical position on
  // different screen orientations.
  gfx::Rect remapped_bounds = AdjustBoundsForRotation(
      bounds_in_parent, source_display, target_display, source_work_area);

  // Step 2: Adjust on work area size changes. The relative position from the
  // center of the window to the center of the work area should be the same.
  AdjustBoundsForWorkArea(source_work_area, target_work_area, remapped_bounds);

  // Step 3: Offscreen protection. The window should be fully visible inside the
  // target display configuration.
  remapped_bounds.AdjustToFit(target_work_area);

  UpdateBoundsDatabaseOfWindow(window, target_window_display_info,
                               remapped_bounds);
  return;
}

void WindowBoundsTracker::RemoveWindowFromBoundsDatabase(aura::Window* window) {
  const auto count = bounds_database_.erase(window);
  CHECK(count);
  if (window_observations_.IsObservingSource(window)) {
    window_observations_.RemoveObservation(window);
  }
}

base::flat_map<WindowBoundsTracker::WindowDisplayInfo, gfx::Rect>&
WindowBoundsTracker::UpdateBoundsDatabaseOfWindow(
    aura::Window* window,
    const WindowDisplayInfo& window_display_info,
    const gfx::Rect& bounds) {
  auto& window_bounds_map = bounds_database_[window];
  CHECK(window_observations_.IsObservingSource(window));
  window_bounds_map.insert_or_assign(window_display_info, bounds);
  return window_bounds_map;
}

}  // namespace ash
