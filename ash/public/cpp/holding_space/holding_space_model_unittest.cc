// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_model.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "base/bind.h"
#include "base/scoped_observation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/chromeos/styles/cros_styles.h"

namespace ash {
namespace {

using UpdatedField = HoldingSpaceModelObserver::UpdatedField;

// Helpers ---------------------------------------------------------------------

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

// ScopedModelObservation ------------------------------------------------------

// A class which observes a `HoldingSpaceModel` within its lifetime. Note that
// this class must not outlive the `model` that it observes.
class ScopedModelObservation : public HoldingSpaceModelObserver {
 public:
  explicit ScopedModelObservation(HoldingSpaceModel* model) {
    observation_.Observe(model);
  }

  ScopedModelObservation(const ScopedModelObservation&) = delete;
  ScopedModelObservation& operator=(const ScopedModelObservation&) = delete;
  ~ScopedModelObservation() override = default;

  // Returns the last updated fields for which `OnHoldingSpaceItemUpdated()`
  // was called, clearing the cached value.
  uint32_t TakeLastUpdatedFields() {
    const uint32_t updated_fields = last_updated_fields_;
    last_updated_fields_ = 0u;
    return updated_fields;
  }

  // Returns the last `HoldingSpaceItem` for which `OnHoldingSpaceItemUpdated()`
  // was called, clearing the cached value.
  const HoldingSpaceItem* TakeLastUpdatedItem() {
    const HoldingSpaceItem* result = last_updated_item_;
    last_updated_item_ = nullptr;
    return result;
  }

  // Returns the count of times for which `OnHoldingSpaceItemUpdated()` was
  // called, clearing the cached value.
  int TakeUpdatedItemCount() {
    const int result = updated_item_count_;
    updated_item_count_ = 0;
    return result;
  }

 private:
  // HoldingSpaceModel::Observer:
  void OnHoldingSpaceItemUpdated(const HoldingSpaceItem* item,
                                 uint32_t updated_fields) override {
    last_updated_item_ = item;
    last_updated_fields_ = updated_fields;
    ++updated_item_count_;
  }

  // The last `HoldingSpaceItem` for which `OnHoldingSpaceItemUpdated()` was
  // called. May be `nullptr` prior to an update event or following a call to
  // `TakeLastUpdatedItem()`.
  const HoldingSpaceItem* last_updated_item_ = nullptr;

  // The last updated fields for which `OnHoldingSpaceItemUpdated()` was called.
  // May be zero prior to an update event or following a call to
  // `TakeLastUpdatedFields()`.
  uint32_t last_updated_fields_ = 0u;

