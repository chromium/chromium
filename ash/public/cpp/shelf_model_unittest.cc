// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_model.h"

#include <set>
#include <string>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model_observer.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// ShelfModelObserver implementation that tracks what message are invoked.
class TestShelfModelObserver : public ShelfModelObserver {
 public:
  TestShelfModelObserver() = default;

  // Returns a string description of the changes that have occurred since this
  // was last invoked. Resets state to initial state.
  std::string StateStringAndClear() {
    std::string result;
    AddToResult("added=%d", added_count_, &result);
    AddToResult("removed=%d", removed_count_, &result);
    AddToResult("changed=%d", changed_count_, &result);
    AddToResult("moved=%d", moved_count_, &result);
    AddToResult("delegate_changed=%d", delegate_changed_count_, &result);
    added_count_ = removed_count_ = changed_count_ = moved_count_ =
        delegate_changed_count_ = 0;
    return result;
  }

  // ShelfModelObserver overrides:
  void ShelfItemAdded(int) override { added_count_++; }
  void ShelfItemRemoved(int, const ShelfItem&) override { removed_count_++; }
  void ShelfItemChanged(int, const ShelfItem&) override { changed_count_++; }
  void ShelfItemMoved(int, int) override { moved_count_++; }
  void ShelfItemDelegateChanged(const ShelfID&,
                                ShelfItemDelegate*,
                                ShelfItemDelegate*) override {
    delegate_changed_count_++;
  }

 private:
  void AddToResult(const std::string& format, int count, std::string* result) {
    if (!count)
      return;
    if (!result->empty())
      *result += " ";
    *result += base::StringPrintf(format.c_str(), count);
  }

  int added_count_ = 0;
  int removed_count_ = 0;
  int changed_count_ = 0;
  int moved_count_ = 0;
  int delegate_changed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestShelfModelObserver);
};

class TestShelfItemDelegate : public ShelfItemDelegate {
 public:
  TestShelfItemDelegate(const ShelfID& shelf_id)
      : ShelfItemDelegate(shelf_id) {}

  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback) override {}
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override {}
  void Close() override {}
};

}  // namespace

class ShelfModelTest : public testing::Test {
 public:
  ShelfModelTest() = default;
  ~ShelfModelTest() override = default;

  void SetUp() override {
    model_.reset(new ShelfModel);
    observer_.reset(new TestShelfModelObserver);
    model_->AddObserver(observer_.get());
  }

  void TearDown() override {
    observer_.reset();
    model_.reset();
  }

  std::unique_ptr<ShelfModel> model_;
  std::unique_ptr<TestShelfModelObserver> observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfModelTest);
};

TEST_F(ShelfModelTest, BasicAssertions) {
  // Add an item.
  ShelfItem item1;
  item1.id = ShelfID("item1");
  item1.type = TYPE_PINNED_APP;
  int index = model_->Add(item1);
  EXPECT_EQ(1, model_->item_count());
  EXPECT_LE(0, model_->ItemIndexByID(item1.id));
  EXPECT_NE(model_->items().end(), model_->ItemByID(item1.id));
  EXPECT_EQ("added=1", observer_->StateStringAndClear());

  // Change to a platform app item.
  item1.type = TYPE_APP;
  model_->Set(index, item1);
  EXPECT_EQ(item1.id, model_->items()[index].id);
  EXPECT_LE(0, model_->ItemIndexByID(item1.id));
  EXPECT_NE(model_->items().end(), model_->ItemByID(item1.id));
  EXPECT_EQ("changed=1", observer_->StateStringAndClear());
  EXPECT_EQ(TYPE_APP, model_->items()[index].type);

  // Remove the item.
  model_->RemoveItemAt(index);
  EXPECT_EQ(0, model_->item_count());
  EXPECT_EQ(-1, model_->ItemIndexByID(item1.id));
  EXPECT_EQ(model_->items().end(), model_->ItemByID(item1.id));
  EXPECT_EQ("removed=1", observer_->StateStringAndClear());

  // Add an app item.
  ShelfItem item2;
  item2.id = ShelfID("item2");
  item2.type = TYPE_PINNED_APP;
  index = model_->Add(item2);
  EXPECT_EQ(1, model_->item_count());
  EXPECT_LE(0, model_->ItemIndexByID(item2.id));
  EXPECT_NE(model_->items().end(), model_->ItemByID(item2.id));
  EXPECT_EQ("added=1", observer_->StateStringAndClear());

  // Change the item type.
  item2.type = TYPE_APP;
  model_->Set(index, item2);
  EXPECT_LE(0, model_->ItemIndexByID(item2.id));
  EXPECT_NE(model_->items().end(), model_->ItemByID(item2.id));
  EXPECT_EQ("changed=1", observer_->StateStringAndClear());
  EXPECT_EQ(TYPE_APP, model_->items()[index].type);

  // Add another item.
  ShelfItem item3;
  item3.id = ShelfID("item3");
  item3.type = TYPE_PINNED_APP;
  model_->Add(item3);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_LE(0, model_->ItemIndexByID(item3.id));
  EXPECT_NE(model_->items().end(), model_->ItemByID(item3.id));
  EXPECT_EQ("added=1", observer_->StateStringAndClear());

  // Move the second to the first.
  model_->Move(1, 0);
  EXPECT_EQ("moved=1", observer_->StateStringAndClear());

  // And back.
  model_->Move(0, 1);
  EXPECT_EQ("moved=1", observer_->StateStringAndClear());

  // Verifies all the items get unique ids.
  std::set<ShelfID> ids;
  for (int i = 0; i < model_->item_count(); ++i)
    ids.insert(model_->items()[i].id);
  EXPECT_EQ(model_->item_count(), static_cast<int>(ids.size()));
}

