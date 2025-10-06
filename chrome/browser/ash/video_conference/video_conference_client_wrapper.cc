// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/video_conference/video_conference_client_wrapper.h"

#include <cstdint>
#include <utility>

#include "ash/system/video_conference/video_conference_common.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-shared.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"

namespace ash {

VideoConferenceClientWrapper::~VideoConferenceClientWrapper() = default;

VideoConferenceClientWrapper::VideoConferenceClientWrapper(
    crosapi::mojom::VideoConferenceManagerClient* client)
    : cpp_client_(client) {}

void VideoConferenceClientWrapper::GetMediaApps(
    crosapi::mojom::VideoConferenceManagerClient::GetMediaAppsCallback
        callback) {
  cpp_client_->GetMediaApps(std::move(callback));
}

void VideoConferenceClientWrapper::ReturnToApp(
    const base::UnguessableToken& id,
    crosapi::mojom::VideoConferenceManagerClient::ReturnToAppCallback
        callback) {
  cpp_client_->ReturnToApp(id, std::move(callback));
}

void VideoConferenceClientWrapper::SetSystemMediaDeviceStatus(
    crosapi::mojom::VideoConferenceMediaDevice device,
    bool enabled,
    crosapi::mojom::VideoConferenceManagerClient::
        SetSystemMediaDeviceStatusCallback callback) {
  cpp_client_->SetSystemMediaDeviceStatus(device, enabled, std::move(callback));
}

void VideoConferenceClientWrapper::StopAllScreenShare() {
  cpp_client_->StopAllScreenShare();
}

VideoConferenceMediaState& VideoConferenceClientWrapper::state() {
  return state_;
}

}  // namespace ash
