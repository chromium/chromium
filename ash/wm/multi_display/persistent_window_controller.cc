// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/multi_display/persistent_window_controller.h"
#include "base/memory/raw_ptr.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/multi_display/persistent_window_info.h"
#include "ash/wm/tablet_mode/scoped_skip_user_session_blocked_check.h"
#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ui/base/display_util.h"
#include "ui/display/manager/display_manager.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// This controls the UMA histogram `kNumOfWindowsRestoredOnDisplayAdded` and
// `kNumOfWindowsRestoredOnScreenRotation`. It should not be changed without
// deprecating these two metrics.
constexpr int kMaxRestoredWindowCount = 50;

display::DisplayManager* GetDisplayManager() {
  return Shell::Get()->display_manager();
}

MruWindowTracker::WindowList GetWindowList() {
  // MRU tracker normally skips windows if called during a non active session.
  // |scoped_skip_user_session_blocked_check| allows us to get the list of MRU
  // windows even when a display is added during for example lock session.
  ScopedSkipUserSessionBlockedCheck scoped_skip_user_session_blocked_check;
  return Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kAllDesks);
}

// Returns true when window cycle list can be processed to perform save/restore
// operations on observing display changes.
bool ShouldProcessWindowList() {
  if (!Shell::Get()->desks_controller()) {
    return false;
  }
  return !GetDisplayManager()->IsInMirrorMode();
}

}  // namespace

// -----------------------------------------------------------------------------
// PersistentWindowController::WindowTracker:

PersistentWindowController::WindowTracker::WindowTracker() = default;

PersistentWindowController::WindowTracker::~WindowTracker() {
  RemoveAll();
}

void PersistentWindowController::WindowTracker::Add(
    aura::Window* window,
    const gfx::Rect& restore_bounds_in_parent) {
  if (window_restore_bounds_map_.emplace(window, restore_bounds_in_parent)
          .second) {
    window->AddObserver(this);
  }
}

void PersistentWindowController::WindowTracker::RemoveAll() {
  for (auto& item : window_restore_bounds_map_) {
    item.first->RemoveObserver(this);
  }
  window_restore_bounds_map_.clear();
}

void PersistentWindowController::WindowTracker::Remove(aura::Window* window) {
  auto iter = window_restore_bounds_map_.find(window);
  if (iter != window_restore_bounds_map_.end()) {
    iter->first->RemoveObserver(this);
    window_restore_bounds_map_.erase(iter);
  }
}

void PersistentWindowController::WindowTracker::OnWindowDestroying(
    aura::Window* window) {
  Remove(window);
}

// -----------------------------------------------------------------------------
// PersistentWindowController:

constexpr char
    PersistentWindowController::kNumOfWindowsRestoredOnDisplayAdded[];
constexpr char
    PersistentWindowController::kNumOfWindowsRestoredOnScreenRotation[];

PersistentWindowController::PersistentWindowController() {
  display_manager_observation_.Observe(Shell::Get()->display_manager());
  Shell::Get()->session_controller()->AddObserver(this);
}

PersistentWindowController::~PersistentWindowController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void PersistentWindowController::OnDisplayAdded(
    const display::Display& new_display) {
  display_added_restore_callback_ =
      base::BindOnce(&PersistentWindowController::
                         MaybeRestorePersistentWindowBoundsOnDisplayAdded,
                     base::Unretained(this));
}

void PersistentWindowController::OnWillRemoveDisplays(
    const display::Displays& removed_displays) {
  for (const auto& [window, restore_bounds_in_parent] :
       need_persistent_info_windows_.window_restore_bounds_map()) {
    WindowState* window_state = WindowState::Get(window);
    window_state->CreatePersistentWindowInfo(
        /*is_landscape_before_rotation=*/false, restore_bounds_in_parent,
        /*for_display_removal=*/true);
  }
  need_persistent_info_windows_.RemoveAll();
  for (const auto& removed : removed_displays) {
    is_landscape_orientation_map_.erase(removed.id());
  }
}

