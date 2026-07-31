// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/auto_todos/in_memory_auto_todos_store.h"

#include <vector>

#include "base/containers/span.h"
#include "base/test/test_future.h"
#include "chrome/browser/context_hub/auto_todos/auto_todo_entry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace context_hub {

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::SizeIs;

class MockStoreObserver : public AutoTodosStore::Observer {
 public:
  MOCK_METHOD(void,
              OnAutoTodosChanged,
              (base::span<const AutoTodoEntry>),
              (override));
};

class InMemoryAutoTodosStoreTest : public ::testing::Test {
 protected:
  InMemoryAutoTodosStore store_;
};

TEST_F(InMemoryAutoTodosStoreTest, AddAndGetSingleItem) {
  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  store_.GetAllItems(get_future.GetCallback());
  EXPECT_TRUE(get_future.Get().empty());

  AutoTodoEntry item;
  item.id = "todo_1";
  item.data =
      FirstPartyData{.source_references = {GURL(
                         "https://mail.google.com/123")},
                     .actionable_url = GURL("https://example.com/checkin")};

  base::test::TestFuture<bool> add_future;
  store_.AddOrUpdateItem(item, add_future.GetCallback());
  EXPECT_TRUE(add_future.Get());

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future2;
  store_.GetAllItems(get_future2.GetCallback());
  auto items = get_future2.Get();
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0].id, "todo_1");
  EXPECT_EQ(items[0].status, AutoTodoEntry::Status::kActive);
  EXPECT_TRUE(items[0].is_first_party());
  EXPECT_FALSE(items[0].is_third_party());
}

TEST_F(InMemoryAutoTodosStoreTest, AddItemGeneratesIdIfEmpty) {
  AutoTodoEntry item;
  item.title = "No ID Todo";

  base::test::TestFuture<bool> add_future;
  store_.AddOrUpdateItem(item, add_future.GetCallback());
  EXPECT_TRUE(add_future.Get());

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  store_.GetAllItems(get_future.GetCallback());
  auto items = get_future.Get();
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0].id, "item_1");
  EXPECT_EQ(items[0].title, "No ID Todo");
}

TEST_F(InMemoryAutoTodosStoreTest, DeleteItem) {
  AutoTodoEntry item;
  item.id = "del_1";

  base::test::TestFuture<bool> add_future;
  store_.AddOrUpdateItem(item, add_future.GetCallback());
  EXPECT_TRUE(add_future.Get());

  base::test::TestFuture<bool> del_future;
  store_.DeleteItem("del_1", del_future.GetCallback());
  EXPECT_TRUE(del_future.Get());

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  store_.GetAllItems(get_future.GetCallback());
  EXPECT_TRUE(get_future.Get().empty());

  base::test::TestFuture<bool> del_fail_future;
  store_.DeleteItem("del_1", del_fail_future.GetCallback());
  EXPECT_FALSE(del_fail_future.Get());
}

TEST_F(InMemoryAutoTodosStoreTest, DeleteItemByTabId) {
  AutoTodoEntry third_party_item;
  third_party_item.id = "tp_1";
  third_party_item.data =
      ThirdPartyData{.tab_id = 123};

  base::test::TestFuture<bool> add_future1;
  store_.AddOrUpdateItem(third_party_item, add_future1.GetCallback());
  EXPECT_TRUE(add_future1.Get());

  base::test::TestFuture<bool> del_tab_future;
  store_.DeleteItemByTabId(123, del_tab_future.GetCallback());
  EXPECT_TRUE(del_tab_future.Get());

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  store_.GetAllItems(get_future.GetCallback());
  auto items = get_future.Get();
  EXPECT_TRUE(get_future.Get().empty());

  base::test::TestFuture<bool> del_tab_fail_future;
  store_.DeleteItemByTabId(123, del_tab_fail_future.GetCallback());
  EXPECT_FALSE(del_tab_fail_future.Get());
}