// Assertions around where items are added.
TEST_F(ShelfModelTest, AddIndices) {
  // Insert a browser shortcut, like Chrome does, it should be added at index 0.
  ShelfItem browser_shortcut;
  browser_shortcut.id = ShelfID("browser");
  browser_shortcut.type = TYPE_BROWSER_SHORTCUT;
  EXPECT_EQ(0, model_->Add(browser_shortcut));

  // App items should be after the browser shortcut.
  ShelfItem item;
  item.type = TYPE_APP;
  item.id = ShelfID("id1");
  int platform_app_index1 = model_->Add(item);
  EXPECT_EQ(1, platform_app_index1);

  // Add another platform app item, it should follow first.
  item.id = ShelfID("id2");
  int platform_app_index2 = model_->Add(item);
  EXPECT_EQ(2, platform_app_index2);

  // TYPE_PINNED_APP priority is higher than TYPE_APP but same as
  // TYPE_BROWSER_SHORTCUT. So TYPE_PINNED_APP is located after
  // TYPE_BROWSER_SHORTCUT.
  item.type = TYPE_PINNED_APP;
  item.id = ShelfID("id3");
  int app_shortcut_index1 = model_->Add(item);
  EXPECT_EQ(1, app_shortcut_index1);

  item.type = TYPE_PINNED_APP;
  item.id = ShelfID("id4");
  int app_shortcut_index2 = model_->Add(item);
  EXPECT_EQ(2, app_shortcut_index2);

  // Check that AddAt() figures out the correct indexes for app shortcuts.
  // TYPE_PINNED_APP and TYPE_BROWSER_SHORTCUT has the same weight.
  // So TYPE_PINNED_APP is located at index 0. And, TYPE_BROWSER_SHORTCUT is
  // located at index 1.
  item.type = TYPE_PINNED_APP;
  item.id = ShelfID("id5");
  int app_shortcut_index3 = model_->AddAt(0, item);
  EXPECT_EQ(0, app_shortcut_index3);

  item.type = TYPE_PINNED_APP;
  item.id = ShelfID("id6");
  int app_shortcut_index4 = model_->AddAt(5, item);
  EXPECT_EQ(4, app_shortcut_index4);

  item.type = TYPE_PINNED_APP;
  item.id = ShelfID("id7");
  int app_shortcut_index5 = model_->AddAt(1, item);
  EXPECT_EQ(1, app_shortcut_index5);

  // Check that AddAt() figures out the correct indexes for apps.
  item.type = TYPE_APP;
  item.id = ShelfID("id8");
  int platform_app_index3 = model_->AddAt(2, item);
  EXPECT_EQ(6, platform_app_index3);

  item.type = TYPE_APP;
  item.id = ShelfID("id9");
  int platform_app_index4 = model_->AddAt(6, item);
  EXPECT_EQ(6, platform_app_index4);

  EXPECT_EQ(TYPE_BROWSER_SHORTCUT, model_->items()[2].type);
}

