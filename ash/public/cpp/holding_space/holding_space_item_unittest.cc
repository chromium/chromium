// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_item.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "base/values.h"
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

  EXPECT_FALSE(deserialized_holding_space_item->IsInitialized());
  EXPECT_TRUE(deserialized_holding_space_item->file_system_url().is_empty());

  deserialized_holding_space_item->Initialize(file_system_url);
  EXPECT_TRUE(deserialized_holding_space_item->IsInitialized());
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

// Tests progress for each holding space item type.
TEST_P(HoldingSpaceItemTest, Progress) {
  // Create a `holding_space_item` w/ explicitly specified progress.
  auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), base::FilePath("file_path"),
      GURL("filesystem::file_system_url"), /*progress=*/0.5f,
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));

  // Since explicitly specified during construction, progress should be `0.5`.
  EXPECT_EQ(holding_space_item->progress(), 0.5f);

  // It should be possible to update progress to a new value.
  EXPECT_TRUE(holding_space_item->UpdateProgress(0.75f));
  EXPECT_EQ(holding_space_item->progress(), 0.75f);

  // It should no-op to try to update progress to its existing value.
  EXPECT_FALSE(holding_space_item->UpdateProgress(0.75f));
  EXPECT_EQ(holding_space_item->progress(), 0.75f);

  // It should be possible to set indeterminate progress.
  EXPECT_TRUE(holding_space_item->UpdateProgress(absl::nullopt));
  EXPECT_EQ(holding_space_item->progress(), absl::nullopt);

  // It should be possible to set progress complete.
  EXPECT_TRUE(holding_space_item->UpdateProgress(1.f));
  EXPECT_EQ(holding_space_item->progress(), 1.f);

  // Once progress has been marked completed, it should become read-only.
  EXPECT_FALSE(holding_space_item->UpdateProgress(0.75f));
  EXPECT_EQ(holding_space_item->progress(), 1.f);

  // Create a `holding_space_item` w/ default progress.
  holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), base::FilePath("file_path"),
      GURL("filesystem::file_system_url"),
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));

  // Since not specified during construction, progress should be `1.f`.
  EXPECT_EQ(holding_space_item->progress(), 1.f);

  // Since progress is marked completed, it should be read-only.
  EXPECT_FALSE(holding_space_item->UpdateProgress(0.75f));
  EXPECT_EQ(holding_space_item->progress(), 1.f);
}

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceItemTest,
                         testing::ValuesIn(GetHoldingSpaceItemTypes()));

}  // namespace ash
