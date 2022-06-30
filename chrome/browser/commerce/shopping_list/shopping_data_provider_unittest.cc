// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/shopping_list/shopping_data_provider.h"
#include "base/logging.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace shopping_list {
namespace {

const char kLeadImageUrl[] = "image.png";
const char kFallbackImageUrl[] = "fallback_image.png";
const char kCurrencyCode[] = "USD";
const char kCountryCode[] = "us";

const char kMainTitle[] = "Title";
const char kFallbackTitle[] = "Fallback Title";

const uint64_t kOfferId = 12345;
const uint64_t kClusterId = 67890;

TEST(ShoppingDataProviderTest, TestDataMergeWithLeadImage) {
  base::test::ScopedFeatureList test_features;
  test_features.InitWithFeatures({commerce::kCommerceAllowLocalImages,
                                  commerce::kCommerceAllowServerImages},
                                 {});

  power_bookmarks::PowerBookmarkMeta meta;
  meta.mutable_lead_image()->set_url(kLeadImageUrl);

  base::DictionaryValue data_map;
  data_map.SetStringKey("image", kFallbackImageUrl);

  MergeData(&meta, data_map);

  EXPECT_EQ(kLeadImageUrl, meta.lead_image().url());
  EXPECT_EQ(1, meta.fallback_images().size());
  EXPECT_EQ(kFallbackImageUrl, meta.fallback_images().Get(0).url());
}

TEST(ShoppingDataProviderTest, TestDataMergeWithNoLeadImage) {
  base::test::ScopedFeatureList test_features;
  test_features.InitWithFeatures({commerce::kCommerceAllowLocalImages,
                                  commerce::kCommerceAllowServerImages},
                                 {});

  power_bookmarks::PowerBookmarkMeta meta;

  base::DictionaryValue data_map;
  data_map.SetStringKey("image", kFallbackImageUrl);

  MergeData(&meta, data_map);

  EXPECT_EQ(kFallbackImageUrl, meta.lead_image().url());
  EXPECT_EQ(0, meta.fallback_images().size());
}

TEST(ShoppingDataProviderTest, TestDataMergeWithTitle) {
  power_bookmarks::PowerBookmarkMeta meta;
  meta.mutable_shopping_specifics()->set_title(kMainTitle);

  base::DictionaryValue data_map;
  data_map.SetStringKey("title", kFallbackTitle);

  MergeData(&meta, data_map);

  EXPECT_EQ(kMainTitle, meta.shopping_specifics().title());
}

TEST(ShoppingDataProviderTest, TestDataMergeWithNoTitle) {
  power_bookmarks::PowerBookmarkMeta meta;

  base::DictionaryValue data_map;
  data_map.SetStringKey("title", kFallbackTitle);

  MergeData(&meta, data_map);

  commerce::BuyableProduct updated_product;

  EXPECT_EQ(kFallbackTitle, meta.shopping_specifics().title());
}

TEST(ShoppingDataProviderTest, TestPopulateShoppingSpecifics) {
  power_bookmarks::PowerBookmarkMeta meta;

  base::DictionaryValue data_map;
  data_map.SetStringKey("title", kMainTitle);

  commerce::BuyableProduct product;
  product.set_title(kMainTitle);
  product.set_image_url(kLeadImageUrl);
  product.set_product_cluster_id(kClusterId);
  product.mutable_current_price()->set_amount_micros(100L);
  product.mutable_current_price()->set_currency_code(kCurrencyCode);
  product.set_offer_id(kOfferId);
  product.set_country_code(kCountryCode);

  power_bookmarks::ShoppingSpecifics out_specifics;

  PopulateShoppingSpecifics(product, &out_specifics);

  EXPECT_EQ(kMainTitle, out_specifics.title());
  EXPECT_EQ(kLeadImageUrl, out_specifics.image_url());
  EXPECT_EQ(kClusterId, out_specifics.product_cluster_id());
  EXPECT_EQ(100L, out_specifics.current_price().amount_micros());
  EXPECT_EQ(kCurrencyCode, out_specifics.current_price().currency_code());
  EXPECT_EQ(kOfferId, out_specifics.offer_id());
  EXPECT_EQ(kCountryCode, out_specifics.country_code());
}

TEST(ShoppingDataProviderTest, TestPopulateShoppingSpecificsMissingData) {
  power_bookmarks::PowerBookmarkMeta meta;

  base::DictionaryValue data_map;
  data_map.SetStringKey("title", kMainTitle);

  commerce::BuyableProduct product;
  product.set_title(kMainTitle);
  product.set_image_url(kLeadImageUrl);

  power_bookmarks::ShoppingSpecifics out_specifics;

  PopulateShoppingSpecifics(product, &out_specifics);

  EXPECT_EQ(kMainTitle, out_specifics.title());
  EXPECT_EQ(kLeadImageUrl, out_specifics.image_url());
  EXPECT_FALSE(out_specifics.has_product_cluster_id());
  EXPECT_FALSE(out_specifics.has_current_price());
  EXPECT_FALSE(out_specifics.has_country_code());
}

}  // namespace
}  // namespace shopping_list
