// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_tray_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_media_state.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-shared.h"

namespace ash {

class VideoConferenceTrayControllerTest : public AshTestBase {
 public:
  VideoConferenceTrayControllerTest() = default;
  VideoConferenceTrayControllerTest(const VideoConferenceTrayControllerTest&) =
      delete;
  VideoConferenceTrayControllerTest& operator=(
      const VideoConferenceTrayControllerTest&) = delete;
  ~VideoConferenceTrayControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kVideoConference);

    // Here we have to create the global instance of `CrasAudioHandler` before
    // `FakeVideoConferenceTrayController`, so we do it here and not do it in
    // `AshTestBase`.
    CrasAudioClient::InitializeFake();
    CrasAudioHandler::InitializeForTesting();

    // Instantiates a fake controller (the real one is created in
    // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
    // ash unit tests).
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    set_create_global_cras_audio_handler(false);
    AshTestBase::SetUp();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    controller_.reset();
    CrasAudioHandler::Shutdown();
    CrasAudioClient::Shutdown();
  }

  VideoConferenceTray* video_conference_tray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->video_conference_tray();
  }

  VideoConferenceTrayButton* camera_icon() {
    return video_conference_tray()->camera_icon();
  }

  VideoConferenceTrayButton* audio_icon() {
    return video_conference_tray()->audio_icon();
  }

  FakeVideoConferenceTrayController* controller() { return controller_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
};

TEST_F(VideoConferenceTrayControllerTest, UpdateButtonWhenCameraMuted) {
  EXPECT_FALSE(camera_icon()->toggled());
  EXPECT_FALSE(camera_icon()->show_privacy_indicator());

  VideoConferenceMediaState state;
  state.is_capturing_camera = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(camera_icon()->show_privacy_indicator());

  // When camera is detected to be muted, the icon should be toggled and doesn't
  // show the privacy indicator.
  controller()->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::ON);
  EXPECT_TRUE(camera_icon()->toggled());
  EXPECT_FALSE(camera_icon()->show_privacy_indicator());

  // When unmuted, privacy indicator should show back.
  controller()->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::OFF);
  EXPECT_FALSE(camera_icon()->toggled());
  EXPECT_TRUE(camera_icon()->show_privacy_indicator());
}

TEST_F(VideoConferenceTrayControllerTest, UpdateButtonWhenMicrophoneMuted) {
  EXPECT_FALSE(audio_icon()->toggled());
  EXPECT_FALSE(audio_icon()->show_privacy_indicator());

  VideoConferenceMediaState state;
  state.is_capturing_microphone = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(audio_icon()->show_privacy_indicator());

  // When microphone is detected to be muted, the icon should be toggled and
  // doesn't show the privacy indicator.
  controller()->OnInputMuteChanged(
      /*mute_on=*/true, CrasAudioHandler::InputMuteChangeMethod::kOther);
  EXPECT_TRUE(audio_icon()->toggled());
  EXPECT_FALSE(audio_icon()->show_privacy_indicator());

  // When unmuted, privacy indicator should show back.
  controller()->OnInputMuteChanged(
      /*mute_on=*/false, CrasAudioHandler::InputMuteChangeMethod::kOther);
  EXPECT_FALSE(audio_icon()->toggled());
  EXPECT_TRUE(audio_icon()->show_privacy_indicator());
}

}  // namespace ash