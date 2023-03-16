// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/video_conference/video_conference_app_service_client.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace {
VideoConferenceAppServiceClient* g_client_instance = nullptr;
}  // namespace

VideoConferenceAppServiceClient::VideoConferenceAppServiceClient()
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

  session_observation_.Observe(Shell::Get()->session_controller());

  // Initialize with current session state.
  OnSessionStateChanged(Shell::Get()->session_controller()->GetSessionState());

  g_client_instance = this;
}

VideoConferenceAppServiceClient::~VideoConferenceAppServiceClient() {
  // C++ clients are responsible for manually calling |UnregisterClient| on the
  // manager when disconnecting.
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->UnregisterClient(client_id_);

  g_client_instance = nullptr;
}

void VideoConferenceAppServiceClient::GetMediaApps(
    GetMediaAppsCallback callback) {
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
        /*title=*/base::UTF8ToUTF16(app_name), /*url=*/absl::nullopt));
  }

  std::move(callback).Run(std::move(apps));
}

void VideoConferenceAppServiceClient::ReturnToApp(
    const base::UnguessableToken& token,
    ReturnToAppCallback callback) {
  // Go through `id_to_app_state_` to find possible app to reactivate.
  // This loop is inevitable unless we use multiple maps which also makes things
  // complicated.
  AppIdString app_id;
  for (const auto& [id, app_state] : id_to_app_state_) {
    if (app_state.token == token) {
      app_id = id;
      break;
    }
  }

  if (app_id.empty()) {
    // This will happen very frequently; this is not an error, but expected
    // behavior. This indicates that the app represented by this id does not
    // belong to this client.
    std::move(callback).Run(false);
    return;
  }

  for (auto* instance : instance_registry_->GetInstances(app_id)) {
    // This is required in unit tests to reactivate an app.
    instance->Window()->Show();
    // This is required in virtual desktop to reactivate an arc++ app.
    instance->Window()->Focus();
  }
  std::move(callback).Run(true);
}

void VideoConferenceAppServiceClient::SetSystemMediaDeviceStatus(
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

void VideoConferenceAppServiceClient::OnCapabilityAccessUpdate(
    const apps::CapabilityAccessUpdate& update) {
  const AppIdString& app_id = update.AppId();
  // Only track Arc++ apps for now. All other apps are tracked by
  // VideoConferenceManagerClientImpl. We should only expand to more types after
  // confirming its compatibility with VideoConferenceManagerClientImpl.
  if (GetAppType(app_id) != apps::AppType::kArc) {
    return;
  }

  // For now, we only care about camera/microphone accessing.
  if (!update.CameraChanged() && !update.MicrophoneChanged()) {
    return;
  }

  const bool is_capturing_camera = update.Camera().value_or(false);
  const bool is_capturing_microphone = update.Microphone().value_or(false);
  const bool is_already_tracked = base::Contains(id_to_app_state_, app_id);

  // We only want to start tracking a app if it starts to accessing
  // microphone/camera.
  if (!is_capturing_camera && !is_capturing_microphone && !is_already_tracked) {
    return;
  }

  if (!is_already_tracked && ::video_conference::ShouldSkipId(app_id)) {
    return;
  }

  AppState& state = GetOrAddAppState(app_id);
  const std::string app_name = GetAppName(app_id);

  if (update.CameraChanged()) {
    state.is_capturing_camera = is_capturing_camera;

    if (is_capturing_camera && camera_system_disabled_) {
      crosapi::CrosapiManager::Get()
          ->crosapi_ash()
          ->video_conference_manager_ash()
          ->NotifyDeviceUsedWhileDisabled(
              crosapi::mojom::VideoConferenceMediaDevice::kCamera,
              base::UTF8ToUTF16(app_name), base::DoNothingAs<void(bool)>());
    }
  }

  if (update.MicrophoneChanged()) {
    state.is_capturing_microphone = is_capturing_microphone;

    if (is_capturing_microphone && microphone_system_disabled_) {
      crosapi::CrosapiManager::Get()
          ->crosapi_ash()
          ->video_conference_manager_ash()
          ->NotifyDeviceUsedWhileDisabled(
              crosapi::mojom::VideoConferenceMediaDevice::kMicrophone,
              base::UTF8ToUTF16(app_name), base::DoNothingAs<void(bool)>());
    }
  }

  HandleMediaUsageUpdate();
}

void VideoConferenceAppServiceClient::OnAppCapabilityAccessCacheWillBeDestroyed(
    apps::AppCapabilityAccessCache* cache) {
  app_capability_observation_.Reset();
  capability_cache_ = nullptr;
}

void VideoConferenceAppServiceClient::OnInstanceUpdate(
    const apps::InstanceUpdate& update) {
  const AppIdString& app_id = update.AppId();

  // An instance of app_id is about to be destructed.
  if (update.IsDestruction() &&
      instance_registry_->GetInstances(app_id).size() <= 1) {
    // The last instance maybe removed after the OnInstanceUpdate, so we post a
    // task to also remove the app_id if the instance is indeed removed.
    base::SequencedTaskRunner::GetCurrentDefault()->PostNonNestableTask(
        FROM_HERE,
        base::BindOnce(&VideoConferenceAppServiceClient::MaybeRemoveApp,
                       weak_ptr_factory_.GetWeakPtr(), app_id));
    return;
  }

  if (update.StateChanged() &&
      update.State() == apps::InstanceState::kVisible &&
      base::Contains(id_to_app_state_, app_id)) {
    id_to_app_state_[app_id].last_activity_time = update.LastUpdatedTime();
    return;
  }
}

void VideoConferenceAppServiceClient::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* cache) {
  instance_registry_observation_.Reset();
  instance_registry_ = nullptr;
}

