// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/capture_mode/capture_mode_api.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/scanner/scanner_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/feature_list.h"
#include "components/prefs/pref_service.h"
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

  // Order here matters: When `AppListControllerImpl` is initialised and
  // indirectly calls this function, the active user session has not been
  // started yet. Gracefully handle this case.
  auto* session_controller = shell->session_controller();
  if (!session_controller ||
      !session_controller->IsActiveUserSessionStarted()) {
    return false;
  }

  auto* pref_service = capture_mode_util::GetActiveUserPrefService();
  if (!pref_service || !pref_service->GetBoolean(prefs::kSunfishEnabled)) {
    return false;
  }

  auto* controller = CaptureModeController::Get();
  return controller && controller->IsSearchAllowedByPolicy();
}

bool CanShowSunfishOrScannerUi() {
  return CanShowSunfishUi() || ScannerController::CanShowUiForShell();
}

}  // namespace ash
