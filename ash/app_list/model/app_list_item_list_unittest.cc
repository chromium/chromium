// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/app_list_item_list.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_item_list_observer.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_model_delegate.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// A fake updater backed by an item list. Used to emulate the process of
// handling the requests from Ash to update items. It helps to avoid the
// reliance on the browser side that does the real job in the product code.
class AppListItemListWithUpdater : public AppListModelDelegate {
 public:
  AppListItemListWithUpdater()
      : item_list_(std::make_unique<AppListItemList>(
            /*app_list_model_delegate=*/this)) {}

  AppListItemListWithUpdater(const AppListItemListWithUpdater&) = delete;
  AppListItemListWithUpdater& operator=(const AppListItemListWithUpdater&) =
      delete;

  ~AppListItemListWithUpdater() override = default;

  // AppListModelDelegate:
  void RequestPositionUpdate(std::string id,
                             const syncer::StringOrdinal& new_position,
                             RequestPositionUpdateReason reason) override {
    item_list_->SetItemPosition(item_list_->FindItem(id), new_position);
  }
  void RequestMoveItemToFolder(std::string id,
                               const std::string& folder_id) override {}
  void RequestMoveItemToRoot(std::string id,
                             syncer::StringOrdinal target_position) override {}
  std::string RequestFolderCreation(std::string merge_target_id,
                                    std::string item_to_merge_id) override {
    return "";
  }
  void RequestFolderRename(std::string folder_id,
                           const std::string& name) override {}
  void RequestAppListSort(AppListSortOrder order) override {}
  void RequestAppListSortRevert() override {}
  void RequestCommitTemporarySortOrder() override {}

  AppListItemList* item_list() { return item_list_.get(); }

 private:
  std::unique_ptr<AppListItemList> item_list_;
};

class TestObserver : public AppListItemListObserver {
 public:
  TestObserver() = default;

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override = default;

  // AppListItemListObserver overriden:
  void OnListItemAdded(size_t index, AppListItem* item) override {
    ++items_added_;
  }

  void OnListItemRemoved(size_t index, AppListItem* item) override {
    ++items_removed_;
  }

  void OnListItemMoved(size_t from_index,
                       size_t to_index,
                       AppListItem* item) override {
    ++items_moved_;
  }

  size_t items_added() const { return items_added_; }
  size_t items_removed() const { return items_removed_; }
  size_t items_moved() const { return items_moved_; }

  void ResetCounts() {
    items_added_ = 0;
    items_removed_ = 0;
    items_moved_ = 0;
  }

 private:
  size_t items_added_ = 0;
  size_t items_removed_ = 0;
  size_t items_moved_ = 0;
};

std::string GetItemId(int id) {
  return base::StringPrintf("Item %d", id);
}

}  // namespace

class AppListItemListTest : public testing::Test {
 public:
  AppListItemListTest() = default;

  AppListItemListTest(const AppListItemListTest&) = delete;
  AppListItemListTest& operator=(const AppListItemListTest&) = delete;

  ~AppListItemListTest() override = default;

  // testing::Test overrides:
  void SetUp() override {
    item_list_ = item_updater_.item_list();
    item_list_->AddObserver(&observer_);
  }

  void TearDown() override { item_list_->RemoveObserver(&observer_); }

 protected:
  AppListItem* FindItem(const std::string& id) {
    return item_list_->FindItem(id);
  }

  bool FindItemIndex(const std::string& id, size_t* index) {
    return item_list_->FindItemIndex(id, index);
  }

  std::unique_ptr<AppListItem> CreateItem(const std::string& name) {
    std::unique_ptr<AppListItem> item(new AppListItem(name));
    size_t nitems = item_list_->item_count();
    syncer::StringOrdinal position;
    if (nitems == 0)
      position = syncer::StringOrdinal::CreateInitialOrdinal();
    else
      position = item_list_->item_at(nitems - 1)->position().CreateAfter();
    item->set_position(position);
    return item;
  }

  AppListItem* CreateAndAddItem(const std::string& name) {
    std::unique_ptr<AppListItem> item(CreateItem(name));
    return item_list_->AddItem(std::move(item));
  }

  std::unique_ptr<AppListItem> RemoveItem(const std::string& id) {
    return item_list_->RemoveItem(id);
  }

  std::unique_ptr<AppListItem> RemoveItemAt(size_t index) {
    return item_list_->RemoveItemAt(index);
  }

