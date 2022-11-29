// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_tray.h"

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

// Returns whether the bitmaps backing the specified `gfx::ImageSkia` are equal.
bool BitmapsAreEqual(const gfx::ImageSkia& a, const gfx::ImageSkia& b) {
  return gfx::BitmapsAreEqual(*a.bitmap(), *b.bitmap());
}

}  // namespace

class VideoConferenceTrayTest : public AshTestBase {
 public:
  VideoConferenceTrayTest() = default;
  VideoConferenceTrayTest(const VideoConferenceTrayTest&) = delete;
  VideoConferenceTrayTest& operator=(const VideoConferenceTrayTest&) = delete;
  ~VideoConferenceTrayTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kVcControlsUi);

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

  VideoConferenceTray* video_conference_tray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->video_conference_tray();
  }

  views::ImageView* expand_indicator() {
    return video_conference_tray()->expand_indicator_;
  }

  FakeVideoConferenceTrayController* controller() { return controller_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
};

TEST_F(VideoConferenceTrayTest, ClickTrayButton) {
  EXPECT_FALSE(video_conference_tray()->GetBubbleView());

  // Clicking the tray button should construct and open up the bubble.
  LeftClickOn(expand_indicator());
  EXPECT_TRUE(video_conference_tray()->GetBubbleView());
  EXPECT_TRUE(video_conference_tray()->GetBubbleView()->GetVisible());

  // Clicking it again should reset the bubble.
  LeftClickOn(expand_indicator());
  EXPECT_FALSE(video_conference_tray()->GetBubbleView());

  LeftClickOn(expand_indicator());
  EXPECT_TRUE(video_conference_tray()->GetBubbleView());
  EXPECT_TRUE(video_conference_tray()->GetBubbleView()->GetVisible());

  // Click anywhere else outside the bubble (i.e. the status area button) should
  // close the bubble.
  LeftClickOn(
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->unified_system_tray());
  EXPECT_FALSE(video_conference_tray()->GetBubbleView());
}

TEST_F(VideoConferenceTrayTest, ExpandIndicator) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);
  auto expected_image = gfx::CreateVectorIcon(
      kUnifiedMenuExpandIcon,
      video_conference_tray()->GetColorProvider()->GetColor(
          kColorAshIconColorPrimary));

  // When the bubble is not open in horizontal shelf, the indicator should point
  // up (not rotated).
  EXPECT_TRUE(BitmapsAreEqual(expected_image, expand_indicator()->GetImage()));

  // When the bubble is open in horizontal shelf, the indicator should point
  // down.
  LeftClickOn(expand_indicator());
  EXPECT_TRUE(
      BitmapsAreEqual(gfx::ImageSkiaOperations::CreateRotatedImage(
                          expected_image, SkBitmapOperations::ROTATION_180_CW),
                      expand_indicator()->GetImage()));

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);

  // When the bubble is not open in left shelf, the indicator should point to
  // the right.
  LeftClickOn(expand_indicator());
  EXPECT_TRUE(
      BitmapsAreEqual(gfx::ImageSkiaOperations::CreateRotatedImage(
                          expected_image, SkBitmapOperations::ROTATION_90_CW),
                      expand_indicator()->GetImage()));

  // When the bubble is open in left shelf, the indicator should point to the
  // left.
  LeftClickOn(expand_indicator());
  EXPECT_TRUE(
      BitmapsAreEqual(gfx::ImageSkiaOperations::CreateRotatedImage(
                          expected_image, SkBitmapOperations::ROTATION_270_CW),
                      expand_indicator()->GetImage()));

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);

  // When the bubble is not open in right shelf, the indicator should point to
  // the left.
  LeftClickOn(expand_indicator());
  EXPECT_TRUE(
      BitmapsAreEqual(gfx::ImageSkiaOperations::CreateRotatedImage(
                          expected_image, SkBitmapOperations::ROTATION_270_CW),
                      expand_indicator()->GetImage()));

  // When the bubble is open in right shelf, the indicator should point to the
  // right.
  LeftClickOn(expand_indicator());
  EXPECT_TRUE(
      BitmapsAreEqual(gfx::ImageSkiaOperations::CreateRotatedImage(
                          expected_image, SkBitmapOperations::ROTATION_90_CW),
                      expand_indicator()->GetImage()));
}

TEST_F(VideoConferenceTrayTest, ToggleCameraButton) {
  auto* camera_icon = video_conference_tray()->camera_icon();
  EXPECT_FALSE(camera_icon->toggled());

  // Click the button should mute the camera.
  LeftClickOn(camera_icon);
  EXPECT_TRUE(controller()->camera_soft_muted());
  EXPECT_TRUE(camera_icon->toggled());

  // Toggle again, should be unmuted.
  LeftClickOn(camera_icon);
  EXPECT_FALSE(controller()->camera_soft_muted());
  EXPECT_FALSE(camera_icon->toggled());
}

TEST_F(VideoConferenceTrayTest, PrivacyIndicator) {
  auto* camera_icon = video_conference_tray()->camera_icon();
  auto* audio_icon = video_conference_tray()->audio_icon();

  // Privacy indicator should be shown when camera is actively capturing video.
  EXPECT_FALSE(camera_icon->show_privacy_indicator());
  VideoConferenceMediaState state;
  state.is_capturing_camera = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(camera_icon->show_privacy_indicator());

  // Privacy indicator should be shown when microphone is actively capturing
  // audio.
  EXPECT_FALSE(audio_icon->show_privacy_indicator());
  state.is_capturing_microphone = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(audio_icon->show_privacy_indicator());

  // Should not show indicator when not capture.
  state.is_capturing_camera = false;
  state.is_capturing_microphone = false;
  controller()->UpdateWithMediaState(state);
  EXPECT_FALSE(camera_icon->show_privacy_indicator());
  EXPECT_FALSE(audio_icon->show_privacy_indicator());
}

}  // namespace ash