// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_model.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "ash/public/cpp/holding_space/holding_space_section.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "base/bind.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {
namespace {

using UpdatedField = HoldingSpaceModelObserver::UpdatedField;

// Helpers ---------------------------------------------------------------------

HoldingSpaceItem::InProgressCommand CreateInProgressCommand(
    HoldingSpaceCommandId command_id) {
  return HoldingSpaceItem::InProgressCommand(command_id, /*label_id=*/-1,
                                             &gfx::kNoneIcon,
                                             /*handler=*/base::DoNothing());
}

std::vector<HoldingSpaceItem::Type> GetHoldingSpaceItemTypes() {
  std::vector<HoldingSpaceItem::Type> types;
  for (int i = 0; i <= static_cast<int>(HoldingSpaceItem::Type::kMaxValue); ++i)
    types.emplace_back(static_cast<HoldingSpaceItem::Type>(i));
  return types;
}

std::unique_ptr<HoldingSpaceImage> CreateFakeHoldingSpaceImage(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) {
  return std::make_unique<HoldingSpaceImage>(
      holding_space_util::GetMaxImageSizeForType(type), file_path,
      /*async_bitmap_resolver=*/base::DoNothing());
}

std::unique_ptr<HoldingSpaceItem> CreateItem(HoldingSpaceItem::Type type) {
  return HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/type, base::FilePath("file_path"),
      GURL("filesystem::file_system_url"),
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));
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

  // Returns the id's of `HoldingSpaceItem`s for which
  // `OnHoldingSpaceItemRemoved()` was called. Also clears the cached values.
  std::vector<std::string> TakeRemovedItems() {
    std::vector<std::string> result;
    result.swap(removed_item_ids_);
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

  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override {
    for (const HoldingSpaceItem* item : items)
      removed_item_ids_.push_back(item->id());
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

  // A vector of item id's that have been removed.
  std::vector<std::string> removed_item_ids_;

  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observation_{this};
};

}  // namespace

// Print out the `HoldingSpaceItem::Type` in the test output.
std::ostream& operator<<(std::ostream& os, const HoldingSpaceItem::Type type) {
  return os << holding_space_util::ToString(type);
}

// HoldingSpaceModelTest -------------------------------------------------------

// Base class for `HoldingSpaceModel` tests, parameterized by the set of all
// holding space item types and whether the predictability feature is enabled.
class HoldingSpaceModelTest
    : public testing::TestWithParam<
          std::tuple<HoldingSpaceItem::Type, /*predictability_enabled=*/bool>> {
 public:
  HoldingSpaceModelTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kHoldingSpacePredictability,
        IsHoldingSpacePredictabilityEnabled());
  }

  // Returns the `HoldingSpaceModel` under test.
  HoldingSpaceModel& model() { return model_; }

  HoldingSpaceItem::Type GetHoldingSpaceItemType() const {
    return std::get<0>(GetParam());
  }

  bool IsHoldingSpacePredictabilityEnabled() const {
    return std::get<1>(GetParam());
  }

 private:
  HoldingSpaceModel model_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HoldingSpaceModelTest,
    testing::Combine(testing::ValuesIn(GetHoldingSpaceItemTypes()),
                     /*predictability_enabled=*/testing::Bool()));

// Tests -----------------------------------------------------------------------

