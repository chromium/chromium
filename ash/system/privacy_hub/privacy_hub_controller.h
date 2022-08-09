// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"

class PrefRegistrySimple;

namespace ash {

class ASH_EXPORT PrivacyHubController {
 public:
  PrivacyHubController();

  PrivacyHubController(const PrivacyHubController&) = delete;
  PrivacyHubController& operator=(const PrivacyHubController&) = delete;

  ~PrivacyHubController();

  CameraPrivacySwitchController* CameraControllerForTest() {
    return &camera_controller_;
  }

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  CameraPrivacySwitchController camera_controller_;
  MicrophonePrivacySwitchController microphone_controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_CONTROLLER_H_
