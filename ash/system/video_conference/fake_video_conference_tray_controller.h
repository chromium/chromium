// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_FAKE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_FAKE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_

#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/gtest_prod_util.h"

namespace ash {

namespace fake_video_conference {
class EffectRepository;
}

namespace video_conference {
FORWARD_DECLARE_TEST(ReturnToAppPanelTest, ReturnToApp);
}  // namespace video_conference

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
  bool GetCameraMuted() override;
  bool GetMicrophoneMuted() override;
  void GetMediaApps(base::OnceCallback<void(MediaApps)> ui_callback) override;
  void ReturnToApp(const base::UnguessableToken& id) override;
  void HandleDeviceUsedWhileDisabled(
      crosapi::mojom::VideoConferenceMediaDevice device,
      const std::u16string& app_name) override;

  // Adds or clears media app(s) in `media_apps_`.
  void AddMediaApp(crosapi::mojom::VideoConferenceMediaAppInfoPtr media_app);
  void ClearMediaApps();

  bool camera_muted() { return camera_muted_; }
  bool microphone_muted() { return microphone_muted_; }
  const std::vector<
      std::pair<crosapi::mojom::VideoConferenceMediaDevice, std::u16string>>&
  device_used_while_disabled_records() {
    return device_used_while_disabled_records_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(video_conference::ReturnToAppPanelTest, ReturnToApp);
  FRIEND_TEST_ALL_PREFIXES(VideoConferenceAppServiceClientTest,
                           HandleDeviceUsedWhileDisabled);

  // A vector containing all currently running media apps. Used for testing.
  MediaApps media_apps_;

  // Indicates whether camera/microphone is muted.
  bool camera_muted_ = false;
  bool microphone_muted_ = false;

  // Records calls of the HandleDeviceUsedWhileDisabled for testing.
  std::vector<
      std::pair<crosapi::mojom::VideoConferenceMediaDevice, std::u16string>>
      device_used_while_disabled_records_;

  // A mapping from the media app's id to its launch state (whether the app is
  // launched and brought to the foreground).
  std::map<base::UnguessableToken, bool> app_to_launch_state_;

  // General-purpose repository for fake effects.
  std::unique_ptr<fake_video_conference::EffectRepository> effect_repository_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_FAKE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_