void PersistentWindowController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (!(changed_metrics & DISPLAY_METRIC_ROTATION)) {
    return;
  }

  const bool was_landscape_before_rotation =
      base::Contains(is_landscape_orientation_map_, display.id())
          ? is_landscape_orientation_map_[display.id()]
          : false;
  for (aura::Window* window : GetWindowList()) {
    if (window->GetRootWindow() !=
        Shell::GetRootWindowForDisplayId(display.id())) {
      continue;
    }
    auto* window_state = WindowState::Get(window);
    if (window_state->persistent_window_info_of_screen_rotation()) {
      continue;
    }
    // Do not restore the bounds on screen rotation of windows that are snapped,
    // maximized or fullscreened. A snapped window is expected to have different
    // snap positions in different orientations, which means different bounds.
    // E.g, a left snapped window in landscape primary is expected to be right
    // snapped in landscape secondary. Restoring is not needed for maximized or
    // fullscreened windows either, since they will be kept maximized or
    // fullscreened after rotation.
    if (window_state->IsSnapped() || window_state->IsMaximized() ||
        window_state->IsFullscreen()) {
      continue;
    }
    window_state->CreatePersistentWindowInfo(
        was_landscape_before_rotation,
        /*restore_bounds_in_parent=*/gfx::Rect(),
        /*for_display_removal=*/false);
  }
  screen_rotation_restore_callback_ =
      base::BindOnce(&PersistentWindowController::
                         MaybeRestorePersistentWindowBoundsOnScreenRotation,
                     base::Unretained(this));
}

void PersistentWindowController::OnWillProcessDisplayChanges() {
  if (!ShouldProcessWindowList()) {
    return;
  }

  for (aura::Window* window : GetWindowList()) {
    WindowState* window_state = WindowState::Get(window);
    // This implies that we keep the first persistent info until they're valid
    // to restore, or until they're cleared by user-invoked bounds change.
    if (PersistentWindowInfo* info =
            window_state->persistent_window_info_of_display_removal();
        info) {
      info->set_display_id_after_removal(
          display::Screen::GetScreen()->GetDisplayNearestWindow(window).id());
      continue;
    }
    // Place the window that needs persistent window info into the temporary
    // set. The persistent window info will be created and set if a display is
    // removed. Store the window's restore bounds in parent here instead of
    // `OnDisplayRemoved`. As the window's restore bounds in parent are
    // converted from its restore bounds in screen, which relies on the
    // displays' layout. And displays' layout will have been updated inside
    // `OnDisplayRemoved`.
    need_persistent_info_windows_.Add(
        window, window_state->HasRestoreBounds()
                    ? window_state->GetRestoreBoundsInParent()
                    : gfx::Rect());
  }
}

void PersistentWindowController::OnDidProcessDisplayChanges(
    const DisplayConfigurationChange& configuration_change) {
  if (display_added_restore_callback_) {
    std::move(display_added_restore_callback_).Run();
  }
  need_persistent_info_windows_.RemoveAll();

  if (screen_rotation_restore_callback_) {
    std::move(screen_rotation_restore_callback_).Run();
  }

  if (display::Screen::GetScreen()) {
    for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
      is_landscape_orientation_map_[display.id()] = display.is_landscape();
    }
  }
}

void PersistentWindowController::OnFirstSessionStarted() {
  if (!display::Screen::GetScreen()) {
    return;
  }

  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    is_landscape_orientation_map_[display.id()] = display.is_landscape();
  }
}