  syncer::StringOrdinal CreatePositionBefore(
      const syncer::StringOrdinal& position) {
    return item_list_->CreatePositionBefore(position);
  }

  bool VerifyItemListOrdinals() {
    bool res = true;
    for (size_t i = 1; i < item_list_->item_count(); ++i) {
      res &= (item_list_->item_at(i - 1)->position().LessThan(
          item_list_->item_at(i)->position()));
    }
    if (!res)
      PrintItems();
    return res;
  }

  bool VerifyItemOrder4(size_t a, size_t b, size_t c, size_t d) {
    if ((GetItemId(a) == item_list_->item_at(0)->id()) &&
        (GetItemId(b) == item_list_->item_at(1)->id()) &&
        (GetItemId(c) == item_list_->item_at(2)->id()) &&
        (GetItemId(d) == item_list_->item_at(3)->id()))
      return true;
    PrintItems();
    return false;
  }

  void PrintItems() {
    VLOG(1) << "ITEMS:";
    for (size_t i = 0; i < item_list_->item_count(); ++i)
      VLOG(1) << " " << item_list_->item_at(i)->ToDebugString();
  }

  AppListItemListWithUpdater item_updater_;
  TestObserver observer_;
  raw_ptr<AppListItemList> item_list_ = nullptr;
};

TEST_F(AppListItemListTest, FindItemIndex) {
  AppListItem* item_0 = CreateAndAddItem(GetItemId(0));
  AppListItem* item_1 = CreateAndAddItem(GetItemId(1));
  AppListItem* item_2 = CreateAndAddItem(GetItemId(2));
  EXPECT_EQ(observer_.items_added(), 3u);
  EXPECT_EQ(item_list_->item_count(), 3u);
  EXPECT_EQ(item_0, item_list_->item_at(0));
  EXPECT_EQ(item_1, item_list_->item_at(1));
  EXPECT_EQ(item_2, item_list_->item_at(2));
  EXPECT_TRUE(VerifyItemListOrdinals());

  size_t index;
  EXPECT_TRUE(FindItemIndex(item_0->id(), &index));
  EXPECT_EQ(index, 0u);
  EXPECT_TRUE(FindItemIndex(item_1->id(), &index));
  EXPECT_EQ(index, 1u);
  EXPECT_TRUE(FindItemIndex(item_2->id(), &index));
  EXPECT_EQ(index, 2u);

  std::unique_ptr<AppListItem> item_3(CreateItem(GetItemId(3)));
  EXPECT_FALSE(FindItemIndex(item_3->id(), &index));
}

TEST_F(AppListItemListTest, RemoveItemAt) {
  AppListItem* item_0 = CreateAndAddItem(GetItemId(0));
  AppListItem* item_1 = CreateAndAddItem(GetItemId(1));
  AppListItem* item_2 = CreateAndAddItem(GetItemId(2));
  EXPECT_EQ(item_list_->item_count(), 3u);
  EXPECT_EQ(observer_.items_added(), 3u);
  size_t index;
  EXPECT_TRUE(FindItemIndex(item_1->id(), &index));
  EXPECT_EQ(index, 1u);
  EXPECT_TRUE(VerifyItemListOrdinals());

  std::unique_ptr<AppListItem> item_removed = RemoveItemAt(1);
  EXPECT_EQ(item_removed.get(), item_1);
  EXPECT_FALSE(FindItem(item_1->id()));
  EXPECT_EQ(item_list_->item_count(), 2u);
  EXPECT_EQ(observer_.items_removed(), 1u);
  EXPECT_EQ(item_list_->item_at(0), item_0);
  EXPECT_EQ(item_list_->item_at(1), item_2);
  EXPECT_TRUE(VerifyItemListOrdinals());
}

TEST_F(AppListItemListTest, RemoveItem) {
  AppListItem* item_0 = CreateAndAddItem(GetItemId(0));
  AppListItem* item_1 = CreateAndAddItem(GetItemId(1));
  AppListItem* item_2 = CreateAndAddItem(GetItemId(2));
  EXPECT_EQ(item_list_->item_count(), 3u);
  EXPECT_EQ(observer_.items_added(), 3u);
  EXPECT_EQ(item_0, item_list_->item_at(0));
  EXPECT_EQ(item_1, item_list_->item_at(1));
  EXPECT_EQ(item_2, item_list_->item_at(2));
  EXPECT_TRUE(VerifyItemListOrdinals());

  size_t index;
  EXPECT_TRUE(FindItemIndex(item_1->id(), &index));
  EXPECT_EQ(index, 1u);

  std::unique_ptr<AppListItem> item_removed = RemoveItem(item_1->id());
  EXPECT_EQ(item_removed.get(), item_1);
  EXPECT_FALSE(FindItem(item_1->id()));
  EXPECT_EQ(item_list_->item_count(), 2u);
  EXPECT_EQ(observer_.items_removed(), 1u);
  EXPECT_TRUE(VerifyItemListOrdinals());
}

