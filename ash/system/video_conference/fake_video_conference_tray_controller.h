// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_FAKE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_FAKE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"

namespace ash {

// A fake version of VideoConferenceTrayController that will be use in tests or
// mocking in the emulator.
class ASH_EXPORT FakeVideoConferenceTrayController
    : public VideoConferenceTrayController {
 public:
  FakeVideoConferenceTrayController();
  FakeVideoConferenceTrayController(const FakeVideoConferenceTrayController&) =
      delete;
  FakeVideoConferenceTrayController& operator=(
      const FakeVideoConferenceTrayController&) = delete;
  ~FakeVideoConferenceTrayController() override = default;

  // VideoConferenceTrayController:
  void SetCameraSoftwareMuted(bool mute_camera) override;

  bool camera_soft_muted() { return camera_soft_muted_; }

 private:
  bool camera_soft_muted_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_FAKE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_