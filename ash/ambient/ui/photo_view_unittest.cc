// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/photo_view.h"

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/image_view.h"

namespace ash {

using AmbientPhotoViewTest = AmbientAshTestBase;

// Test that image is scaled to fill screen width when image is portrait and
// screen is portrait. The top and bottom of the image will be cut off, as
// the height of the image is taller than the height of the screen.
TEST_F(AmbientPhotoViewTest, ShouldResizePortraitImageForPortraitScreen) {
  SetPhotoViewImageSize(/*width=*/10, /*height=*/20);

  UpdateDisplay("600x800");

  ShowAmbientScreen();

  FastForwardToNextImage();

  auto* image_view = GetAmbientBackgroundImageView();

  // Image should be full width. Image height should extend above and below the
  // visible part of the screen.
  ASSERT_EQ(image_view->GetImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/-200, /*width=*/600, /*height=*/1200));
  ASSERT_EQ(image_view->GetRelatedImageBoundsForTesting(), gfx::Rect());
}

// Test that image is scaled to fill screen width when the image is landscape
// and the screen is portrait. There will be black bars to the top and bottom of
// the image, as the height of the image is less than the height of the screen.
TEST_F(AmbientPhotoViewTest, ShouldResizeLandscapeImageForPortraitScreen) {
  SetPhotoViewImageSize(/*width=*/30, /*height=*/20);

  UpdateDisplay("600x800");

  ShowAmbientScreen();

  FastForwardToNextImage();

  auto* image_view = GetAmbientBackgroundImageView();

  // Image should be full width. Image should have equal empty space top and
  // bottom.
  ASSERT_EQ(image_view->GetImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/200, /*width=*/600, /*height=*/400));
  ASSERT_EQ(image_view->GetRelatedImageBoundsForTesting(), gfx::Rect());
}

// Test that two portrait images are scaled and tiled to fill screen when the
// screen is landscape.
TEST_F(AmbientPhotoViewTest, ShouldTileTwoPortraitImagesForLandscapeScreen) {
  SetPhotoViewImageSize(/*width=*/10, /*height=*/20);

  UpdateDisplay("808x600");

  ShowAmbientScreen();

  FastForwardToNextImage();

  auto* image_view = GetAmbientBackgroundImageView();

  // Will tile two portrait images. Each image will fill 400x600 area with 8px
  // spaing in between.
  // Image should be full width. Image height should extend above and below the
  // visible part of the view.
  ASSERT_EQ(image_view->GetImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/-100, /*width=*/400, /*height=*/800));
  ASSERT_EQ(image_view->GetRelatedImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/-100, /*width=*/400, /*height=*/800));
}

// Test that two portrait images are scaled and tiled to fill screen when the
// screen is landscape. For odd screen width, the first image will take one more
// pixel.
TEST_F(AmbientPhotoViewTest,
       ShouldTileTwoPortraitImagesForLandscapeScreenWithOddWidth) {
  SetPhotoViewImageSize(/*width=*/10, /*height=*/20);

  constexpr int kScreenWidth = 809;
  constexpr int kScreenHeight = 600;
  std::string display_size = base::NumberToString(kScreenWidth) + "x" +
                             base::NumberToString(kScreenHeight);
  UpdateDisplay(display_size);

  ShowAmbientScreen();

  FastForwardToNextImage();

  auto* image_view = GetAmbientBackgroundImageView();

  // Will tile two portrait images.
  // The first image will fill 401x802 area with 8px spaing in between. Image
  // should be full width. Image height should extend above and below the
  // visible part of the view.
  const int image_width = (kScreenWidth - kMarginLeftOfRelatedImageDip + 1) / 2;
  const int image_height = 20 * image_width / 10;
  const int y = (kScreenHeight - image_height) / 2;
  ASSERT_EQ(image_view->GetImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/y, /*width=*/image_width,
                      /*height=*/image_height));
  ASSERT_EQ(image_view->GetRelatedImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/-100, /*width=*/400, /*height=*/800));
}

// Test that landscape images can be tiled when screen is landscape as long as
// they are related.
TEST_F(AmbientPhotoViewTest,
       ShouldNotTileTwoLandscapeImagesForLandscapeScreen) {
  SetPhotoViewImageSize(/*width=*/20, /*height=*/10);

  UpdateDisplay("808x600");

  ShowAmbientScreen();

  FastForwardToNextImage();

  auto* image_view = GetAmbientBackgroundImageView();

  // Show two landscape image.
  // Image should be full height. Image width should extend equally to the left
  // and right of the visible part of the screen.
  ASSERT_EQ(image_view->GetImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/200, /*width=*/400, /*height=*/200));
  ASSERT_EQ(image_view->GetRelatedImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/200, /*width=*/400, /*height=*/200));
}

// Test that only have one available image will not be tiled when screen is
// landscape.
TEST_F(AmbientPhotoViewTest,
       ShouldNotTileIfRelatedImageIsNullForLandscapeScreen) {
  SetPhotoViewImageSize(/*width=*/10, /*height=*/20);

  UpdateDisplay("800x600");

  ShowAmbientScreen();

  FastForwardToNextImage();

  auto* image_view = GetAmbientBackgroundImageView();

  // Remove the related image.
  image_view->ResetRelatedImageForTesting();

  // Trigger layout.
  UpdateDisplay("808x600");

  // Only show one portrait image.
  // Image should be full height. Image should have equal empty space left and
  // right.
  ASSERT_EQ(image_view->GetImageBoundsForTesting(),
            gfx::Rect(/*x=*/254, /*y=*/0, /*width=*/300, /*height=*/600));
  ASSERT_EQ(image_view->GetRelatedImageBoundsForTesting(), gfx::Rect());
}

