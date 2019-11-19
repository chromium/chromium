// Copyright 2014 The Chromium Authors. All rights reserved.
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
  // Test that an avatar icon isn't changed.
  const gfx::Image& profile_image(
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_PROFILE_AVATAR_0));
  gfx::Image result =
      profiles::GetSizedAvatarIcon(profile_image, false, 50, 50);

  EXPECT_FALSE(gfx::test::IsEmpty(result));
  EXPECT_TRUE(gfx::test::AreImagesEqual(profile_image, result));

  // Test that a rectangular picture (e.g., GAIA image) is changed.
  gfx::Image rect_picture(gfx::test::CreateImage());

  gfx::Size size(30, 20);
  gfx::Image result2 = profiles::GetSizedAvatarIcon(
      rect_picture, true, size.width(), size.height());

  VerifyScaling(result2, size);
}

TEST(ProfileInfoUtilTest, WebUIIcon) {
  // Test that an avatar icon isn't changed.
  const gfx::Image& profile_image(
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_PROFILE_AVATAR_0));
  gfx::Image result = profiles::GetAvatarIconForWebUI(profile_image, false);
  EXPECT_FALSE(gfx::test::IsEmpty(result));
  EXPECT_TRUE(gfx::test::AreImagesEqual(profile_image, result));

  // Test that a rectangular picture is changed.
  gfx::Image rect_picture(gfx::test::CreateImage());
  gfx::Size size(profiles::kAvatarIconSize, profiles::kAvatarIconSize);
  gfx::Image result2 = profiles::GetAvatarIconForWebUI(rect_picture, true);

  VerifyScaling(result2, size);
}

TEST(ProfileInfoUtilTest, TitleBarIcon) {
  int width = 100;
  int height = 40;

  // Test that an avatar icon isn't changed.
  const gfx::Image& profile_image(
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_PROFILE_AVATAR_0));
  gfx::Image result = profiles::GetAvatarIconForTitleBar(
      profile_image, false, width, height);
  EXPECT_FALSE(gfx::test::IsEmpty(result));
  EXPECT_TRUE(gfx::test::AreImagesEqual(profile_image, result));

  // Test that a rectangular picture is changed.
  gfx::Image rect_picture(gfx::test::CreateImage());

  gfx::Size size(width, height);
  gfx::Image result2 = profiles::GetAvatarIconForTitleBar(
      rect_picture, true, width, height);

  VerifyScaling(result2, size);
}

}  // namespace
