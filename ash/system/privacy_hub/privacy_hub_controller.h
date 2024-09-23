// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/public/cpp/privacy_hub_delegate.h"
#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "ash/system/privacy_hub/geolocation_privacy_switch_controller.h"
#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"
#include "ash/system/privacy_hub/speak_on_mute_detection_privacy_switch_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "base/values.h"

class PrefRegistrySimple;

namespace ash {

// Used to override the value of the LED Fallback value in tests.
// Should not be nested.
// TODO(b/289510726): remove when all cameras fully support the software
// switch.
class ASH_EXPORT ScopedLedFallbackForTesting {
 public:
  explicit ScopedLedFallbackForTesting(bool value);

  ScopedLedFallbackForTesting(const ScopedLedFallbackForTesting&) = delete;
  ScopedLedFallbackForTesting& operator=(const ScopedLedFallbackForTesting&) =
      delete;

  ~ScopedLedFallbackForTesting();

  const bool value;
};

class ASH_EXPORT PrivacyHubController {
 public:
  explicit PrivacyHubController(base::PassKey<PrivacyHubController>);

  PrivacyHubController(const PrivacyHubController&) = delete;
  PrivacyHubController& operator=(const PrivacyHubController&) = delete;

  ~PrivacyHubController();

  // Creates the PrivacyHub controller with the appropriate sub-components based
  // on the feature flags.
  static std::unique_ptr<PrivacyHubController> CreatePrivacyHubController();

  // Returns the PrivacyHubController instance from the Shell if it exists,
  // otherwise returns nullptr.
  static PrivacyHubController* Get();

  // Gets the camera controller if available.
  CameraPrivacySwitchController* camera_controller();

  // Gets the microphone controller if available.
  MicrophonePrivacySwitchController* microphone_controller();

  // Gets the speak-on-mute controller if available.
  SpeakOnMuteDetectionPrivacySwitchController* speak_on_mute_controller();

  // Gets the geolocation controller.
  GeolocationPrivacySwitchController* geolocation_controller();

  CameraPrivacySwitchController* CameraSynchronizerForTest();

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Sets the frontend adapter (to be used from webui)
  void SetFrontend(PrivacyHubDelegate* ptr);

  // Returns the adapter that can be used to modify the frontend
  PrivacyHubDelegate* frontend() { return frontend_; }

  // Checks if we use the fallback solution for the camera LED.
  // Returns the boolean value via callback.
  // (go/privacy-hub:camera-led-fallback).
  // TODO(b/289510726): remove when all cameras fully support the software
  // switch.
  bool UsingCameraLEDFallback();

  // This checks whether the LED fallback mechanism is used directly (using the
  // filesystem). Is used during initialization and can be used externally in
  // case that the controller object does not exist. (E.g. to initialize the
  // PrivacyHubNotificationController, which exists even if privacy hub is
  // disabled). Should be used only in that case to avoid repeated blocking
  // calls to the filesystem.
  static bool CheckCameraLEDFallbackDirectly();

  // ARC++ geolocation toggle is migrating to ChromeOS. ChromeOS has 3 states
  // for geolocation access level, while ARC++ has 2. These function implements
  // the mapping from ChromeOS's `GeolocationAccessLevel`s to ARC++ boolean
  // values.
  static bool CrosToArcGeolocationPermissionMapping(
      GeolocationAccessLevel access_level);

 private:
  // Used for first time initialization of the cached value.
  // Can be called only once.
  void InitUsingCameraLEDFallback();

  std::unique_ptr<CameraPrivacySwitchController> camera_controller_;
  std::unique_ptr<MicrophonePrivacySwitchController> microphone_controller_;
  std::unique_ptr<SpeakOnMuteDetectionPrivacySwitchController>
      speak_on_mute_controller_;
  std::unique_ptr<GeolocationPrivacySwitchController>
      geolocation_switch_controller_;
  raw_ptr<PrivacyHubDelegate> frontend_ = nullptr;
  bool using_camera_led_fallback_ = true;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_CONTROLLER_H_
