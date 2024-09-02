// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/video_conference/video_conference_ash_feature_client.h"

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/camera_mic/vm_camera_mic_manager.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

VideoConferenceAshFeatureClient* g_client_instance = nullptr;

constexpr char kCrostiniVmId[] = "Linux";
constexpr char kPluginVmId[] = "PluginVm";
constexpr char kBorealisId[] = "Borealis";

// Returns an "Id" as an identifier for the VmType.
std::string ToVideoConferenceAppId(VmCameraMicManager::VmType vm_type) {
  switch (vm_type) {
    case VmCameraMicManager::VmType::kCrostiniVm:
      return kCrostiniVmId;
    case VmCameraMicManager::VmType::kPluginVm:
      return kPluginVmId;
    case VmCameraMicManager::VmType::kBorealis:
      return kBorealisId;
  }
}

}  // namespace

VideoConferenceAshFeatureClient::VideoConferenceAshFeatureClient()
    : client_id_(base::UnguessableToken::Create()),
      status_(crosapi::mojom::VideoConferenceMediaUsageStatus::New(
          /*client_id=*/client_id_,
          /*has_media_app=*/false,
          /*has_camera_permission=*/false,
          /*has_microphone_permission=*/false,
          /*is_capturing_camera=*/false,
          /*is_capturing_microphone=*/false,
          /*is_capturing_screen=*/false)) {
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->RegisterCppClient(this, client_id_);

  CHECK(!g_client_instance);
  g_client_instance = this;

  // `VmCameraMicManager` may have updated camera/mic capturing states even
  // before `VmCameraMicManager` is initialized. Manually check the states at
  // initialization to ensure the states are up-to-date.
  VmCameraMicManager* vm_camera_mic_manager = VmCameraMicManager::Get();
  if (vm_camera_mic_manager) {
    const std::array<VmCameraMicManager::VmType, 3> vm_types{
        VmCameraMicManager::VmType::kCrostiniVm,
        VmCameraMicManager::VmType::kPluginVm,
        VmCameraMicManager::VmType::kBorealis};
    const std::array<VmCameraMicManager::DeviceType, 2> device_types{
        VmCameraMicManager::DeviceType::kMic,
        VmCameraMicManager::DeviceType::kCamera};

    for (VmCameraMicManager::VmType vm_type : vm_types) {
      for (VmCameraMicManager::DeviceType device_type : device_types) {
        if (vm_camera_mic_manager->IsDeviceActive(vm_type, device_type)) {
          OnVmDeviceUpdated(vm_type, device_type, true);
        }
      }
    }
  }
}

VideoConferenceAshFeatureClient::~VideoConferenceAshFeatureClient() {
  // C++ clients are responsible for manually calling |UnregisterClient| on the
  // manager when disconnecting.
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->UnregisterClient(client_id_);

  g_client_instance = nullptr;
}

void VideoConferenceAshFeatureClient::GetMediaApps(
    GetMediaAppsCallback callback) {
  std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> apps;

  for (const auto& [app_id, app_state] : id_to_app_state_) {
    const std::string app_name = GetAppName(app_id);

    apps.push_back(crosapi::mojom::VideoConferenceMediaAppInfo::New(
        /*id=*/app_state.token,
        /*last_activity_time=*/app_state.last_activity_time,
        /*is_capturing_camera=*/app_state.is_capturing_camera,
        /*is_capturing_microphone=*/app_state.is_capturing_microphone,
        /*is_capturing_screen=*/false,
        /*title=*/base::UTF8ToUTF16(app_name),
        /*url=*/std::nullopt,
        /*app_type=*/GetAppType(app_id)));
  }

  std::move(callback).Run(std::move(apps));
}

void VideoConferenceAshFeatureClient::ReturnToApp(
    const base::UnguessableToken& token,
    ReturnToAppCallback callback) {
  // Currently, for Vms, we treat the whole VM as one app, so it is not clear
  // which one to return to.
  std::move(callback).Run(true);
}

void VideoConferenceAshFeatureClient::SetSystemMediaDeviceStatus(
    crosapi::mojom::VideoConferenceMediaDevice device,
    bool disabled,
    SetSystemMediaDeviceStatusCallback callback) {
  switch (device) {
    case crosapi::mojom::VideoConferenceMediaDevice::kCamera:
      camera_system_disabled_ = disabled;
      std::move(callback).Run(true);
      return;
    case crosapi::mojom::VideoConferenceMediaDevice::kMicrophone:
      microphone_system_disabled_ = disabled;
      std::move(callback).Run(true);
      return;
    case crosapi::mojom::VideoConferenceMediaDevice::kUnusedDefault:
      std::move(callback).Run(false);
      return;
  }
}

void VideoConferenceAshFeatureClient::StopAllScreenShare() {}

