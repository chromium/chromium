// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/reorder/app_list_reorder_core.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "components/crx_file/id_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using AppListReorderCoreTest = testing::Test;
using crx_file::id_util::GenerateId;

namespace app_list {

// Verifies that when calculating an item's target position under the specified
// sort order, if the item's current position already maintains the order,
// its position should not change.
TEST_F(AppListReorderCoreTest, CalculatePositionForItemAlreadyInOrder) {
  ChromeAppListItem item1(/*profile=*/nullptr, GenerateId("Id1"),
                          /*model_updater=*/nullptr);
  item1.SetChromeName("A");
  item1.SetChromePosition(syncer::StringOrdinal::CreateInitialOrdinal());

  ChromeAppListItem item2(/*profile=*/nullptr, GenerateId("Id2"),
                          /*model_updater=*/nullptr);
  item2.SetChromeName("B");
  item2.SetChromePosition(item1.position().CreateAfter());

  ChromeAppListItem item3(/*profile=*/nullptr, GenerateId("Id3"),
                          /*model_updater=*/nullptr);
  item3.SetChromeName("C");
  item3.SetChromePosition(item2.position().CreateAfter());

  ChromeAppListItem item4(/*profile=*/nullptr, GenerateId("Id4"),
                          /*model_updater=*/nullptr);
  item4.SetChromeName("D");
  item4.SetChromePosition(item3.position().CreateAfter());

  std::vector<const ChromeAppListItem*> items{&item1, &item2, &item3, &item4};
  syncer::StringOrdinal target_position;
  bool success = reorder::CalculateItemPositionInOrder(
      ash::AppListSortOrder::kNameAlphabetical, item1.metadata(), items,
      /*global_items=*/nullptr, &target_position);
  EXPECT_TRUE(success);
  EXPECT_TRUE(target_position.Equals(item1.position()));

  success = reorder::CalculateItemPositionInOrder(
      ash::AppListSortOrder::kNameAlphabetical, item2.metadata(), items,
      /*global_items=*/nullptr, &target_position);
  EXPECT_TRUE(success);
  EXPECT_TRUE(target_position.Equals(item2.position()));

  // Change the item name. The new name does not break the alphabetical order.
  // Verify that the item's position under sort order does not change.
  item3.SetChromeName("ca");
  success = reorder::CalculateItemPositionInOrder(
      ash::AppListSortOrder::kNameAlphabetical, item3.metadata(), items,
      /*global_items=*/nullptr, &target_position);
  EXPECT_TRUE(success);
  EXPECT_TRUE(target_position.Equals(item3.position()));

  success = reorder::CalculateItemPositionInOrder(
      ash::AppListSortOrder::kNameAlphabetical, item4.metadata(), items,
      /*global_items=*/nullptr, &target_position);
  EXPECT_TRUE(success);
  EXPECT_TRUE(target_position.Equals(item4.position()));
}

TEST_F(AppListReorderCoreTest, CalculatePositionForItemNotInOrder) {
  // Prepare four items. Note that `item3` is out of order.
  ChromeAppListItem item1(/*profile=*/nullptr, GenerateId("Id1"),
                          /*model_updater=*/nullptr);
  item1.SetChromeName("A");
  item1.SetChromePosition(syncer::StringOrdinal::CreateInitialOrdinal());

  ChromeAppListItem item2(/*profile=*/nullptr, GenerateId("Id2"),
                          /*model_updater=*/nullptr);
  item2.SetChromeName("B");
  item2.SetChromePosition(item1.position().CreateAfter());

  ChromeAppListItem item4(/*profile=*/nullptr, GenerateId("Id4"),
                          /*model_updater=*/nullptr);
  item4.SetChromeName("D");
  item4.SetChromePosition(item2.position().CreateAfter());

  ChromeAppListItem item3(/*profile=*/nullptr, GenerateId("Id3"),
                          /*model_updater=*/nullptr);
  item3.SetChromeName("C");

  // Calculate `item3`'s position in order.
  std::vector<const ChromeAppListItem*> items{&item1, &item2, &item3, &item4};
  syncer::StringOrdinal target_position;
  bool success = reorder::CalculateItemPositionInOrder(
      ash::AppListSortOrder::kNameAlphabetical, item3.metadata(), items,
      /*global_items=*/nullptr, &target_position);
  EXPECT_TRUE(success);

  // Verify that `target_position` is between `item2` and `item4`.
  EXPECT_TRUE(target_position.GreaterThan(item2.position()) &&
              target_position.LessThan(item4.position()));
}

TEST_F(AppListReorderCoreTest, CalculatePositionForEphemeralItemNotInOrder) {
  // Prepare four items. Note that `ephemeral_item1` is an ephemeral app.
  // Current order: `item1`, `item2`, `item3`, `ephemeral_item1`.
  ChromeAppListItem item1(/*profile=*/nullptr, GenerateId("Id1"),
                          /*model_updater=*/nullptr);
  item1.SetChromeName("A");
  item1.SetChromePosition(syncer::StringOrdinal::CreateInitialOrdinal());

  ChromeAppListItem item2(/*profile=*/nullptr, GenerateId("Id2"),
                          /*model_updater=*/nullptr);
  item2.SetChromeName("B");
  item2.SetChromePosition(item1.position().CreateAfter());

  ChromeAppListItem item3(/*profile=*/nullptr, GenerateId("Id3"),
                          /*model_updater=*/nullptr);
  item3.SetChromeName("C");
  item3.SetChromePosition(item2.position().CreateAfter());

  ChromeAppListItem ephemeral_item1(/*profile=*/nullptr, GenerateId("Id4"),
                                    /*model_updater=*/nullptr);
  ephemeral_item1.SetChromeName("D");
  ephemeral_item1.SetIsEphemeral(true);
  ephemeral_item1.SetChromePosition(item3.position().CreateAfter());

  // New order: `ephemeral_item1`, `item1`, `item2`, `&item3`.
  std::vector<const ChromeAppListItem*> items{&item1, &item2, &item3,
                                              &ephemeral_item1};
  syncer::StringOrdinal target_position;

  // Calculate `ephemeral_item1`'s position in order.
  bool success = reorder::CalculateItemPositionInOrder(
      ash::AppListSortOrder::kAlphabeticalEphemeralAppFirst,
      ephemeral_item1.metadata(), items,
      /*global_items=*/nullptr, &target_position);
  EXPECT_TRUE(success);

  // Verify that `target_position` is less than `item1`.
  EXPECT_TRUE(target_position.LessThan(item1.position()));
}

}  // namespace app_list
