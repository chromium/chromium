// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"

#include "ash/shell.h"
#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ash/camera_presence_notifier.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"

namespace ash::privacy_hub_util {

namespace {
PrivacyHubController* ControllerIfAvailable() {
  if (!Shell::HasInstance()) {
    // Shell may not be available when used from a test.
    return nullptr;
  }
  Shell* const shell = Shell::Get();
  DCHECK(shell != nullptr);
  return shell->privacy_hub_controller();
}
}  // namespace

void SetFrontend(PrivacyHubDelegate* ptr) {
  PrivacyHubController* const controller = ControllerIfAvailable();
  if (controller != nullptr) {
    // Controller may not be available when used from a test.
    controller->set_frontend(ptr);
  }
}

cros::mojom::CameraPrivacySwitchState CameraHWSwitchState() {
  PrivacyHubController* const controller = ControllerIfAvailable();
  if (controller == nullptr) {
    return cros::mojom::CameraPrivacySwitchState::UNKNOWN;
  }
  return controller->camera_controller().HWSwitchState();
}

bool MicrophoneSwitchState() {
  return ui::MicrophoneMuteSwitchMonitor::Get()->microphone_mute_switch_on();
}

bool HasActiveInputDeviceForSimpleUsage() {
  return CrasAudioHandler::Get()->HasActiveInputDeviceForSimpleUsage();
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

}  // namespace ash::privacy_hub_util
