// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/fake_video_conference_tray_controller.h"

#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-shared.h"

namespace ash {

FakeVideoConferenceTrayController::FakeVideoConferenceTrayController() =
    default;

void FakeVideoConferenceTrayController::SetCameraSoftwareMuted(
    bool mute_camera) {
  camera_soft_muted_ = mute_camera;
  OnCameraSWPrivacySwitchStateChanged(
      camera_soft_muted_ ? cros::mojom::CameraPrivacySwitchState::ON
                         : cros::mojom::CameraPrivacySwitchState::OFF);
}

}  // namespace ash