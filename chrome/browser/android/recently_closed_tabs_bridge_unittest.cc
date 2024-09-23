// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/recently_closed_tabs_bridge.h"

#include "components/sessions/core/tab_restore_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace recent_tabs {
namespace {

// ----- TabIterator TEST HELPERS -----

// Create a new tab entry with `tabstrip_index` of `tab_counter` and increment
// `tab_counter`.
std::unique_ptr<sessions::tab_restore::Tab> MakeTab(int* tab_counter) {
  auto tab = std::make_unique<sessions::tab_restore::Tab>();
  tab->tabstrip_index = (*tab_counter)++;
  return tab;
}

// Add a single tab to `entries`.
void AddTab(sessions::TabRestoreService::Entries& entries, int* tab_counter) {
  entries.push_back(MakeTab(tab_counter));
}

// Add `tab_count` tabs as a group entry with `title` to `entries`.
void AddGroupWithTabs(sessions::TabRestoreService::Entries& entries,
                      const std::u16string& title,
                      int tab_count,
                      int* tab_counter) {
  entries.push_back(std::make_unique<sessions::tab_restore::Group>());
  auto* group =
      static_cast<sessions::tab_restore::Group*>(entries.back().get());
  group->visual_data = tab_groups::TabGroupVisualData(title, 0);
  for (int i = 0; i < tab_count; ++i) {
    group->tabs.push_back(MakeTab(tab_counter));
  }
}

// Add `tab_count` tabs as a window entry with `user_title` to `entries`.
void AddWindowWithTabs(sessions::TabRestoreService::Entries& entries,
                       const std::string& user_title,
                       int tab_count,
                       int* tab_counter) {
  entries.push_back(std::make_unique<sessions::tab_restore::Window>());
  auto* window =
      static_cast<sessions::tab_restore::Window*>(entries.back().get());
  window->user_title = user_title;
  for (int i = 0; i < tab_count; ++i) {
    window->tabs.push_back(MakeTab(tab_counter));
  }
}

// ----- TabIterator TESTS BEGIN -----

// Test iteration over empty set.
TEST(RecentlyClosedTabsBridge_TabIterator, Empty) {
  sessions::TabRestoreService::Entries entries;
  ASSERT_EQ(TabIterator::begin(entries), TabIterator::end(entries));
}

// Test iteration over tab entries.
TEST(RecentlyClosedTabsBridge_TabIterator, TabsOnly) {
  sessions::TabRestoreService::Entries entries;
  int tab_counter = 0;
  AddTab(entries, &tab_counter);
  AddTab(entries, &tab_counter);
  AddTab(entries, &tab_counter);
  AddTab(entries, &tab_counter);

  int tab_count = 0;
  auto it = TabIterator::begin(entries);
  for (; it != TabIterator::end(entries); ++it) {
    EXPECT_TRUE(it.IsCurrentEntryTab());
    EXPECT_EQ(tab_count++, it->tabstrip_index);
  }
  EXPECT_EQ(tab_count, 4);
  ASSERT_EQ(it, TabIterator::end(entries));
}

// Test iteration over all entry types.
TEST(RecentlyClosedTabsBridge_TabIterator, AllEntryTypes) {
  sessions::TabRestoreService::Entries entries;
  int tab_counter = 0;
  AddGroupWithTabs(entries, u"foo", 2, &tab_counter);
  AddTab(entries, &tab_counter);
  AddWindowWithTabs(entries, "bar", 3, &tab_counter);
  AddTab(entries, &tab_counter);

  // Group 2 tabs.
  auto it = TabIterator::begin(entries);
  ASSERT_NE(it, TabIterator::end(entries));
  EXPECT_FALSE(it.IsCurrentEntryTab());
  auto entry = it.CurrentEntry();
  EXPECT_EQ(sessions::tab_restore::Type::GROUP, (*entry)->type);
  EXPECT_EQ(
      u"foo",
      static_cast<sessions::tab_restore::Group&>(**entry).visual_data.title());
  EXPECT_EQ(1, it->tabstrip_index);
  ++it;
  ASSERT_NE(it, TabIterator::end(entries));
  EXPECT_FALSE(it.IsCurrentEntryTab());
  EXPECT_EQ(entry, it.CurrentEntry());
  EXPECT_EQ(0, it->tabstrip_index);
  ++it;

  // Tab
  ASSERT_NE(it, TabIterator::end(entries));
  EXPECT_TRUE(it.IsCurrentEntryTab());
  EXPECT_EQ(2, it->tabstrip_index);
  ++it;

  // Window 3 tabs.
  ASSERT_NE(it, TabIterator::end(entries));
  EXPECT_FALSE(it.IsCurrentEntryTab());
  entry = it.CurrentEntry();
  EXPECT_EQ(sessions::tab_restore::Type::WINDOW, (*entry)->type);
  EXPECT_EQ("bar",
            static_cast<sessions::tab_restore::Window&>(**entry).user_title);
  EXPECT_EQ(5, it->tabstrip_index);
  ++it;
  ASSERT_NE(it, TabIterator::end(entries));
  EXPECT_FALSE(it.IsCurrentEntryTab());
  EXPECT_EQ(entry, it.CurrentEntry());
  EXPECT_EQ(4, it->tabstrip_index);
  ++it;
  ASSERT_NE(it, TabIterator::end(entries));
  EXPECT_FALSE(it.IsCurrentEntryTab());
  EXPECT_EQ(entry, it.CurrentEntry());
  EXPECT_EQ(3, it->tabstrip_index);
  ++it;

  // Tab
  ASSERT_NE(it, TabIterator::end(entries));
  EXPECT_TRUE(it.IsCurrentEntryTab());
  EXPECT_EQ(6, it->tabstrip_index);
  ++it;

  ASSERT_EQ(it, TabIterator::end(entries));
}

// Test iteration over entries including an empty group.
TEST(RecentlyClosedTabsBridge_TabIterator, EmptyGroup) {
  sessions::TabRestoreService::Entries entries;
  int tab_counter = 0;
  AddTab(entries, &tab_counter);
  AddGroupWithTabs(entries, u"foo", 0, &tab_counter);
  AddTab(entries, &tab_counter);

  auto it = TabIterator::begin(entries);
  ASSERT_NE(it, TabIterator::end(entries));
  EXPECT_TRUE(it.IsCurrentEntryTab());
  EXPECT_EQ(0, it->tabstrip_index);
  ++it;
  // Group with 0 tabs is skipped.

  ASSERT_NE(it, TabIterator::end(entries));
  EXPECT_TRUE(it.IsCurrentEntryTab());
  EXPECT_EQ(1, it->tabstrip_index);
  ++it;

  ASSERT_EQ(it, TabIterator::end(entries));
}

// Test iteration over entries including an empty window.
TEST(RecentlyClosedTabsBridge_TabIterator, EmptyWindow) {
  sessions::TabRestoreService::Entries entries;
  int tab_counter = 0;
  AddTab(entries, &tab_counter);
  AddWindowWithTabs(entries, "foo", 0, &tab_counter);
  AddTab(entries, &tab_counter);

  auto it = TabIterator::begin(entries);
  ASSERT_NE(it, TabIterator::end(entries));
  EXPECT_TRUE(it.IsCurrentEntryTab());
  EXPECT_EQ(0, it->tabstrip_index);
  ++it;
  // Window with 0 tabs is skipped.

  ASSERT_NE(it, TabIterator::end(entries));
  EXPECT_TRUE(it.IsCurrentEntryTab());
  EXPECT_EQ(1, it->tabstrip_index);
  ++it;

  ASSERT_EQ(it, TabIterator::end(entries));
}

// Test iteration over entries when the first few entries are empty.
TEST(RecentlyClosedTabsBridge_TabIterator, EmptyFirstEntries) {
  sessions::TabRestoreService::Entries entries;
  int tab_counter = 0;
  AddGroupWithTabs(entries, u"foo", 0, &tab_counter);
  AddGroupWithTabs(entries, u"bar", 0, &tab_counter);
  AddWindowWithTabs(entries, "baz", 0, &tab_counter);
  AddTab(entries, &tab_counter);

  // Group with 0 tabs is skipped.
  auto it = TabIterator::begin(entries);
  ASSERT_NE(it, TabIterator::end(entries));
  EXPECT_TRUE(it.IsCurrentEntryTab());
  EXPECT_EQ(0, it->tabstrip_index);
  ++it;

  ASSERT_EQ(it, TabIterator::end(entries));
}

}  // namespace
}  // namespace recent_tabs
