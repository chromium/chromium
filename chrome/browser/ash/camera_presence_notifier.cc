// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/camera_presence_notifier.h"

#include "base/callback_helpers.h"
#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/video_capture_service.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

namespace ash {

namespace {

// Interval between checks for camera presence.
const int kCameraCheckIntervalSeconds = 3;

}  // namespace

CameraPresenceNotifier::CameraPresenceNotifier(CameraPresenceCallback callback)
    : camera_present_on_last_check_(false), callback_(callback) {
  DCHECK(callback_) << "Notifier must be created with a non-null callback.";

  content::GetVideoCaptureService().ConnectToVideoSourceProvider(
      video_source_provider_remote_.BindNewPipeAndPassReceiver());
  video_source_provider_remote_.set_disconnect_handler(base::BindOnce(
      &CameraPresenceNotifier::VideoSourceProviderDisconnectHandler,
      weak_factory_.GetWeakPtr()));
}

CameraPresenceNotifier::~CameraPresenceNotifier() {
  // video_source_provider_remote_ expects to be released on the sequence where
  // it was created.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CameraPresenceNotifier::VideoSourceProviderDisconnectHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "VideoSourceProvider is Disconnected";
  callback_ = base::NullCallback();
}

void CameraPresenceNotifier::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Always pass through First Run on start to ensure an event is emitted ASAP.
  state_ = State::kFirstRun;
  CheckCameraPresence();
  camera_check_timer_.Start(
      FROM_HERE, base::Seconds(kCameraCheckIntervalSeconds),
      base::BindRepeating(&CameraPresenceNotifier::CheckCameraPresence,
                          weak_factory_.GetWeakPtr()));
}

void CameraPresenceNotifier::Stop() {
  state_ = State::kStopped;
  camera_check_timer_.Stop();
}

void CameraPresenceNotifier::CheckCameraPresence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  video_source_provider_remote_->GetSourceInfos(base::BindOnce(
      &CameraPresenceNotifier::OnGotSourceInfos, weak_factory_.GetWeakPtr()));
}

void CameraPresenceNotifier::OnGotSourceInfos(
    const std::vector<media::VideoCaptureDeviceInfo>& devices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool camera_presence = !devices.empty();

  bool presence_changed = camera_presence != camera_present_on_last_check_;
  camera_present_on_last_check_ = camera_presence;

  if (state_ == State::kStopped)
    return;

  bool run_callback = (state_ == State::kFirstRun || presence_changed);
  state_ = State::kStarted;
  if (callback_ && run_callback)
    callback_.Run(camera_present_on_last_check_);
}

}  // namespace ash
