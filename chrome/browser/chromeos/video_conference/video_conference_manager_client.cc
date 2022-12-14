// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/video_conference/video_conference_manager_client.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/video_conference/video_conference_app_permissions.h"
#include "chrome/browser/chromeos/video_conference/video_conference_media_listener.h"
#include "chrome/browser/chromeos/video_conference/video_conference_web_app.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#else
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace video_conference {

VideoConferenceManagerClientImpl::VideoConferenceManagerClientImpl()
    : client_id_(base::UnguessableToken::Create()),
      status_(crosapi::mojom::VideoConferenceMediaUsageStatus::New(
          /*client_id=*/client_id_,
          /*has_media_app=*/false,
          /*has_camera_permission=*/false,
          /*has_microphone_permission=*/false,
          /*is_capturing_camera=*/false,
          /*is_capturing_microphone=*/false,
          /*is_capturing_screen=*/false)) {
  media_listener_ = std::make_unique<
      VideoConferenceMediaListener>(/*media_usage_update_callback=*/
                                    base::BindRepeating(
                                        &VideoConferenceManagerClientImpl::
                                            HandleMediaUsageUpdate,
                                        base::Unretained(this)),
                                    /*create_vc_web_app_callback=*/base::
                                        BindRepeating(
                                            &VideoConferenceManagerClientImpl::
                                                CreateVideoConferenceWebApp,
                                            base::Unretained(this)));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Bind remote and pass receiver to VideoConferenceManagerAsh.
  chromeos::LacrosService::Get()->BindVideoConferenceManager(
      remote_.BindNewPipeAndPassReceiver());
  // Register the mojo client.
  remote_->RegisterMojoClient(receiver_.BindNewPipeAndPassRemote(), client_id_,
                              base::BindOnce([](bool success) {
                                if (!success) {
                                  LOG(ERROR)
                                      << "VideoConferenceManagerClientImpl "
                                         "RegisterMojoClient did not succeed.";
                                }
                              }));
#else
  // Register the C++ (non-mojo) client.
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->RegisterCppClient(this, client_id_);
#endif
}

VideoConferenceManagerClientImpl::~VideoConferenceManagerClientImpl() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // C++ clients are responsible for manually calling |UnregisterClient| on the
  // manager when disconnecting.
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->UnregisterClient(client_id_);
#endif
}

void VideoConferenceManagerClientImpl::RemoveMediaApp(
    const base::UnguessableToken& id) {
  if (id_to_webcontents_.erase(id)) {
    HandleMediaUsageUpdate();
  }
}

VideoConferenceWebApp*
VideoConferenceManagerClientImpl::CreateVideoConferenceWebApp(
    content::WebContents* web_contents) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  // Callback to handle cleanup when the webcontents is destroyed or its primary
  // page changes.
  auto remove_media_app_callback =
      base::BindRepeating(&VideoConferenceManagerClientImpl::RemoveMediaApp,
                          weak_ptr_factory_.GetWeakPtr());

  content::WebContentsUserData<VideoConferenceWebApp>::CreateForWebContents(
      web_contents, id, std::move(remove_media_app_callback));

  id_to_webcontents_.try_emplace(id, web_contents);

  return content::WebContentsUserData<VideoConferenceWebApp>::FromWebContents(
      web_contents);
}

void VideoConferenceManagerClientImpl::HandleMediaUsageUpdate() {
  bool is_capturing_camera = false;
  bool is_capturing_microphone = false;
  bool is_capturing_screen = false;

  for (auto [id, web_contents] : id_to_webcontents_) {
    auto* web_app =
        content::WebContentsUserData<VideoConferenceWebApp>::FromWebContents(
            web_contents);
    DCHECK(web_app)
        << "WebContents with no corresponding VideoConferenceWebApp.";

    is_capturing_camera |= web_app->state().is_capturing_camera;
    is_capturing_microphone |= web_app->state().is_capturing_microphone;
    is_capturing_screen |= web_app->state().is_capturing_screen;
  }

  auto permissions = GetAggregatedPermissions();

  crosapi::mojom::VideoConferenceMediaUsageStatusPtr status =
      crosapi::mojom::VideoConferenceMediaUsageStatus::New(
          /*client_id=*/client_id_,
          /*has_media_app=*/!id_to_webcontents_.empty(),
          /*has_camera_permission=*/permissions.has_camera_permission,
          /*has_microphone_permission=*/permissions.has_microphone_permission,
          /*is_capturing_camera=*/is_capturing_camera,
          /*is_capturing_microphone=*/is_capturing_microphone,
          /*is_capturing_screen=*/is_capturing_screen);

  // If `status` equal the previously sent status, don't notify manager.
  if (status.Equals(status_)) {
    return;
  }
  status_ = status->Clone();

  NotifyManager(std::move(status));
}

