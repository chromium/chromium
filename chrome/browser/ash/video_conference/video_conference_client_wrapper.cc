// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/video_conference/video_conference_client_wrapper.h"

#include <cstdint>
#include <utility>

#include "ash/system/video_conference/video_conference_common.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-shared.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

VideoConferenceClientWrapper::VideoConferenceClientWrapper(
    mojo::PendingRemote<crosapi::mojom::VideoConferenceManagerClient> client,
    const base::UnguessableToken& client_id,
    VideoConferenceManagerAsh* vc_manager)
    : vc_manager_(vc_manager) {
  mojo_client_.Bind(std::move(client));

  // Unregister the client on VideoConferenceManagerAsh upon disconnect.
  mojo_client_.set_disconnect_handler(base::BindOnce(
      [](const base::UnguessableToken& client_id,
         VideoConferenceManagerAsh* vc_manager) {
        DCHECK(vc_manager);
        vc_manager->UnregisterClient(client_id);
      },
      client_id, vc_manager_.get()));
}

VideoConferenceClientWrapper::~VideoConferenceClientWrapper() = default;

VideoConferenceClientWrapper::VideoConferenceClientWrapper(
    crosapi::mojom::VideoConferenceManagerClient* client,
    VideoConferenceManagerAsh* vc_manager)
    : vc_manager_(vc_manager), cpp_client_(client) {}

void VideoConferenceClientWrapper::GetMediaApps(
    crosapi::mojom::VideoConferenceManagerClient::GetMediaAppsCallback
        callback) {
  if (cpp_client_ != nullptr) {
    // Call method on ash client directly.
    cpp_client_->GetMediaApps(std::move(callback));
  } else {
    // Call method on lacros client over mojo.
    DCHECK(mojo_client_.is_bound());
    mojo_client_->GetMediaApps(std::move(callback));
  }
}

void VideoConferenceClientWrapper::ReturnToApp(
    const base::UnguessableToken& id,
    crosapi::mojom::VideoConferenceManagerClient::ReturnToAppCallback
        callback) {
  if (cpp_client_ != nullptr) {
    cpp_client_->ReturnToApp(id, std::move(callback));
  } else {
    DCHECK(mojo_client_.is_bound());
    mojo_client_->ReturnToApp(id, std::move(callback));
  }
}

void VideoConferenceClientWrapper::SetSystemMediaDeviceStatus(
    crosapi::mojom::VideoConferenceMediaDevice device,
    bool disabled,
    crosapi::mojom::VideoConferenceManagerClient::
        SetSystemMediaDeviceStatusCallback callback) {
  if (cpp_client_ != nullptr) {
    cpp_client_->SetSystemMediaDeviceStatus(device, disabled,
                                            std::move(callback));
  } else {
    DCHECK(mojo_client_.is_bound());
    mojo_client_->SetSystemMediaDeviceStatus(device, disabled,
                                             std::move(callback));
  }
}

void VideoConferenceClientWrapper::StopAllScreenShare() {
  if (cpp_client_ != nullptr) {
    cpp_client_->StopAllScreenShare();
  } else {
    DCHECK(mojo_client_.is_bound());
    mojo_client_->StopAllScreenShare();
  }
}

VideoConferenceMediaState& VideoConferenceClientWrapper::state() {
  return state_;
}

}  // namespace ash