// Test that the indexes for the running applications are properly determined.
TEST_F(ShelfModelTest, FirstRunningAppIndex) {
  // Insert the browser shortcut at index 0 and check that the running
  // application index would be behind it.
  ShelfItem item;
  item.id = ShelfID("browser");
  item.type = TYPE_BROWSER_SHORTCUT;
  EXPECT_EQ(0, model_->Add(item));
  EXPECT_EQ(1, model_->FirstRunningAppIndex());

  // Insert an application shortcut and make sure that the running application
  // index would be behind it.
  item.type = TYPE_PINNED_APP;
  item.id = ShelfID("pinned app");
  EXPECT_EQ(1, model_->Add(item));
  EXPECT_EQ(2, model_->FirstRunningAppIndex());

  // Insert a two app items and check the first running app index.
  item.type = TYPE_APP;
  item.id = ShelfID("app1");
  EXPECT_EQ(2, model_->Add(item));
  EXPECT_EQ(2, model_->FirstRunningAppIndex());
  item.id = ShelfID("app2");
  EXPECT_EQ(3, model_->Add(item));
  EXPECT_EQ(2, model_->FirstRunningAppIndex());
}

// Test item reordering on type/weight (eg. pinning) changes. crbug.com/248769.
TEST_F(ShelfModelTest, ReorderOnTypeChanges) {
  // Add three pinned items.
  ShelfItem item1;
  item1.type = TYPE_PINNED_APP;
  item1.id = ShelfID("id1");
  int app1_index = model_->Add(item1);
  EXPECT_EQ(0, app1_index);

  ShelfItem item2;
  item2.type = TYPE_PINNED_APP;
  item2.id = ShelfID("id2");
  int app2_index = model_->Add(item2);
  EXPECT_EQ(1, app2_index);

  ShelfItem item3;
  item3.type = TYPE_PINNED_APP;
  item3.id = ShelfID("id3");
  int app3_index = model_->Add(item3);
  EXPECT_EQ(2, app3_index);

  // Unpinning an item moves it behind the shortcuts.
  EXPECT_EQ(item3.id, model_->items()[2].id);
  item2.type = TYPE_APP;
  model_->Set(app2_index, item2);
  EXPECT_EQ(item2.id, model_->items()[2].id);
}

// Test getting the index of ShelfIDs as a check for item presence.
TEST_F(ShelfModelTest, ItemIndexByID) {
  // Expect empty and unknown ids to return the invalid index -1.
  EXPECT_EQ(-1, model_->ItemIndexByID(ShelfID()));
  EXPECT_EQ(-1, model_->ItemIndexByID(ShelfID("foo")));
  EXPECT_EQ(-1, model_->ItemIndexByID(ShelfID("foo", "bar")));

  // Add an item and expect to get a valid index for its id.
  ShelfItem item1;
  item1.type = TYPE_PINNED_APP;
  item1.id = ShelfID("app_id1", "launch_id1");
  const int index1 = model_->Add(item1);
  EXPECT_EQ(index1, model_->ItemIndexByID(item1.id));

  // Add another item and expect to get another valid index for its id.
  ShelfItem item2;
  item2.type = TYPE_APP;
  item2.id = ShelfID("app_id2", "launch_id2");
  const int index2 = model_->Add(item2);
  EXPECT_EQ(index2, model_->ItemIndexByID(item2.id));

  // Removing the first item should yield an invalid index for that item.
  model_->RemoveItemAt(index1);
  EXPECT_EQ(-1, model_->ItemIndexByID(item1.id));
  // The index of the second item should be decremented, but still valid.
  EXPECT_EQ(index2 - 1, model_->ItemIndexByID(item2.id));
  EXPECT_LE(0, model_->ItemIndexByID(item2.id));
}

