// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/capture_mode/capture_mode_api.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/feature_list.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {

void CaptureScreenshotsOfAllDisplays() {
  CaptureModeController::Get()->CaptureScreenshotsOfAllDisplays();
}

bool IsSunfishOrScannerEnabled() {
  // Returns true if sunfish session can be started, which is true if either the
  // Sunfish or Scanner feature flag is enabled. Note Scanner operations will
  // only be available if the secret key is matched.
  return features::IsSunfishFeatureEnabled() || features::IsScannerEnabled();
}

bool IsSunfishAllowedAndEnabled() {
  Shell* shell = Shell::HasInstance() ? Shell::Get() : nullptr;
  return IsSunfishOrScannerEnabled() &&
         // When `AppListControllerImpl` is initialised and indirectly calls
         // this function, the active user session has not been started yet.
         // Gracefully handle this case.
         shell && shell->session_controller()->IsActiveUserSessionStarted() &&
         capture_mode_util::GetActiveUserPrefService()->GetBoolean(
             prefs::kSunfishEnabled);
}

}  // namespace ash