void PersistentWindowController::
    MaybeRestorePersistentWindowBoundsOnDisplayAdded() {
  if (!ShouldProcessWindowList()) {
    return;
  }

  int window_restored_count = 0;
  // Maybe add the windows to a new display via SetBoundsInScreen() depending on
  // their persistent window info. Go backwards so that if they do get added to
  // another root window's container, the stacking order will match the MRU
  // order (windows added first are stacked at the bottom).
  std::vector<raw_ptr<aura::Window, VectorExperimental>> mru_window_list =
      GetWindowList();
  for (aura::Window* window : base::Reversed(mru_window_list)) {
    WindowState* window_state = WindowState::Get(window);
    if (!window_state->persistent_window_info_of_display_removal()) {
      continue;
    }
    PersistentWindowInfo* info =
        window_state->persistent_window_info_of_display_removal();
    const int64_t persistent_display_id = info->display_id();
    auto* display_manager = GetDisplayManager();
    if (!display_manager->IsDisplayIdValid(persistent_display_id)) {
      continue;
    }
    const auto& display =
        display_manager->GetDisplayForId(persistent_display_id);

    // It is possible to have display size in pixel changes, such as changing
    // cable, bad cable signal etc., but it should be rare.
    if (display.GetSizeInPixel() != info->display_size_in_pixel()) {
      continue;
    }
    const int64_t display_id_after_removal = info->display_id_after_removal();
    const bool is_display_changed =
        persistent_display_id != display_id_after_removal;
    if (!is_display_changed) {
      // Update based on the the window's current bounds if its physical display
      // never changed. This will help keep the window's bounds to match its
      // current window state.
      gfx::Rect bounds = window->bounds();
      wm::ConvertRectToScreen(
          Shell::GetRootWindowForDisplayId(persistent_display_id), &bounds);
      window->SetBoundsInScreen(bounds, display);
    } else {
      // Update |persistent_window_bounds| based on
      // |persistent_display_bounds|'s position change. This ensures that
      // |persistent_window_bounds| is associated with the right target display.
      gfx::Rect persistent_window_bounds = info->window_bounds_in_screen();
      const gfx::Vector2d offset = display.bounds().OffsetFromOrigin() -
                                   info->display_offset_from_origin_in_screen();
      persistent_window_bounds.Offset(offset);
      window->SetBoundsInScreen(persistent_window_bounds, display);
    }

    // Set the restore bounds back if window's physical display changed, and now
    // we are moving it back to its previous display. Or its physical display
    // didn't change, and currently the window still has restore bounds set.
    if ((is_display_changed || window_state->HasRestoreBounds()) &&
        !info->restore_bounds_in_parent().IsEmpty()) {
      const gfx::Rect restore_bounds = info->restore_bounds_in_parent();
      // Use the stored window's restore bounds in parent to set the window's
      // restore bounds in screen. The conversion from the bounds in parent to
      // the bounds in screen will be based on the current displays' layout.
      window_state->SetRestoreBoundsInParent(restore_bounds);
    }
    // Reset persistent window info every time the window bounds have restored.
    window_state->reset_persistent_window_info_of_display_removal();

    ++window_restored_count;
  }

  if (window_restored_count != 0) {
    base::UmaHistogramExactLinear(kNumOfWindowsRestoredOnDisplayAdded,
                                  window_restored_count,
                                  kMaxRestoredWindowCount);
  }
}

void PersistentWindowController::
    MaybeRestorePersistentWindowBoundsOnScreenRotation() {
  if (!ShouldProcessWindowList()) {
    return;
  }

  int window_restored_count = 0;
  for (aura::Window* window : GetWindowList()) {
    WindowState* window_state = WindowState::Get(window);
    if (!window_state->persistent_window_info_of_screen_rotation()) {
      continue;
    }
    PersistentWindowInfo* info =
        window_state->persistent_window_info_of_screen_rotation();
    const int64_t display_id = info->display_id();
    auto* display_manager = GetDisplayManager();
    if (!display_manager->IsDisplayIdValid(display_id)) {
      continue;
    }

    // Restore window's bounds if we are rotating back to the screen orientation
    // that window's bounds was stored. Note, `kLandscapePrimary` and
    // `kLandscapeSecondary` will be treated the same in this case since
    // window's bounds should be the same in each landscape orientation. Same
    // for portrait screen orientation.
    if (display_manager->GetDisplayForId(display_id).is_landscape() ==
        info->is_landscape()) {
      window->SetBounds(info->window_bounds_in_screen());
      ++window_restored_count;
    }
  }

  if (window_restored_count != 0) {
    base::UmaHistogramExactLinear(kNumOfWindowsRestoredOnScreenRotation,
                                  window_restored_count,
                                  kMaxRestoredWindowCount);
  }
}

}  // namespace ash
