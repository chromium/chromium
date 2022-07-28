// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_CAMERA_PRIVACY_SWITCH_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_HUB_CAMERA_PRIVACY_SWITCH_CONTROLLER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "components/prefs/pref_change_registrar.h"

namespace ash {

// Enumeration of camera switch states.
enum class CameraSWPrivacySwitchSetting { kDisabled, kEnabled };

// Abstraction for communication with the backend camera switch.
class CameraPrivacySwitchAPI {
 public:
  virtual ~CameraPrivacySwitchAPI() = default;

  // Sets the SW Privacy Switch value in the CrOS Camera service.
  virtual void SetCameraSWPrivacySwitch(CameraSWPrivacySwitchSetting) = 0;
};

// A singleton class that acts as a bridge between Privacy Hub UI and backend.
// It listens on both ends and changes UI to reflect changes in
// the backend and notifies the backend of changes in the user
// preference setting.
class ASH_EXPORT CameraPrivacySwitchController : public SessionObserver {
 public:
  CameraPrivacySwitchController();

  CameraPrivacySwitchController(const CameraPrivacySwitchController&) = delete;
  CameraPrivacySwitchController& operator=(
      const CameraPrivacySwitchController&) = delete;

  ~CameraPrivacySwitchController() override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // Handles user toggling the camera switch on Privacy Hub UI.
  void OnPreferenceChanged(const std::string& pref_name);

  void SetCameraPrivacySwitchAPIForTest(
      std::unique_ptr<CameraPrivacySwitchAPI> switch_api);

 private:
  // Retrieves the current value of the user pref.
  CameraSWPrivacySwitchSetting GetUserSwitchPreference();

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::unique_ptr<CameraPrivacySwitchAPI> switch_api_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_HUB_CAMERA_PRIVACY_SWITCH_CONTROLLER_H_
