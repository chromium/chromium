// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_item.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/scoped_locale.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/chromeos/styles/cros_styles.h"

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
      holding_space_util::GetMaxImageSizeForType(type), file_path,
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

// Tests setting the accessible name for each holding space item type.
TEST_P(HoldingSpaceItemTest, AccessibleName) {
  // Force locale since strings are being verified.
  base::ScopedLocale scoped_locale("en_US.UTF-8");

  // Create a `holding_space_item`.
  auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), base::FilePath("file_path"),
      GURL("filesystem::file_system_url"),
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));

  // Initially the accessible name should be based on the backing file.
  EXPECT_EQ(holding_space_item->GetAccessibleName(), u"file_path");

  // If primary text is set, that should affect accessible name.
  EXPECT_TRUE(holding_space_item->SetText(u"Primary text"));
  EXPECT_EQ(holding_space_item->GetAccessibleName(), u"Primary text");

  // If secondary text is set, that should affect accessible name.
  EXPECT_TRUE(holding_space_item->SetSecondaryText(u"Secondary text"));
  EXPECT_EQ(holding_space_item->GetAccessibleName(),
            u"Primary text, Secondary text");

  // It should be possible to override accessible name.
  EXPECT_TRUE(holding_space_item->SetAccessibleName(u"Accessible name"));
  EXPECT_EQ(holding_space_item->GetAccessibleName(), u"Accessible name");

  // It should no-op to try to override accessible name w/ existing values.
  EXPECT_FALSE(holding_space_item->SetAccessibleName(u"Accessible name"));
  EXPECT_EQ(holding_space_item->GetAccessibleName(), u"Accessible name");

  // It should be possible to remove the accessible name override.
  EXPECT_TRUE(holding_space_item->SetAccessibleName(absl::nullopt));
  EXPECT_EQ(holding_space_item->GetAccessibleName(),
            u"Primary text, Secondary text");
}

// Tests pause for each holding space item type.
TEST_P(HoldingSpaceItemTest, Pause) {
  // Create an in-progress `holding_space_item`.
  auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), base::FilePath("file_path"),
      GURL("filesystem::file_system_url"),
      HoldingSpaceProgress(/*current_bytes=*/50, /*total_bytes=*/100),
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));

  // Initially items are not paused.
  EXPECT_FALSE(holding_space_item->IsPaused());

  // It should be possible to update pause to a new value.
  EXPECT_TRUE(holding_space_item->SetPaused(true));
  EXPECT_TRUE(holding_space_item->IsPaused());

  // It should no-op to try to update pause to its existing value.
  EXPECT_FALSE(holding_space_item->SetPaused(true));
  EXPECT_TRUE(holding_space_item->IsPaused());

  // Once progress has been marked completed, items are no longer paused.
  EXPECT_TRUE(holding_space_item->SetProgress(
      HoldingSpaceProgress(/*current_bytes=*/100, /*total_bytes=*/100)));
  EXPECT_TRUE(holding_space_item->progress().IsComplete());
  EXPECT_FALSE(holding_space_item->IsPaused());

  // It should no-op to try to update pause for items which are not in-progress.
  EXPECT_FALSE(holding_space_item->SetPaused(true));
  EXPECT_FALSE(holding_space_item->IsPaused());
  EXPECT_FALSE(holding_space_item->SetPaused(false));
  EXPECT_FALSE(holding_space_item->IsPaused());
}

