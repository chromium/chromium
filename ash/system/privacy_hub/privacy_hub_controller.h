// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_CONTROLLER_H_

#include "ash/ash_export.h"
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

class ASH_EXPORT PrivacyHubController {
 public:
  // This enum defines the access levels of the signals of the Privacy Hub
  // features (namely microphone, camera and geolocation) for the entire
  // ChromeOS ecosystem.
  // Don't modify or reorder the enum elements. New values can be added at the
  // end. These values shall be in sync with the
  // DeviceLoginScreenGeolocationAccessLevelProto::GeolocationAccessLevel.
  enum class AccessLevel {
    kDisallowed = 0,
    kAllowed = 1,
    kMaxValue = kAllowed,
  };

  PrivacyHubController(base::PassKey<PrivacyHubController>);

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

  // Gets the geolocation controller if available.
  GeolocationPrivacySwitchController* geolocation_controller();

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Sets the frontend adapter (to be used from webui)
  void set_frontend(PrivacyHubDelegate* ptr) { frontend_ = ptr; }

  // Returns the adapter that can be used to modify the frontend
  PrivacyHubDelegate* frontend() { return frontend_; }

 private:
  std::unique_ptr<CameraPrivacySwitchController> camera_controller_;
  std::unique_ptr<CameraPrivacySwitchDisabled> camera_disabled_;
  std::unique_ptr<MicrophonePrivacySwitchController> microphone_controller_;
  std::unique_ptr<SpeakOnMuteDetectionPrivacySwitchController>
      speak_on_mute_controller_;
  std::unique_ptr<GeolocationPrivacySwitchController>
      geolocation_switch_controller_;
  raw_ptr<PrivacyHubDelegate> frontend_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_CONTROLLER_H_
