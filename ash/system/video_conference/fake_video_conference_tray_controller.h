// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_FAKE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_FAKE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"

namespace ash {

namespace fake_video_conference {
class EffectRepository;
}

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

  ~FakeVideoConferenceTrayController() override;

  // VideoConferenceTrayController:
  void SetCameraMuted(bool muted) override;
  void SetMicrophoneMuted(bool muted) override;
  void GetMediaApps(base::OnceCallback<void(MediaApps)> ui_callback) override;

  // Adds or clears media app(s) in `media_apps_`.
  void AddMediaApp(crosapi::mojom::VideoConferenceMediaAppInfoPtr media_app);
  void ClearMediaApps();

  bool camera_muted() { return camera_muted_; }
  bool microphone_muted() { return microphone_muted_; }

 private:
  // A vector containing all currently running media apps. Used for testing.
  MediaApps media_apps_;

  // Indicates whether camera/microphone is muted.
  bool camera_muted_ = false;
  bool microphone_muted_ = false;

  // General-purpose repository for fake effects.
  std::unique_ptr<fake_video_conference::EffectRepository> effect_repository_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_FAKE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_