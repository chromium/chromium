// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_tray_controller.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-shared.h"

namespace ash {

namespace {
VideoConferenceTrayController* g_controller_instance = nullptr;
}  // namespace

VideoConferenceTrayController::VideoConferenceTrayController() {
  DCHECK(!g_controller_instance);
  g_controller_instance = this;

  media::CameraHalDispatcherImpl::GetInstance()->AddCameraPrivacySwitchObserver(
      this);
}

VideoConferenceTrayController::~VideoConferenceTrayController() {
  DCHECK_EQ(this, g_controller_instance);
  g_controller_instance = nullptr;

  media::CameraHalDispatcherImpl::GetInstance()
      ->RemoveCameraPrivacySwitchObserver(this);
}

// static
VideoConferenceTrayController* VideoConferenceTrayController::Get() {
  return g_controller_instance;
}

void VideoConferenceTrayController::OnCameraSWPrivacySwitchStateChanged(
    cros::mojom::CameraPrivacySwitchState state) {
  for (auto* root_window_controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    DCHECK(root_window_controller);
    DCHECK(root_window_controller->GetStatusAreaWidget());

    root_window_controller->GetStatusAreaWidget()
        ->video_conference_tray()
        ->camera_icon()
        ->SetToggled(state == cros::mojom::CameraPrivacySwitchState::ON);
  }
}

}  // namespace ash