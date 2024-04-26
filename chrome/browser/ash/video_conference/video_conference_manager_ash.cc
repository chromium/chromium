// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/webui/vc_background_ui/url_constants.h"
#include "base/barrier_callback.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/video_conference/video_conference_client_wrapper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {

VideoConferenceManagerAsh::VideoConferenceManagerAsh() {
  if (ash::features::IsVideoConferenceEnabled()) {
    GetTrayController()->Initialize(this);
  }
}

VideoConferenceManagerAsh::~VideoConferenceManagerAsh() = default;

void VideoConferenceManagerAsh::RegisterCppClient(
    crosapi::mojom::VideoConferenceManagerClient* client,
    const base::UnguessableToken& client_id) {
  client_id_to_wrapper_.try_emplace(client_id, client, this);
}

void VideoConferenceManagerAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::VideoConferenceManager> receiver) {
  // At present the pending receiver should only be from lacros-chrome but
  // in the future there will be other mojo clients as well.
  receivers_.Add(this, std::move(receiver));
}

void VideoConferenceManagerAsh::GetMediaApps(
    base::OnceCallback<void(MediaApps)> ui_callback) {
  // Because the lacros client communicates over mojo, the GetMediaApps method
  // on all clients is asynchronous and passes results via callbacks. The
  // |done_callback| defined below gets the collected results from the
  // BarrierCallback, flattens them into a single vector, and passes them
  // to the |ui_callback| provided to VideoConferenceManagerAsh::GetMediaApps.
  base::OnceCallback<void(std::vector<MediaApps>)> done_callback =
      base::BindOnce(
          [](base::OnceCallback<void(MediaApps)> callback,
             std::vector<MediaApps> apps_from_all_clients) {
            MediaApps apps;

            for (MediaApps& apps_from_client : apps_from_all_clients) {
              for (auto& app : apps_from_client) {
                apps.push_back(std::move(app));
              }
            }

            // Sort all apps based on last activity time.
            std::sort(
                apps.begin(), apps.end(),
                [](const crosapi::mojom::VideoConferenceMediaAppInfoPtr& app1,
                   const crosapi::mojom::VideoConferenceMediaAppInfoPtr& app2) {
                  return app1->last_activity_time > app2->last_activity_time;
                });

            // Call bound |ui_callback| with aggregated app info structs.
            std::move(callback).Run(std::move(apps));
          },
          std::move(ui_callback));

  const auto barrier_callback = base::BarrierCallback<MediaApps>(
      client_id_to_wrapper_.size(), std::move(done_callback));

  for (auto& [_, client_wrapper] : client_id_to_wrapper_) {
    client_wrapper.GetMediaApps(barrier_callback);
  }
}

void VideoConferenceManagerAsh::ReturnToApp(const base::UnguessableToken& id) {
  for (auto& [_, client_wrapper] : client_id_to_wrapper_) {
    client_wrapper.ReturnToApp(id, base::DoNothing());
  }
}

void VideoConferenceManagerAsh::SetSystemMediaDeviceStatus(
    crosapi::mojom::VideoConferenceMediaDevice device,
    bool disabled) {
  for (auto& [_, client_wrapper] : client_id_to_wrapper_) {
    client_wrapper.SetSystemMediaDeviceStatus(
        device, disabled, base::BindOnce([](bool success) {
          if (!success) {
            LOG(ERROR)
                << "VideoConferenceClient::SetSystemMediaDeviceStatus was "
                   "unsuccessful.";
          }
        }));
  }
}

void VideoConferenceManagerAsh::StopAllScreenShare() {
  for (auto& [_, client_wrapper] : client_id_to_wrapper_) {
    client_wrapper.StopAllScreenShare();
  }
}

void VideoConferenceManagerAsh::CreateBackgroundImage() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  ash::SystemAppLaunchParams params;
  params.launch_source = apps::LaunchSource::kFromShelf;
  ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::VC_BACKGROUND,
                               params);
}

