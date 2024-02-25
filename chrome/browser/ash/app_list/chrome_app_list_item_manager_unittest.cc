// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/chrome_app_list_item_manager.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/test/fake_app_list_model_updater.h"
#include "components/crx_file/id_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using crx_file::id_util::GenerateId;

class ChromeAppListItemManagerTest : public testing::Test {
 public:
  ChromeAppListItemManagerTest()
      : model_updater_(std::make_unique<FakeAppListModelUpdater>(
            /*profile=*/nullptr,
            /*order_delegate=*/nullptr)) {}

  ChromeAppListItemManagerTest(const ChromeAppListItemManagerTest&) = delete;
  ChromeAppListItemManagerTest& operator=(const ChromeAppListItemManagerTest&) =
      delete;

  ~ChromeAppListItemManagerTest() override = default;

 protected:
  const std::map<std::string,
                 std::vector<raw_ptr<ChromeAppListItem, VectorExperimental>>>&
  folder_item_mapping() {
    return item_manager_.folder_item_mappings_;
  }

  ChromeAppListItemManager item_manager_;
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;
};

// Verifies that when adding app list items with valid positions,
// `ChromeAppListItemManager` ensures that a folder's children are arranged
// following the position order.
TEST_F(ChromeAppListItemManagerTest, AddItemsWithValidPosition) {
  // Create a folder.
  const std::string kFolderId = GenerateId("folder_id1");
  auto folder = std::make_unique<ChromeAppListItem>(nullptr, kFolderId,
                                                    model_updater_.get());
  folder->SetChromeIsFolder(true);
  folder->SetChromePosition(syncer::StringOrdinal::CreateInitialOrdinal());
  item_manager_.AddChromeItem(std::move(folder));

  EXPECT_EQ(1u, folder_item_mapping().size());
  const std::vector<raw_ptr<ChromeAppListItem, VectorExperimental>>& children =
      folder_item_mapping().find(kFolderId)->second;
  EXPECT_EQ(0u, children.size());

  // Add three children to the folder.
  const std::string kChildId1 = GenerateId("child_id1");
  auto child1 = std::make_unique<ChromeAppListItem>(nullptr, kChildId1,
                                                    model_updater_.get());
  child1->SetChromePosition(syncer::StringOrdinal::CreateInitialOrdinal());
  child1->SetChromeName("A");
  child1->SetFolderId(kFolderId);
  auto* child1_ptr = item_manager_.AddChromeItem(std::move(child1));

  const std::string kChildId2 = GenerateId("child_id2");
  auto child2 = std::make_unique<ChromeAppListItem>(nullptr, kChildId2,
                                                    model_updater_.get());
  child2->SetChromePosition(child1_ptr->position().CreateBefore());
  child2->SetChromeName("B");
  child2->SetFolderId(kFolderId);
  auto* child2_ptr = item_manager_.AddChromeItem(std::move(child2));

  const std::string kChildId3 = GenerateId("child_id3");
  auto child3 = std::make_unique<ChromeAppListItem>(nullptr, kChildId3,
                                                    model_updater_.get());
  child3->SetChromePosition(
      child1_ptr->position().CreateBetween(child2_ptr->position()));
  child3->SetChromeName("C");
  child3->SetFolderId(kFolderId);
  item_manager_.AddChromeItem(std::move(child3));

  // Verify the children order after adding.
  EXPECT_EQ(3u, children.size());
  EXPECT_EQ("B", children[0]->name());
  EXPECT_EQ("C", children[1]->name());
  EXPECT_EQ("A", children[2]->name());

  // Remove one child then verify the order.
  item_manager_.RemoveChromeItem(kChildId1);
  EXPECT_EQ(2u, children.size());
  EXPECT_EQ("B", children[0]->name());
  EXPECT_EQ("C", children[1]->name());

  // Remove one child then verify the order.
  item_manager_.RemoveChromeItem(kChildId2);
  EXPECT_EQ(1u, children.size());
  EXPECT_EQ("C", children[0]->name());

  // Remove one child then verify the order.
  item_manager_.RemoveChromeItem(kChildId3);
  EXPECT_EQ(0u, children.size());
  EXPECT_EQ(1u, folder_item_mapping().size());

  // Remove the folder.
  item_manager_.RemoveChromeItem(kFolderId);
  EXPECT_EQ(0u, folder_item_mapping().size());
}

// Verify that `ChromeAppListItemManager` works as expected when adding items
// with invalid positions.
TEST_F(ChromeAppListItemManagerTest, AddItemsWithInvalidPosition) {
  // Create a folder.
  const std::string kFolderId = GenerateId("folder_id");
  auto folder = std::make_unique<ChromeAppListItem>(nullptr, kFolderId,
                                                    model_updater_.get());
  folder->SetChromeIsFolder(true);
  folder->SetChromePosition(syncer::StringOrdinal::CreateInitialOrdinal());
  item_manager_.AddChromeItem(std::move(folder));

  // Add three children to the folder.
  const std::string kChildId1 = GenerateId("child_id1");
  auto child1 = std::make_unique<ChromeAppListItem>(nullptr, kChildId1,
                                                    model_updater_.get());
  child1->SetChromeName("A");
  child1->SetFolderId(kFolderId);
  item_manager_.AddChromeItem(std::move(child1));

  const std::string kChildId2 = GenerateId("child_id2");
  auto child2 = std::make_unique<ChromeAppListItem>(nullptr, kChildId2,
                                                    model_updater_.get());
  child2->SetChromeName("B");
  child2->SetFolderId(kFolderId);
  item_manager_.AddChromeItem(std::move(child2));

  const std::string kChildId3 = GenerateId("child_id3");
  auto child3 = std::make_unique<ChromeAppListItem>(nullptr, kChildId3,
                                                    model_updater_.get());
  child3->SetChromeName("C");
  child3->SetFolderId(kFolderId);
  item_manager_.AddChromeItem(std::move(child3));

  // Verify the children order after adding.
  const std::vector<raw_ptr<ChromeAppListItem, VectorExperimental>>& children =
      folder_item_mapping().find(kFolderId)->second;
  EXPECT_EQ(3u, children.size());
  EXPECT_EQ("A", children[0]->name());
  EXPECT_EQ("B", children[1]->name());
  EXPECT_EQ("C", children[2]->name());

  // Verify the children position order. A child without valid position should
  // be placed at the end.
  EXPECT_TRUE(children[0]->position().LessThan(children[1]->position()));
  EXPECT_TRUE(children[1]->position().LessThan(children[2]->position()));
}