TEST_F(AppListItemListTest, MoveItem) {
  CreateAndAddItem(GetItemId(0));
  CreateAndAddItem(GetItemId(1));
  CreateAndAddItem(GetItemId(2));
  CreateAndAddItem(GetItemId(3));

  EXPECT_TRUE(VerifyItemOrder4(0, 1, 2, 3));

  item_list_->MoveItem(0, 0);
  EXPECT_EQ(0u, observer_.items_moved());
  EXPECT_TRUE(VerifyItemOrder4(0, 1, 2, 3));

  item_list_->MoveItem(0, 1);
  EXPECT_EQ(1u, observer_.items_moved());
  EXPECT_TRUE(VerifyItemListOrdinals());
  EXPECT_TRUE(VerifyItemOrder4(1, 0, 2, 3));

  item_list_->MoveItem(1, 2);
  EXPECT_EQ(2u, observer_.items_moved());
  EXPECT_TRUE(VerifyItemListOrdinals());
  EXPECT_TRUE(VerifyItemOrder4(1, 2, 0, 3));

  item_list_->MoveItem(2, 1);
  EXPECT_EQ(3u, observer_.items_moved());
  EXPECT_TRUE(VerifyItemListOrdinals());
  EXPECT_TRUE(VerifyItemOrder4(1, 0, 2, 3));

  item_list_->MoveItem(3, 0);
  EXPECT_EQ(4u, observer_.items_moved());
  EXPECT_TRUE(VerifyItemListOrdinals());
  EXPECT_TRUE(VerifyItemOrder4(3, 1, 0, 2));

  item_list_->MoveItem(0, 3);
  EXPECT_EQ(5u, observer_.items_moved());
  EXPECT_TRUE(VerifyItemListOrdinals());
  EXPECT_TRUE(VerifyItemOrder4(1, 0, 2, 3));
}

TEST_F(AppListItemListTest, SamePosition) {
  CreateAndAddItem(GetItemId(0));
  CreateAndAddItem(GetItemId(1));
  CreateAndAddItem(GetItemId(2));
  CreateAndAddItem(GetItemId(3));
  item_list_->SetItemPosition(item_list_->item_at(2),
                              item_list_->item_at(1)->position());
  EXPECT_TRUE(VerifyItemOrder4(0, 1, 2, 3));
  EXPECT_TRUE(item_list_->item_at(1)->position().Equals(
      item_list_->item_at(2)->position()));
  // Moving an item to position 1 should fix the position.
  observer_.ResetCounts();
  item_list_->MoveItem(0, 1);
  EXPECT_TRUE(VerifyItemOrder4(1, 0, 2, 3));
  EXPECT_TRUE(item_list_->item_at(0)->position().LessThan(
      item_list_->item_at(1)->position()));
  EXPECT_TRUE(item_list_->item_at(1)->position().LessThan(
      item_list_->item_at(2)->position()));
  EXPECT_TRUE(item_list_->item_at(2)->position().LessThan(
      item_list_->item_at(3)->position()));
  // One extra move notification for fixed position.
  EXPECT_EQ(2u, observer_.items_moved());

  // Restore the original order.
  item_list_->MoveItem(1, 0);
  EXPECT_TRUE(VerifyItemOrder4(0, 1, 2, 3));

  // Set all four items to the same position.
  item_list_->SetItemPosition(item_list_->item_at(1),
                              item_list_->item_at(0)->position());
  item_list_->SetItemPosition(item_list_->item_at(2),
                              item_list_->item_at(0)->position());
  item_list_->SetItemPosition(item_list_->item_at(3),
                              item_list_->item_at(0)->position());
  // Move should fix the position of the items.
  observer_.ResetCounts();
  item_list_->MoveItem(0, 1);
  EXPECT_TRUE(VerifyItemOrder4(1, 0, 2, 3));
  EXPECT_TRUE(item_list_->item_at(0)->position().LessThan(
      item_list_->item_at(1)->position()));
  EXPECT_TRUE(item_list_->item_at(1)->position().LessThan(
      item_list_->item_at(2)->position()));
  EXPECT_TRUE(item_list_->item_at(2)->position().LessThan(
      item_list_->item_at(3)->position()));

  // Three items' positions get updated: the one that is moved explicitly
  // through `AppListItemList::MoveItem()`; two items that are moved when
  // fixing positions.
  EXPECT_EQ(3u, observer_.items_moved());
}