void VideoConferenceManagerAsh::NotifyMediaUsageUpdate(
    crosapi::mojom::VideoConferenceMediaUsageStatusPtr status,
    NotifyMediaUsageUpdateCallback callback) {
  if (auto it = client_id_to_wrapper_.find(status->client_id);
      it != client_id_to_wrapper_.end()) {
    it->second.state() = {
        .has_media_app = status->has_media_app,
        .has_camera_permission = status->has_camera_permission,
        .has_microphone_permission = status->has_microphone_permission,
        .is_capturing_camera = status->is_capturing_camera,
        .is_capturing_microphone = status->is_capturing_microphone,
        .is_capturing_screen = status->is_capturing_screen};
  } else {
    LOG(ERROR) << "VideoConferenceManagerAsh::NotifyMediaUsageUpdate client_id "
                  "does not exist.";
    std::move(callback).Run(false);
    return;
  }

  SendUpdatedState();
  std::move(callback).Run(true);
}

void VideoConferenceManagerAsh::RegisterMojoClient(
    mojo::PendingRemote<crosapi::mojom::VideoConferenceManagerClient> client,
    const base::UnguessableToken& client_id,
    RegisterMojoClientCallback callback) {
  client_id_to_wrapper_.try_emplace(client_id, std::move(client), client_id,
                                    this);
  std::move(callback).Run(true);
}

void VideoConferenceManagerAsh::NotifyDeviceUsedWhileDisabled(
    crosapi::mojom::VideoConferenceMediaDevice device,
    const std::u16string& app_name,
    NotifyDeviceUsedWhileDisabledCallback callback) {
  // TODO(crbug.com/40240249): Remove this conditional check once it becomes
  // possible to enable ash features in lacros browsertests.
  if (ash::features::IsVideoConferenceEnabled()) {
    GetTrayController()->HandleDeviceUsedWhileDisabled(std::move(device),
                                                       app_name);
  }
  std::move(callback).Run(true);
}

void VideoConferenceManagerAsh::NotifyClientUpdate(
    crosapi::mojom::VideoConferenceClientUpdatePtr update) {
  // TODO(crbug.com/40240249): Remove this conditional check once it becomes
  // possible to enable ash features in lacros browsertests.
  if (ash::features::IsVideoConferenceEnabled()) {
    GetTrayController()->HandleClientUpdate(std::move(update));
  }
}

void VideoConferenceManagerAsh::UnregisterClient(
    const base::UnguessableToken& client_id) {
  client_id_to_wrapper_.erase(client_id);
  SendUpdatedState();
}

VideoConferenceMediaState VideoConferenceManagerAsh::GetAggregatedState() {
  VideoConferenceMediaState state;

  for (auto& [_, client_wrapper] : client_id_to_wrapper_) {
    auto& client_state = client_wrapper.state();

    state.has_media_app |= client_state.has_media_app;
    state.has_camera_permission |= client_state.has_camera_permission;
    state.has_microphone_permission |= client_state.has_microphone_permission;
    state.is_capturing_camera |= client_state.is_capturing_camera;
    state.is_capturing_microphone |= client_state.is_capturing_microphone;
    state.is_capturing_screen |= client_state.is_capturing_screen;
  }

  // Theoretically, capturing should imply permission, but we have seen bugs
  // in permission checker that returns inconsisitent result with capturing,
  // which leads to a bad ui to the user. This workaround is not ideal but will
  // prevent showing the bad ui.
  // TODO(b/291147970): consider removing this.
  state.has_camera_permission |= state.is_capturing_camera;
  state.has_microphone_permission |= state.is_capturing_microphone;

  return state;
}

void VideoConferenceManagerAsh::SendUpdatedState() {
  // TODO(crbug.com/40240249): Remove this conditional check once it becomes
  // possible to enable ash features in lacros browsertests.
  if (ash::features::IsVideoConferenceEnabled()) {
    GetTrayController()->UpdateWithMediaState(GetAggregatedState());
  }
}

VideoConferenceTrayController* VideoConferenceManagerAsh::GetTrayController() {
  VideoConferenceTrayController* tray_controller =
      VideoConferenceTrayController::Get();
  DCHECK(tray_controller);
  return tray_controller;
}

}  // namespace ash
