// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_model.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "base/scoped_observation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

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
      HoldingSpaceImage::GetMaxSizeForType(type), file_path,
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

  // Returns the last `HoldingSpaceItem` for which `OnHoldingSpaceItemUpdated()`
  // was called, clearing the cached value.
  const HoldingSpaceItem* TakeLastUpdatedItem() {
    const HoldingSpaceItem* result = last_updated_item_;
    last_updated_item_ = nullptr;
    return result;
  }

 private:
  // HoldingSpaceModel::Observer:
  void OnHoldingSpaceItemUpdated(const HoldingSpaceItem* item) override {
    last_updated_item_ = item;
  }

  // The last `HoldingSpaceItem` for which `OnHoldingSpaceItemUpdated()` was
  // called. May be `nullptr` prior to an update event or following a call to
  // `TakeLastUpdatedItem()`.
  const HoldingSpaceItem* last_updated_item_ = nullptr;

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

// Verifies that `HoldingSpaceModel::UpdateProgressForItem()` works as intended.
TEST_P(HoldingSpaceModelTest, UpdateProgressForItem) {
  ScopedModelObservation observation(&model());

  // Verify the `model()` is initially empty.
  EXPECT_EQ(model().items().size(), 0u);

  // Create a holding space `item`.
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      /*type=*/GetParam(), base::FilePath("file_path"),
      GURL("filesystem::file_system_url"),
      /*progress=*/absl::nullopt,
      /*image_resolver=*/base::BindOnce(&CreateFakeHoldingSpaceImage));
  auto* item_ptr = item.get();

  // Add `item` to the `model()`.
  model().AddItem(std::move(item));
  EXPECT_EQ(model().items().size(), 1u);
  EXPECT_EQ(model().items()[0].get(), item_ptr);

  // Verify progress is indeterminate.
  EXPECT_EQ(item_ptr->progress(), absl::nullopt);

  // Update progress to `0.5f`.
  model().UpdateProgressForItem(item_ptr->id(), 0.5f);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(item_ptr->progress(), 0.5f);

  // Update progress to `0.5f` again. This should no-op.
  model().UpdateProgressForItem(item_ptr->id(), 0.5f);
  EXPECT_FALSE(observation.TakeLastUpdatedItem());
  EXPECT_EQ(item_ptr->progress(), 0.5f);

  // Update progress to indeterminate.
  model().UpdateProgressForItem(item_ptr->id(), absl::nullopt);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(item_ptr->progress(), absl::nullopt);

  // Update progress to complete.
  model().UpdateProgressForItem(item_ptr->id(), 1.f);
  EXPECT_EQ(observation.TakeLastUpdatedItem(), item_ptr);
  EXPECT_EQ(item_ptr->progress(), 1.f);

  // Update progress to `0.5f`. This should no-op as progress becomes read-only
  // after being marked completed.
  model().UpdateProgressForItem(item_ptr->id(), 0.5f);
  EXPECT_FALSE(observation.TakeLastUpdatedItem());
  EXPECT_EQ(item_ptr->progress(), 1.f);
}

}  // namespace ash
