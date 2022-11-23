// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/video_conference/video_conference_tray_controller_impl.h"

#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-shared.h"

namespace ash {

VideoConferenceTrayControllerImpl::VideoConferenceTrayControllerImpl() =
    default;

VideoConferenceTrayControllerImpl::~VideoConferenceTrayControllerImpl() =
    default;

void VideoConferenceTrayControllerImpl::SetCameraSoftwareMuted(
    bool mute_camera) {
  media::CameraHalDispatcherImpl::GetInstance()->SetCameraSWPrivacySwitchState(
      mute_camera ? cros::mojom::CameraPrivacySwitchState::ON
                  : cros::mojom::CameraPrivacySwitchState::OFF);
}

}  // namespace ash
