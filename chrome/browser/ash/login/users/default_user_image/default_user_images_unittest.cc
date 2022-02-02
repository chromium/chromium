// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace default_user_image {

TEST(DefaultUserImagesTest, CurrentImageSetShouldBeEligible) {
  std::unique_ptr<base::ListValue> current_default_images =
      default_user_image::GetCurrentImageSetAsListValue();

  for (auto& image_data : current_default_images.get()->GetListDeprecated()) {
    const auto index = image_data.FindIntPath("index");
    EXPECT_TRUE(index.has_value());
    EXPECT_TRUE(IsValidIndex(index.value()));
    EXPECT_TRUE(IsInCurrentImageSet(index.value()));

    const auto* url = image_data.FindStringPath("url");
    EXPECT_TRUE(url);
    EXPECT_EQ(GetDefaultImageUrl(index.value()).spec(), url->c_str());

    const auto* title = image_data.FindStringPath("title");
    EXPECT_TRUE(title);
  }
}

}  // namespace default_user_image
}  // namespace ash
