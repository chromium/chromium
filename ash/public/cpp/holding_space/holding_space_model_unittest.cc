// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_model.h"

#include <memory>
#include <optional>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_item_updated_fields.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "ash/public/cpp/holding_space/holding_space_section.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {
namespace {

// Aliases ---------------------------------------------------------------------

using testing::VariantWith;

// Helpers ---------------------------------------------------------------------

HoldingSpaceItem::InProgressCommand CreateInProgressCommand(
    HoldingSpaceCommandId command_id) {
  return HoldingSpaceItem::InProgressCommand(command_id, /*label_id=*/-1,
                                             &gfx::kNoneIcon,
                                             /*handler=*/base::DoNothing());
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
  HoldingSpaceItemUpdatedFields TakeLastUpdatedFields() {
    HoldingSpaceItemUpdatedFields result = last_updated_fields_;
    last_updated_fields_ = HoldingSpaceItemUpdatedFields();
    return result;
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
  void OnHoldingSpaceItemUpdated(
      const HoldingSpaceItem* item,
      const HoldingSpaceItemUpdatedFields& updated_fields) override {
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
  raw_ptr<const HoldingSpaceItem> last_updated_item_ = nullptr;

  // The last updated fields for which `OnHoldingSpaceItemUpdated()` was called.
  // May be empty prior to an update event or following a call to
  // `TakeLastUpdatedFields()`.
  HoldingSpaceItemUpdatedFields last_updated_fields_;

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
// holding space item types.
class HoldingSpaceModelTest
    : public testing::TestWithParam<HoldingSpaceItem::Type> {
 public:
  // Returns the `HoldingSpaceModel` under test.
  HoldingSpaceModel& model() { return model_; }

  HoldingSpaceItem::Type GetHoldingSpaceItemType() const { return GetParam(); }

 private:
  HoldingSpaceModel model_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HoldingSpaceModelTest,
    testing::ValuesIn(holding_space_util::GetAllItemTypes()));

// Tests -----------------------------------------------------------------------

// Verifies that updating fields which affect accessible name WAI.
TEST_P(HoldingSpaceModelTest, UpdateItem_AccessibleName) {
  ScopedModelObservation observation(&model());

  // Verify the `model()` is initially empty.
  EXPECT_EQ(model().items().size(), 0u);

  // Create a holding space `item`.
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetHoldingSpaceItemType(),
      HoldingSpaceFile(base::FilePath("file_path"),
                       HoldingSpaceFile::FileSystemType::kTest,
                       GURL("filesystem::file_system_url")),
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
  HoldingSpaceItemUpdatedFields expected_update;
  expected_update.previous_accessible_name = item_ptr->GetAccessibleName();
  expected_update.previous_text = item_ptr->GetText();
  std::u16string text(u"text");
  model().UpdateItem(item_ptr->id())->SetText(text);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetAccessibleName(), text);

  // Update secondary text. Because accessible name is not overridden, this
  // should result in an update to the computed accessible name field.
  expected_update = HoldingSpaceItemUpdatedFields();
  expected_update.previous_accessible_name = item_ptr->GetAccessibleName();
  expected_update.previous_secondary_text = item_ptr->secondary_text();
  std::u16string secondary_text(u"secondary_text");
  model().UpdateItem(item_ptr->id())->SetSecondaryText(secondary_text);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetAccessibleName(),
            base::StrCat({text, u", ", secondary_text}));

  // Update accessible name. Note that accessible name field is now overridden
  // from its previously computed value.
  expected_update = HoldingSpaceItemUpdatedFields();
  expected_update.previous_accessible_name = item_ptr->GetAccessibleName();
  std::optional<std::u16string> accessible_name(u"accessible_name");
  model().UpdateItem(item_ptr->id())->SetAccessibleName(accessible_name);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetAccessibleName(), accessible_name);

  // Update text. Because accessible name is overridden, this should *not*
  // result in an update to the computed accessible name field.
  expected_update = HoldingSpaceItemUpdatedFields();
  expected_update.previous_text = item_ptr->GetText();
  text = u"updated_text";
  model().UpdateItem(item_ptr->id())->SetText(text);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetAccessibleName(), accessible_name);

  // Update secondary text. Because accessible name is overridden, this should
  // *not* result in an update to the computed accessible name field.
  expected_update = HoldingSpaceItemUpdatedFields();
  expected_update.previous_secondary_text = item_ptr->secondary_text();
  secondary_text = u"updated_secondary_text";
  model().UpdateItem(item_ptr->id())->SetSecondaryText(secondary_text);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetAccessibleName(), accessible_name);

  // Update accessible name. Note that accessible name field is no longer being
  // overridden from its computed value.
  expected_update = HoldingSpaceItemUpdatedFields();
  expected_update.previous_accessible_name = item_ptr->GetAccessibleName();
  accessible_name = std::nullopt;
  model().UpdateItem(item_ptr->id())->SetAccessibleName(accessible_name);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetAccessibleName(),
            base::StrCat({text, u", ", secondary_text}));
}