TEST_F(InMemoryAutoTodosStoreTest, ClearStore) {
  AutoTodoEntry item1;
  item1.id = "1";
  AutoTodoEntry item2;
  item2.id = "2";

  base::test::TestFuture<bool> add_future1, add_future2;
  store_.AddOrUpdateItem(item1, add_future1.GetCallback());
  EXPECT_TRUE(add_future1.Get());
  store_.AddOrUpdateItem(item2, add_future2.GetCallback());
  EXPECT_TRUE(add_future2.Get());

  base::test::TestFuture<void> clear_future;
  store_.Clear(clear_future.GetCallback());
  EXPECT_TRUE(clear_future.Wait());

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  store_.GetAllItems(get_future.GetCallback());
  EXPECT_TRUE(get_future.Get().empty());
}

TEST_F(InMemoryAutoTodosStoreTest, ObserverNotifiedOnAdd) {
  MockStoreObserver observer;
  store_.AddObserver(&observer);

  AutoTodoEntry item;
  item.id = "todo_1";

  EXPECT_CALL(observer, OnAutoTodosChanged(SizeIs(1)));
  base::test::TestFuture<bool> add_future;
  store_.AddOrUpdateItem(item, add_future.GetCallback());
  EXPECT_TRUE(add_future.Get());
}

TEST_F(InMemoryAutoTodosStoreTest, ObserverNotifiedOnDelete) {
  AutoTodoEntry item;
  item.id = "todo_1";
  store_.AddOrUpdateItem(item, base::DoNothing());

  MockStoreObserver observer;
  store_.AddObserver(&observer);

  EXPECT_CALL(observer, OnAutoTodosChanged(IsEmpty()));
  base::test::TestFuture<bool> del_future;
  store_.DeleteItem("todo_1", del_future.GetCallback());
  EXPECT_TRUE(del_future.Get());

  // Delete non-existent item should not notify.
  EXPECT_CALL(observer, OnAutoTodosChanged(_)).Times(0);
  base::test::TestFuture<bool> del_fail_future;
  store_.DeleteItem("non_existent", del_fail_future.GetCallback());
  EXPECT_FALSE(del_fail_future.Get());
}

TEST_F(InMemoryAutoTodosStoreTest, ObserverNotifiedOnDeleteByTabId) {
  AutoTodoEntry tp_item;
  tp_item.id = "tp_1";
  tp_item.data = ThirdPartyData{.tab_id = 42};
  store_.AddOrUpdateItem(tp_item, base::DoNothing());

  MockStoreObserver observer;
  store_.AddObserver(&observer);

  EXPECT_CALL(observer, OnAutoTodosChanged(IsEmpty()));
  base::test::TestFuture<bool> del_tab_future;
  store_.DeleteItemByTabId(42, del_tab_future.GetCallback());
  EXPECT_TRUE(del_tab_future.Get());

  // Delete non-existent tab ID should not notify.
  EXPECT_CALL(observer, OnAutoTodosChanged(_)).Times(0);
  base::test::TestFuture<bool> del_tab_fail_future;
  store_.DeleteItemByTabId(999, del_tab_fail_future.GetCallback());
  EXPECT_FALSE(del_tab_fail_future.Get());
}

TEST_F(InMemoryAutoTodosStoreTest, ObserverNotifiedOnClear) {
  AutoTodoEntry item;
  item.id = "todo_1";
  store_.AddOrUpdateItem(item, base::DoNothing());

  MockStoreObserver observer;
  store_.AddObserver(&observer);

  EXPECT_CALL(observer, OnAutoTodosChanged(IsEmpty()));
  base::test::TestFuture<void> clear_future;
  store_.Clear(clear_future.GetCallback());
  EXPECT_TRUE(clear_future.Wait());
}

TEST_F(InMemoryAutoTodosStoreTest, ObserverNotNotifiedAfterRemoval) {
  MockStoreObserver observer;
  store_.AddObserver(&observer);
  store_.RemoveObserver(&observer);

  AutoTodoEntry item;
  item.id = "todo_1";

  EXPECT_CALL(observer, OnAutoTodosChanged(_)).Times(0);
  store_.AddOrUpdateItem(item, base::DoNothing());
}

}  // namespace context_hub
