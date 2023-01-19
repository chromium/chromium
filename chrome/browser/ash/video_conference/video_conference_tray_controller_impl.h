// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_CONTROLLER_IMPL_H_

#include "ash/system/video_conference/video_conference_tray_controller.h"

namespace ash {

// Implementation for VideoConferenceTrayController.
class VideoConferenceTrayControllerImpl : public VideoConferenceTrayController {
 public:
  VideoConferenceTrayControllerImpl();

  VideoConferenceTrayControllerImpl(const VideoConferenceTrayControllerImpl&) =
      delete;
  VideoConferenceTrayControllerImpl& operator=(
      const VideoConferenceTrayControllerImpl&) = delete;

  ~VideoConferenceTrayControllerImpl() override;

  // VideoConferenceTrayController:
  void SetCameraMuted(bool muted) override;
  void SetMicrophoneMuted(bool muted) override;
  void GetMediaApps(base::OnceCallback<void(MediaApps)> ui_callback) override;
  void ReturnToApp(const base::UnguessableToken& id) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_CONTROLLER_IMPL_H_
