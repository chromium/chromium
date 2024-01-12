// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/bounds_tracker/window_bounds_tracker.h"

#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/work_area_insets.h"
#include "base/auto_reset.h"
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
//
// This factor is the ratio between the target and source work area sizes, i.e.:
//
// factor_x = target_work_area.width() / source_work_area.width();
// factor_y = target_work_area.height() / source_work_area.height();
//
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

// Gets the display's in root coordinate and in session work area, which will
// always be used while doing window bounds remapping and restoring.
gfx::Rect GetDisplayInSessionWorkArea(int64_t display_id) {
  display::Display display;
  display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id, &display);
  CHECK(display.is_valid());
  const gfx::Insets insets =
      WorkAreaInsets::ForWindow(Shell::GetRootWindowForDisplayId(display_id))
          ->in_session_user_work_area_insets();
  gfx::Rect work_area(display.size());
  work_area.Inset(insets);
  return work_area;
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
  // Stops observing `window` and removes it from the `bounds_database_` if
  // it has bounds stored.
  bounds_database_.erase(window);
  window_observations_.RemoveObservation(window);
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
  RestoreWindowToCachedBounds(window);
}

void WindowBoundsTracker::OnWindowRemovingFromRootWindow(
    aura::Window* window,
    aura::Window* new_root) {
  // Check whether we should remap or restore `window` on its root window
  // changes. Needed if 1) the window was moved between displays through the
  // shortcut `kMoveActiveWindowBetweenDisplays` or 2) removing the window's
  // host display and the window will be moved to the current primary display or
  // 3) restoring the window back to its previous host display on display
  // reconnection.
  const bool is_moving_window_between_displays =
      window == moving_window_between_displays_;
  const bool should_remap_or_restore =
      is_moving_window_between_displays ||
      RootWindowController::ForWindow(window->GetRootWindow())
          ->is_shutting_down() ||
      is_restoring_window_on_display_added_;
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

void WindowBoundsTracker::OnWillSwapDisplayRootWindows(
    int64_t first_display_id,
    int64_t second_display_id) {
  CHECK_NE(first_display_id, second_display_id);
  for (const auto& window : window_observations_.sources()) {
    const int64_t window_display_id =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
    if (window_display_id == first_display_id) {
      RemapOrRestore(window, second_display_id);
    } else if (window_display_id == second_display_id) {
      RemapOrRestore(window, first_display_id);
    }
  }
}

void WindowBoundsTracker::OnDisplayRootWindowsSwapped(
    int64_t first_display_id,
    int64_t second_display_id) {
  CHECK_NE(first_display_id, second_display_id);
  for (const auto& window : window_observations_.sources()) {
    const int64_t window_display_id =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
    if (window_display_id == first_display_id ||
        window_display_id == second_display_id) {
      RestoreWindowToCachedBounds(window);
    }
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
      base::AutoReset<bool> in_restoring(&is_restoring_window_on_display_added_,
                                         true);
      window_util::MoveWindowToDisplay(iter->first, candidate_old_display_id);
      iter = window_to_display_map_.erase(iter);
    } else {
      ++iter;
    }
  }
}