// Test that image is scaled to fill screen height when the image is landscape
// and no related image when the screen is landscape.
// The image will be zoomed in and the left and right will be cut off, as the
// width of the image is greater than the width of the screen.
TEST_F(AmbientPhotoViewTest, ShouldResizeLandscapeImageForLandscapeScreen) {
  SetPhotoViewImageSize(/*width=*/30, /*height=*/20);

  UpdateDisplay("800x600");

  ShowAmbientScreen();

  FastForwardToNextImage();

  auto* image_view = GetAmbientBackgroundImageView();

  // Remove the related image.
  image_view->ResetRelatedImageForTesting();

  // Trigger layout.
  UpdateDisplay("808x600");

  // Image should be full height. Image width should extend equally to the left
  // and right of the visible part of the screen.
  ASSERT_EQ(image_view->GetImageBoundsForTesting(),
            gfx::Rect(/*x=*/-46, /*y=*/0, /*width=*/900, /*height=*/600));
  ASSERT_EQ(image_view->GetRelatedImageBoundsForTesting(), gfx::Rect());
}

// Test that when rotates to portrait screen, will dynamically only show one
// portrait image.
TEST_F(AmbientPhotoViewTest, ShouldNotTileWhenRotateToPortraitScreen) {
  SetPhotoViewImageSize(/*width=*/10, /*height=*/20);

  UpdateDisplay("808x600");

  ShowAmbientScreen();

  FastForwardToNextImage();

  auto* image_view = GetAmbientBackgroundImageView();

  // Will tile two portrait images. Each image will fill 400x600 area with 8px
  // spaing in between.
  // Image should be full width. Image height should extend above and below the
  // visible part of the view.
  ASSERT_EQ(image_view->GetImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/-100, /*width=*/400, /*height=*/800));
  ASSERT_EQ(image_view->GetRelatedImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/-100, /*width=*/400, /*height=*/800));

  // Rotate screen.
  UpdateDisplay("600x808");
  // Only one image will show.
  // Image should be full width. Image height should extend above and below the
  // visible part of the screen.
  ASSERT_EQ(image_view->GetImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/-196, /*width=*/600, /*height=*/1200));
  ASSERT_EQ(image_view->GetRelatedImageBoundsForTesting(), gfx::Rect());
}

// Test that when rotates to landscape screen, will dynamically tile two
// portrait images.
TEST_F(AmbientPhotoViewTest, ShouldTileWhenRotateToLandscapeScreen) {
  SetPhotoViewImageSize(/*width=*/10, /*height=*/20);

  UpdateDisplay("600x808");

  ShowAmbientScreen();

  FastForwardToNextImage();

  auto* image_view = GetAmbientBackgroundImageView();

  // Only one image will show.
  // Image should be full width. Image height should extend above and below the
  // visible part of the screen.
  ASSERT_EQ(image_view->GetImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/-196, /*width=*/600, /*height=*/1200));
  ASSERT_EQ(image_view->GetRelatedImageBoundsForTesting(), gfx::Rect());

  // Rotate screen.
  UpdateDisplay("808x600");

  // Will tile two portrait images. Each image will fill 400x600 area with 8px
  // spaing in between.
  // Image should be full width. Image height should extend above and below the
  // visible part of the view.
  ASSERT_EQ(image_view->GetImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/-100, /*width=*/400, /*height=*/800));
  ASSERT_EQ(image_view->GetRelatedImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/-100, /*width=*/400, /*height=*/800));
}

// Test that two protrat images will resize when bounds changes in landscape.
TEST_F(AmbientPhotoViewTest, ShouldResizeTiledPortraitImagesWhenBoundsChanged) {
  SetPhotoViewImageSize(/*width=*/10, /*height=*/20);

  UpdateDisplay("808x600");

  ShowAmbientScreen();

  FastForwardToNextImage();

  auto* image_view = GetAmbientBackgroundImageView();

  // Will tile two portrait images. Each image will fill 400x600 area with 8px
  // spaing in between.
  // Image should be full width. Image height should extend above and below the
  // visible part of the view.
  ASSERT_EQ(image_view->GetImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/-100, /*width=*/400, /*height=*/800));
  ASSERT_EQ(image_view->GetRelatedImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/-100, /*width=*/400, /*height=*/800));

  // Bounds changes so that image will be shown in portrait view.
  UpdateDisplay("508x200");

  // Will tile two portrait images. Each image will fill 250x200 area with 8px
  // spaing in between.
  // Image should be full height. Image should have equal empty space left and
  // right.
  ASSERT_EQ(image_view->GetImageBoundsForTesting(),
            gfx::Rect(/*x=*/75, /*y=*/0, /*width=*/100, /*height=*/200));
  ASSERT_EQ(image_view->GetRelatedImageBoundsForTesting(),
            gfx::Rect(/*x=*/75, /*y=*/0, /*width=*/100, /*height=*/200));

  // Bounds changes so that image will be shown in portrait view.
  UpdateDisplay("308x200");

  // Will tile two portrait images. Each image will fill 150x200 area with 8px
  // spaing in between.
  // Image should be full width. Image height should extend above and below the
  // visible part of the view.
  ASSERT_EQ(image_view->GetImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/-50, /*width=*/150, /*height=*/300));
  ASSERT_EQ(image_view->GetRelatedImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/-50, /*width=*/150, /*height=*/300));

  // Bounds changes to exact aspect ratio of the image.
  UpdateDisplay("208x200");

  // Will tile two portrait images. Each image will fill 100x200 area with 8px
  // spaing in between.
  // Image should be full width and height.
  ASSERT_EQ(image_view->GetImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/0, /*width=*/100, /*height=*/200));
  ASSERT_EQ(image_view->GetRelatedImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/0, /*width=*/100, /*height=*/200));
}

}  // namespace ash