TEST_F(AppListItemListTest, CreatePositionBefore) {
  CreateAndAddItem(GetItemId(0));
  syncer::StringOrdinal position0 = item_list_->item_at(0)->position();
  syncer::StringOrdinal new_position;
  new_position = CreatePositionBefore(position0.CreateBefore());
  EXPECT_TRUE(new_position.LessThan(position0));
  new_position = CreatePositionBefore(position0);
  EXPECT_TRUE(new_position.LessThan(position0));
  new_position = CreatePositionBefore(position0.CreateAfter());
  EXPECT_TRUE(new_position.GreaterThan(position0));

  CreateAndAddItem(GetItemId(1));
  syncer::StringOrdinal position1 = item_list_->item_at(1)->position();
  EXPECT_TRUE(position1.GreaterThan(position0));
  new_position = CreatePositionBefore(position1);
  EXPECT_TRUE(new_position.GreaterThan(position0));
  EXPECT_TRUE(new_position.LessThan(position1));

  // Invalid ordinal should return a position at the end of the list.
  new_position = CreatePositionBefore(syncer::StringOrdinal());
  EXPECT_TRUE(new_position.GreaterThan(position1));
}

TEST_F(AppListItemListTest, SetItemPosition) {
  CreateAndAddItem(GetItemId(0));
  CreateAndAddItem(GetItemId(1));
  CreateAndAddItem(GetItemId(2));
  CreateAndAddItem(GetItemId(3));
  EXPECT_TRUE(VerifyItemOrder4(0, 1, 2, 3));

  // No change to position.
  item_list_->SetItemPosition(item_list_->item_at(0),
                              item_list_->item_at(0)->position());
  EXPECT_TRUE(VerifyItemListOrdinals());
  EXPECT_TRUE(VerifyItemOrder4(0, 1, 2, 3));
  // No order change.
  item_list_->SetItemPosition(item_list_->item_at(0),
                              item_list_->item_at(0)->position().CreateBetween(
                                  item_list_->item_at(1)->position()));
  EXPECT_TRUE(VerifyItemListOrdinals());
  EXPECT_TRUE(VerifyItemOrder4(0, 1, 2, 3));
  // 0 -> 1
  item_list_->SetItemPosition(item_list_->item_at(0),
                              item_list_->item_at(1)->position().CreateBetween(
                                  item_list_->item_at(2)->position()));
  EXPECT_TRUE(VerifyItemListOrdinals());
  EXPECT_TRUE(VerifyItemOrder4(1, 0, 2, 3));
  // 1 -> 2
  item_list_->SetItemPosition(item_list_->item_at(1),
                              item_list_->item_at(2)->position().CreateBetween(
                                  item_list_->item_at(3)->position()));
  EXPECT_TRUE(VerifyItemListOrdinals());
  EXPECT_TRUE(VerifyItemOrder4(1, 2, 0, 3));
  // 0 -> last
  item_list_->SetItemPosition(item_list_->item_at(0),
                              item_list_->item_at(3)->position().CreateAfter());
  EXPECT_TRUE(VerifyItemListOrdinals());
  EXPECT_TRUE(VerifyItemOrder4(2, 0, 3, 1));
  // last -> last
  item_list_->SetItemPosition(item_list_->item_at(3),
                              item_list_->item_at(3)->position().CreateAfter());
  EXPECT_TRUE(VerifyItemListOrdinals());
  EXPECT_TRUE(VerifyItemOrder4(2, 0, 3, 1));
}

TEST_F(AppListItemListTest, MoveItemPastEnd) {
  CreateAndAddItem(GetItemId(0));
  CreateAndAddItem(GetItemId(1));
  CreateAndAddItem(GetItemId(2));
  CreateAndAddItem(GetItemId(3));
  EXPECT_TRUE(VerifyItemOrder4(0, 1, 2, 3));

  // Move the first item one step beyond the end of the list.
  item_list_->MoveItem(0, 4);

  // No crash, item moves to the end.
  EXPECT_TRUE(VerifyItemOrder4(1, 2, 3, 0));
}

}  // namespace ash
