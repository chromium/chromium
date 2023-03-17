// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_tray.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"

namespace {

constexpr char kToggleButtonHistogramName[] =
    "Ash.VideoConferenceTray.ToggleBubbleButton.Click";
constexpr char kCameraMuteHistogramName[] =
    "Ash.VideoConferenceTray.CameraMuteButton.Click";
constexpr char kMicrophoneMuteHistogramName[] =
    "Ash.VideoConferenceTray.MicrophoneMuteButton.Click";
constexpr char kStopScreenShareHistogramName[] =
    "Ash.VideoConferenceTray.StopScreenShareButton.Click";

void SetSessionState(session_manager::SessionState state) {
  ash::SessionInfo info;
  info.state = state;
  ash::Shell::Get()->session_controller()->SetSessionInfo(info);
}

}  // namespace

namespace ash {

class VideoConferenceTrayTest : public AshTestBase {
 public:
  VideoConferenceTrayTest() = default;
  VideoConferenceTrayTest(const VideoConferenceTrayTest&) = delete;
  VideoConferenceTrayTest& operator=(const VideoConferenceTrayTest&) = delete;
  ~VideoConferenceTrayTest() override = default;

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

  VideoConferenceTray* GetSecondaryVideoConferenceTray() {
    Shelf* const shelf =
        Shell::GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id())
            ->shelf();
    return shelf->status_area_widget()->video_conference_tray();
  }

  VideoConferenceTray* video_conference_tray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->video_conference_tray();
  }

  IconButton* toggle_bubble_button() {
    return video_conference_tray()->toggle_bubble_button_;
  }

  VideoConferenceTrayButton* camera_icon() {
    return video_conference_tray()->camera_icon();
  }

  VideoConferenceTrayButton* audio_icon() {
    return video_conference_tray()->audio_icon();
  }

  VideoConferenceTrayButton* screen_share_icon() {
    return video_conference_tray()->screen_share_icon();
  }

  // Make the tray and buttons visible by setting `VideoConferenceMediaState`,
  // and return the state so it can be modified.
  VideoConferenceMediaState SetTrayAndButtonsVisible() {
    VideoConferenceMediaState state;
    state.has_media_app = true;
    state.has_camera_permission = true;
    state.has_microphone_permission = true;
    state.is_capturing_screen = true;
    controller()->UpdateWithMediaState(state);
    return state;
  }

  FakeVideoConferenceTrayController* controller() { return controller_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
};

TEST_F(VideoConferenceTrayTest, ClickTrayButton) {
  base::HistogramTester histogram_tester;
  SetTrayAndButtonsVisible();

  EXPECT_FALSE(video_conference_tray()->GetBubbleView());

  // Clicking the toggle button should construct and open up the bubble.
  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(video_conference_tray()->GetBubbleView());
  EXPECT_TRUE(video_conference_tray()->GetBubbleView()->GetVisible());
  EXPECT_TRUE(toggle_bubble_button()->toggled());
  histogram_tester.ExpectBucketCount(kToggleButtonHistogramName, true, 1);

  // Clicking it again should reset the bubble.
  LeftClickOn(toggle_bubble_button());
  EXPECT_FALSE(video_conference_tray()->GetBubbleView());
  EXPECT_FALSE(toggle_bubble_button()->toggled());
  histogram_tester.ExpectBucketCount(kToggleButtonHistogramName, false, 1);

  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(video_conference_tray()->GetBubbleView());
  EXPECT_TRUE(video_conference_tray()->GetBubbleView()->GetVisible());
  EXPECT_TRUE(toggle_bubble_button()->toggled());
  histogram_tester.ExpectBucketCount(kToggleButtonHistogramName, true, 2);

  // Click anywhere else outside the bubble (i.e. the status area button) should
  // close the bubble.
  LeftClickOn(
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->unified_system_tray());
  EXPECT_FALSE(video_conference_tray()->GetBubbleView());
  EXPECT_FALSE(toggle_bubble_button()->toggled());
}

