// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

PrivacyHubController::PrivacyHubController() = default;

PrivacyHubController::~PrivacyHubController() = default;

// static
void PrivacyHubController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kDeviceGeolocationAllowed,
      static_cast<int>(PrivacyHubController::AccessLevel::kDisallowed));
}

// static
void PrivacyHubController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kUserCameraAllowed, true);
  registry->RegisterBooleanPref(prefs::kUserMicrophoneAllowed, true);
  registry->RegisterBooleanPref(prefs::kUserGeolocationAllowed, true);
}

}  // namespace ash