  // The count of times for which `OnHoldingSpaceItemUpdated()` was called. May
  // be reset following a call to `TakeUpdatedItemCount()`.
  int updated_item_count_ = 0;

  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observation_{this};
};

}  // namespace

// HoldingSpaceModelTest -------------------------------------------------------

// Base class for `HoldingSpaceModel` tests, parameterized by the set of all
// holding space item types.
class HoldingSpaceModelTest
    : public testing::TestWithParam<HoldingSpaceItem::Type> {
 public:
  // Returns the `HoldingSpaceModel` under test.
  HoldingSpaceModel& model() { return model_; }

 private:
  HoldingSpaceModel model_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceModelTest,
                         testing::ValuesIn(GetHoldingSpaceItemTypes()));

// Tests -----------------------------------------------------------------------

// Verifies that updating fields which affect accessible name WAI.
TEST_P(HoldingSpaceModelTest, UpdateItem_AccessibleName) {
  ScopedModelObservation observation(&model());

  // Verify the `model()` is initially empty.
  EXPECT_EQ(model().items().size(), 0u);

  // Create a holding space `item`.
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), base::FilePath("file_path"),
      GURL("filesystem::file_system_url"),
      HoldingSpaceProgress(/*current_bytes=*/0, /*total_bytes=*/100),
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));
  auto* item_ptr = item.get();

  // Add `item` to the `model()`.
  model().AddItem(std::move(item));
  EXPECT_EQ(model().items().size(), 1u);
  EXPECT_EQ(model().items()[0].get(), item_ptr);

  // Initially accessible name is the lossy display name of the backing file.
  EXPECT_EQ(item_ptr->GetAccessibleName(), u"file_path");

  // Update text. Because accessible name is not overridden, this should result
  // in an update to the computed accessible name field.
  model().UpdateItem(item_ptr->id())->SetText(u"text");
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_TRUE(observation.TakeLastUpdatedFields() &
              UpdatedField::kAccessibleName);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetAccessibleName(), u"text");

  // Update secondary text. Because accessible name is not overridden, this
  // should result in an update to the computed accessible name field.
  model().UpdateItem(item_ptr->id())->SetSecondaryText(u"secondary_text");
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_TRUE(observation.TakeLastUpdatedFields() &
              UpdatedField::kAccessibleName);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetAccessibleName(), u"text, secondary_text");

  // Update accessible name. Note that accessible name field is now overridden
  // from its previously computed value.
  model().UpdateItem(item_ptr->id())->SetAccessibleName(u"accessible_name");
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), UpdatedField::kAccessibleName);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetAccessibleName(), u"accessible_name");

  // Update text. Because accessible name is overridden, this should *not*
  // result in an update to the computed accessible name field.
  model().UpdateItem(item_ptr->id())->SetText(u"updated_text");
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), UpdatedField::kText);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetAccessibleName(), u"accessible_name");

  // Update secondary text. Because accessible name is overridden, this should
  // *not* result in an update to the computed accessible name field.
  model()
      .UpdateItem(item_ptr->id())
      ->SetSecondaryText(u"updated_secondary_text");
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), UpdatedField::kSecondaryText);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetAccessibleName(), u"accessible_name");

  // Update accessible name. Note that accessible name field is no longer being
  // overridden from its computed value.
  model().UpdateItem(item_ptr->id())->SetAccessibleName(absl::nullopt);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), UpdatedField::kAccessibleName);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetAccessibleName(),
            u"updated_text, updated_secondary_text");
}

// Verifies that updating multiple item attributes is atomic.
TEST_P(HoldingSpaceModelTest, UpdateItem_Atomic) {
  ScopedModelObservation observation(&model());

  // Verify the `model()` is initially empty.
  EXPECT_EQ(model().items().size(), 0u);

  // Create a holding space `item`.
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), base::FilePath("file_path"),
      GURL("filesystem::file_system_url"),
      HoldingSpaceProgress(/*current_bytes=*/0, /*total_bytes=*/100),
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));
  auto* item_ptr = item.get();

  // Add `item` to the `model()`.
  model().AddItem(std::move(item));
  EXPECT_EQ(model().items().size(), 1u);
  EXPECT_EQ(model().items()[0].get(), item_ptr);

  // Update accessible name.
  model().UpdateItem(item_ptr->id())->SetAccessibleName(u"accessible_name");
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), UpdatedField::kAccessibleName);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetAccessibleName(), u"accessible_name");

  // Update backing file.
  base::FilePath updated_file_path("updated_file_path");
  GURL updated_file_system_url("filesystem::updated_file_system_url");
  model()
      .UpdateItem(item_ptr->id())
      ->SetBackingFile(updated_file_path, updated_file_system_url);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), UpdatedField::kBackingFile);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->file_path(), updated_file_path);
  EXPECT_EQ(item_ptr->file_system_url(), updated_file_system_url);

  // Update paused state.
  model().UpdateItem(item_ptr->id())->SetPaused(true);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), UpdatedField::kPaused);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_TRUE(item_ptr->IsPaused());

  // Update progress.
  model()
      .UpdateItem(item_ptr->id())
      ->SetProgress(
          HoldingSpaceProgress(/*current_bytes=*/50, /*total_bytes=*/100));
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), UpdatedField::kProgress);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->progress().GetValue(), 0.5f);

  // Update text.
  model().UpdateItem(item_ptr->id())->SetText(u"text");
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), UpdatedField::kText);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetText(), u"text");

  // Update secondary text.
  model().UpdateItem(item_ptr->id())->SetSecondaryText(u"secondary_text");
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), UpdatedField::kSecondaryText);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->secondary_text(), u"secondary_text");

  // Update secondary text color.
  model()
      .UpdateItem(item_ptr->id())
      ->SetSecondaryTextColor(cros_styles::ColorName::kTextColorAlert);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(),
            UpdatedField::kSecondaryTextColor);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->secondary_text_color(),
            cros_styles::ColorName::kTextColorAlert);

  // Update all attributes.
  updated_file_path = base::FilePath("again_updated_file_path");
  updated_file_system_url = GURL("filesystem::again_updated_file_system_url");
  model()
      .UpdateItem(item_ptr->id())
      ->SetAccessibleName(u"updated_accessible_name")
      .SetBackingFile(updated_file_path, updated_file_system_url)
      .SetText(u"updated_text")
      .SetSecondaryText(u"updated_secondary_text")
      .SetSecondaryTextColor(cros_styles::ColorName::kTextColorWarning)
      .SetPaused(false)
      .SetProgress(
          HoldingSpaceProgress(/*current_bytes=*/75, /*total_bytes=*/100));
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(),
            UpdatedField::kAccessibleName | UpdatedField::kBackingFile |
                UpdatedField::kPaused | UpdatedField::kProgress |
                UpdatedField::kSecondaryText |
                UpdatedField::kSecondaryTextColor | UpdatedField::kText);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetAccessibleName(), u"updated_accessible_name");
  EXPECT_EQ(item_ptr->file_path(), updated_file_path);
  EXPECT_EQ(item_ptr->file_system_url(), updated_file_system_url);
  EXPECT_FALSE(item_ptr->IsPaused());
  EXPECT_EQ(item_ptr->progress().GetValue(), 0.75f);
  EXPECT_EQ(item_ptr->GetText(), u"updated_text");
  EXPECT_EQ(item_ptr->secondary_text(), u"updated_secondary_text");
  EXPECT_EQ(item_ptr->secondary_text_color(),
            cros_styles::ColorName::kTextColorWarning);
}