void VideoConferenceManagerClientImpl::GetMediaApps(
    GetMediaAppsCallback callback) {
  std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> apps;

  for (auto [_, web_contents] : id_to_webcontents_) {
    auto* web_app =
        content::WebContentsUserData<VideoConferenceWebApp>::FromWebContents(
            web_contents);
    DCHECK(web_app)
        << "WebContents with no corresponding VideoConferenceWebApp.";

    auto& app_state = web_app->state();

    apps.push_back(crosapi::mojom::VideoConferenceMediaAppInfo::New(
        /*id=*/app_state.id,
        /*last_activity_time=*/app_state.last_activity_time,
        /*is_capturing_camera=*/app_state.is_capturing_camera,
        /*is_capturing_microphone=*/app_state.is_capturing_microphone,
        /*is_capturing_screen=*/app_state.is_capturing_screen,
        /*title=*/web_contents->GetTitle(), /*url=*/web_contents->GetURL()));
  }

  std::move(callback).Run(std::move(apps));
}

void VideoConferenceManagerClientImpl::ReturnToApp(
    const base::UnguessableToken& id,
    ReturnToAppCallback callback) {
  if (auto it = id_to_webcontents_.find(id); it != id_to_webcontents_.end()) {
    auto* web_app =
        content::WebContentsUserData<VideoConferenceWebApp>::FromWebContents(
            it->second);
    DCHECK(web_app)
        << "WebContents with no corresponding VideoConferenceWebApp.";

    web_app->ActivateApp();
    std::move(callback).Run(true);
  } else {
    // As the manager calls `ReturnToApp` on all clients, it is normal and
    // expected that a client doesn't have any `VideoConferenceWebApp` with the
    // provided `id`.
    std::move(callback).Run(false);
  }
}

void VideoConferenceManagerClientImpl::SetSystemMediaDeviceStatus(
    crosapi::mojom::VideoConferenceMediaDevice device,
    bool disabled,
    SetSystemMediaDeviceStatusCallback callback) {
  switch (device) {
    case crosapi::mojom::VideoConferenceMediaDevice::kCamera:
      camera_system_disabled_ = disabled;
      std::move(callback).Run(true);
      break;
    case crosapi::mojom::VideoConferenceMediaDevice::kMicrophone:
      microphone_system_disabled_ = disabled;
      std::move(callback).Run(true);
      break;
    case crosapi::mojom::VideoConferenceMediaDevice::kUnusedDefault:
      std::move(callback).Run(false);
      return;
  }
}

void VideoConferenceManagerClientImpl::NotifyManager(
    crosapi::mojom::VideoConferenceMediaUsageStatusPtr status) {
  auto callback = base::BindOnce([](bool success) {
    if (!success) {
      LOG(ERROR)
          << "VideoConferenceManager::NotifyMediaUsageUpdate did not succeed.";
    }
  });

  // Send updated media usage state to VcManager.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  remote_->NotifyMediaUsageUpdate(std::move(status), std::move(callback));
#else
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->NotifyMediaUsageUpdate(std::move(status), std::move(callback));
#endif
}

VideoConferencePermissions
VideoConferenceManagerClientImpl::GetAggregatedPermissions() {
  bool has_camera_permission = false;
  bool has_microphone_permission = false;

  for (auto& [_, web_contents] : id_to_webcontents_) {
    auto* web_app =
        content::WebContentsUserData<VideoConferenceWebApp>::FromWebContents(
            web_contents);
    DCHECK(web_app)
        << "WebContents with no corresponding VideoConferenceWebApp.";

    auto permissions = web_app->GetPermissions();
    has_camera_permission |= permissions.has_camera_permission;
    has_microphone_permission |= permissions.has_microphone_permission;
  }

  return {.has_camera_permission = has_camera_permission,
          .has_microphone_permission = has_microphone_permission};
}

}  // namespace video_conference
