// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/holding_space/holding_space_presenter.h"

#include <memory>
#include <set>
#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/test/ash_test_base.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "ui/gfx/image/image.h"

namespace ash {

namespace {

constexpr HoldingSpaceItem::Type kItemTypes[] = {
    HoldingSpaceItem::Type::kPinnedFile, HoldingSpaceItem::Type::kScreenshot,
    HoldingSpaceItem::Type::kDownload};

}  // namespace

class HoldingSpacePresenterTest
    : public AshTestBase,
      public testing::WithParamInterface<HoldingSpaceItem::Type> {
 public:
  HoldingSpacePresenterTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kTemporaryHoldingSpace);
  }
  HoldingSpacePresenterTest(const HoldingSpacePresenterTest& other) = delete;
  HoldingSpacePresenterTest& operator=(const HoldingSpacePresenterTest& other) =
      delete;
  ~HoldingSpacePresenterTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    holding_space_presenter_ = std::make_unique<HoldingSpacePresenter>();
  }

  void TearDown() override {
    holding_space_presenter_.reset();
    AshTestBase::TearDown();
  }

  HoldingSpaceModel* primary_model() { return &primary_model_; }

  HoldingSpacePresenter* GetHoldingSpacePresenter() {
    return holding_space_presenter_.get();
  }

  HoldingSpaceItem::Type GetTestItemType() const { return GetParam(); }

  std::string GetTestItemIdForType(HoldingSpaceItem::Type type,
                                   const std::string& path) {
    return HoldingSpaceItem::GetFileBackedItemId(type, base::FilePath(path));
  }

  std::string GetTestItemId(const std::string& path) {
    return GetTestItemIdForType(GetTestItemType(), path);
  }

  std::unique_ptr<HoldingSpaceItem> CreateTestItemForType(
      HoldingSpaceItem::Type type,
      const std::string& path) {
    return HoldingSpaceItem::CreateFileBackedItem(
        type, base::FilePath(path),
        GURL(base::StrCat({"filesystem:base://", path})), gfx::ImageSkia());
  }

  std::unique_ptr<HoldingSpaceItem> CreateTestItem(const std::string& path) {
    return CreateTestItemForType(GetTestItemType(), path);
  }

 private:
  std::unique_ptr<HoldingSpacePresenter> holding_space_presenter_;
  HoldingSpaceModel primary_model_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(ItemType,
                         HoldingSpacePresenterTest,
                         testing::ValuesIn(kItemTypes));

// Tests that items already in the model when an active model is set get added
// to the holding space.
TEST_P(HoldingSpacePresenterTest, ModelWithExistingItems) {
  EXPECT_EQ(std::vector<std::string>(),
            GetHoldingSpacePresenter()->GetItemIds(GetTestItemType()));

  const std::string item_1_id = GetTestItemId("item_1");
  primary_model()->AddItem(CreateTestItem("item_1"));
  ASSERT_TRUE(primary_model()->GetItem(item_1_id));
  ASSERT_EQ(item_1_id, primary_model()->GetItem(item_1_id)->id());

  const std::string item_2_id = GetTestItemId("item_2");
  primary_model()->AddItem(CreateTestItem("item_2"));
  ASSERT_TRUE(primary_model()->GetItem(item_2_id));
  ASSERT_EQ(item_2_id, primary_model()->GetItem(item_2_id)->id());

  // Note - the item ID is missing a suffix to verify the items do not get
  // sorted by their IDs.
  const std::string item_3_id = GetTestItemId("item");
  primary_model()->AddItem(CreateTestItem("item"));

  HoldingSpaceController::Get()->SetModel(primary_model());
  EXPECT_EQ(std::vector<std::string>({item_1_id, item_2_id, item_3_id}),
            GetHoldingSpacePresenter()->GetItemIds(GetTestItemType()));

  HoldingSpaceController::Get()->SetModel(nullptr);
  EXPECT_EQ(std::vector<std::string>(),
            GetHoldingSpacePresenter()->GetItemIds(GetTestItemType()));
}

// Tests that the holding space presenter picks up existing model items if a
// model is set and non-empty on presenter's creation.
TEST_P(HoldingSpacePresenterTest, NonEmptyModelOnPresenterCreation) {
  // Initiate non-empty holding space model.
  const std::string item_1_id = GetTestItemId("item_1");
  primary_model()->AddItem(CreateTestItem("item_1"));
  const std::string item_2_id = GetTestItemId("item_2");
  primary_model()->AddItem(CreateTestItem("item_2"));
  HoldingSpaceController::Get()->SetModel(primary_model());

  // Create a new holding space presenter, and verify it picked up the existing
  // model items.
  auto secondary_presenter = std::make_unique<HoldingSpacePresenter>();
  EXPECT_EQ(std::vector<std::string>({item_1_id, item_2_id}),
            secondary_presenter->GetItemIds(GetTestItemType()));

  HoldingSpaceController::Get()->SetModel(nullptr);
  EXPECT_EQ(std::vector<std::string>(),
            GetHoldingSpacePresenter()->GetItemIds(GetTestItemType()));
  EXPECT_EQ(std::vector<std::string>(),
            secondary_presenter->GetItemIds(GetTestItemType()));
}

