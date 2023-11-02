// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"

#include "ash/public/cpp/default_user_image.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace default_user_image {

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

}  // namespace default_user_image
}  // namespace ash