void WindowBoundsTracker::SetWindowBoundsChangedByUser(
    aura::Window* window,
    bool bounds_changed_by_user) {
  if (!window_observations_.IsObservingSource(window)) {
    return;
  }

  // The window's current bounds will always be stored as a restore bounds at
  // its the first time `WindowDisplayInfo` changes.
  const auto iter = bounds_database_.find(window);
  if (iter == bounds_database_.end()) {
    return;
  }

  const int64_t display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
  if (bounds_changed_by_user) {
    iter->second.displays_with_window_user_assigned_bounds.insert(display_id);
  } else {
    iter->second.displays_with_window_user_assigned_bounds.erase(display_id);
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
// WindowBoundsTracker::WindowBoundsInfo:

WindowBoundsTracker::WindowBoundsInfo::WindowBoundsInfo(
    const gfx::Rect& given_bounds_in_parent,
    bool given_is_restore_bounds)
    : bounds_in_parent(given_bounds_in_parent),
      is_restore_bounds(given_is_restore_bounds) {}

// -----------------------------------------------------------------------------
// WindowBoundsTracker::WindowBoundsEntry:

WindowBoundsTracker::WindowBoundsEntry::WindowBoundsEntry() = default;

WindowBoundsTracker::WindowBoundsEntry::WindowBoundsEntry(WindowBoundsEntry&&) =
    default;

WindowBoundsTracker::WindowBoundsEntry&
WindowBoundsTracker::WindowBoundsEntry::operator=(WindowBoundsEntry&&) =
    default;

WindowBoundsTracker::WindowBoundsEntry::~WindowBoundsEntry() = default;

bool WindowBoundsTracker::WindowBoundsEntry::
    ShouldUseCurrentBoundsAsRestoreBounds(int64_t display_id) const {
  return window_bounds_map.empty() ||
         displays_with_window_user_assigned_bounds.contains(display_id);
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
  gfx::Rect source_work_area = GetDisplayInSessionWorkArea(source_display_id);

  const auto& window_bounds_entry = UpdateBoundsDatabaseOfWindow(
      window,
      WindowDisplayInfo(source_display_id, source_display.rotation(),
                        source_work_area),
      window->bounds(), /*is_current_bounds=*/true);
  const auto& window_bounds_map = window_bounds_entry.window_bounds_map;
  CHECK(!window_bounds_map.empty());

  display::Display target_display;
  screen->GetDisplayWithDisplayId(target_display_id, &target_display);
  CHECK(target_display.is_valid());
  const gfx::Rect target_work_area =
      GetDisplayInSessionWorkArea(target_display_id);
  const WindowDisplayInfo target_window_display_info(
      target_display_id, target_display.rotation(), target_work_area);
  const auto iter = window_bounds_map.find(target_window_display_info);

  // If the current stored bounds for the `target_window_display_info` are
  // already restore bounds, there's no need to recalculate the remapping bounds
  // again as the restore bounds will be used to restore the window to its
  // previous bounds when it goes back to the target `WindowDisplayInfo`.
  if (iter != window_bounds_map.end() && iter->second.is_restore_bounds) {
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
                               remapped_bounds, /*is_current_bounds=*/false);
  return;
}

WindowBoundsTracker::WindowBoundsEntry&
WindowBoundsTracker::UpdateBoundsDatabaseOfWindow(
    aura::Window* window,
    const WindowDisplayInfo& window_display_info,
    const gfx::Rect& bounds,
    bool is_current_bounds) {
  auto& window_bounds_entry = bounds_database_[window];
  CHECK(window_observations_.IsObservingSource(window));
  if (!is_current_bounds ||
      window_bounds_entry.ShouldUseCurrentBoundsAsRestoreBounds(
          window_display_info.display_id)) {
    window_bounds_entry.window_bounds_map.insert_or_assign(
        window_display_info, WindowBoundsInfo(bounds, is_current_bounds));
  }
  return window_bounds_entry;
}

void WindowBoundsTracker::RestoreWindowToCachedBounds(aura::Window* window) {
  const auto iter = bounds_database_.find(window);
  CHECK(iter != bounds_database_.end());
  auto& window_bounds_map = iter->second.window_bounds_map;
  CHECK(!window_bounds_map.empty());
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  const WindowDisplayInfo window_display_info(
      display.id(), display.rotation(),
      GetDisplayInSessionWorkArea(display.id()));
  const auto bounds_iter = window_bounds_map.find(window_display_info);
  CHECK(bounds_iter != window_bounds_map.end());

  window->SetBounds(bounds_iter->second.bounds_in_parent);
  // Remove the stored non-restore-bounds from the database after it has been
  // used. As the non-restore-bounds will never be used to restore the window
  // later, the recalculation will be triggered instead.
  if (!bounds_iter->second.is_restore_bounds) {
    window_bounds_map.erase(bounds_iter);
  }
}

}  // namespace ash