// Verifies that updating items will no-op appropriately.
TEST_P(HoldingSpaceModelTest, UpdateItem_Noop) {
  ScopedModelObservation observation(&model());

  // Verify the `model()` is initially empty.
  EXPECT_EQ(model().items().size(), 0u);

  // Create a holding space `item`.
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), base::FilePath("file_path"),
      GURL("filesystem::file_system_url"), HoldingSpaceProgress(),
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));
  auto* item_ptr = item.get();

  // Add `item` to the `model()`.
  model().AddItem(std::move(item));
  EXPECT_EQ(model().items().size(), 1u);
  EXPECT_EQ(model().items()[0].get(), item_ptr);

  // Perform a no-op update. No observers should be notified.
  model().UpdateItem(item_ptr->id());
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 0);

  // Perform another no-op update. No observers should be notified.
  model()
      .UpdateItem(item_ptr->id())
      ->SetAccessibleName(absl::nullopt)
      .SetBackingFile(item_ptr->file_path(), item_ptr->file_system_url())
      .SetText(absl::nullopt)
      .SetSecondaryText(absl::nullopt)
      .SetSecondaryTextColor(absl::nullopt)
      .SetPaused(item_ptr->IsPaused())
      .SetProgress(item_ptr->progress());
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 0);
}

// Verifies that updating item paused state works as intended.
TEST_P(HoldingSpaceModelTest, UpdateItem_Pause) {
  ScopedModelObservation observation(&model());

  // Verify the `model()` is initially empty.
  EXPECT_EQ(model().items().size(), 0u);

  // Create a holding space `item`.
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), base::FilePath("file_path"),
      GURL("filesystem::file_system_url"),
      HoldingSpaceProgress(/*current_bytes=*/0, /*total_bytes=*/100),
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));
  auto* item_ptr = item.get();

  // Add `item` to the `model()`.
  model().AddItem(std::move(item));
  EXPECT_EQ(model().items().size(), 1u);
  EXPECT_EQ(model().items()[0].get(), item_ptr);

  // Verify the item is not paused.
  EXPECT_FALSE(item_ptr->IsPaused());

  // Attempt to update pause to `false`. This should no-op.
  model().UpdateItem(item_ptr->id())->SetPaused(false);
  EXPECT_FALSE(observation.TakeLastUpdatedItem());
  EXPECT_EQ(observation.TakeLastUpdatedFields(), 0u);
  EXPECT_FALSE(item_ptr->IsPaused());

  // Update pause to `true`.
  model().UpdateItem(item_ptr->id())->SetPaused(true);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), UpdatedField::kPaused);
  EXPECT_TRUE(item_ptr->IsPaused());

  // Update pause to `false`.
  model().UpdateItem(item_ptr->id())->SetPaused(false);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), UpdatedField::kPaused);
  EXPECT_FALSE(item_ptr->IsPaused());

  // Update pause to `true` and progress to completion. Because the item is no
  // longer in progress, it should no longer be paused.
  model()
      .UpdateItem(item_ptr->id())
      ->SetPaused(true)
      .SetProgress(
          HoldingSpaceProgress(/*current_bytes=*/100, /*total_bytes=*/100));
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(),
            UpdatedField::kPaused | UpdatedField::kProgress);
  EXPECT_TRUE(item_ptr->progress().IsComplete());
  EXPECT_FALSE(item_ptr->IsPaused());

  // Attempts to update pause should no-op for completed items.
  model().UpdateItem(item_ptr->id())->SetPaused(true);
  EXPECT_FALSE(observation.TakeLastUpdatedItem());
  EXPECT_EQ(observation.TakeLastUpdatedFields(), 0u);
  EXPECT_FALSE(item_ptr->IsPaused());
  model().UpdateItem(item_ptr->id())->SetPaused(false);
  EXPECT_FALSE(observation.TakeLastUpdatedItem());
  EXPECT_EQ(observation.TakeLastUpdatedFields(), 0u);
  EXPECT_FALSE(item_ptr->IsPaused());
}

