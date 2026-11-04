// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_icon_cache.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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

TEST(BirchIconCacheTest, EvictsLeastRecentlyUsedIconsBeyondMaxCapacity) {
  BirchIconCache cache;
  auto make_key = [](size_t index) {
    return base::StrCat(
        {"https://example.com/icon/", base::NumberToString(index)});
  };

  for (size_t i = 0; i < BirchIconCache::kMaxCacheSize; ++i) {
    cache.Put(make_key(i), gfx::test::CreateImageSkia(16));
  }
  EXPECT_EQ(cache.size_for_test(), BirchIconCache::kMaxCacheSize);

  // Accessing entry 0 promotes it to most-recently-used.
  EXPECT_FALSE(cache.Get(make_key(0)).isNull());

  // Adding one more entry beyond capacity should evict entry 1 (LRU).
  cache.Put(make_key(BirchIconCache::kMaxCacheSize),
            gfx::test::CreateImageSkia(16));

  EXPECT_EQ(cache.size_for_test(), BirchIconCache::kMaxCacheSize);
  EXPECT_TRUE(cache.Get(make_key(1)).isNull());
  EXPECT_FALSE(cache.Get(make_key(0)).isNull());
  EXPECT_FALSE(cache.Get(make_key(BirchIconCache::kMaxCacheSize)).isNull());
}

TEST(BirchIconCacheTest, Clear) {
  BirchIconCache cache;
  cache.Put("key", gfx::test::CreateImageSkia(16));
  EXPECT_EQ(cache.size_for_test(), 1u);

  cache.Clear();
  EXPECT_EQ(cache.size_for_test(), 0u);
  EXPECT_TRUE(cache.Get("key").isNull());
}

}  // namespace ash
