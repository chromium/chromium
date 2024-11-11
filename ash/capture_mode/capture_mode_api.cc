// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/capture_mode/capture_mode_api.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/feature_list.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {

void CaptureScreenshotsOfAllDisplays() {
  CaptureModeController::Get()->CaptureScreenshotsOfAllDisplays();
}

bool IsSunfishFeatureEnabledWithFeatureKey() {
  const bool is_sunfish_feature_enabled =
      base::FeatureList::IsEnabled(features::kSunfishFeature);
  // Allow Google accounts to bypass the secret key check.
  if (Shell* shell = Shell::HasInstance() ? Shell::Get() : nullptr;
      shell && shell->session_controller() &&
      gaia::IsGoogleInternalAccountEmail(
          shell->session_controller()->GetActiveAccountId().GetUserEmail())) {
    return is_sunfish_feature_enabled;
  }

  return is_sunfish_feature_enabled && switches::IsSunfishSecretKeyMatched();
}

bool CanStartSunfishSession() {
  // Returns true if sunfish session can be started, which is true if either the
  // Sunfish or Scanner feature flag is enabled. Note Scanner operations will
  // only be available if the secret key is matched.
  return IsSunfishFeatureEnabledWithFeatureKey() ||
         features::IsScannerEnabled();
}

}  // namespace ash
