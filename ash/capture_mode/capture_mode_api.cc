// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/capture_mode/capture_mode_api.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/scanner/scanner_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/screen_pinning_controller.h"
#include "base/feature_list.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {

void CaptureScreenshotsOfAllDisplays() {
  CaptureModeController::Get()->CaptureScreenshotsOfAllDisplays();
}

bool CanShowSunfishUi() {
  if (!features::IsSunfishFeatureEnabled()) {
    return false;
  }

  Shell* shell = Shell::HasInstance() ? Shell::Get() : nullptr;
  if (!shell) {
    return false;
  }

  // Do not allow showing sunfish UI in pinned mode.
  auto* screen_pinning_controller = shell->screen_pinning_controller();
  if (!screen_pinning_controller || screen_pinning_controller->IsPinned()) {
    return false;
  }

  // Order here matters: When `AppListControllerImpl` is initialised and
  // indirectly calls this function, the active user session has not been
  // started yet. Gracefully handle this case.
  auto* session_controller = shell->session_controller();
  if (!session_controller ||
      !session_controller->IsActiveUserSessionStarted()) {
    return false;
  }

  // Only allow signed-in regular and child users to use Sunfish features.
  std::optional<user_manager::UserType> user_type =
      session_controller->GetUserType();
  // This can only be called while a user is logged in, so `user_type` should
  // never be empty.
  CHECK(user_type);
  if (user_type != user_manager::UserType::kRegular &&
      user_type != user_manager::UserType::kChild) {
    return false;
  }

  auto* controller = CaptureModeController::Get();
  if (!controller->ActiveUserDefaultSearchProviderIsGoogle()) {
    return false;
  }

  return controller && controller->IsSearchAllowedByPolicy();
}

bool CanShowSunfishOrScannerUi() {
  return CanShowSunfishUi() || ScannerController::CanShowUiForShell();
}

}  // namespace ash
