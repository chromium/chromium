// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/camera_presence_notifier.h"

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

CameraPresenceNotifier::CameraPresenceNotifier()
    : camera_present_on_last_check_(false) {
  video_source_provider_remote_.reset();
  content::GetVideoCaptureService().ConnectToVideoSourceProvider(
      video_source_provider_remote_.BindNewPipeAndPassReceiver());
  video_source_provider_remote_.set_disconnect_handler(base::BindOnce(
      &CameraPresenceNotifier::VideoSourceProviderDisconnectHandler,
      base::Unretained(this)));
}

void CameraPresenceNotifier::VideoSourceProviderDisconnectHandler() {
  LOG(ERROR) << "VideoSourceProvider is Disconnected";
}

CameraPresenceNotifier::~CameraPresenceNotifier() {}

// static
CameraPresenceNotifier* CameraPresenceNotifier::GetInstance() {
  return base::Singleton<CameraPresenceNotifier>::get();
}

void CameraPresenceNotifier::AddObserver(
    CameraPresenceNotifier::Observer* observer) {
  bool had_no_observers = observers_.empty();
  observers_.AddObserver(observer);
  observer->OnCameraPresenceCheckDone(camera_present_on_last_check_);
  if (had_no_observers) {
    CheckCameraPresence();
    camera_check_timer_.Start(FROM_HERE,
                              base::Seconds(kCameraCheckIntervalSeconds), this,
                              &CameraPresenceNotifier::CheckCameraPresence);
  }
}

void CameraPresenceNotifier::CheckCameraPresence() {
  auto task_runner = content::BrowserThread::GetTaskRunnerForThread(
      content::BrowserThread::UI);
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraPresenceNotifier::CheckPresenceOnUIThread,
                     base::Unretained(this)));
}

void CameraPresenceNotifier::CheckPresenceOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  video_source_provider_remote_->GetSourceInfos(base::BindOnce(
      &CameraPresenceNotifier::OnGotSourceInfos, base::Unretained(this)));
}

void CameraPresenceNotifier::OnGotSourceInfos(
    const std::vector<media::VideoCaptureDeviceInfo>& devices) {
  bool camera_presence = !devices.empty();
  if (camera_presence != camera_present_on_last_check_) {
    camera_present_on_last_check_ = camera_presence;
    for (auto& observer : observers_)
      observer.OnCameraPresenceCheckDone(camera_present_on_last_check_);
  }
}

void CameraPresenceNotifier::RemoveObserver(
    CameraPresenceNotifier::Observer* observer) {
  observers_.RemoveObserver(observer);
  if (observers_.empty()) {
    camera_check_timer_.Stop();
    camera_present_on_last_check_ = false;
  }
}

}  // namespace ash
