// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_avatar_icon_util.h"

#include "chrome/grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

// Helper function to check that the image is sized properly
// and supports multiple pixel densities.
void VerifyScaling(gfx::Image& image, gfx::Size& size) {
  gfx::Size canvas_size(100, 100);
  gfx::Canvas canvas(canvas_size, 1.0f, false);
  gfx::Canvas canvas2(canvas_size, 2.0f, false);

  ASSERT_FALSE(gfx::test::IsEmpty(image));
  EXPECT_EQ(image.Size(), size);

  gfx::ImageSkia image_skia = *image.ToImageSkia();
  canvas.DrawImageInt(image_skia, 15, 10);
  EXPECT_TRUE(image.ToImageSkia()->HasRepresentation(1.0f));

  canvas2.DrawImageInt(image_skia, 15, 10);
  EXPECT_TRUE(image.ToImageSkia()->HasRepresentation(2.0f));
}

TEST(ProfileInfoUtilTest, SizedMenuIcon) {
  gfx::Image rect_picture(gfx::test::CreateImage(100, 50));

  gfx::Size size(30, 20);
  gfx::Image result =
      profiles::GetSizedAvatarIcon(rect_picture, size.width(), size.height());

  VerifyScaling(result, size);
}

TEST(ProfileInfoUtilTest, WebUIIcon) {
  gfx::Image rect_picture(gfx::test::CreateImage(100, 50));
  gfx::Size size(profiles::kAvatarIconSize, profiles::kAvatarIconSize);
  gfx::Image result = profiles::GetAvatarIconForWebUI(rect_picture);

  VerifyScaling(result, size);
}

TEST(ProfileInfoUtilTest, TitleBarIcon) {
  int width = 100;
  int height = 40;

  // Test that an avatar icon isn't changed.
  const gfx::Image& profile_image(
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_PROFILE_AVATAR_0));
  gfx::Image result =
      profiles::GetAvatarIconForTitleBar(profile_image, width, height);
  EXPECT_FALSE(gfx::test::IsEmpty(result));
  EXPECT_TRUE(gfx::test::AreImagesEqual(profile_image, result));
}

}  // namespace