// Verifies that updating multiple item attributes is atomic.
TEST_P(HoldingSpaceModelTest, UpdateItem_Atomic) {
  ScopedModelObservation observation(&model());

  // Verify the `model()` is initially empty.
  EXPECT_EQ(model().items().size(), 0u);

  // Create a holding space `item`.
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetHoldingSpaceItemType(),
      HoldingSpaceFile(base::FilePath("file_path"),
                       HoldingSpaceFile::FileSystemType::kTest,
                       GURL("filesystem::file_system_url")),
      HoldingSpaceProgress(/*current_bytes=*/0, /*total_bytes=*/100),
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));
  auto* item_ptr = item.get();

  // Add `item` to the `model()`.
  model().AddItem(std::move(item));
  EXPECT_EQ(model().items().size(), 1u);
  EXPECT_EQ(model().items()[0].get(), item_ptr);

  // Update accessible name.
  HoldingSpaceItemUpdatedFields expected_update;
  expected_update.previous_accessible_name = item_ptr->GetAccessibleName();
  std::u16string accessible_name(u"accessible_name");
  model().UpdateItem(item_ptr->id())->SetAccessibleName(accessible_name);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetAccessibleName(), accessible_name);

  // Update backing file.
  expected_update = HoldingSpaceItemUpdatedFields();
  expected_update.previous_backing_file = item_ptr->file();
  expected_update.previous_text = item_ptr->GetText();
  HoldingSpaceFile backing_file(base::FilePath("updated_file_path"),
                                HoldingSpaceFile::FileSystemType::kTest,
                                GURL("filesystem::updated_file_system_url"));
  model().UpdateItem(item_ptr->id())->SetBackingFile(backing_file);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->file(), backing_file);

  // Update in-progress commands.
  expected_update = HoldingSpaceItemUpdatedFields();
  expected_update.previous_in_progress_commands =
      item_ptr->in_progress_commands();
  std::vector<HoldingSpaceItem::InProgressCommand> in_progress_commands;
  in_progress_commands.push_back(
      CreateInProgressCommand(HoldingSpaceCommandId::kCancelItem));
  model()
      .UpdateItem(item_ptr->id())
      ->SetInProgressCommands(in_progress_commands);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->in_progress_commands(), in_progress_commands);

  // Update progress.
  expected_update = HoldingSpaceItemUpdatedFields();
  expected_update.previous_progress = item_ptr->progress();
  HoldingSpaceProgress progress(/*current_bytes=*/50, /*total_bytes=*/100);
  model().UpdateItem(item_ptr->id())->SetProgress(progress);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->progress(), progress);

  // Update text.
  expected_update = HoldingSpaceItemUpdatedFields();
  expected_update.previous_text = item_ptr->GetText();
  std::u16string text(u"text");
  model().UpdateItem(item_ptr->id())->SetText(text);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetText(), text);

  // Update secondary text.
  expected_update = HoldingSpaceItemUpdatedFields();
  expected_update.previous_secondary_text = item_ptr->secondary_text();
  std::u16string secondary_text(u"secondary_text");
  model().UpdateItem(item_ptr->id())->SetSecondaryText(secondary_text);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->secondary_text(), secondary_text);

  // Update secondary text color.
  expected_update = HoldingSpaceItemUpdatedFields();
  expected_update.previous_secondary_text_color_variant =
      item_ptr->secondary_text_color_variant();
  ui::ColorId secondary_text_color_id(cros_tokens::kTextColorAlert);
  model()
      .UpdateItem(item_ptr->id())
      ->SetSecondaryTextColorVariant(secondary_text_color_id);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_THAT(*item_ptr->secondary_text_color_variant(),
              VariantWith<ui::ColorId>(secondary_text_color_id));

  // Update all attributes.
  expected_update = HoldingSpaceItemUpdatedFields();
  expected_update.previous_accessible_name = item_ptr->GetAccessibleName();
  expected_update.previous_backing_file = item_ptr->file();
  expected_update.previous_in_progress_commands =
      item_ptr->in_progress_commands();
  expected_update.previous_progress = item_ptr->progress();
  expected_update.previous_secondary_text = item_ptr->secondary_text();
  expected_update.previous_secondary_text_color_variant =
      item_ptr->secondary_text_color_variant();
  expected_update.previous_text = item_ptr->GetText();
  accessible_name = u"updated_accessible_name";
  backing_file = HoldingSpaceFile(base::FilePath("updated_file_path"),
                                  HoldingSpaceFile::FileSystemType::kLocal,
                                  GURL("file_system::updated_file_system_url"));
  in_progress_commands.push_back(
      CreateInProgressCommand(HoldingSpaceCommandId::kPauseItem));
  progress = HoldingSpaceProgress(/*current_bytes=*/75, /*total_bytes=*/100);
  secondary_text = u"updated_secondary_text";
  secondary_text_color_id = cros_tokens::kTextColorWarning;
  text = u"updated_text";
  model()
      .UpdateItem(item_ptr->id())
      ->SetAccessibleName(accessible_name)
      .SetBackingFile(backing_file)
      .SetInProgressCommands(in_progress_commands)
      .SetText(text)
      .SetSecondaryText(secondary_text)
      .SetSecondaryTextColorVariant(secondary_text_color_id)
      .SetProgress(progress);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(observation.TakeUpdatedItemCount(), 1);
  EXPECT_EQ(item_ptr->GetAccessibleName(), accessible_name);
  EXPECT_EQ(item_ptr->file(), backing_file);
  EXPECT_EQ(item_ptr->in_progress_commands(), in_progress_commands);
  EXPECT_EQ(item_ptr->progress(), progress);
  EXPECT_EQ(item_ptr->GetText(), text);
  EXPECT_EQ(item_ptr->secondary_text(), secondary_text);
  EXPECT_THAT(*item_ptr->secondary_text_color_variant(),
              VariantWith<ui::ColorId>(secondary_text_color_id));
}