// Verifies that updating fields which affect accessible name WAI.
TEST_P(HoldingSpaceModelTest, UpdateItem_AccessibleName) {
  ScopedModelObservation observation(&model());

  // Verify the `model()` is initially empty.
  EXPECT_EQ(model().items().size(), 0u);

  // Create a holding space `item`.
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetHoldingSpaceItemType(), base::FilePath("file_path"),
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
      /*type=*/GetHoldingSpaceItemType(), base::FilePath("file_path"),
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

  // Update in-progress commands.
  std::vector<HoldingSpaceItem::InProgressCommand> in_progress_commands;
  in_progress_commands.push_back(
      CreateInProgressCommand(HoldingSpaceCommandId::kCancelItem));
  model()
      .UpdateItem(item_ptr->id())
      ->SetInProgressCommands(in_progress_commands);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(),
            UpdatedField::kInProgressCommands);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->in_progress_commands(), in_progress_commands);

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
      ->SetSecondaryTextColorId(cros_tokens::kTextColorAlert);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(),
            UpdatedField::kSecondaryTextColor);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->secondary_text_color_id(), cros_tokens::kTextColorAlert);

  // Update all attributes.
  in_progress_commands.push_back(
      CreateInProgressCommand(HoldingSpaceCommandId::kPauseItem));
  updated_file_path = base::FilePath("again_updated_file_path");
  updated_file_system_url = GURL("filesystem::again_updated_file_system_url");
  model()
      .UpdateItem(item_ptr->id())
      ->SetAccessibleName(u"updated_accessible_name")
      .SetBackingFile(updated_file_path, updated_file_system_url)
      .SetInProgressCommands(in_progress_commands)
      .SetText(u"updated_text")
      .SetSecondaryText(u"updated_secondary_text")
      .SetSecondaryTextColorId(cros_tokens::kTextColorWarning)
      .SetProgress(
          HoldingSpaceProgress(/*current_bytes=*/75, /*total_bytes=*/100));
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(),
            UpdatedField::kAccessibleName | UpdatedField::kBackingFile |
                UpdatedField::kInProgressCommands | UpdatedField::kProgress |
                UpdatedField::kSecondaryText |
                UpdatedField::kSecondaryTextColor | UpdatedField::kText);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetAccessibleName(), u"updated_accessible_name");
  EXPECT_EQ(item_ptr->file_path(), updated_file_path);
  EXPECT_EQ(item_ptr->file_system_url(), updated_file_system_url);
  EXPECT_EQ(item_ptr->in_progress_commands(), in_progress_commands);
  EXPECT_EQ(item_ptr->progress().GetValue(), 0.75f);
  EXPECT_EQ(item_ptr->GetText(), u"updated_text");
  EXPECT_EQ(item_ptr->secondary_text(), u"updated_secondary_text");
  EXPECT_EQ(item_ptr->secondary_text_color_id(),
            cros_tokens::kTextColorWarning);
}

// Verifies that updating items will no-op appropriately.
TEST_P(HoldingSpaceModelTest, UpdateItem_Noop) {
  ScopedModelObservation observation(&model());

  // Verify the `model()` is initially empty.
  EXPECT_EQ(model().items().size(), 0u);

  // Create a holding space `item`.
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetHoldingSpaceItemType(), base::FilePath("file_path"),
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
      .SetInProgressCommands({})
      .SetText(absl::nullopt)
      .SetSecondaryText(absl::nullopt)
      .SetSecondaryTextColorId(absl::nullopt)
      .SetProgress(item_ptr->progress());
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 0);
}

// Verifies that updating item in-progress commands as intended.
TEST_P(HoldingSpaceModelTest, UpdateItem_InProgressCommands) {
  ScopedModelObservation observation(&model());

  // Verify the `model()` is initially empty.
  EXPECT_EQ(model().items().size(), 0u);

  // Create an in-progress holding space `item`.
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetHoldingSpaceItemType(), base::FilePath("file_path"),
      GURL("filesystem::file_system_url"),
      HoldingSpaceProgress(/*current_bytes=*/0, /*total_bytes=*/100),
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));
  auto* item_ptr = item.get();

  // Add `item` to the `model()`.
  model().AddItem(std::move(item));
  EXPECT_EQ(model().items().size(), 1u);
  EXPECT_EQ(model().items()[0].get(), item_ptr);

  // Verify the item has no in-progress commands.
  EXPECT_TRUE(item_ptr->in_progress_commands().empty());

  // Attempt to update in-progress commands to empty. This should no-op.
  model().UpdateItem(item_ptr->id())->SetInProgressCommands({});
  EXPECT_FALSE(observation.TakeLastUpdatedItem());
  EXPECT_EQ(observation.TakeLastUpdatedFields(), 0u);
  EXPECT_TRUE(item_ptr->in_progress_commands().empty());

  // Update in-progress commands.
  std::vector<HoldingSpaceItem::InProgressCommand> in_progress_commands;
  in_progress_commands.push_back(
      CreateInProgressCommand(HoldingSpaceCommandId::kCancelItem));
  model()
      .UpdateItem(item_ptr->id())
      ->SetInProgressCommands(in_progress_commands);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(),
            UpdatedField::kInProgressCommands);
  EXPECT_EQ(item_ptr->in_progress_commands(), in_progress_commands);

  // Update in-progress commands again.
  in_progress_commands.push_back(
      CreateInProgressCommand(HoldingSpaceCommandId::kPauseItem));
  model()
      .UpdateItem(item_ptr->id())
      ->SetInProgressCommands(in_progress_commands);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(),
            UpdatedField::kInProgressCommands);
  EXPECT_EQ(item_ptr->in_progress_commands(), in_progress_commands);

  // Update in-progress commands and progress to completion. Because the item is
  // no longer in progress, in-progress commands should be empty.
  in_progress_commands.push_back(
      CreateInProgressCommand(HoldingSpaceCommandId::kResumeItem));
  model()
      .UpdateItem(item_ptr->id())
      ->SetInProgressCommands(in_progress_commands)
      .SetProgress(
          HoldingSpaceProgress(/*current_bytes=*/100, /*total_bytes=*/100));
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(),
            UpdatedField::kInProgressCommands | UpdatedField::kProgress);
  EXPECT_TRUE(item_ptr->progress().IsComplete());
  EXPECT_TRUE(item_ptr->in_progress_commands().empty());

  // Attempts to update in-progress commands should no-op for completed items.
  model()
      .UpdateItem(item_ptr->id())
      ->SetInProgressCommands(in_progress_commands);
  EXPECT_FALSE(observation.TakeLastUpdatedItem());
  EXPECT_EQ(observation.TakeLastUpdatedFields(), 0u);
  EXPECT_TRUE(item_ptr->in_progress_commands().empty());
}

