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
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "geolocation_privacy_switch_controller.h"

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

  PrivacyHubController();

  PrivacyHubController(const PrivacyHubController&) = delete;
  PrivacyHubController& operator=(const PrivacyHubController&) = delete;

  ~PrivacyHubController();

  CameraPrivacySwitchController& camera_controller() {
    return camera_controller_;
  }
  MicrophonePrivacySwitchController& microphone_controller() {
    return microphone_controller_;
  }
  GeolocationPrivacySwitchController& geolocation_controller() {
    return geolocation_switch_controller_;
  }

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Sets the frontend adapter (to be used from webui)
  void set_frontend(PrivacyHubDelegate* ptr) { frontend_ = ptr; }

  // Returns the adapter that can be used to modify the frontend
  PrivacyHubDelegate* frontend() { return frontend_; }

 private:
  CameraPrivacySwitchController camera_controller_;
  MicrophonePrivacySwitchController microphone_controller_;
  GeolocationPrivacySwitchController geolocation_switch_controller_;
  raw_ptr<PrivacyHubDelegate> frontend_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_CONTROLLER_H_
