// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_tray.h"

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/test/event_generator.h"
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

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kVcControlsUi);

    AshTestBase::SetUp();
  }

  VideoConferenceTray* video_conference_tray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->video_conference_tray();
  }

  views::ImageView* expand_indicator() {
    return video_conference_tray()->expand_indicator_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary));

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

}  // namespace ash