// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_icon_cache.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

TEST(BirchIconCacheTest, NotFound) {
  BirchIconCache cache;
  gfx::ImageSkia icon = cache.Get("not-present");
  EXPECT_TRUE(icon.isNull());
}

TEST(BirchIconCacheTest, Found) {
  BirchIconCache cache;
  gfx::ImageSkia input_icon = gfx::test::CreateImageSkia(16);
  cache.Put("key", input_icon);
  gfx::ImageSkia output_icon = cache.Get("key");
  EXPECT_FALSE(output_icon.isNull());
  EXPECT_TRUE(input_icon.BackedBySameObjectAs(output_icon));
}

}  // namespace ash
