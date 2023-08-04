// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"
#include "ash/system/privacy_hub/speak_on_mute_detection_privacy_switch_controller.h"
#include "base/feature_list.h"
#include "base/types/pass_key.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

PrivacyHubController::PrivacyHubController(
    base::PassKey<PrivacyHubController>) {}

PrivacyHubController::~PrivacyHubController() = default;

// static
std::unique_ptr<PrivacyHubController>
PrivacyHubController::CreatePrivacyHubController() {
  auto privacy_hub_controller = std::make_unique<PrivacyHubController>(
      base::PassKey<PrivacyHubController>());

  if (features::IsCrosPrivacyHubEnabled()) {
    privacy_hub_controller->camera_controller_ =
        std::make_unique<CameraPrivacySwitchController>();
    privacy_hub_controller->microphone_controller_ =
        std::make_unique<MicrophonePrivacySwitchController>();
    privacy_hub_controller->speak_on_mute_controller_ =
        std::make_unique<SpeakOnMuteDetectionPrivacySwitchController>();
    privacy_hub_controller->geolocation_switch_controller_ =
        std::make_unique<GeolocationPrivacySwitchController>();
    return privacy_hub_controller;
  }

  if (!base::FeatureList::IsEnabled(features::kVideoConference)) {
    privacy_hub_controller->camera_disabled_ =
        std::make_unique<CameraPrivacySwitchDisabled>();
  }
  if (features::IsMicMuteNotificationsEnabled()) {
    // TODO(b/264388354) Until PrivacyHub is enabled for all keep this around
    // for the already existing microphone notifications to continue working.
    privacy_hub_controller->microphone_controller_ =
        std::make_unique<MicrophonePrivacySwitchController>();
  }
  return privacy_hub_controller;
}

// static
PrivacyHubController* PrivacyHubController::Get() {
  // TODO(b/288854399): Remove this if.
  if (!Shell::HasInstance()) {
    // Shell may not be available when used from a test.
    return nullptr;
  }
  Shell* const shell = Shell::Get();
  return shell->privacy_hub_controller();
}

// static
void PrivacyHubController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  // TODO(b/286526469): Sync this pref with the device owner's location
  // permission `kUserGeolocationAllowed`.
  registry->RegisterIntegerPref(
      prefs::kDeviceGeolocationAllowed,
      static_cast<int>(PrivacyHubController::AccessLevel::kAllowed));
}

// static
void PrivacyHubController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kUserCameraAllowed, true);
  registry->RegisterBooleanPref(prefs::kUserMicrophoneAllowed, true);
  registry->RegisterBooleanPref(prefs::kUserSpeakOnMuteDetectionEnabled, false);
  registry->RegisterBooleanPref(prefs::kShouldShowSpeakOnMuteOptInNudge, true);
  registry->RegisterIntegerPref(prefs::kSpeakOnMuteOptInNudgeShownCount, 0);
  registry->RegisterBooleanPref(prefs::kUserGeolocationAllowed, true);
}

CameraPrivacySwitchController* PrivacyHubController::camera_controller() {
  return camera_controller_.get();
}

MicrophonePrivacySwitchController*
PrivacyHubController::microphone_controller() {
  return microphone_controller_.get();
}

SpeakOnMuteDetectionPrivacySwitchController*
PrivacyHubController::speak_on_mute_controller() {
  return speak_on_mute_controller_.get();
}

GeolocationPrivacySwitchController*
PrivacyHubController::geolocation_controller() {
  return geolocation_switch_controller_.get();
}

}  // namespace ash
