// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/fullscreen_controller.h"

#include <limits>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/fullscreen_notification_bubble.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/check.h"
#include "base/strings/string_util.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/wm/fullscreen/keep_fullscreen_for_url_checker.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Exits full screen to avoid the web page or app mimicking the lock screen if
// the active window is in full screen mode and the shelf is not visible. Do not
// exit fullscreen if the shelf is visible while in fullscreen because the shelf
// makes it harder for a web page or app to mimic the lock screen.
void ExitFullscreenIfActive() {
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

// Receives the result from the request to Lacros and exits full screen, if
// required. |callback| will be invoked to signal readiness for session lock.
void OnShouldExitFullscreenResult(base::OnceClosure callback,
                                  bool should_exit_fullscreen) {
  if (should_exit_fullscreen)
    ExitFullscreenIfActive();

  std::move(callback).Run();
}

}  // namespace

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
void FullscreenController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kFullscreenAlertEnabled, true,
                                PrefRegistry::PUBLIC);
}

void FullscreenController::MaybeExitFullscreenBeforeLock(
    base::OnceClosure callback) {
  // Check whether it is allowed to keep full screen on unlock.
  if (!features::IsFullscreenAfterUnlockAllowed()) {
    ExitFullscreenIfActive();
    std::move(callback).Run();
    return;
  }

  // Nothing to do if the active window is not in full screen mode.
  WindowState* active_window_state = WindowState::ForActiveWindow();
  if (!active_window_state || !active_window_state->IsFullscreen()) {
    std::move(callback).Run();
    return;
  }

  // Do not exit fullscreen for a Borealis window. We do additional checks here
  // to avoid entering a screen lock with a window which has a not-allowed
  // property combination. We use CHECKs as those combination should never
  // happen.
  if (active_window_state->window()->GetProperty(
          chromeos::kNoExitFullscreenOnLock)) {
    CHECK(active_window_state->window()->GetProperty(
        chromeos::kUseOverviewToExitFullscreen))
        << "Property combination not allowed. kUseOverviewToExitFullscreen "
           "must be true if kNoExitFullscreenOnLock is true.";
    std::move(callback).Run();
    return;
  }

  if (!keep_fullscreen_checker_) {
    keep_fullscreen_checker_ =
        std::make_unique<chromeos::KeepFullscreenForUrlChecker>(
            Shell::Get()->session_controller()->GetPrimaryUserPrefService());
  }

  // Always exit full screen if the allowlist policy is unset.
  if (!keep_fullscreen_checker_
           ->IsKeepFullscreenWithoutNotificationPolicySet()) {
    ExitFullscreenIfActive();
    std::move(callback).Run();
    return;
  }

  // Try to get the URL of the active window from the shell delegate.
  const GURL& url =
      Shell::Get()->shell_delegate()->GetLastCommittedURLForWindowIfAny(
          active_window_state->window());

  // If the chrome shell delegate did not return a URL for the active window, it
  // could be a Lacros window and it should check with Lacros whether the
  // FullscreenController should exit full screen mode.
  if (url.is_empty() &&
      active_window_state->window()->GetProperty(chromeos::kAppTypeKey) ==
          chromeos::AppType::LACROS) {
    auto should_exit_fullscreen_callback =
        base::BindOnce(&OnShouldExitFullscreenResult, std::move(callback));
    Shell::Get()->shell_delegate()->ShouldExitFullscreenBeforeLock(
        std::move(should_exit_fullscreen_callback));
    return;
  }

  // Check if it is allowed by user pref to keep full screen for the window URL.
  if (keep_fullscreen_checker_->ShouldExitFullscreenForUrl(url))
    ExitFullscreenIfActive();
  std::move(callback).Run();
}

void FullscreenController::MaybeShowNotification() {
  if (!features::IsFullscreenAlertBubbleEnabled())
    return;

  auto* session_controller = Shell::Get()->session_controller();

  // Check if a user session is active to exclude OOBE process.
  if (session_controller->GetSessionState() !=
      session_manager::SessionState::ACTIVE) {
    return;
  }

  auto* prefs = session_controller->GetPrimaryUserPrefService();

  if (!prefs->GetBoolean(prefs::kFullscreenAlertEnabled))
    return;

  // Check if the activate window is fullscreen.
  WindowState* active_window_state = WindowState::ForActiveWindow();
  if (!active_window_state || !active_window_state->IsFullscreen())
    return;

  // Check if the shelf is visible.
  Shelf* shelf = Shelf::ForWindow(active_window_state->window());
  const bool shelf_visible =
      shelf->GetVisibilityState() == ShelfVisibilityState::SHELF_VISIBLE;

  if (shelf_visible && !active_window_state->GetHideShelfWhenFullscreen())
    return;

  if (!bubble_)
    bubble_ = std::make_unique<FullscreenNotificationBubble>();

  bubble_->ShowForWindowState(active_window_state);
}

void FullscreenController::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  if (session_controller_->login_status() != LoginStatus::GUEST)
    return;

  ExitFullscreenIfActive();
}

void FullscreenController::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& proto) {
  if (session_controller_->login_status() != LoginStatus::GUEST)
    return;

  if (proto.off() || proto.dimmed())
    ExitFullscreenIfActive();
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
      MaybeShowNotification();
    device_in_dark_ = false;
  }
}

void FullscreenController::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    base::TimeTicks timestamp) {
  // Show alert when the lid is opened. This also covers the case when the user
  // turns off "Sleep when cover is closed".
  if (state == chromeos::PowerManagerClient::LidState::OPEN)
    MaybeShowNotification();
}

}  // namespace ash
