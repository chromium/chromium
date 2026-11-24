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
          IDR_PROFILE_AVATAR_26));
  gfx::Image result =
      profiles::GetAvatarIconForTitleBar(profile_image, width, height);
  EXPECT_FALSE(gfx::test::IsEmpty(result));
  EXPECT_TRUE(gfx::test::AreImagesEqual(profile_image, result));
}

// The indices of the removed legacy avatar icons, below the placeholder avatar
// index, are not valid default avatar indices.
TEST(ProfileInfoUtilTest, LegacyAvatarIndicesAreInvalid) {
  const size_t placeholder_index = profiles::GetPlaceholderAvatarIndex();
  for (size_t i = 0; i < placeholder_index; ++i) {
    SCOPED_TRACE(i);
    EXPECT_FALSE(profiles::IsDefaultAvatarIconIndex(i));
    EXPECT_EQ(placeholder_index,
              profiles::GetSanitizedAvatarIndex(static_cast<int>(i)));
  }

  size_t icon_index = 0;
  EXPECT_FALSE(profiles::IsDefaultAvatarIconUrl(
      "chrome://theme/IDR_PROFILE_AVATAR_0", &icon_index));
  EXPECT_FALSE(profiles::IsDefaultAvatarIconUrl(
      "chrome://theme/IDR_PROFILE_AVATAR_25", &icon_index));
  EXPECT_EQ(placeholder_index, profiles::GetSanitizedAvatarIndex(0));

  EXPECT_TRUE(
      profiles::IsLegacyAvatarIconUrl("chrome://theme/IDR_PROFILE_AVATAR_0"));
  EXPECT_TRUE(
      profiles::IsLegacyAvatarIconUrl("chrome://theme/IDR_PROFILE_AVATAR_25"));
  EXPECT_FALSE(profiles::IsLegacyAvatarIconUrl(
      profiles::GetDefaultAvatarIconUrl(placeholder_index)));
  // Unknown indices beyond the known avatars are not legacy.
  EXPECT_FALSE(
      profiles::IsLegacyAvatarIconUrl("chrome://theme/IDR_PROFILE_AVATAR_999"));
  EXPECT_FALSE(profiles::IsLegacyAvatarIconUrl("chrome://theme/IDR_FOO_0"));
  EXPECT_FALSE(profiles::IsLegacyAvatarIconUrl(""));
}

TEST(ProfileInfoUtilTest, ValidAvatarIndices) {
  const size_t placeholder_index = profiles::GetPlaceholderAvatarIndex();
  for (size_t i = placeholder_index; i < profiles::GetDefaultAvatarIconCount();
       ++i) {
    SCOPED_TRACE(i);
    EXPECT_TRUE(profiles::IsDefaultAvatarIconIndex(i));
    EXPECT_EQ(i, profiles::GetSanitizedAvatarIndex(static_cast<int>(i)));

    size_t icon_index = 0;
    EXPECT_TRUE(profiles::IsDefaultAvatarIconUrl(
        profiles::GetDefaultAvatarIconUrl(i), &icon_index));
    EXPECT_EQ(i, icon_index);
  }

  EXPECT_FALSE(profiles::IsDefaultAvatarIconIndex(
      profiles::GetDefaultAvatarIconCount()));
  EXPECT_EQ(placeholder_index, profiles::GetSanitizedAvatarIndex(-1));
}

}  // namespace
