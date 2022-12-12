// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/video_conference/video_conference_tray_controller_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "components/prefs/pref_service.h"

namespace ash {

VideoConferenceTrayControllerImpl::VideoConferenceTrayControllerImpl() =
    default;

VideoConferenceTrayControllerImpl::~VideoConferenceTrayControllerImpl() =
    default;

void VideoConferenceTrayControllerImpl::SetCameraMuted(bool muted) {
  // Change user pref to let Privacy Hub enable/disable the camera.
  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!pref_service)
    return;
  pref_service->SetBoolean(prefs::kUserCameraAllowed, !muted);
}

void VideoConferenceTrayControllerImpl::SetMicrophoneMuted(bool muted) {
  // Change user pref to let Privacy Hub enable/disable the microphone.
  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!pref_service)
    return;
  pref_service->SetBoolean(prefs::kUserMicrophoneAllowed, !muted);
}

}  // namespace ash
