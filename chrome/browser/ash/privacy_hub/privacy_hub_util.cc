// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"

#include <string>

#include "ash/shell.h"
#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ash/camera_presence_notifier.h"

namespace ash::privacy_hub_util {

void SetFrontend(PrivacyHubDelegate* ptr) {
  PrivacyHubController* const controller = PrivacyHubController::Get();
  if (controller != nullptr) {
    // Controller may not be available when used from a test.
    controller->set_frontend(ptr);
  }
}

bool MicrophoneSwitchState() {
  return ui::MicrophoneMuteSwitchMonitor::Get()->microphone_mute_switch_on();
}

void SetUpCameraCountObserver() {
  DCHECK(Shell::Get());
  if (PrivacyHubController* privacy_hub_controller =
          Shell::Get()->privacy_hub_controller()) {
    CameraPrivacySwitchController& camera_controller =
        privacy_hub_controller->camera_controller();
    base::RepeatingCallback<void(int)> update_camera_count_in_privacy_hub =
        base::BindRepeating(
            [](CameraPrivacySwitchController* controller, int camera_count) {
              controller->OnCameraCountChanged(camera_count);
            },
            &camera_controller);
    auto notifier = std::make_unique<CameraPresenceNotifier>(
        std::move(update_camera_count_in_privacy_hub));
    notifier->Start();

    static const char kUserDataKey = '\0';
    camera_controller.SetUserData(&kUserDataKey, std::move(notifier));
  }
}

// Notifies the Privacy Hub controller.
void TrackGeolocationAttempted(const std::string& name) {
  PrivacyHubController* controller = PrivacyHubController::Get();
  // TODO(b/288854399): Remove this if.
  if (controller) {
    controller->geolocation_controller().TrackGeolocationAttempted(name);
  }
}

// Notifies the Privacy Hub controller.
void TrackGeolocationRelinquished(const std::string& name) {
  PrivacyHubController* controller = PrivacyHubController::Get();
  if (controller) {
    controller->geolocation_controller().TrackGeolocationRelinquished(name);
  }
}

}  // namespace ash::privacy_hub_util
