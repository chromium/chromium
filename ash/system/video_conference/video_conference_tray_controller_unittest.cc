// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_tray_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

constexpr char kVideoConferenceTraySpeakOnMuteDetectedNudgeId[] =
    "video_conference_tray_nudge_ids.speak_on_mute_detected";

constexpr char kVideoConferenceTrayUseWhileDisabledNudgeId[] =
    "video_conference_tray_nudge_ids.use_while_disabled";

bool IsNudgeShown(const std::string& id) {
  return Shell::Get()->anchored_nudge_manager()->IsNudgeShown(id);
}

const std::u16string& GetNudgeText(const std::string& id) {
  return Shell::Get()->anchored_nudge_manager()->GetNudgeText(id);
}

views::View* GetNudgeAnchorView(const std::string& id) {
  return Shell::Get()->anchored_nudge_manager()->GetNudgeAnchorView(id);
}

}  // namespace

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
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kCameraEffectsSupportedByHardware);

    // Instantiates a fake controller (the real one is created in
    // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
    // ash unit tests).
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    AshTestBase::SetUp();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    controller_.reset();
  }

  // Returns the VC tray from the primary display. If testing multiple displays,
  // VC nudges will be shown anchored to the tray in the active display.
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

  // Make the tray and buttons visible by setting `VideoConferenceMediaState`,
  // and return the state so it can be modified.
  VideoConferenceMediaState SetTrayAndButtonsVisible() {
    VideoConferenceMediaState state;
    state.has_media_app = true;
    state.has_camera_permission = true;
    state.has_microphone_permission = true;
    state.is_capturing_screen = true;
    state.is_capturing_microphone = true;
    controller()->UpdateWithMediaState(state);
    return state;
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

TEST_F(VideoConferenceTrayControllerTest, CameraHardwareMuted) {
  // The camera icon should only be un-toggled if it is not hardware and
  // software muted.
  controller()->OnCameraHWPrivacySwitchStateChanged(
      /*device_id=*/"device_id", cros::mojom::CameraPrivacySwitchState::ON);
  controller()->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::ON);
  EXPECT_TRUE(camera_icon()->toggled());

  controller()->OnCameraHWPrivacySwitchStateChanged(
      /*device_id=*/"device_id", cros::mojom::CameraPrivacySwitchState::ON);
  controller()->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::OFF);
  EXPECT_TRUE(camera_icon()->toggled());

  controller()->OnCameraHWPrivacySwitchStateChanged(
      /*device_id=*/"device_id", cros::mojom::CameraPrivacySwitchState::OFF);
  controller()->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::ON);
  EXPECT_TRUE(camera_icon()->toggled());

  controller()->OnCameraHWPrivacySwitchStateChanged(
      /*device_id=*/"device_id", cros::mojom::CameraPrivacySwitchState::OFF);
  controller()->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::OFF);
  EXPECT_FALSE(camera_icon()->toggled());
}

TEST_F(VideoConferenceTrayControllerTest, ClickCameraWhenHardwareMuted) {
  controller()->OnCameraHWPrivacySwitchStateChanged(
      /*device_id=*/"device_id", cros::mojom::CameraPrivacySwitchState::ON);
  EXPECT_TRUE(camera_icon()->toggled());

  // Clicking the camera button when it is hardware-muted should not un-toggle
  // the button.
  LeftClickOn(camera_icon());
  EXPECT_TRUE(camera_icon()->toggled());
}

TEST_F(VideoConferenceTrayControllerTest,
       HandleCameraUsedWhileSoftwaredDisabled) {
  auto* app_name = u"app_name";
  auto camera_device_name =
      l10n_util::GetStringUTF16(IDS_ASH_VIDEO_CONFERENCE_CAMERA_NAME);
  auto* nudge_id = kVideoConferenceTrayUseWhileDisabledNudgeId;

  SetTrayAndButtonsVisible();

  controller()->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::ON);

  // No nudge is shown before `HandleDeviceUsedWhileDisabled()` is called.
  EXPECT_FALSE(IsNudgeShown(nudge_id));

  controller()->HandleDeviceUsedWhileDisabled(
      crosapi::mojom::VideoConferenceMediaDevice::kCamera, app_name);

  // Nudge should be displayed. Showing that app is accessing while camera is
  // software-muted.
  ASSERT_TRUE(IsNudgeShown(nudge_id));
  EXPECT_EQ(GetNudgeAnchorView(nudge_id), camera_icon());
  EXPECT_EQ(GetNudgeText(nudge_id),
            l10n_util::GetStringFUTF16(
                IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_SOFTWARE_DISABLED,
                app_name, camera_device_name));
}

