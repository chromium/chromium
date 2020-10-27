// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_item.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "base/bind_helpers.h"
#include "base/test/bind_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

namespace {

std::vector<HoldingSpaceItem::Type> GetHoldingSpaceItemTypes() {
  std::vector<HoldingSpaceItem::Type> types;
  for (int i = 0; i <= static_cast<int>(HoldingSpaceItem::Type::kMaxValue); ++i)
    types.push_back(static_cast<HoldingSpaceItem::Type>(i));
  return types;
}

}  // namespace

using HoldingSpaceItemTest = testing::TestWithParam<HoldingSpaceItem::Type>;

// Tests round-trip serialization for each holding space item type.
TEST_P(HoldingSpaceItemTest, Serialization) {
  const base::FilePath file_path("file_path");
  const GURL file_system_url("file_system_url");
  const gfx::ImageSkia placeholder(gfx::test::CreateImageSkia(10, 10));

  const auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), file_path, file_system_url,
      std::make_unique<HoldingSpaceImage>(
          placeholder, /*async_bitmap_resolver=*/base::DoNothing()));

  const base::DictionaryValue serialized_holding_space_item =
      holding_space_item->Serialize();

  const auto deserialized_holding_space_item = HoldingSpaceItem::Deserialize(
      serialized_holding_space_item,
      /*file_system_url_resolver=*/
      base::BindLambdaForTesting(
          [&](const base::FilePath& file_path) { return file_system_url; }),
      /*image_resolver=*/
      base::BindLambdaForTesting([&](HoldingSpaceItem::Type type,
                                     const base::FilePath& file_path) {
        return std::make_unique<HoldingSpaceImage>(
            placeholder, /*async_bitmap_resolver=*/base::DoNothing());
      }));

  EXPECT_EQ(*deserialized_holding_space_item, *holding_space_item);
}

// Tests deserialization of id for each holding space item type.
TEST_P(HoldingSpaceItemTest, DeserializeId) {
  const auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), base::FilePath("file_path"), GURL("file_system_url"),
      std::make_unique<HoldingSpaceImage>(
          /*placeholder=*/gfx::test::CreateImageSkia(10, 10),
          /*async_bitmap_resolver=*/base::DoNothing()));

  const base::DictionaryValue serialized_holding_space_item =
      holding_space_item->Serialize();

  const std::string& deserialized_holding_space_id =
      HoldingSpaceItem::DeserializeId(serialized_holding_space_item);

  EXPECT_EQ(deserialized_holding_space_id, holding_space_item->id());
}

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceItemTest,
                         testing::ValuesIn(GetHoldingSpaceItemTypes()));

}  // namespace ash
