// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/recently_closed_tabs_bridge.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/android/recently_closed_tabs_bridge.h"
#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_impl.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
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

// ----- RecentlyClosedTabsBridge TEST HELPERS -----

// Setup required test environment and TabRestoreService.
class RecentlyClosedTabsBridgeTest : public ChromeRenderViewHostTestHarness {
 protected:
  raw_ptr<sessions::TabRestoreService> tab_restore_service_ = nullptr;
  raw_ptr<recent_tabs::RecentlyClosedTabsBridge> bridge_ = nullptr;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Create tab_restore_service and override the TabRestoreServiceFactory.
    CreateService();
    ASSERT_TRUE(tab_restore_service_);

    bridge_ = new RecentlyClosedTabsBridge(nullptr, profile());
  }

  void TearDown() override {
    if (bridge_) {
      bridge_->Destroy(nullptr);
      bridge_ = nullptr;
    }
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void CreateService() {
    // Override the TabRestoreServiceFactory to use a custom testing instance.
    TabRestoreServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context) {
          Profile* profile = Profile::FromBrowserContext(context);
          auto service = std::make_unique<sessions::TabRestoreServiceImpl>(
              std::make_unique<ChromeTabRestoreServiceClient>(profile),
              profile->GetPrefs(), nullptr);

          return std::unique_ptr<KeyedService>(std::move(service));
        }));

    // Retrieve and cache the raw pointer for use in the test class.
    tab_restore_service_ = TabRestoreServiceFactory::GetForProfile(profile());
    EXPECT_TRUE(tab_restore_service_);
  }

  std::optional<SessionID> AddHistoricalEntries(int index) {
    sessions::LiveTab* live_tab =
        sessions::ContentLiveTab::GetOrCreateForWebContents(web_contents());
    EXPECT_TRUE(live_tab);
    return tab_restore_service_->CreateHistoricalTab(live_tab, index);
  }

  void NavigateToNonEmptyPage() {
    NavigateAndCommit(GURL("https://google.com"));
    auto* entry = web_contents()->GetController().GetLastCommittedEntry();
    ASSERT_TRUE(entry);
    ASSERT_FALSE(entry->GetURL().is_empty());
  }
};

// ----- RecentlyClosedTabsBridge TESTS BEGIN -----

// Verify ClearAllRecentlyUsedClosedEntries clears all TabRestoreService entries.
TEST_F(RecentlyClosedTabsBridgeTest, ClearAllRecentlyUsedClosedEntries) {
  // Create 3 entries.
  NavigateToNonEmptyPage();
  AddHistoricalEntries(0);
  AddHistoricalEntries(1);
  AddHistoricalEntries(2);

  // Verify there are 3 entries in the TabRestoreService.
  EXPECT_EQ(tab_restore_service_->entries().size(), 3U);

  // Trigger clear all entries.
  bridge_->ClearRecentlyClosedEntries(nullptr);

  // Verify the entries in the TabRestoreService are cleared.
  EXPECT_EQ(tab_restore_service_->entries().size(), 0U);
}

// Verify that ClearLeastRecentlyUsedClosedEntries removes the specified number of least
// recently used entries.
TEST_F(RecentlyClosedTabsBridgeTest, ClearLeastRecentlyUsedClosedEntries) {
  // Create 3 entries.
  NavigateToNonEmptyPage();
  AddHistoricalEntries(0);
  AddHistoricalEntries(1);
  std::optional<SessionID> sessionId = AddHistoricalEntries(2);
  ASSERT_TRUE(sessionId.has_value());

  // Verify there are 3 entries in the TabRestoreService.
  EXPECT_EQ(tab_restore_service_->entries().size(), 3U);

  // Trigger clear 2 least recently used entries.
  bridge_->ClearLeastRecentlyUsedClosedEntries(nullptr, 2);

  // Verify only 1 entry remaining in the TabRestoreService.
  EXPECT_EQ(tab_restore_service_->entries().size(), 1U);

  // Verify the remaining entry matches the ID of the most recently added entry.
  EXPECT_EQ(tab_restore_service_->entries().front()->id.id(), sessionId->id());
}

// Verify that ClearLeastRecentlyUsedClosedEntries removes all recently closed
// entries when the specified size is bigger than the entries size.
TEST_F(RecentlyClosedTabsBridgeTest,
       ClearLeastRecentlyUsedClosedEntries_ClearAll) {
  // Create 3 entries.
  NavigateToNonEmptyPage();
  AddHistoricalEntries(0);
  AddHistoricalEntries(1);
  std::optional<SessionID> sessionId = AddHistoricalEntries(2);
  ASSERT_TRUE(sessionId.has_value());

  // Verify there are 3 entries in the TabRestoreService.
  EXPECT_EQ(tab_restore_service_->entries().size(), 3U);

  // Trigger clear 4 least recently used entries.
  bridge_->ClearLeastRecentlyUsedClosedEntries(nullptr, 4);

  // Verify the entries in the TabRestoreService are cleared.
  EXPECT_EQ(tab_restore_service_->entries().size(), 0U);
}

}  // namespace
}  // namespace recent_tabs