TEST_F(VideoConferenceTrayControllerTest,
       HandleMicrophoneUsedWhileSoftwaredDisabled) {
  auto* app_name = u"app_name";
  auto microphone_device_name =
      l10n_util::GetStringUTF16(IDS_ASH_VIDEO_CONFERENCE_MICROPHONE_NAME);
  auto* nudge_id = kVideoConferenceTrayUseWhileDisabledNudgeId;

  SetTrayAndButtonsVisible();

  controller()->OnInputMuteChanged(
      /*mute_on=*/true, CrasAudioHandler::InputMuteChangeMethod::kOther);

  // No nudge is shown before `HandleDeviceUsedWhileDisabled()` is called.
  EXPECT_FALSE(IsNudgeShown(nudge_id));

  controller()->HandleDeviceUsedWhileDisabled(
      crosapi::mojom::VideoConferenceMediaDevice::kMicrophone, app_name);

  // Nudge should be displayed. Showing that app is accessing while microphone
  // is software-muted.
  ASSERT_TRUE(IsNudgeShown(nudge_id));
  EXPECT_EQ(GetNudgeAnchorView(nudge_id), audio_icon());
  EXPECT_EQ(GetNudgeText(nudge_id),
            l10n_util::GetStringFUTF16(
                IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_SOFTWARE_DISABLED,
                app_name, microphone_device_name));
}

TEST_F(VideoConferenceTrayControllerTest,
       HandleCameraUsedWhileHardwaredDisabled) {
  auto* app_name = u"app_name";
  auto camera_device_name =
      l10n_util::GetStringUTF16(IDS_ASH_VIDEO_CONFERENCE_CAMERA_NAME);
  auto* nudge_id = kVideoConferenceTrayUseWhileDisabledNudgeId;

  SetTrayAndButtonsVisible();

  controller()->OnCameraHWPrivacySwitchStateChanged(
      /*device_id=*/"device_id", cros::mojom::CameraPrivacySwitchState::ON);

  // No nudge is shown before `HandleDeviceUsedWhileDisabled()` is called.
  EXPECT_FALSE(IsNudgeShown(nudge_id));

  controller()->HandleDeviceUsedWhileDisabled(
      crosapi::mojom::VideoConferenceMediaDevice::kCamera, app_name);

  // Nudge should be displayed. Showing that app is accessing while camera is
  // hardware-muted.
  ASSERT_TRUE(IsNudgeShown(nudge_id));
  EXPECT_EQ(GetNudgeAnchorView(nudge_id), camera_icon());
  EXPECT_EQ(GetNudgeText(nudge_id),
            l10n_util::GetStringFUTF16(
                IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_HARDWARE_DISABLED,
                app_name, camera_device_name));
}

TEST_F(VideoConferenceTrayControllerTest,
       HandleMicrophoneUsedWhileHardwaredDisabled) {
  auto* app_name = u"app_name";
  auto microphone_device_name =
      l10n_util::GetStringUTF16(IDS_ASH_VIDEO_CONFERENCE_MICROPHONE_NAME);
  auto* nudge_id = kVideoConferenceTrayUseWhileDisabledNudgeId;

  SetTrayAndButtonsVisible();

  controller()->OnInputMuteChanged(
      /*mute_on=*/true,
      CrasAudioHandler::InputMuteChangeMethod::kPhysicalShutter);

  // No nudge is shown before `HandleDeviceUsedWhileDisabled()` is called.
  EXPECT_FALSE(IsNudgeShown(nudge_id));

  controller()->HandleDeviceUsedWhileDisabled(
      crosapi::mojom::VideoConferenceMediaDevice::kMicrophone, app_name);

  // Nudge should be displayed. Showing that app is accessing while microphone
  // is hardware-muted.
  ASSERT_TRUE(IsNudgeShown(nudge_id));
  EXPECT_EQ(GetNudgeAnchorView(nudge_id), audio_icon());
  EXPECT_EQ(GetNudgeText(nudge_id),
            l10n_util::GetStringFUTF16(
                IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_HARDWARE_DISABLED,
                app_name, microphone_device_name));
}

TEST_F(VideoConferenceTrayControllerTest, SpeakOnMuteNudge) {
  auto* nudge_id = kVideoConferenceTraySpeakOnMuteDetectedNudgeId;

  SetTrayAndButtonsVisible();

  // No nudge is shown before `OnSpeakOnMuteDetected()` is called.
  EXPECT_FALSE(IsNudgeShown(nudge_id));

  // Nudge should be displayed. Showing that client is speaking while on mute.
  controller()->OnSpeakOnMuteDetected();
  ASSERT_TRUE(IsNudgeShown(nudge_id));
  EXPECT_EQ(GetNudgeAnchorView(nudge_id), audio_icon());
  EXPECT_EQ(GetNudgeText(nudge_id),
            l10n_util::GetStringUTF16(
                IDS_ASH_VIDEO_CONFERENCE_TOAST_SPEAK_ON_MUTE_DETECTED));

  Shell::Get()->anchored_nudge_manager()->Cancel(nudge_id);

  // Nudge should not be displayed as there is a cool down period for the nudge.
  controller()->OnSpeakOnMuteDetected();
  EXPECT_FALSE(IsNudgeShown(nudge_id));

  controller()->OnInputMuteChanged(
      /*mute_on=*/false,
      CrasAudioHandler::InputMuteChangeMethod::kPhysicalShutter);
  controller()->OnInputMuteChanged(
      /*mute_on=*/true,
      CrasAudioHandler::InputMuteChangeMethod::kPhysicalShutter);

  // Nudge should be displayed again as the mute action will reset the nudge
  // cool down timer.
  controller()->OnSpeakOnMuteDetected();
  EXPECT_TRUE(IsNudgeShown(nudge_id));
}

}  // namespace ash