// Verifies that updating item progress works as intended.
TEST_P(HoldingSpaceModelTest, UpdateItem_Progress) {
  ScopedModelObservation observation(&model());

  // Verify the `model()` is initially empty.
  EXPECT_EQ(model().items().size(), 0u);

  // Create a holding space `item`.
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), base::FilePath("file_path"),
      GURL("filesystem::file_system_url"),
      HoldingSpaceProgress(/*current_bytes=*/absl::nullopt,
                           /*total_bytes=*/100),
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));
  auto* item_ptr = item.get();

  // Add `item` to the `model()`.
  model().AddItem(std::move(item));
  EXPECT_EQ(model().items().size(), 1u);
  EXPECT_EQ(model().items()[0].get(), item_ptr);

  // Verify progress is indeterminate.
  EXPECT_TRUE(item_ptr->progress().IsIndeterminate());

  // Update progress to `0.5f`.
  model()
      .UpdateItem(item_ptr->id())
      ->SetProgress(
          HoldingSpaceProgress(/*current_bytes=*/50, /*total_bytes=*/100));
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), UpdatedField::kProgress);
  EXPECT_EQ(item_ptr->progress().GetValue(), 0.5f);

  // Update progress to `0.5f` again. This should no-op.
  model()
      .UpdateItem(item_ptr->id())
      ->SetProgress(
          HoldingSpaceProgress(/*current_bytes=*/50, /*total_bytes=*/100));
  EXPECT_FALSE(observation.TakeLastUpdatedItem());
  EXPECT_EQ(observation.TakeLastUpdatedFields(), 0u);
  EXPECT_EQ(item_ptr->progress().GetValue(), 0.5f);

  // Update progress to indeterminate.
  model()
      .UpdateItem(item_ptr->id())
      ->SetProgress(HoldingSpaceProgress(/*current_bytes=*/absl::nullopt,
                                         /*total_bytes=*/100));
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), UpdatedField::kProgress);
  EXPECT_TRUE(item_ptr->progress().IsIndeterminate());

  // Update progress to complete.
  model()
      .UpdateItem(item_ptr->id())
      ->SetProgress(
          HoldingSpaceProgress(/*current_bytes=*/100, /*total_bytes=*/100));
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), UpdatedField::kProgress);
  EXPECT_TRUE(item_ptr->progress().IsComplete());

  // Update progress to `0.5f`. This should no-op as progress becomes read-only
  // after being marked completed.
  model()
      .UpdateItem(item_ptr->id())
      ->SetProgress(
          HoldingSpaceProgress(/*current_bytes=*/50, /*total_bytes=*/100));
  EXPECT_FALSE(observation.TakeLastUpdatedItem());
  EXPECT_EQ(observation.TakeLastUpdatedFields(), 0u);
  EXPECT_TRUE(item_ptr->progress().IsComplete());
}

}  // namespace ash