// Verifies that holding space handles holding space model changes.
TEST_P(HoldingSpacePresenterTest, AddingAndRemovingModelItems) {
  HoldingSpaceController::Get()->SetModel(primary_model());
  EXPECT_EQ(std::vector<std::string>(),
            GetHoldingSpacePresenter()->GetItemIds(GetTestItemType()));

  // Add some items to the model, and verify the items get picked up by the
  // presenter.
  const std::string item_1_id = GetTestItemId("item_1");
  primary_model()->AddItem(CreateTestItem("item_1"));
  EXPECT_EQ(std::vector<std::string>({item_1_id}),
            GetHoldingSpacePresenter()->GetItemIds(GetTestItemType()));

  const std::string item_2_id = GetTestItemId("item_2");
  primary_model()->AddItem(CreateTestItem("item_2"));
  EXPECT_EQ(std::vector<std::string>({item_1_id, item_2_id}),
            GetHoldingSpacePresenter()->GetItemIds(GetTestItemType()));

  const std::string item_3_id = GetTestItemId("item_3");
  primary_model()->AddItem(CreateTestItem("item_3"));
  EXPECT_EQ(std::vector<std::string>({item_1_id, item_2_id, item_3_id}),
            GetHoldingSpacePresenter()->GetItemIds(GetTestItemType()));

  primary_model()->RemoveItem(item_2_id);
  EXPECT_EQ(std::vector<std::string>({item_1_id, item_3_id}),
            GetHoldingSpacePresenter()->GetItemIds(GetTestItemType()));

  primary_model()->RemoveAll();
  EXPECT_EQ(std::vector<std::string>(),
            GetHoldingSpacePresenter()->GetItemIds(GetTestItemType()));

  const std::string item_4_id = GetTestItemId("item_4");
  primary_model()->AddItem(CreateTestItem("item_4"));
  EXPECT_EQ(std::vector<std::string>({item_4_id}),
            GetHoldingSpacePresenter()->GetItemIds(GetTestItemType()));

  // Holding space should be cleared if the active model is reset.
  HoldingSpaceController::Get()->SetModel(nullptr);
  EXPECT_EQ(std::vector<std::string>(),
            GetHoldingSpacePresenter()->GetItemIds(GetTestItemType()));
}

// Verifies that presenter trackes items with the same backing URL but different
// types separately.
TEST_P(HoldingSpacePresenterTest, SameUrlsWithDifferentType) {
  HoldingSpaceController::Get()->SetModel(primary_model());
  EXPECT_EQ(std::vector<std::string>(),
            GetHoldingSpacePresenter()->GetItemIds(GetTestItemType()));

  // Add some items to the model, and verify the items get picked up by the
  // presenter.
  for (const auto& type : kItemTypes)
    primary_model()->AddItem(CreateTestItemForType(type, "item_1"));

  // Collect items of all types, and verify they all have distinct IDs.
  std::set<std::string> all_ids;
  for (const auto& type : kItemTypes) {
    std::vector<std::string> item_ids =
        GetHoldingSpacePresenter()->GetItemIds(type);
    EXPECT_EQ(std::vector<std::string>({GetTestItemIdForType(type, "item_1")}),
              item_ids);
    all_ids.insert(item_ids.begin(), item_ids.end());
  }

  EXPECT_EQ(base::size(kItemTypes), all_ids.size());
  EXPECT_EQ(base::size(kItemTypes), primary_model()->items().size());

  // Remove the item for the test parameter type, and verify that is the only
  // item removed.
  primary_model()->RemoveItem(GetTestItemId("item_1"));

  all_ids.clear();
  for (const auto& type : kItemTypes) {
    std::vector<std::string> item_ids =
        GetHoldingSpacePresenter()->GetItemIds(type);
    if (type == GetTestItemType()) {
      EXPECT_EQ(std::vector<std::string>(), item_ids);
    } else {
      EXPECT_EQ(
          std::vector<std::string>({GetTestItemIdForType(type, "item_1")}),
          item_ids);
    }
    all_ids.insert(item_ids.begin(), item_ids.end());
  }

  EXPECT_EQ(base::size(kItemTypes) - 1, all_ids.size());
  EXPECT_EQ(base::size(kItemTypes) - 1, primary_model()->items().size());
}

// Verifies that the holding space gets updated when the active model changes.
TEST_P(HoldingSpacePresenterTest, ModelChange) {
  const std::string primary_model_item_id = GetTestItemId("primary_model_item");
  primary_model()->AddItem(CreateTestItem("primary_model_item"));

  HoldingSpaceController::Get()->SetModel(primary_model());
  EXPECT_EQ(std::vector<std::string>({primary_model_item_id}),
            GetHoldingSpacePresenter()->GetItemIds(GetTestItemType()));

  HoldingSpaceModel secondary_model;
  const std::string secondary_model_item_id =
      GetTestItemId("secondary_model_item");
  secondary_model.AddItem(CreateTestItem("secondary_model_item"));

  EXPECT_EQ(std::vector<std::string>({primary_model_item_id}),
            GetHoldingSpacePresenter()->GetItemIds(GetTestItemType()));

  HoldingSpaceController::Get()->SetModel(&secondary_model);

  EXPECT_EQ(std::vector<std::string>({secondary_model_item_id}),
            GetHoldingSpacePresenter()->GetItemIds(GetTestItemType()));

  HoldingSpaceController::Get()->SetModel(nullptr);
  EXPECT_EQ(std::vector<std::string>(),
            GetHoldingSpacePresenter()->GetItemIds(GetTestItemType()));
}

}  // namespace ash
