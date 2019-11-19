// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/camera_presence_notifier.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/camera_detector.h"

namespace chromeos {

namespace {

// Interval between checks for camera presence.
const int kCameraCheckIntervalSeconds = 3;

}  // namespace

CameraPresenceNotifier::CameraPresenceNotifier()
    : camera_present_on_last_check_(false) {}

CameraPresenceNotifier::~CameraPresenceNotifier() {}

// static
CameraPresenceNotifier* CameraPresenceNotifier::GetInstance() {
  return base::Singleton<CameraPresenceNotifier>::get();
}

void CameraPresenceNotifier::AddObserver(
    CameraPresenceNotifier::Observer* observer) {
  bool had_no_observers = !observers_.might_have_observers();
  observers_.AddObserver(observer);
  observer->OnCameraPresenceCheckDone(camera_present_on_last_check_);
  if (had_no_observers) {
    CheckCameraPresence();
    camera_check_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(kCameraCheckIntervalSeconds),
        this,
        &CameraPresenceNotifier::CheckCameraPresence);
  }
}

void CameraPresenceNotifier::RemoveObserver(
    CameraPresenceNotifier::Observer* observer) {
  observers_.RemoveObserver(observer);
  if (!observers_.might_have_observers()) {
    camera_check_timer_.Stop();
    camera_present_on_last_check_ = false;
  }
}

void CameraPresenceNotifier::CheckCameraPresence() {
  CameraDetector::StartPresenceCheck(
      base::Bind(&CameraPresenceNotifier::OnCameraPresenceCheckDone,
                 weak_factory_.GetWeakPtr()));
}

void CameraPresenceNotifier::OnCameraPresenceCheckDone() {
  bool is_camera_present =
      CameraDetector::camera_presence() == CameraDetector::kCameraPresent;
  if (is_camera_present != camera_present_on_last_check_) {
    camera_present_on_last_check_ = is_camera_present;
    for (auto& observer : observers_)
      observer.OnCameraPresenceCheckDone(camera_present_on_last_check_);
  }
}

}  // namespace chromeos
