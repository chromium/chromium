// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/persistent_window_controller.h"

#include "ash/display/persistent_window_info.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

display::DisplayManager* GetDisplayManager() {
  return Shell::Get()->display_manager();
}

MruWindowTracker::WindowList GetWindowList() {
  return Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kAllDesks);
}

// Returns true when window cycle list can be processed to perform save/restore
// operations on observing display changes.
bool ShouldProcessWindowList() {
  // Window cycle list exists in active user session only.
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted())
    return false;

  if (GetDisplayManager()->IsInMirrorMode())
    return false;

  return true;
}

}  // namespace

constexpr char PersistentWindowController::kNumOfWindowsRestoredHistogramName[];

PersistentWindowController::PersistentWindowController() {
  display::Screen::GetScreen()->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->window_tree_host_manager()->AddObserver(this);
}

PersistentWindowController::~PersistentWindowController() {
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
}

void PersistentWindowController::OnWillProcessDisplayChanges() {
  if (!ShouldProcessWindowList())
    return;

  for (auto* window : GetWindowList()) {
    WindowState* window_state = WindowState::Get(window);
    // This implies that we keep the first persistent info until they're valid
    // to restore, or until they're cleared by user-invoked bounds change.
    if (window_state->persistent_window_info())
      continue;
    window_state->SetPersistentWindowInfo(PersistentWindowInfo(window));
  }
}

void PersistentWindowController::OnDisplayAdded(
    const display::Display& new_display) {
  restore_callback_ = base::BindOnce(
      &PersistentWindowController::MaybeRestorePersistentWindowBounds,
      base::Unretained(this));
}

void PersistentWindowController::OnSessionStateChanged(
    session_manager::SessionState state) {
  MaybeRestorePersistentWindowBounds();
}

void PersistentWindowController::OnDisplayConfigurationChanged() {
  if (restore_callback_)
    std::move(restore_callback_).Run();
}

void PersistentWindowController::MaybeRestorePersistentWindowBounds() {
  if (!ShouldProcessWindowList())
    return;

  display::Screen* screen = display::Screen::GetScreen();
  int window_restored_count = 0;
  for (auto* window : GetWindowList()) {
    WindowState* window_state = WindowState::Get(window);
    if (!window_state->persistent_window_info())
      continue;
    PersistentWindowInfo persistent_window_info =
        *window_state->persistent_window_info();
    const int64_t persistent_display_id = persistent_window_info.display_id;
    if (persistent_display_id == screen->GetDisplayNearestWindow(window).id())
      continue;
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
    DCHECK(display.bounds().size() == persistent_display_bounds.size());
    const gfx::Vector2d offset = display.bounds().OffsetFromOrigin() -
                                 persistent_display_bounds.OffsetFromOrigin();
    persistent_window_bounds.Offset(offset);

    window->SetBoundsInScreen(persistent_window_bounds, display);
    // Reset persistent window info everytime the window bounds have restored.
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
