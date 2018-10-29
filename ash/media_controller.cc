// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media_controller.h"

#include "ash/public/cpp/ash_features.h"
#include "base/feature_list.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace ash {

MediaController::MediaController(service_manager::Connector* connector)
    : connector_(connector) {}

MediaController::~MediaController() = default;

void MediaController::BindRequest(mojom::MediaControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void MediaController::AddObserver(MediaCaptureObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaController::RemoveObserver(MediaCaptureObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MediaController::SetClient(mojom::MediaClientAssociatedPtrInfo client) {
  client_.Bind(std::move(client));
}

void MediaController::NotifyCaptureState(
    const base::flat_map<AccountId, mojom::MediaCaptureState>& capture_states) {
  for (auto& observer : observers_)
    observer.OnMediaCaptureChanged(capture_states);
}

void MediaController::HandleMediaPlayPause() {
  // If media session media key handling is enabled. Toggle play pause using the
  // media session service.
  if (base::FeatureList::IsEnabled(features::kMediaSessionAccelerators)) {
    if (GetMediaSessionController())
      GetMediaSessionController()->ToggleSuspendResume();
    return;
  }

  if (client_)
    client_->HandleMediaPlayPause();
}

void MediaController::HandleMediaNextTrack() {
  // If media session media key handling is enabled. Fire next track using the
  // media session service.
  if (base::FeatureList::IsEnabled(features::kMediaSessionAccelerators)) {
    if (GetMediaSessionController())
      GetMediaSessionController()->NextTrack();
    return;
  }

  if (client_)
    client_->HandleMediaNextTrack();
}

void MediaController::HandleMediaPrevTrack() {
  // If media session media key handling is enabled. Fire previous track using
  // the media session service.
  if (base::FeatureList::IsEnabled(features::kMediaSessionAccelerators)) {
    if (GetMediaSessionController())
      GetMediaSessionController()->PreviousTrack();
    return;
  }

  if (client_)
    client_->HandleMediaPrevTrack();
}

void MediaController::RequestCaptureState() {
  if (client_)
    client_->RequestCaptureState();
}

void MediaController::SuspendMediaSessions() {
  if (client_)
    client_->SuspendMediaSessions();
}

void MediaController::SetMediaSessionControllerForTest(
    media_session::mojom::MediaControllerPtr controller) {
  media_session_controller_ptr_ = std::move(controller);
}

void MediaController::FlushForTesting() {
  client_.FlushForTesting();
  media_session_controller_ptr_.FlushForTesting();
}

media_session::mojom::MediaController*
MediaController::GetMediaSessionController() {
  // |connector_| can be null in tests.
  if (connector_ && !media_session_controller_ptr_.is_bound()) {
    connector_->BindInterface(media_session::mojom::kServiceName,
                              &media_session_controller_ptr_);

    media_session_controller_ptr_.set_connection_error_handler(
        base::BindRepeating(&MediaController::OnMediaSessionControllerError,
                            base::Unretained(this)));
  }

  return media_session_controller_ptr_.get();
}

void MediaController::OnMediaSessionControllerError() {
  media_session_controller_ptr_.reset();
}

}  // namespace ash
