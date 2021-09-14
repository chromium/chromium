// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/shopping_list/shopping_data_provider.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/power_bookmarks/proto/power_bookmark_meta.pb.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace shopping_list {
namespace {

const char kLeadImageUrl[] = "image.png";
const char kFallbackImageUrl[] = "fallback_image.png";

const char kMainTitle[] = "Title";
const char kFallbackTitle[] = "Fallback Title";

TEST(ShoppingDataProviderTest, TestDataMergeWithLeadImage) {
  power_bookmarks::PowerBookmarkMeta meta;
  meta.mutable_lead_image()->set_url(kLeadImageUrl);

  base::DictionaryValue data_map;
  data_map.SetString("image", kFallbackImageUrl);

  MergeData(&meta, data_map);

  EXPECT_EQ(kLeadImageUrl, meta.lead_image().url());
  EXPECT_EQ(1, meta.fallback_images().size());
  EXPECT_EQ(kFallbackImageUrl, meta.fallback_images().Get(0).url());
}

TEST(ShoppingDataProviderTest, TestDataMergeWithNoLeadImage) {
  power_bookmarks::PowerBookmarkMeta meta;

  base::DictionaryValue data_map;
  data_map.SetString("image", kFallbackImageUrl);

  MergeData(&meta, data_map);

  EXPECT_EQ(kFallbackImageUrl, meta.lead_image().url());
  EXPECT_EQ(0, meta.fallback_images().size());
}

TEST(ShoppingDataProviderTest, TestDataMergeWithTitle) {
  power_bookmarks::PowerBookmarkMeta meta;
  meta.mutable_shopping_specifics()->set_title(kMainTitle);

  base::DictionaryValue data_map;
  data_map.SetString("title", kFallbackTitle);

  MergeData(&meta, data_map);

  EXPECT_EQ(kMainTitle, meta.shopping_specifics().title());
}

TEST(ShoppingDataProviderTest, TestDataMergeWithNoTitle) {
  power_bookmarks::PowerBookmarkMeta meta;

  base::DictionaryValue data_map;
  data_map.SetString("title", kFallbackTitle);

  MergeData(&meta, data_map);

  commerce::BuyableProduct updated_product;

  EXPECT_EQ(kFallbackTitle, meta.shopping_specifics().title());
}

}  // namespace
}  // namespace shopping_list
