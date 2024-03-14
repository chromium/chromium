// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_controller.h"

#include <cstddef>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/shell.h"
#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "ash/system/privacy_hub/geolocation_privacy_switch_controller.h"
#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"
#include "ash/system/privacy_hub/speak_on_mute_detection_privacy_switch_controller.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/types/pass_key.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {
ScopedLedFallbackForTesting* g_scoped_led_fallback_for_testing = nullptr;
}

PrivacyHubController::PrivacyHubController(
    base::PassKey<PrivacyHubController>) {
  InitUsingCameraLEDFallback();
}

PrivacyHubController::~PrivacyHubController() = default;

// static
std::unique_ptr<PrivacyHubController>
PrivacyHubController::CreatePrivacyHubController() {
  auto privacy_hub_controller = std::make_unique<PrivacyHubController>(
      base::PassKey<PrivacyHubController>());

  privacy_hub_controller->geolocation_switch_controller_ =
      std::make_unique<GeolocationPrivacySwitchController>();

  privacy_hub_controller->camera_controller_ =
      std::make_unique<CameraPrivacySwitchController>();
  privacy_hub_controller->microphone_controller_ =
      std::make_unique<MicrophonePrivacySwitchController>();
  privacy_hub_controller->speak_on_mute_controller_ =
      std::make_unique<SpeakOnMuteDetectionPrivacySwitchController>();

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
  // permission `kUserGeolocationAccessLevel`.
  registry->RegisterIntegerPref(
      prefs::kDeviceGeolocationAllowed,
      static_cast<int>(GeolocationAccessLevel::kAllowed));
}

// static
void PrivacyHubController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kUserCameraAllowed, true);
  registry->RegisterBooleanPref(prefs::kUserCameraAllowedPreviousValue, true);
  registry->RegisterBooleanPref(prefs::kUserGeolocationAccuracyEnabled, true);
  registry->RegisterBooleanPref(prefs::kUserMicrophoneAllowed, true);
  registry->RegisterBooleanPref(
      prefs::kUserSpeakOnMuteDetectionEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kShouldShowSpeakOnMuteOptInNudge, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      prefs::kSpeakOnMuteOptInNudgeShownCount, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      prefs::kUserGeolocationAccessLevel,
      static_cast<int>(GeolocationAccessLevel::kAllowed));
  registry->RegisterIntegerPref(
      prefs::kUserPreviousGeolocationAccessLevel,
      static_cast<int>(GeolocationAccessLevel::kDisallowed));
}

void PrivacyHubController::SetFrontend(PrivacyHubDelegate* ptr) {
  frontend_ = ptr;
  if (camera_controller()) {
    camera_controller()->SetFrontend(frontend_);
  }
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

bool PrivacyHubController::UsingCameraLEDFallback() {
  if (g_scoped_led_fallback_for_testing) {
    return g_scoped_led_fallback_for_testing->value;
  }
  return using_camera_led_fallback_;
}

void PrivacyHubController::InitUsingCameraLEDFallback() {
  using_camera_led_fallback_ = CheckCameraLEDFallbackDirectly();
}

// static
bool PrivacyHubController::CheckCameraLEDFallbackDirectly() {
  // Check that the file created by the camera service exists.
  const base::FilePath kPath(
      "/run/camera/camera_ids_with_sw_privacy_switch_fallback");
  if (!base::PathExists(kPath) || !base::PathIsReadable(kPath)) {
    // The camera service should create the file always. However we keep this
    // for backward compatibility when deployed with an older version of the OS
    // and forward compatibility when the fallback is eventually dropped.
    return false;
  }
  int64_t file_size{};
  const bool file_size_read_success = base::GetFileSize(kPath, &file_size);
  CHECK(file_size_read_success);

  return (file_size != 0ll);
}

// static
bool PrivacyHubController::CrosToArcGeolocationPermissionMapping(
    GeolocationAccessLevel access_level) {
  switch (access_level) {
    case GeolocationAccessLevel::kAllowed:
      return true;
    case GeolocationAccessLevel::kOnlyAllowedForSystem:
    case GeolocationAccessLevel::kDisallowed:
      return false;
    default:
      NOTREACHED();
  }
}

CameraPrivacySwitchController*
PrivacyHubController::CameraSynchronizerForTest() {
  CHECK(camera_controller());
  return camera_controller();
}

ScopedLedFallbackForTesting::ScopedLedFallbackForTesting(bool value)
    : value(value) {
  CHECK(!g_scoped_led_fallback_for_testing);
  g_scoped_led_fallback_for_testing = this;
}

ScopedLedFallbackForTesting::~ScopedLedFallbackForTesting() {
  CHECK_EQ(this, g_scoped_led_fallback_for_testing);
  g_scoped_led_fallback_for_testing = nullptr;
}

}  // namespace ash
