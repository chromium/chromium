// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_tray.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/focus_cycler.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

class VideoConferenceTrayPixelTest : public AshTestBase {
 public:
  VideoConferenceTrayPixelTest() : AshTestBase() {}
  VideoConferenceTrayPixelTest(const VideoConferenceTrayPixelTest&) = delete;
  VideoConferenceTrayPixelTest& operator=(const VideoConferenceTrayPixelTest&) =
      delete;
  ~VideoConferenceTrayPixelTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kVcStopAllScreenShare,
                              features::kFeatureManagementVideoConference},
        /*disabled_features=*/{features::kVcBackgroundReplace});
    // TODO(b/334375880): Add a specific pixel test for the feature
    // VcBackgroundReplace.

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

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
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

  VideoConferenceTray* video_conference_tray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->video_conference_tray();
  }

  VideoConferenceTrayButton* audio_icon() {
    return video_conference_tray()->audio_icon();
  }

  FakeVideoConferenceTrayController* controller() { return controller_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
};

TEST_F(VideoConferenceTrayPixelTest, BasicPixelTest) {
  SetTrayAndButtonsVisible();
  EXPECT_TRUE(video_conference_tray()->GetVisible());
  EXPECT_TRUE(audio_icon()->GetVisible());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_no_focus_not_toggled",
      /*revision_number=*/3, video_conference_tray()));

  Shell::Get()->focus_cycler()->FocusWidget(
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())
          ->shelf_widget()
          ->status_area_widget());

  while (!audio_icon()->HasFocus()) {
    PressAndReleaseKey(ui::VKEY_TAB);
  }

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_audio_focused_not_toggled",
      /*revision_number=*/3, video_conference_tray()));

  PressAndReleaseKey(ui::VKEY_RETURN);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_audio_focused_and_toggled",
      /*revision_number=*/5, video_conference_tray()));

  // Un-toggle the audio icon, then focus the video icon.
  PressAndReleaseKey(ui::VKEY_RETURN);
  PressAndReleaseKey(ui::VKEY_TAB);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_video_focused_not_toggled",
      /*revision_number=*/3, video_conference_tray()));

  PressAndReleaseKey(ui::VKEY_RETURN);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_video_focused_and_toggled",
      /*revision_number=*/3, video_conference_tray()));

  // Un-toggle the video icon, then focus the screen capture icon.
  PressAndReleaseKey(ui::VKEY_RETURN);
  PressAndReleaseKey(ui::VKEY_TAB);

  // For screen capture, the button cannot be toggled.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_screen_capture_focused_not_toggled",
      /*revision_number=*/3, video_conference_tray()));

  // Focus the toggle button icon.
  PressAndReleaseKey(ui::VKEY_TAB);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_toggle_bubble_focused_not_toggled",
      /*revision_number=*/3, video_conference_tray()));

  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_toggle_bubble_focused_and_toggled",
      /*revision_number=*/3, video_conference_tray()));
}

TEST_F(VideoConferenceTrayPixelTest, VideoConferenceTrayIconStates) {
  VideoConferenceMediaState state;
  state.has_media_app = true;
  state.has_camera_permission = true;
  state.has_microphone_permission = true;
  state.is_capturing_camera = true;
  state.is_capturing_microphone = true;
  state.is_capturing_screen = true;
  controller()->UpdateWithMediaState(state);

  auto* camera_icon = video_conference_tray()->camera_icon();
  auto* audio_icon = video_conference_tray()->audio_icon();
  auto* screen_share_icon = video_conference_tray()->screen_share_icon();

  ASSERT_TRUE(video_conference_tray()->GetVisible());
  ASSERT_TRUE(camera_icon);
  ASSERT_TRUE(audio_icon);
  ASSERT_TRUE(screen_share_icon);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_camera_icon_with_indicator",
      /*revision_number=*/0, camera_icon));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_audio_icon_with_indicator",
      /*revision_number=*/0, audio_icon));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_screen_share_icon_with_indicator",
      /*revision_number=*/0, screen_share_icon));

  // Toggle to mute the icons.
  LeftClickOn(camera_icon);
  LeftClickOn(audio_icon);

  ASSERT_TRUE(camera_icon->toggled());
  ASSERT_TRUE(audio_icon->toggled());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_camera_icon_muted",
      /*revision_number=*/0, camera_icon));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_audio_icon_muted",
      /*revision_number=*/0, audio_icon));

  // Toggle again to unmute.
  LeftClickOn(camera_icon);
  LeftClickOn(audio_icon);

  state.is_capturing_camera = false;
  state.is_capturing_microphone = false;
  controller()->UpdateWithMediaState(state);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_camera_icon",
      /*revision_number=*/0, camera_icon));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_audio_icon",
      /*revision_number=*/0, audio_icon));
}

}  // namespace ash
