// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/video_conference/video_conference_client_base.h"

#include "base/check_deref.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"

namespace ash {

namespace {

crosapi::mojom::VideoConferenceAppType ToVideoConferenceAppType(
    apps::AppType app_type) {
  switch (app_type) {
    case apps::AppType::kArc:
      return crosapi::mojom::VideoConferenceAppType::kArcApp;
    case apps::AppType::kChromeApp:
      return crosapi::mojom::VideoConferenceAppType::kChromeApp;
    case apps::AppType::kWeb:
      return crosapi::mojom::VideoConferenceAppType::kWebApp;
    case apps::AppType::kExtension:
      return crosapi::mojom::VideoConferenceAppType::kChromeExtension;
    case apps::AppType::kCrostini:
      return crosapi::mojom::VideoConferenceAppType::kCrostiniVm;
    case apps::AppType::kPluginVm:
      return crosapi::mojom::VideoConferenceAppType::kPluginVm;
    case apps::AppType::kBorealis:
      return crosapi::mojom::VideoConferenceAppType::kBorealis;
    default:
      return crosapi::mojom::VideoConferenceAppType::kAppServiceUnknown;
  }
}

}  // namespace

VideoConferenceClientBase::VideoConferenceClientBase(
    VideoConferenceManagerAsh* video_conference_manager_ash)
    : client_id_(base::UnguessableToken::Create()),
      status_(client_id_),
      video_conference_manager_ash_(CHECK_DEREF(video_conference_manager_ash)) {
  video_conference_manager_ash_->RegisterCppClient(this, client_id_);
}

VideoConferenceClientBase::~VideoConferenceClientBase() {
  video_conference_manager_ash_->UnregisterClient(client_id_);
}

void VideoConferenceClientBase::GetMediaApps(GetMediaAppsCallback callback) {
  std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> apps;

  for (const auto& [app_id, app_state] : id_to_app_state_) {
    const std::string app_name = GetAppName(app_id);
    // app_name should not be empty.
    if (app_name.empty()) {
      continue;
    }

    apps.push_back(crosapi::mojom::VideoConferenceMediaAppInfo::New(
        /*id=*/app_state.token,
        /*last_activity_time=*/app_state.last_activity_time,
        /*is_capturing_camera=*/app_state.is_capturing_camera,
        /*is_capturing_microphone=*/app_state.is_capturing_microphone,
        /*is_capturing_screen=*/false,
        /*title=*/base::UTF8ToUTF16(app_name),
        /*url=*/std::nullopt,
        /*app_type=*/ToVideoConferenceAppType(GetAppType(app_id))));
  }

  std::move(callback).Run(std::move(apps));
}
void VideoConferenceClientBase::SetSystemMediaDeviceStatus(
    VideoConferenceMediaDevice device,
    bool enabled,
    SetSystemMediaDeviceStatusCallback callback) {
  switch (device) {
    case VideoConferenceMediaDevice::kCamera:
      camera_system_enabled_ = enabled;
      std::move(callback).Run(true);
      return;
    case VideoConferenceMediaDevice::kMicrophone:
      microphone_system_enabled_ = enabled;
      std::move(callback).Run(true);
      return;
  }
}

void VideoConferenceClientBase::HandleMediaUsageUpdate() {
  VideoConferenceMediaUsageStatus new_status(client_id_);
  new_status.state.has_media_app = !id_to_app_state_.empty();

  for (const auto& [app_id, app_state] : id_to_app_state_) {
    new_status.state.is_capturing_camera |= app_state.is_capturing_camera;
    new_status.state.is_capturing_microphone |=
        app_state.is_capturing_microphone;

    VideoConferencePermissions permissions = GetAppPermission(app_id);
    new_status.state.has_camera_permission |= permissions.has_camera_permission;
    new_status.state.has_microphone_permission |=
        permissions.has_microphone_permission;
  }

  // If `status` equals the previously sent status, don't notify manager.
  if (new_status == status_) {
    return;
  }
  status_ = new_status;

  auto callback = base::BindOnce([](bool success) {
    if (!success) {
      LOG(ERROR)
          << "VideoConferenceManager::NotifyMediaUsageUpdate did not succeed.";
    }
  });
  video_conference_manager_ash_->NotifyMediaUsageUpdate(std::move(new_status),
                                                        std::move(callback));
}

}  // namespace ash