TEST_F(VideoConferenceTrayTest, ToggleBubbleButtonRotation) {
  SetTrayAndButtonsVisible();

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);

  // When the bubble is not open in horizontal shelf, the indicator should point
  // up (not rotated).
  EXPECT_EQ(0,
            video_conference_tray()->GetRotationValueForToggleBubbleButton());

  // When the bubble is open in horizontal shelf, the indicator should point
  // down.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(180,
            video_conference_tray()->GetRotationValueForToggleBubbleButton());

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);

  // When the bubble is not open in left shelf, the indicator should point to
  // the right.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(90,
            video_conference_tray()->GetRotationValueForToggleBubbleButton());

  // When the bubble is open in left shelf, the indicator should point to the
  // left.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(270,
            video_conference_tray()->GetRotationValueForToggleBubbleButton());

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);

  // When the bubble is not open in right shelf, the indicator should point to
  // the left.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(270,
            video_conference_tray()->GetRotationValueForToggleBubbleButton());

  // When the bubble is open in right shelf, the indicator should point to the
  // right.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(90,
            video_conference_tray()->GetRotationValueForToggleBubbleButton());
}

// Makes sure that the tray does not animate to new inkdrop state when
// activated, which is the default behavior of `TrayBackgroundView`.
TEST_F(VideoConferenceTrayTest, ToggleBubbleInkdrop) {
  auto* ink_drop = views::InkDrop::Get(video_conference_tray())->GetInkDrop();

  SetTrayAndButtonsVisible();
  EXPECT_EQ(views::InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  // Open bubble, the tray should not install inkdrop.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(views::InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  // Close the bubble, inkdrop should still be hidden.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(views::InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());
}

TEST_F(VideoConferenceTrayTest, TrayVisibility) {
  // We only show the tray when there are any running media app(s).
  VideoConferenceMediaState state;
  state.has_media_app = true;
  state.has_camera_permission = true;
  state.has_microphone_permission = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(video_conference_tray()->GetVisible());
  EXPECT_TRUE(audio_icon()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());

  // At first, the tray, as well as audio and camera icons should still be
  // visible.
  EXPECT_TRUE(video_conference_tray()->GetVisible());
  EXPECT_TRUE(audio_icon()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());

  state.has_media_app = false;
  state.has_camera_permission = false;
  state.has_microphone_permission = false;
  controller()->UpdateWithMediaState(state);

  EXPECT_FALSE(video_conference_tray()->GetVisible());
  EXPECT_FALSE(audio_icon()->GetVisible());
  EXPECT_FALSE(camera_icon()->GetVisible());
}

TEST_F(VideoConferenceTrayTest, TrayVisibilityOnSecondaryDisplay) {
  UpdateDisplay("800x700,800x700");

  VideoConferenceMediaState state;
  state.has_media_app = true;
  state.has_camera_permission = true;
  state.has_microphone_permission = true;
  controller()->UpdateWithMediaState(state);
  ASSERT_TRUE(GetSecondaryVideoConferenceTray()->GetVisible());

  auto* audio_icon = GetSecondaryVideoConferenceTray()->audio_icon();
  auto* camera_icon = GetSecondaryVideoConferenceTray()->camera_icon();

  ASSERT_TRUE(audio_icon->GetVisible());
  ASSERT_TRUE(camera_icon->GetVisible());

  state.has_media_app = false;
  state.has_camera_permission = false;
  state.has_microphone_permission = false;
  controller()->UpdateWithMediaState(state);

  EXPECT_FALSE(GetSecondaryVideoConferenceTray()->GetVisible());
  EXPECT_FALSE(audio_icon->GetVisible());
  EXPECT_FALSE(camera_icon->GetVisible());
}

TEST_F(VideoConferenceTrayTest, CameraButtonVisibility) {
  // Camera icon should only be visible when permission has been granted.
  VideoConferenceMediaState state;
  state.has_camera_permission = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(camera_icon()->GetVisible());

  state.has_camera_permission = false;
  controller()->UpdateWithMediaState(state);
  EXPECT_FALSE(camera_icon()->GetVisible());
}

TEST_F(VideoConferenceTrayTest, MicrophoneButtonVisibility) {
  // Microphone icon should only be visible when permission has been granted.
  VideoConferenceMediaState state;
  state.has_microphone_permission = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(audio_icon()->GetVisible());

  state.has_microphone_permission = false;
  controller()->UpdateWithMediaState(state);
  EXPECT_FALSE(audio_icon()->GetVisible());
}

TEST_F(VideoConferenceTrayTest, ScreenshareButtonVisibility) {
  auto* screen_share_icon = video_conference_tray()->screen_share_icon();

  VideoConferenceMediaState state;
  state.is_capturing_screen = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(screen_share_icon->GetVisible());
  EXPECT_TRUE(screen_share_icon->show_privacy_indicator());

  state.is_capturing_screen = false;
  controller()->UpdateWithMediaState(state);
  EXPECT_FALSE(screen_share_icon->GetVisible());
  EXPECT_FALSE(screen_share_icon->show_privacy_indicator());
}

TEST_F(VideoConferenceTrayTest, ToggleCameraButton) {
  base::HistogramTester histogram_tester;
  SetTrayAndButtonsVisible();

  EXPECT_FALSE(camera_icon()->toggled());

  // Click the button should mute the camera.
  LeftClickOn(camera_icon());
  EXPECT_TRUE(controller()->GetCameraMuted());
  EXPECT_TRUE(camera_icon()->toggled());
  histogram_tester.ExpectBucketCount(kCameraMuteHistogramName, false, 1);

  // Toggle again, should be unmuted.
  LeftClickOn(camera_icon());
  EXPECT_FALSE(controller()->GetCameraMuted());
  EXPECT_FALSE(camera_icon()->toggled());
  histogram_tester.ExpectBucketCount(kCameraMuteHistogramName, true, 1);
}

TEST_F(VideoConferenceTrayTest, ToggleMicrophoneButton) {
  base::HistogramTester histogram_tester;
  SetTrayAndButtonsVisible();

  EXPECT_FALSE(audio_icon()->toggled());

  // Click the button should mute the microphone.
  LeftClickOn(audio_icon());
  EXPECT_TRUE(controller()->GetMicrophoneMuted());
  EXPECT_TRUE(audio_icon()->toggled());
  histogram_tester.ExpectBucketCount(kMicrophoneMuteHistogramName, false, 1);

  // Toggle again, should be unmuted.
  LeftClickOn(audio_icon());
  EXPECT_FALSE(controller()->GetMicrophoneMuted());
  EXPECT_FALSE(audio_icon()->toggled());
  histogram_tester.ExpectBucketCount(kMicrophoneMuteHistogramName, true, 1);
}

TEST_F(VideoConferenceTrayTest, ClickScreenshareButton) {
  base::HistogramTester histogram_tester;
  SetTrayAndButtonsVisible();

  bool stop_callback_called = false;

  auto stop_callback = base::BindRepeating(
      [](bool* stop_callback_called) { *stop_callback_called = true; },
      base::Unretained(&stop_callback_called));

  Shell::Get()->system_tray_notifier()->NotifyScreenAccessStart(
      stop_callback, base::RepeatingClosure(), std::u16string());

  // Click the screen share button should trigger the screen access stop
  // callback.
  LeftClickOn(screen_share_icon());
  EXPECT_TRUE(stop_callback_called);
  histogram_tester.ExpectBucketCount(kStopScreenShareHistogramName, true, 1);

  stop_callback_called = false;
  Shell::Get()->system_tray_notifier()->NotifyRemotingScreenShareStart(
      stop_callback);

  // Click the screen share button should not trigger the remoting share stop
  // callback.
  LeftClickOn(screen_share_icon());
  EXPECT_FALSE(stop_callback_called);
}

TEST_F(VideoConferenceTrayTest, PrivacyIndicator) {
  auto state = SetTrayAndButtonsVisible();

  // Privacy indicator should be shown when camera is actively capturing video.
  EXPECT_FALSE(camera_icon()->show_privacy_indicator());
  state.is_capturing_camera = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(camera_icon()->show_privacy_indicator());

  // Privacy indicator should be shown when microphone is actively capturing
  // audio.
  EXPECT_FALSE(audio_icon()->show_privacy_indicator());
  state.is_capturing_microphone = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(audio_icon()->show_privacy_indicator());

  // Should not show indicator when not capture.
  state.is_capturing_camera = false;
  state.is_capturing_microphone = false;
  controller()->UpdateWithMediaState(state);
  EXPECT_FALSE(camera_icon()->show_privacy_indicator());
  EXPECT_FALSE(audio_icon()->show_privacy_indicator());
}

TEST_F(VideoConferenceTrayTest, CameraIconPrivacyIndicatorOnToggled) {
  auto state = SetTrayAndButtonsVisible();

  state.is_capturing_camera = true;
  controller()->UpdateWithMediaState(state);

  EXPECT_TRUE(camera_icon()->show_privacy_indicator());
  EXPECT_TRUE(camera_icon()->GetVisible());

  // Privacy indicator should not be shown when camera button is toggled.
  LeftClickOn(camera_icon());
  EXPECT_FALSE(camera_icon()->show_privacy_indicator());
}

TEST_F(VideoConferenceTrayTest, MicrophoneIconPrivacyIndicatorOnToggled) {
  auto state = SetTrayAndButtonsVisible();
  state.is_capturing_microphone = true;
  controller()->UpdateWithMediaState(state);

  EXPECT_TRUE(audio_icon()->show_privacy_indicator());

  // Privacy indicator should not be shown when audio button is toggled.
  LeftClickOn(audio_icon());
  EXPECT_FALSE(audio_icon()->show_privacy_indicator());
}

// Tests that the `VideoConferenceTray` is visible when a display is connected
// after a session begins.
TEST_F(VideoConferenceTrayTest, MultiDisplayVideoConferenceTrayVisibility) {
  SetTrayAndButtonsVisible();
  ASSERT_TRUE(video_conference_tray()->GetVisible());

  // Attach a second display, the VideoConferenceTray on the second display
  // should be visible.
  UpdateDisplay("800x700,800x700");

  EXPECT_TRUE(GetSecondaryVideoConferenceTray()->GetVisible());
}

// Tests that privacy indicators update on secondary displays when a capture
// session begins.
TEST_F(VideoConferenceTrayTest, PrivacyIndicatorOnSecondaryDisplay) {
  auto state = SetTrayAndButtonsVisible();
  ASSERT_TRUE(video_conference_tray()->GetVisible());
  UpdateDisplay("800x700,800x700");
  ASSERT_TRUE(GetSecondaryVideoConferenceTray()->GetVisible());

  state.is_capturing_camera = true;
  controller()->UpdateWithMediaState(state);
  auto* secondary_camera_icon =
      GetSecondaryVideoConferenceTray()->camera_icon();
  EXPECT_TRUE(secondary_camera_icon->GetVisible());
  EXPECT_TRUE(secondary_camera_icon->show_privacy_indicator());

  // Privacy indicator should be shown when microphone is actively capturing
  // audio.
  auto* secondary_audio_icon = GetSecondaryVideoConferenceTray()->audio_icon();
  EXPECT_FALSE(secondary_audio_icon->show_privacy_indicator());
  state.is_capturing_microphone = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(secondary_audio_icon->show_privacy_indicator());

  // Should not show indicator when not capturing.
  state.is_capturing_camera = false;
  state.is_capturing_microphone = false;
  controller()->UpdateWithMediaState(state);

  EXPECT_FALSE(secondary_camera_icon->show_privacy_indicator());
  EXPECT_FALSE(secondary_audio_icon->show_privacy_indicator());
}

// Tests that the camera toggle state updates across displays.
TEST_F(VideoConferenceTrayTest, CameraButtonToggleAcrossDisplays) {
  SetTrayAndButtonsVisible();
  ASSERT_TRUE(video_conference_tray()->GetVisible());
  UpdateDisplay("800x700,800x700");
  ASSERT_TRUE(GetSecondaryVideoConferenceTray()->GetVisible());

  // Mute the camera on the primary display.
  LeftClickOn(camera_icon());
  ASSERT_TRUE(controller()->GetCameraMuted());
  ASSERT_TRUE(camera_icon()->toggled());

  // The secondary display camera icon should be toggled.
  auto* secondary_camera_icon =
      GetSecondaryVideoConferenceTray()->camera_icon();
  EXPECT_TRUE(secondary_camera_icon->toggled());

  // Unmute the camera on the secondary display.
  LeftClickOn(secondary_camera_icon);

  // The secondary display camera icon should not be toggled.
  EXPECT_FALSE(secondary_camera_icon->toggled());

  // The primary display camera icon should also not be toggled and the camera
  // should not be muted.
  EXPECT_FALSE(controller()->GetCameraMuted());
  EXPECT_FALSE(camera_icon()->toggled());
}

// Tests that the audio toggle state updates across displays.
TEST_F(VideoConferenceTrayTest, AudioButtonToggleAcrossDisplays) {
  SetTrayAndButtonsVisible();
  ASSERT_TRUE(video_conference_tray()->GetVisible());
  UpdateDisplay("800x700,800x700");
  ASSERT_TRUE(GetSecondaryVideoConferenceTray()->GetVisible());

  // Mute the audio on the primary display.
  LeftClickOn(audio_icon());
  ASSERT_TRUE(controller()->GetMicrophoneMuted());
  ASSERT_TRUE(audio_icon()->toggled());

  // The secondary display audio icon should be toggled.
  auto* secondary_audio_icon = GetSecondaryVideoConferenceTray()->audio_icon();
  EXPECT_TRUE(secondary_audio_icon->toggled());

  // Unmute the audio on the secondary display.
  LeftClickOn(secondary_audio_icon);

  // The secondary display audio icon should not be toggled.
  EXPECT_FALSE(secondary_audio_icon->toggled());

  // The primary display audio icon should also not be toggled and the audio
  // should not be muted.
  EXPECT_FALSE(controller()->GetMicrophoneMuted());
  EXPECT_FALSE(audio_icon()->toggled());
}

// Tests that the camera privacy indicators update on toggle across displays.
TEST_F(VideoConferenceTrayTest,
       PrivacyIndicatorToggleCameraOnSecondaryDisplay) {
  auto state = SetTrayAndButtonsVisible();
  ASSERT_TRUE(video_conference_tray()->GetVisible());
  UpdateDisplay("800x700,800x700");
  ASSERT_TRUE(GetSecondaryVideoConferenceTray()->GetVisible());

  // Turn privacy indicators on for the camera.
  state.is_capturing_camera = true;
  controller()->UpdateWithMediaState(state);

  // Toggle the camera off on the primary, the indicator should be updated on
  // the secondary.
  auto* secondary_camera_icon =
      GetSecondaryVideoConferenceTray()->camera_icon();
  LeftClickOn(camera_icon());
  ASSERT_FALSE(camera_icon()->show_privacy_indicator());
  EXPECT_FALSE(secondary_camera_icon->show_privacy_indicator());

  // Toggle the camera back on on the secondary, the indicator should be updated
  // on the primary.
  LeftClickOn(secondary_camera_icon);
  ASSERT_TRUE(secondary_camera_icon->show_privacy_indicator());
  EXPECT_TRUE(camera_icon()->show_privacy_indicator());
}

// Tests that the microphone privacy indicators update on toggle across
// displays.
TEST_F(VideoConferenceTrayTest, PrivacyIndicatorToggleAudioOnSecondaryDisplay) {
  auto state = SetTrayAndButtonsVisible();
  ASSERT_TRUE(video_conference_tray()->GetVisible());
  UpdateDisplay("800x700,800x700");
  ASSERT_TRUE(GetSecondaryVideoConferenceTray()->GetVisible());

  // Turn privacy indicators on for the microphone.
  state.is_capturing_microphone = true;
  controller()->UpdateWithMediaState(state);

  auto* secondary_audio_icon = GetSecondaryVideoConferenceTray()->audio_icon();

  // Toggle the audio off on the primary, the indicator should be updated on the
  // secondary.
  LeftClickOn(audio_icon());
  ASSERT_FALSE(audio_icon()->show_privacy_indicator());
  EXPECT_FALSE(secondary_audio_icon->show_privacy_indicator());

  // Toggle the audio back on on the secondary, the indicator should be updated
  // on the primary.
  LeftClickOn(secondary_audio_icon);
  ASSERT_TRUE(secondary_audio_icon->show_privacy_indicator());
  EXPECT_TRUE(audio_icon()->show_privacy_indicator());
}

// Tests that the tray is visible only in an active session.
TEST_F(VideoConferenceTrayTest, SessionChanged) {
  SetTrayAndButtonsVisible();

  SetSessionState(session_manager::SessionState::OOBE);
  EXPECT_FALSE(video_conference_tray()->GetVisible());

  SetSessionState(session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_FALSE(video_conference_tray()->GetVisible());

  SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(video_conference_tray()->GetVisible());

  // Locks screen. The tray should be hidden.
  SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_FALSE(video_conference_tray()->GetVisible());

  // Switches back to active. The tray should show.
  SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(video_conference_tray()->GetVisible());
}

}  // namespace ash