// Verifies that updating items will no-op appropriately.
TEST_P(HoldingSpaceModelTest, UpdateItem_Noop) {
  ScopedModelObservation observation(&model());

  // Verify the `model()` is initially empty.
  EXPECT_EQ(model().items().size(), 0u);

  // Create a holding space `item`.
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetHoldingSpaceItemType(),
      HoldingSpaceFile(base::FilePath("file_path"),
                       HoldingSpaceFile::FileSystemType::kTest,
                       GURL("filesystem::file_system_url")),
      HoldingSpaceProgress(),
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
      ->SetAccessibleName(std::nullopt)
      .SetBackingFile(item_ptr->file())
      .SetInProgressCommands({})
      .SetText(std::nullopt)
      .SetSecondaryText(std::nullopt)
      .SetSecondaryTextColorVariant(std::nullopt)
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
      /*type=*/GetHoldingSpaceItemType(),
      HoldingSpaceFile(base::FilePath("file_path"),
                       HoldingSpaceFile::FileSystemType::kTest,
                       GURL("filesystem::file_system_url")),
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
  EXPECT_TRUE(observation.TakeLastUpdatedFields().IsEmpty());
  EXPECT_TRUE(item_ptr->in_progress_commands().empty());

  // Update in-progress commands.
  HoldingSpaceItemUpdatedFields expected_update;
  expected_update.previous_in_progress_commands =
      item_ptr->in_progress_commands();
  std::vector<HoldingSpaceItem::InProgressCommand> in_progress_commands;
  in_progress_commands.push_back(
      CreateInProgressCommand(HoldingSpaceCommandId::kCancelItem));
  model()
      .UpdateItem(item_ptr->id())
      ->SetInProgressCommands(in_progress_commands);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(item_ptr->in_progress_commands(), in_progress_commands);

  // Update in-progress commands again.
  expected_update = HoldingSpaceItemUpdatedFields();
  expected_update.previous_in_progress_commands =
      item_ptr->in_progress_commands();
  in_progress_commands.push_back(
      CreateInProgressCommand(HoldingSpaceCommandId::kPauseItem));
  model()
      .UpdateItem(item_ptr->id())
      ->SetInProgressCommands(in_progress_commands);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(item_ptr->in_progress_commands(), in_progress_commands);

  // Update in-progress commands and progress to completion. Because the item is
  // no longer in progress, in-progress commands should be empty.
  expected_update = HoldingSpaceItemUpdatedFields();
  expected_update.previous_in_progress_commands =
      item_ptr->in_progress_commands();
  expected_update.previous_progress = item_ptr->progress();
  in_progress_commands.push_back(
      CreateInProgressCommand(HoldingSpaceCommandId::kResumeItem));
  HoldingSpaceProgress progress(/*current_bytes=*/100, /*total_bytes=*/100);
  model()
      .UpdateItem(item_ptr->id())
      ->SetInProgressCommands(in_progress_commands)
      .SetProgress(progress);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(item_ptr->progress(), progress);
  EXPECT_TRUE(item_ptr->in_progress_commands().empty());

  // Attempts to update in-progress commands should no-op for completed items.
  model()
      .UpdateItem(item_ptr->id())
      ->SetInProgressCommands(in_progress_commands);
  EXPECT_FALSE(observation.TakeLastUpdatedItem());
  EXPECT_TRUE(observation.TakeLastUpdatedFields().IsEmpty());
  EXPECT_TRUE(item_ptr->in_progress_commands().empty());
}