// Tests progress for each holding space item type.
TEST_P(HoldingSpaceItemTest, Progress) {
  // Create a `holding_space_item` w/ explicitly specified progress.
  auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), base::FilePath("file_path"),
      GURL("filesystem::file_system_url"),
      HoldingSpaceProgress(/*current_bytes=*/50, /*total_bytes=*/100),
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));

  // Since explicitly specified during construction, progress should be `0.5f`.
  EXPECT_EQ(holding_space_item->progress().GetValue(), 0.5f);

  // It should be possible to update progress to a new value.
  EXPECT_TRUE(holding_space_item->SetProgress(
      HoldingSpaceProgress(/*current_bytes=*/75, /*total_bytes=*/100)));
  EXPECT_EQ(holding_space_item->progress().GetValue(), 0.75f);

  // It should no-op to try to update progress to its existing value.
  EXPECT_FALSE(holding_space_item->SetProgress(
      HoldingSpaceProgress(/*current_bytes=*/75, /*total_bytes=*/100)));
  EXPECT_EQ(holding_space_item->progress().GetValue(), 0.75f);

  // It should be possible to set indeterminate progress.
  EXPECT_TRUE(holding_space_item->SetProgress(HoldingSpaceProgress(
      /*current_bytes=*/absl::nullopt, /*total_bytes=*/100)));
  EXPECT_TRUE(holding_space_item->progress().IsIndeterminate());

  // It should be possible to set progress complete.
  EXPECT_TRUE(holding_space_item->SetProgress(
      HoldingSpaceProgress(/*current_bytes=*/100, /*total_bytes=*/100)));
  EXPECT_TRUE(holding_space_item->progress().IsComplete());

  // Once progress has been marked completed, it should become read-only.
  EXPECT_FALSE(holding_space_item->SetProgress(
      HoldingSpaceProgress(/*current_bytes=*/75, /*total_bytes=*/100)));
  EXPECT_TRUE(holding_space_item->progress().IsComplete());

  // Create a `holding_space_item` w/ default progress.
  holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), base::FilePath("file_path"),
      GURL("filesystem::file_system_url"),
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));

  // Since not specified during construction, progress should be complete.
  EXPECT_TRUE(holding_space_item->progress().IsComplete());

  // Since progress is marked completed, it should be read-only.
  EXPECT_FALSE(holding_space_item->SetProgress(
      HoldingSpaceProgress(/*current_bytes=*/75, /*total_bytes=*/100)));
  EXPECT_TRUE(holding_space_item->progress().IsComplete());
}

// Tests setting the secondary text for each holding space item type.
TEST_P(HoldingSpaceItemTest, SecondaryText) {
  // Create a `holding_space_item`.
  auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), base::FilePath("file_path"),
      GURL("filesystem::file_system_url"),
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));

  // Initially the secondary text should be absent.
  EXPECT_FALSE(holding_space_item->secondary_text());

  // It should be possible to update secondary text to a new value.
  EXPECT_TRUE(holding_space_item->SetSecondaryText(u"secondary_text"));
  EXPECT_EQ(holding_space_item->secondary_text().value(), u"secondary_text");

  // It should no-op to try to update secondary text to its existing value.
  EXPECT_FALSE(holding_space_item->SetSecondaryText(u"secondary_text"));
  EXPECT_EQ(holding_space_item->secondary_text().value(), u"secondary_text");

  // It should be possible to unset secondary text.
  EXPECT_TRUE(holding_space_item->SetSecondaryText(absl::nullopt));
  EXPECT_FALSE(holding_space_item->secondary_text());
}

// Tests setting the secondary text color for each holding space item type.
TEST_P(HoldingSpaceItemTest, SecondaryTextColor) {
  // Create a `holding_space_item`.
  auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), base::FilePath("file_path"),
      GURL("filesystem::file_system_url"),
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));

  // Initially the secondary text color should be absent.
  EXPECT_FALSE(holding_space_item->secondary_text_color());

  // It should be possible to update secondary text color to a new value.
  EXPECT_TRUE(holding_space_item->SetSecondaryTextColor(
      cros_styles::ColorName::kTextColorAlert));
  EXPECT_EQ(holding_space_item->secondary_text_color().value(),
            cros_styles::ColorName::kTextColorAlert);

  // It should no-op to try to update secondary text color to existing values.
  EXPECT_FALSE(holding_space_item->SetSecondaryTextColor(
      cros_styles::ColorName::kTextColorAlert));
  EXPECT_EQ(holding_space_item->secondary_text_color().value(),
            cros_styles::ColorName::kTextColorAlert);

  // It should be possible to unset secondary text color.
  EXPECT_TRUE(holding_space_item->SetSecondaryTextColor(absl::nullopt));
  EXPECT_FALSE(holding_space_item->secondary_text_color());
}

// Tests setting the text for each holding space item type.
TEST_P(HoldingSpaceItemTest, Text) {
  // Create a `holding_space_item`.
  auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), base::FilePath("file_path"),
      GURL("filesystem::file_system_url"),
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));

  // Initially the text should reflect the backing file.
  EXPECT_EQ(holding_space_item->GetText(), u"file_path");

  // It should be possible to update text to a new value.
  EXPECT_TRUE(holding_space_item->SetText(u"text"));
  EXPECT_EQ(holding_space_item->GetText(), u"text");

  // It should no-op to try to update text to its existing value.
  EXPECT_FALSE(holding_space_item->SetText(u"text"));
  EXPECT_EQ(holding_space_item->GetText(), u"text");

  // It should be possible to unset text which will once again cause text to
  // reflect the backing file.
  EXPECT_TRUE(holding_space_item->SetText(absl::nullopt));
  EXPECT_EQ(holding_space_item->GetText(), u"file_path");
}

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceItemTest,
                         testing::ValuesIn(GetHoldingSpaceItemTypes()));

}  // namespace ash