// Test pinning and unpinning a closed app, and checking if it is pinned.
TEST_F(ShelfModelTest, ClosedAppPinning) {
  const std::string app_id("app_id");

  // Check the initial state.
  EXPECT_FALSE(model_->IsAppPinned(app_id));
  EXPECT_EQ(0, model_->item_count());

  // Pinning a previously unknown app should add an item.
  model_->PinAppWithID(app_id);
  EXPECT_TRUE(model_->IsAppPinned(app_id));
  EXPECT_EQ(1, model_->item_count());
  EXPECT_EQ(TYPE_PINNED_APP, model_->items()[0].type);
  EXPECT_EQ(app_id, model_->items()[0].id.app_id);

  // Pinning the same app id again should have no change.
  model_->PinAppWithID(app_id);
  EXPECT_TRUE(model_->IsAppPinned(app_id));
  EXPECT_EQ(1, model_->item_count());
  EXPECT_EQ(TYPE_PINNED_APP, model_->items()[0].type);
  EXPECT_EQ(app_id, model_->items()[0].id.app_id);

  // Unpinning the app should remove the item.
  model_->UnpinAppWithID(app_id);
  EXPECT_FALSE(model_->IsAppPinned(app_id));
  EXPECT_EQ(0, model_->item_count());

  // Unpinning the same app id again should have no change.
  model_->UnpinAppWithID(app_id);
  EXPECT_FALSE(model_->IsAppPinned(app_id));
  EXPECT_EQ(0, model_->item_count());
}

// Test pinning and unpinning a running app, and checking if it is pinned.
TEST_F(ShelfModelTest, RunningAppPinning) {
  const std::string app_id("app_id");

  // Check the initial state.
  EXPECT_FALSE(model_->IsAppPinned(app_id));
  EXPECT_EQ(0, model_->item_count());

  // Add an example running app.
  ShelfItem item;
  item.type = TYPE_APP;
  item.status = STATUS_RUNNING;
  item.id = ShelfID(app_id);
  const int index = model_->Add(item);

  // The item should be added but not pinned.
  EXPECT_FALSE(model_->IsAppPinned(app_id));
  EXPECT_EQ(1, model_->item_count());
  EXPECT_EQ(TYPE_APP, model_->items()[index].type);
  EXPECT_EQ(item.id, model_->items()[index].id);

  // Pinning the item should just change its type.
  model_->PinAppWithID(app_id);
  EXPECT_TRUE(model_->IsAppPinned(app_id));
  EXPECT_EQ(1, model_->item_count());
  EXPECT_EQ(TYPE_PINNED_APP, model_->items()[index].type);
  EXPECT_EQ(item.id, model_->items()[index].id);

  // Pinning the same app id again should have no change.
  model_->PinAppWithID(app_id);
  EXPECT_TRUE(model_->IsAppPinned(app_id));
  EXPECT_EQ(1, model_->item_count());
  EXPECT_EQ(TYPE_PINNED_APP, model_->items()[index].type);
  EXPECT_EQ(item.id, model_->items()[index].id);

  // Unpinning the app should leave the item unpinnned but running.
  model_->UnpinAppWithID(app_id);
  EXPECT_FALSE(model_->IsAppPinned(app_id));
  EXPECT_EQ(1, model_->item_count());
  EXPECT_EQ(TYPE_APP, model_->items()[index].type);
  EXPECT_EQ(item.id, model_->items()[index].id);

  // Unpinning the same app id again should have no change.
  model_->UnpinAppWithID(app_id);
  EXPECT_FALSE(model_->IsAppPinned(app_id));
  EXPECT_EQ(1, model_->item_count());
  EXPECT_EQ(TYPE_APP, model_->items()[index].type);
  EXPECT_EQ(item.id, model_->items()[index].id);
}

// Tests that apps are updated properly when notifications are added or removed.
TEST_F(ShelfModelTest, AddRemoveNotification) {
  const std::string app_id("app_id");
  const std::string notification_id("notification_id");

  // Add an example running app.
  ShelfItem item;
  item.type = TYPE_APP;
  item.status = STATUS_RUNNING;
  item.id = ShelfID(app_id);
  const int index = model_->Add(item);

  EXPECT_FALSE(model_->items()[index].has_notification);

  // Add a notification for the app.
  model_->AddNotificationRecord(app_id, notification_id);

  EXPECT_TRUE(model_->items()[index].has_notification);

  // Remove the notification.
  model_->RemoveNotificationRecord(notification_id);

  EXPECT_FALSE(model_->items()[index].has_notification);
}

