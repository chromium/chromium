// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/persistent_window_controller.h"

#include "ash/display/persistent_window_info.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/tablet_mode/scoped_skip_user_session_blocked_check.h"
#include "ash/wm/window_state.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/metrics/histogram_macros.h"
#include "ui/display/manager/display_manager.h"

namespace ash {

namespace {

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
  if (!Shell::Get()->desks_controller())
    return false;
  return !GetDisplayManager()->IsInMirrorMode();
}

}  // namespace

constexpr char PersistentWindowController::kNumOfWindowsRestoredHistogramName[];

PersistentWindowController::PersistentWindowController() = default;

PersistentWindowController::~PersistentWindowController() = default;

void PersistentWindowController::OnWillProcessDisplayChanges() {
  if (!ShouldProcessWindowList())
    return;

  for (auto* window : GetWindowList()) {
    WindowState* window_state = WindowState::Get(window);
    // This implies that we keep the first persistent info until they're valid
    // to restore, or until they're cleared by user-invoked bounds change.
    if (window_state->persistent_window_info())
      continue;
    // Place the window that needs persistent window info into the temporary
    // set. The persistent window info will be created and set if a display is
    // removed.
    need_persistent_info_windows_.Add(window);
  }
}

void PersistentWindowController::OnDisplayAdded(
    const display::Display& new_display) {
  restore_callback_ = base::BindOnce(
      &PersistentWindowController::MaybeRestorePersistentWindowBounds,
      base::Unretained(this));
}

void PersistentWindowController::OnDisplayRemoved(
    const display::Display& old_display) {
  for (aura::Window* window : need_persistent_info_windows_.windows()) {
    WindowState* window_state = WindowState::Get(window);
    window_state->SetPersistentWindowInfo(PersistentWindowInfo(window));
  }
  need_persistent_info_windows_.RemoveAll();
}

void PersistentWindowController::OnDidProcessDisplayChanges() {
  if (restore_callback_)
    std::move(restore_callback_).Run();
  need_persistent_info_windows_.RemoveAll();
}

void PersistentWindowController::MaybeRestorePersistentWindowBounds() {
  if (!ShouldProcessWindowList())
    return;

  int window_restored_count = 0;
  // Maybe add the windows to a new display via SetBoundsInScreen() depending on
  // their persistent window info. Go backwards so that if they do get added to
  // another root window's container, the stacking order will match the MRU
  // order (windows added first are stacked at the bottom).
  std::vector<aura::Window*> mru_window_list = GetWindowList();
  for (auto* window : base::Reversed(mru_window_list)) {
    WindowState* window_state = WindowState::Get(window);
    if (!window_state->persistent_window_info())
      continue;
    PersistentWindowInfo persistent_window_info =
        *window_state->persistent_window_info();
    const int64_t persistent_display_id = persistent_window_info.display_id;
    auto* display_manager = GetDisplayManager();
    if (!display_manager->IsDisplayIdValid(persistent_display_id))
      continue;
    const auto& display =
        display_manager->GetDisplayForId(persistent_display_id);

    // Update |persistent_window_bounds| based on |persistent_display_bounds|'s
    // position change. This ensures that |persistent_window_bounds| is
    // associated with the right target display.
    gfx::Rect persistent_window_bounds =
        persistent_window_info.window_bounds_in_screen;
    const auto& persistent_display_bounds =
        persistent_window_info.display_bounds_in_screen;
    // It is possible to have display size change, such as changing cable, bad
    // cable signal etc., but it should be rare.
    if (display.bounds().size() != persistent_display_bounds.size())
      continue;
    const gfx::Vector2d offset = display.bounds().OffsetFromOrigin() -
                                 persistent_display_bounds.OffsetFromOrigin();
    persistent_window_bounds.Offset(offset);

    window->SetBoundsInScreen(persistent_window_bounds, display);
    if (persistent_window_info.restore_bounds_in_screen) {
      gfx::Rect restore_bounds =
          *persistent_window_info.restore_bounds_in_screen;
      restore_bounds.Offset(offset);
      window_state->SetRestoreBoundsInScreen(restore_bounds);
    }
    // Reset persistent window info every time the window bounds have restored.
    window_state->ResetPersistentWindowInfo();

    ++window_restored_count;
  }

  if (window_restored_count != 0) {
    UMA_HISTOGRAM_COUNTS_100(
        PersistentWindowController::kNumOfWindowsRestoredHistogramName,
        window_restored_count);
  }
}


}  // namespace ash
