// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_slideshow_photo_config.h"

#include "ash/ambient/model/ambient_backend_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

using ::testing::Eq;

TEST(AmbientSlideshowPhotoConfigTest, GetNumAssetsInTopic) {
  AmbientSlideshowPhotoConfig photo_config;
  PhotoWithDetails downloaded_topic;
  EXPECT_THAT(photo_config.GetNumAssetsInTopic(downloaded_topic), Eq(0));
  downloaded_topic.photo =
      gfx::test::CreateImageSkia(/*width=*/100, /*height=*/100);
  EXPECT_THAT(photo_config.GetNumAssetsInTopic(downloaded_topic), Eq(1));
  downloaded_topic.related_photo =
      gfx::test::CreateImageSkia(/*width=*/100, /*height=*/100);
  EXPECT_THAT(photo_config.GetNumAssetsInTopic(downloaded_topic), Eq(1));
}

}  // namespace ash
