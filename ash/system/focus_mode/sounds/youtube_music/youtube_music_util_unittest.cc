// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_util.h"

#include <array>
#include <memory>

#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_types.h"
#include "base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::youtube_music {

namespace {

// Tests that `FindBestImage` returns the right image to use.
TEST(YouTubeMusicUtilTest, FindBestImage) {
  std::array<Image, 4> image_data = {
      Image(50, 50, GURL()),
      Image(100, 100, GURL()),
      Image(200, 200, GURL()),
      Image(300, 300, GURL()),
  };
  std::vector<Image> test_images;
  EXPECT_EQ(FindBestImage(test_images).ToString(), Image().ToString());

  test_images.emplace_back(image_data[0]);
  EXPECT_EQ(FindBestImage(test_images).ToString(), image_data[0].ToString());

  test_images.emplace_back(image_data[1]);
  EXPECT_EQ(FindBestImage(test_images).ToString(), image_data[1].ToString());

  test_images.emplace_back(image_data[2]);
  EXPECT_EQ(FindBestImage(test_images).ToString(), image_data[2].ToString());

  test_images.emplace_back(image_data[3]);
  EXPECT_EQ(FindBestImage(test_images).ToString(), image_data[2].ToString());
}

}  // namespace

}  // namespace ash::youtube_music