// Verifies that updating item progress works as intended.
TEST_P(HoldingSpaceModelTest, UpdateItem_Progress) {
  ScopedModelObservation observation(&model());

  // Verify the `model()` is initially empty.
  EXPECT_EQ(model().items().size(), 0u);

  // Create a holding space `item`.
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetHoldingSpaceItemType(),
      HoldingSpaceFile(base::FilePath("file_path"),
                       HoldingSpaceFile::FileSystemType::kTest,
                       GURL("filesystem::file_system_url")),
      HoldingSpaceProgress(/*current_bytes=*/std::nullopt,
                           /*total_bytes=*/100),
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));
  auto* item_ptr = item.get();

  // Add `item` to the `model()`.
  model().AddItem(std::move(item));
  EXPECT_EQ(model().items().size(), 1u);
  EXPECT_EQ(model().items()[0].get(), item_ptr);

  // Verify progress is indeterminate.
  EXPECT_TRUE(item_ptr->progress().IsIndeterminate());

  // Update progress to a determinate, in-progress value.
  HoldingSpaceItemUpdatedFields expected_update;
  expected_update.previous_progress = item_ptr->progress();
  HoldingSpaceProgress progress(/*current_bytes=*/50, /*total_bytes=*/100);
  model().UpdateItem(item_ptr->id())->SetProgress(progress);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(item_ptr->progress(), progress);
  EXPECT_FALSE(item_ptr->progress().IsComplete());
  EXPECT_FALSE(item_ptr->progress().IsIndeterminate());

  // Update progress to the same value. This should no-op.
  progress = HoldingSpaceProgress(/*current_bytes=*/50, /*total_bytes=*/100);
  model().UpdateItem(item_ptr->id())->SetProgress(progress);
  EXPECT_FALSE(observation.TakeLastUpdatedItem());
  EXPECT_TRUE(observation.TakeLastUpdatedFields().IsEmpty());
  EXPECT_EQ(item_ptr->progress(), progress);
  EXPECT_FALSE(item_ptr->progress().IsComplete());
  EXPECT_FALSE(item_ptr->progress().IsIndeterminate());

  // Update progress to indeterminate.
  expected_update = HoldingSpaceItemUpdatedFields();
  expected_update.previous_progress = item_ptr->progress();
  progress = HoldingSpaceProgress(/*current_bytes=*/std::nullopt,
                                  /*total_bytes=*/100);
  model().UpdateItem(item_ptr->id())->SetProgress(progress);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(item_ptr->progress(), progress);
  EXPECT_FALSE(item_ptr->progress().IsComplete());
  EXPECT_TRUE(item_ptr->progress().IsIndeterminate());

  // Update progress to complete.
  expected_update = HoldingSpaceItemUpdatedFields();
  expected_update.previous_progress = item_ptr->progress();
  progress = HoldingSpaceProgress(/*current_bytes=*/100, /*total_bytes=*/100);
  model().UpdateItem(item_ptr->id())->SetProgress(progress);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(observation.TakeLastUpdatedFields(), expected_update);
  EXPECT_EQ(item_ptr->progress(), progress);
  EXPECT_TRUE(item_ptr->progress().IsComplete());
  EXPECT_FALSE(item_ptr->progress().IsIndeterminate());

  // Update progress to an in-progress value. This should no-op as progress
  // becomes read-only after being marked completed.
  progress = item_ptr->progress();
  model()
      .UpdateItem(item_ptr->id())
      ->SetProgress(
          HoldingSpaceProgress(/*current_bytes=*/50, /*total_bytes=*/100));
  EXPECT_FALSE(observation.TakeLastUpdatedItem());
  EXPECT_TRUE(observation.TakeLastUpdatedFields().IsEmpty());
  EXPECT_EQ(item_ptr->progress(), progress);
  EXPECT_TRUE(item_ptr->progress().IsComplete());
  EXPECT_FALSE(item_ptr->progress().IsIndeterminate());
}

}  // namespace ash
