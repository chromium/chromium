// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/video_conference/video_conference_ash_feature_client.h"

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/camera_mic/vm_camera_mic_manager.h"
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

VideoConferenceAshFeatureClient::VideoConferenceAshFeatureClient(
    VideoConferenceManagerAsh* video_conference_manager_ash)
    : VideoConferenceClientBase(video_conference_manager_ash) {
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
  g_client_instance = nullptr;
}

void VideoConferenceAshFeatureClient::ReturnToApp(
    const base::UnguessableToken& token,
    ReturnToAppCallback callback) {
  // Currently, for Vms, we treat the whole VM as one app, so it is not clear
  // which one to return to.
  std::move(callback).Run(true);
}

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
      !camera_system_enabled_) {
    video_conference_manager_ash_->NotifyDeviceUsedWhileDisabled(
        crosapi::mojom::VideoConferenceMediaDevice::kCamera,
        base::UTF8ToUTF16(app_name), base::DoNothingAs<void(bool)>());
  }

  if (device_type == VmCameraMicManager::DeviceType::kMic && is_capturing &&
      !microphone_system_enabled_) {
    video_conference_manager_ash_->NotifyDeviceUsedWhileDisabled(
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

apps::AppType VideoConferenceAshFeatureClient::GetAppType(
    const AppIdString& app_id) {
  if (app_id == kCrostiniVmId) {
    return apps::AppType::kCrostini;
  }

  if (app_id == kPluginVmId) {
    return apps::AppType::kPluginVm;
  }

  if (app_id == kBorealisId) {
    return apps::AppType::kBorealis;
  }

  return apps::AppType::kUnknown;
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

}  // namespace ash
