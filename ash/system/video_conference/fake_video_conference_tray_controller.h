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
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"

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
  void StopAllScreenShare() override;
  VideoConferenceTrayEffectsManager& GetEffectsManager() override;

  void SetEffectsManager(VideoConferenceTrayEffectsManager* effects_manager);
  void GetMediaApps(base::OnceCallback<void(MediaApps)> ui_callback) override;
  void ReturnToApp(const base::UnguessableToken& id) override;
  void HandleDeviceUsedWhileDisabled(
      crosapi::mojom::VideoConferenceMediaDevice device,
      const std::u16string& app_name) override;
  void HandleClientUpdate(
      crosapi::mojom::VideoConferenceClientUpdatePtr update) override;

  // Adds or clears media app(s) in `media_apps_`.
  void AddMediaApp(crosapi::mojom::VideoConferenceMediaAppInfoPtr media_app);
  void ClearMediaApps();

  const std::vector<
      std::pair<crosapi::mojom::VideoConferenceMediaDevice, std::u16string>>&
  device_used_while_disabled_records() {
    return device_used_while_disabled_records_;
  }

  const crosapi::mojom::VideoConferenceClientUpdatePtr& last_client_update() {
    return last_client_update_;
  }
  int stop_all_screen_share_count() { return stop_all_screen_share_count_; }

  const MediaApps& media_apps() { return media_apps_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(video_conference::ReturnToAppPanelTest, ReturnToApp);
  FRIEND_TEST_ALL_PREFIXES(VideoConferenceAppServiceClientTest,
                           HandleDeviceUsedWhileDisabled);

  // A vector containing all currently running media apps. Used for testing.
  MediaApps media_apps_;

  // Indicates whether microphone is muted.
  bool microphone_muted_ = false;

  // Record number of times StopAllScreenShare is called.
  int stop_all_screen_share_count_ = 0;

  // Records calls of the HandleDeviceUsedWhileDisabled for testing.
  std::vector<
      std::pair<crosapi::mojom::VideoConferenceMediaDevice, std::u16string>>
      device_used_while_disabled_records_;

  // A mapping from the media app's id to its launch state (whether the app is
  // launched and brought to the foreground).
  std::map<base::UnguessableToken, bool> app_to_launch_state_;

  // The `VideoConferenceTrayEffectsManager` that should be used. Can be
  // specified by `SetEffectsManager()`. Mainly used by tests that require use
  // of a fake VC effects manager over the manager used in production. If null,
  // then a call to `GetEffectsManager()` will return the result of the base
  // `VideoConferenceTrayController::GetEffectsManager()`.
  raw_ptr<VideoConferenceTrayEffectsManager> effects_manager_;

  // General-purpose repository for fake effects.
  std::unique_ptr<fake_video_conference::EffectRepository> effect_repository_;

  // Last client update received.
  crosapi::mojom::VideoConferenceClientUpdatePtr last_client_update_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_FAKE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_