void VideoConferenceAshFeatureClient::OnVmDeviceUpdated(
    VmCameraMicManager::VmType vm_type,
    VmCameraMicManager::DeviceType device_type,
    bool is_capturing) {
  const AppIdString& app_id = ToVideoConferenceAppId(vm_type);

  const bool is_already_tracked = base::Contains(id_to_app_state_, app_id);

  // We only want to start tracking a app if it starts to accessing
  // microphone/camera.
  if (!is_already_tracked && !is_capturing) {
    return;
  }

  AppState& state = GetOrAddAppState(app_id);
  const std::string app_name = GetAppName(app_id);

  if (device_type == VmCameraMicManager::DeviceType::kCamera) {
    state.is_capturing_camera = is_capturing;
  }

  if (device_type == VmCameraMicManager::DeviceType::kMic) {
    state.is_capturing_microphone = is_capturing;
  }

  MaybeRemoveApp(app_id);
  HandleMediaUsageUpdate();

  // This will be an AnchoredNudge, which is only visible if the tray is
  // visible; so we have to call this after HandleMediaUsageUpdate.
  if (device_type == VmCameraMicManager::DeviceType::kCamera && is_capturing &&
      camera_system_disabled_) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->video_conference_manager_ash()
        ->NotifyDeviceUsedWhileDisabled(
            crosapi::mojom::VideoConferenceMediaDevice::kCamera,
            base::UTF8ToUTF16(app_name), base::DoNothingAs<void(bool)>());
  }

  if (device_type == VmCameraMicManager::DeviceType::kMic && is_capturing &&
      microphone_system_disabled_) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->video_conference_manager_ash()
        ->NotifyDeviceUsedWhileDisabled(
            crosapi::mojom::VideoConferenceMediaDevice::kMicrophone,
            base::UTF8ToUTF16(app_name), base::DoNothingAs<void(bool)>());
  }
}

// static
VideoConferenceAshFeatureClient* VideoConferenceAshFeatureClient::Get() {
  return g_client_instance;
}

// For Ash Features, we simply keep the app_id and app_name as the same.
std::string VideoConferenceAshFeatureClient::GetAppName(
    const AppIdString& app_id) {
  return app_id;
}

// Get current camera/microphone permission of the `app_id`.
VideoConferenceAshFeatureClient::VideoConferencePermissions
VideoConferenceAshFeatureClient::GetAppPermission(const AppIdString& app_id) {
  VideoConferencePermissions permissions{false, false};

  // Get permission from prefs based in the app_id.
  auto* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  if (app_id == kCrostiniVmId) {
    permissions.has_microphone_permission =
        prefs->GetBoolean(crostini::prefs::kCrostiniMicAllowed);
  }
  if (app_id == kBorealisId) {
    permissions.has_microphone_permission =
        prefs->GetBoolean(borealis::prefs::kBorealisMicAllowed);
  }
  if (app_id == kPluginVmId) {
    permissions.has_camera_permission =
        prefs->GetBoolean(plugin_vm ::prefs::kPluginVmCameraAllowed);
    permissions.has_microphone_permission =
        prefs->GetBoolean(plugin_vm ::prefs::kPluginVmMicAllowed);
  }
  return permissions;
}

crosapi::mojom::VideoConferenceAppType
VideoConferenceAshFeatureClient::GetAppType(const AppIdString& app_id) {
  if (app_id == kCrostiniVmId) {
    return crosapi::mojom::VideoConferenceAppType::kCrostiniVm;
  }

  if (app_id == kPluginVmId) {
    return crosapi::mojom::VideoConferenceAppType::kPluginVm;
  }

  if (app_id == kBorealisId) {
    return crosapi::mojom::VideoConferenceAppType::kBorealis;
  }

  return crosapi::mojom::VideoConferenceAppType::kAshClientUnknown;
}

VideoConferenceAshFeatureClient::AppState&
VideoConferenceAshFeatureClient::GetOrAddAppState(const std::string& app_id) {
  if (!base::Contains(id_to_app_state_, app_id)) {
    id_to_app_state_[app_id] = AppState{base::UnguessableToken::Create(),
                                        base::Time::Now(), false, false};
  }
  return id_to_app_state_[app_id];
}

void VideoConferenceAshFeatureClient::MaybeRemoveApp(
    const AppIdString& app_id) {
  if (!id_to_app_state_[app_id].is_capturing_microphone &&
      !id_to_app_state_[app_id].is_capturing_camera) {
    id_to_app_state_.erase(app_id);
  }
}

void VideoConferenceAshFeatureClient::HandleMediaUsageUpdate() {
  crosapi::mojom::VideoConferenceMediaUsageStatusPtr new_status =
      crosapi::mojom::VideoConferenceMediaUsageStatus::New();
  new_status->client_id = client_id_;
  new_status->has_media_app = !id_to_app_state_.empty();

  for (const auto& [app_id, app_state] : id_to_app_state_) {
    new_status->is_capturing_camera |= app_state.is_capturing_camera;
    new_status->is_capturing_microphone |= app_state.is_capturing_microphone;

    VideoConferencePermissions permissions = GetAppPermission(app_id);
    new_status->has_camera_permission |= permissions.has_camera_permission;
    new_status->has_microphone_permission |=
        permissions.has_microphone_permission;
  }

  // If `status` equals the previously sent status, don't notify manager.
  if (new_status.Equals(status_)) {
    return;
  }
  status_ = new_status->Clone();

  auto callback = base::BindOnce([](bool success) {
    if (!success) {
      LOG(ERROR)
          << "VideoConferenceManager::NotifyMediaUsageUpdate did not succeed.";
    }
  });
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->NotifyMediaUsageUpdate(std::move(new_status), std::move(callback));
}

}  // namespace ash
