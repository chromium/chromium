// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/fullscreen_controller.h"

#include <limits>

#include "ash/login/ui/lock_screen.h"
#include "ash/session/fullscreen_alert_bubble.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/dbus/power_manager/idle.pb.h"

namespace ash {

FullscreenController::FullscreenController(
    SessionControllerImpl* session_controller)
    : session_controller_(session_controller) {
  auto* power_manager = chromeos::PowerManagerClient::Get();
  // Might be nullptr in tests.
  if (power_manager) {
    power_manager->AddObserver(this);
  }
}

FullscreenController::~FullscreenController() {
  auto* power_manager = chromeos::PowerManagerClient::Get();
  if (power_manager) {
    power_manager->RemoveObserver(this);
  }
}

// static
void FullscreenController::MaybeExitFullscreen() {
  // If the active window is fullscreen, exit fullscreen to avoid the web page
  // or app mimicking the lock screen. Do not exit fullscreen if the shelf is
  // visible while in fullscreen because the shelf makes it harder for a web
  // page or app to mimic the lock screen.
  WindowState* active_window_state = WindowState::ForActiveWindow();
  if (!active_window_state || !active_window_state->IsFullscreen())
    return;

  Shelf* shelf = Shelf::ForWindow(active_window_state->window());
  const bool shelf_visible =
      shelf->GetVisibilityState() == ShelfVisibilityState::SHELF_VISIBLE;

  if (shelf_visible && !active_window_state->GetHideShelfWhenFullscreen())
    return;

  const WMEvent event(WM_EVENT_TOGGLE_FULLSCREEN);
  active_window_state->OnWMEvent(&event);
}

void FullscreenController::MaybeShowAlert() {
  // 1. Check if there is a lock/login screen showing up.
  if (LockScreen::HasInstance())
    return;

  // 2. Check if the activate window is fullscreen.
  WindowState* active_window_state = WindowState::ForActiveWindow();
  if (!active_window_state || !active_window_state->IsFullscreen())
    return;

  // 3. Check if the shelf is visible.
  Shelf* shelf = Shelf::ForWindow(active_window_state->window());
  const bool shelf_visible =
      shelf->GetVisibilityState() == ShelfVisibilityState::SHELF_VISIBLE;

  if (shelf_visible && !active_window_state->GetHideShelfWhenFullscreen())
    return;

  if (!bubble_)
    bubble_ = std::make_unique<FullscreenAlertBubble>();

  bubble_->Show();
}

void FullscreenController::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  if (session_controller_->login_status() != LoginStatus::GUEST)
    return;

  MaybeExitFullscreen();
}

void FullscreenController::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& proto) {
  if (session_controller_->login_status() != LoginStatus::GUEST)
    return;

  if (proto.off() || proto.dimmed())
    MaybeExitFullscreen();
}

void FullscreenController::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  // Show alert when the device returns from low (epsilon) brightness which
  // covers three cases.
  // 1. The device returns from sleep.
  // 2. The device lid is opended (with sleep on).
  // 3. The device returns from low display brightness.
  double epsilon = std::numeric_limits<double>::epsilon();
  if (change.percent() <= epsilon) {
    device_in_dark_ = true;
  } else {
    if (device_in_dark_)
      MaybeShowAlert();
    device_in_dark_ = false;
  }
}

void FullscreenController::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    const base::TimeTicks& timestamp) {
  // Show alert when the lid is opened. This also covers the case when the user
  // turn off "Sleep when cover is closed".
  if (state == chromeos::PowerManagerClient::LidState::OPEN)
    MaybeShowAlert();
}

}  // namespace ash
