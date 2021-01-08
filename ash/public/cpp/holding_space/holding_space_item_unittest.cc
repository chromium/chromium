// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_item.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

std::vector<HoldingSpaceItem::Type> GetHoldingSpaceItemTypes() {
  std::vector<HoldingSpaceItem::Type> types;
  for (int i = 0; i <= static_cast<int>(HoldingSpaceItem::Type::kMaxValue); ++i)
    types.push_back(static_cast<HoldingSpaceItem::Type>(i));
  return types;
}

std::unique_ptr<HoldingSpaceImage> CreateFakeHoldingSpaceImage(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) {
  return std::make_unique<HoldingSpaceImage>(
      HoldingSpaceImage::GetMaxSizeForType(type), file_path,
      /*async_bitmap_resolver=*/base::DoNothing());
}

}  // namespace

using HoldingSpaceItemTest = testing::TestWithParam<HoldingSpaceItem::Type>;

// Tests round-trip serialization for each holding space item type.
TEST_P(HoldingSpaceItemTest, Serialization) {
  const base::FilePath file_path("file_path");
  const GURL file_system_url("filesystem:file_system_url");

  const auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), file_path, file_system_url,
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));

  const base::DictionaryValue serialized_holding_space_item =
      holding_space_item->Serialize();

  const auto deserialized_holding_space_item = HoldingSpaceItem::Deserialize(
      serialized_holding_space_item,
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));

  EXPECT_FALSE(deserialized_holding_space_item->IsFinalized());
  EXPECT_TRUE(deserialized_holding_space_item->file_system_url().is_empty());

  deserialized_holding_space_item->Finalize(file_system_url);
  EXPECT_TRUE(deserialized_holding_space_item->IsFinalized());
  EXPECT_EQ(*deserialized_holding_space_item, *holding_space_item);
}

// Tests deserialization of id for each holding space item type.
TEST_P(HoldingSpaceItemTest, DeserializeId) {
  const auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), base::FilePath("file_path"),
      GURL("filesystem:file_system_url"),
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));

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