// Tests that apps pick up their notifications when they are added.
TEST_F(ShelfModelTest, AddAppAfterNotification) {
  const std::string app_id("app_id");
  const std::string notification_id("notification_id");

  ShelfItem item;
  item.type = TYPE_APP;
  item.status = STATUS_RUNNING;
  item.id = ShelfID(app_id);

  // Add a notification for the app.
  model_->AddNotificationRecord(app_id, notification_id);

  // Add an app with a matching app id.
  const int index = model_->Add(item);

  EXPECT_TRUE(model_->items()[index].has_notification);

  // Remove and re-add the app.
  model_->RemoveItemAt(index);
  EXPECT_EQ(index, model_->Add(item));

  // Test that the notification persists.
  EXPECT_TRUE(model_->items()[index].has_notification);
}

// Tests that pinned apps pick up their notifications if they were recieved
// before the app existed on the shelf.
TEST_F(ShelfModelTest, PinAppAfterNotification) {
  const std::string app_id("app_id");
  const std::string notification_id("notification_id");

  // Add an example app, but don't pin it.
  ShelfItem item;
  item.type = TYPE_APP;
  item.status = STATUS_RUNNING;
  item.id = ShelfID(app_id);

  // Add a notification for the app.
  model_->AddNotificationRecord(app_id, notification_id);

  // Pin the app after the notification posts.
  model_->PinAppWithID(app_id);

  const int index = model_->ItemIndexByAppID(app_id);
  EXPECT_TRUE(model_->items()[index].has_notification);

  // Un-pin and re-pin the app.
  model_->UnpinAppWithID(app_id);
  model_->PinAppWithID(app_id);
  EXPECT_EQ(index, model_->ItemIndexByAppID(app_id));

  // Test that the notification persists.
  EXPECT_TRUE(model_->items()[index].has_notification);
}

// Tests that the ShelfItem.has_notification is set to false only when there are
// 0 notifications.
TEST_F(ShelfModelTest, MultipleNotificationsPerAppBasic) {
  const std::string app_id("app_id");
  const std::string notification_id_0("notification_id_0");
  // Add an example app.
  ShelfItem item;
  item.type = TYPE_APP;
  item.status = STATUS_RUNNING;
  item.id = ShelfID(app_id);
  const int index = model_->Add(item);
  EXPECT_EQ(index, model_->ItemIndexByAppID(app_id));

  // Add the first notification for this app.
  model_->AddNotificationRecord(app_id, notification_id_0);
  // Add the second notification on the same app.
  const std::string notification_id_1("notification_id_1");
  model_->AddNotificationRecord(app_id, notification_id_1);
  EXPECT_TRUE(model_->items()[index].has_notification);

  // Remove one notification.
  model_->RemoveNotificationRecord(notification_id_1);
  EXPECT_TRUE(model_->items()[index].has_notification);

  // Remove the last notification.
  model_->RemoveNotificationRecord(notification_id_0);
  EXPECT_FALSE(model_->items()[index].has_notification);
}

// Test that RemoveItemAndTakeShelfItemDelegate has the same effect as
// RemoveItemAt and returns the correct delegate.
TEST_F(ShelfModelTest, RemoveItemAndTakeShelfItemDelegate) {
  // Add an item.
  ShelfItem item1;
  item1.id = ShelfID("item1");
  item1.type = TYPE_PINNED_APP;
  model_->Add(item1);
  EXPECT_EQ(1, model_->item_count());
  EXPECT_LE(0, model_->ItemIndexByID(item1.id));
  EXPECT_NE(model_->items().end(), model_->ItemByID(item1.id));
  EXPECT_EQ("added=1", observer_->StateStringAndClear());

  // Set item delegate.
  auto* delegate = new TestShelfItemDelegate(item1.id);
  model_->SetShelfItemDelegate(item1.id,
                               std::unique_ptr<ShelfItemDelegate>(delegate));
  EXPECT_EQ("delegate_changed=1", observer_->StateStringAndClear());

  // Remove the item.
  auto taken_delegate = model_->RemoveItemAndTakeShelfItemDelegate(item1.id);
  EXPECT_EQ(0, model_->item_count());
  EXPECT_EQ(-1, model_->ItemIndexByID(item1.id));
  EXPECT_EQ(model_->items().end(), model_->ItemByID(item1.id));
  EXPECT_EQ("removed=1", observer_->StateStringAndClear());
  EXPECT_EQ(delegate, taken_delegate.get());
}

}  // namespace ash
