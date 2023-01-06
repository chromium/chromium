// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/video_conference/video_conference_app_service_client.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/instance_registry.h"

namespace ash {

VideoConferenceAppServiceClient::VideoConferenceAppServiceClient() {
  ChromeUserManager::Get()->AddSessionStateObserver(this);
}

VideoConferenceAppServiceClient::~VideoConferenceAppServiceClient() {
  ChromeUserManager::Get()->RemoveSessionStateObserver(this);
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
    instance->Window()->Show();
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

void VideoConferenceAppServiceClient::ActiveUserChanged(
    user_manager::User* active_user) {
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(active_user);
  auto* ash_proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(ash_proxy);
  instance_registry_ = &ash_proxy->InstanceRegistry();
  app_registry_ = &ash_proxy->AppRegistryCache();
}

std::string VideoConferenceAppServiceClient::GetAppName(
    const AppIdString& app_id) {
  std::string app_name;
  app_registry_->ForOneApp(app_id, [&app_name](const apps::AppUpdate& update) {
    app_name = update.Name();
  });
  return app_name;
}

}  // namespace ash