void VideoConferenceAppServiceClient::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (state != session_manager::SessionState::ACTIVE) {
    return;
  }

  Profile* profile = ProfileManager::GetActiveUserProfile();
  user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();

  // Skip the profile that AppServiceProxy is not available.
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile) ||
      !active_user) {
    instance_registry_observation_.Reset();
    app_capability_observation_.Reset();
    return;
  }

  auto* ash_proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  instance_registry_ = &ash_proxy->InstanceRegistry();
  app_registry_ = &ash_proxy->AppRegistryCache();

  instance_registry_observation_.Reset();
  instance_registry_observation_.Observe(instance_registry_);

  capability_cache_ =
      apps::AppCapabilityAccessCacheWrapper::Get().GetAppCapabilityAccessCache(
          active_user->GetAccountId());
  app_capability_observation_.Reset();
  app_capability_observation_.Observe(capability_cache_);
}

VideoConferenceAppServiceClient*
VideoConferenceAppServiceClient::GetForTesting() {
  return g_client_instance;
}

std::string VideoConferenceAppServiceClient::GetAppName(
    const AppIdString& app_id) {
  std::string app_name;
  app_registry_->ForOneApp(app_id, [&app_name](const apps::AppUpdate& update) {
    app_name = update.Name();
  });
  return app_name;
}

// Get current camera/microphone permission of the `app_id`.
VideoConferenceAppServiceClient::VideoConferencePermissions
VideoConferenceAppServiceClient::GetAppPermission(const AppIdString& app_id) {
  VideoConferencePermissions permissions;

  app_registry_->ForOneApp(app_id, [&permissions](
                                       const apps::AppUpdate& update) {
    for (const auto& permission : update.Permissions()) {
      if (permission->permission_type == apps::PermissionType::kCamera) {
        permissions.has_camera_permission = permission->IsPermissionEnabled();
      }
      if (permission->permission_type == apps::PermissionType::kMicrophone) {
        permissions.has_microphone_permission =
            permission->IsPermissionEnabled();
      }
    }
  });
  return permissions;
}

apps::AppType VideoConferenceAppServiceClient::GetAppType(
    const AppIdString& app_id) {
  apps::AppType type = apps::AppType::kUnknown;
  app_registry_->ForOneApp(app_id, [&type](const apps::AppUpdate& update) {
    type = update.AppType();
  });
  return type;
}

VideoConferenceAppServiceClient::AppState&
VideoConferenceAppServiceClient::GetOrAddAppState(const std::string& app_id) {
  if (!base::Contains(id_to_app_state_, app_id)) {
    id_to_app_state_[app_id] = AppState{base::UnguessableToken::Create(),
                                        base::Time::Now(), false, false};
  }
  return id_to_app_state_[app_id];
}

void VideoConferenceAppServiceClient::MaybeRemoveApp(
    const AppIdString& app_id) {
  // The app_id should also be removed if:
  // (1) all running instances of app_id are destructed.
  // (2) in an extreme case, the instance_registry_ is reset.
  if (!instance_registry_ || !instance_registry_->ContainsAppId(app_id)) {
    id_to_app_state_.erase(app_id);
    HandleMediaUsageUpdate();
  }
}

void VideoConferenceAppServiceClient::HandleMediaUsageUpdate() {
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
