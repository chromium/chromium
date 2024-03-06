// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"

#include <string_view>

#include "ash/public/cpp/default_user_image.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char k100PercentPrefix[] = "default_100_percent/";
constexpr char k200PercentPrefix[] = "default_200_percent/";

}  // namespace

namespace ash::default_user_image {

TEST(DefaultUserImagesTest, CurrentImageSetShouldBeEligible) {
  std::vector<DefaultUserImage> current_default_images =
      default_user_image::GetCurrentImageSet();

  for (auto& image : current_default_images) {
    const auto index = image.index;
    EXPECT_TRUE(IsValidIndex(index));
    EXPECT_TRUE(IsInCurrentImageSet(index));

    const auto default_user_image = GetDefaultUserImage(index);
    EXPECT_EQ(default_user_image.url, image.url);
    EXPECT_EQ(default_user_image.title, image.title);
  }
}

TEST(DefaultUserImagesTest, RandomlyPickedIndexShouldBeCurrent) {
  const auto index = default_user_image::GetRandomDefaultImageIndex();
  EXPECT_TRUE(IsValidIndex(index));
  EXPECT_TRUE(IsInCurrentImageSet(index));

  const auto default_user_image = GetDefaultUserImage(index);
  EXPECT_FALSE(default_user_image.title.empty());
}

TEST(DefaultUserImagesTest, CurrentImageSetShouldBeEligibleWithFlag) {
  std::vector<DefaultUserImage> current_default_images =
      default_user_image::GetCurrentImageSet();

  for (auto& image : current_default_images) {
    const auto index = image.index;
    EXPECT_TRUE(IsValidIndex(index));
    EXPECT_TRUE(IsInCurrentImageSet(index));

    const auto default_user_image = GetDefaultUserImage(index);
    EXPECT_EQ(default_user_image.url, image.url);
    EXPECT_EQ(default_user_image.title, image.title);

    const auto url_string = image.url.spec();

    // Current image set should have support for 200 percent scale factor.
    EXPECT_NE(url_string.find(k200PercentPrefix), std::string_view::npos);
  }
}

TEST(DefaultUserImagesTest, AllDefaultImagesShouldHaveCorrectInfoWithFlag) {
  for (auto index = 0; index < kDefaultImagesCount; index++) {
    EXPECT_TRUE(IsValidIndex(index));
    bool is_current = IsInCurrentImageSet(index);

    const auto default_user_image = GetDefaultUserImage(index);

    // Images in the current set should have a valid title.
    if (is_current) {
      EXPECT_FALSE(default_user_image.title.empty());
    }

    const auto url_string = default_user_image.url.spec();
    if (index <= kLastLegacyImageIndex) {
      EXPECT_NE(url_string.find(k100PercentPrefix), std::string_view::npos);
    } else {
      EXPECT_NE(url_string.find(k200PercentPrefix), std::string_view::npos);
    }
  }
}

}  // namespace ash::default_user_image