// Verifies that updating item progress works as intended.
TEST_P(HoldingSpaceModelTest, UpdateItem_Progress) {
  ScopedModelObservation observation(&model());

  // Verify the `model()` is initially empty.
  EXPECT_EQ(model().items().size(), 0u);

  // Create a holding space `item`.
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetHoldingSpaceItemType(), base::FilePath("file_path"),
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

TEST_P(HoldingSpaceModelTest, EnforcesMaxItemCountsPerSection) {
  ScopedModelObservation observation(&model());

  // Verify the `model()` is initially empty.
  EXPECT_EQ(model().items().size(), 0u);

  // Cache the section to which the parameterized type belongs.
  const HoldingSpaceSection* section =
      GetHoldingSpaceSection(GetHoldingSpaceItemType());
  ASSERT_TRUE(section);

  // Add the maximum count of items allowed for the section or some high number
  // if the section does not specify a maximum item count restriction.
  constexpr size_t kMaxItemCount = 50u;
  for (size_t i = 0u; i < section->max_item_count.value_or(kMaxItemCount); ++i)
    model().AddItem(CreateItem(GetHoldingSpaceItemType()));
  ASSERT_EQ(model().items().size(),
            section->max_item_count.value_or(kMaxItemCount));

  // Cache the IDs of items which may be expected to be removed later.
  constexpr size_t kExtraItemCount = 2u;
  ASSERT_GE(model().items().size(), kExtraItemCount);
  std::vector<std::string> item_ids;
  for (int i = kExtraItemCount - 1; i >= 0; --i)
    item_ids.push_back(model().items()[i]->id());

  // Add extra items of the same type to the model.
  std::vector<std::unique_ptr<HoldingSpaceItem>> items;
  for (size_t i = 0u; i < kExtraItemCount; ++i)
    items.push_back(CreateItem(GetHoldingSpaceItemType()));
  model().AddItems(std::move(items));

  // Cache a lambda to return whether the `model()` contains an item for `id`.
  auto model_contains_item_for_id = [&](const std::string& id) -> bool {
    return model().GetItem(id);
  };

  // If the feature flag is enabled and the section specifies a maximum item
  // count restriction, assert that the oldest items were removed. Otherwise,
  // nothing should have been removed from the `model()`.
  if (IsHoldingSpacePredictabilityEnabled() && section->max_item_count) {
    EXPECT_EQ(model().items().size(), section->max_item_count);
    EXPECT_TRUE(base::ranges::none_of(item_ids, model_contains_item_for_id));
    EXPECT_THAT(observation.TakeRemovedItems(),
                testing::ElementsAreArray(item_ids));
  } else {
    EXPECT_EQ(
        model().items().size(),
        section->max_item_count.value_or(kMaxItemCount) + kExtraItemCount);
    EXPECT_TRUE(base::ranges::all_of(item_ids, model_contains_item_for_id));
    EXPECT_TRUE(observation.TakeRemovedItems().empty());
  }
}

}  // namespace ash
