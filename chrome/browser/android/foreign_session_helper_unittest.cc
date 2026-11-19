// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/foreign_session_helper.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/fake_open_tabs_ui_delegate.h"
#include "components/sync_sessions/fake_session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using ::testing::ElementsAre;

sessions::SerializedNavigationEntry MakeNavigation(
    const std::string& url_spec) {
  sessions::SerializedNavigationEntry entry;
  entry.set_virtual_url(GURL(url_spec));
  return entry;
}

std::unique_ptr<sessions::SessionTab> MakeTab(
    const std::vector<std::string>& urls,
    int current_index = -1) {
  auto tab = std::make_unique<sessions::SessionTab>();
  for (const auto& url : urls) {
    tab->navigations.push_back(MakeNavigation(url));
  }
  tab->current_navigation_index = current_index;
  return tab;
}

std::unique_ptr<sessions::SessionWindow> MakeWindow(
    std::vector<std::unique_ptr<sessions::SessionTab>> tabs) {
  auto window = std::make_unique<sessions::SessionWindow>();
  window->tabs = std::move(tabs);
  return window;
}

std::unique_ptr<sync_sessions::SyncedSession> MakeSession(
    std::vector<std::unique_ptr<sessions::SessionWindow>> windows) {
  auto session = std::make_unique<sync_sessions::SyncedSession>();
  int window_id = 1;
  for (auto& window : windows) {
    auto session_window =
        std::make_unique<sync_sessions::SyncedSessionWindow>();
    session_window->wrapped_window.tabs = std::move(window->tabs);
    session->windows[SessionID::FromSerializedValue(window_id++)] =
        std::move(session_window);
  }
  return session;
}

TEST(ForeignSessionHelperTest, ShouldSkipTabEmptyNavigations) {
  sessions::SessionTab empty_tab;
  EXPECT_TRUE(ForeignSessionHelper::ShouldSkipTab(empty_tab));
}

TEST(ForeignSessionHelperTest, ShouldSkipTabEmptyUrl) {
  auto tab = MakeTab({""});
  EXPECT_TRUE(ForeignSessionHelper::ShouldSkipTab(*tab));
}

TEST(ForeignSessionHelperTest, ShouldSkipTabInvalidUrl) {
  auto tab = MakeTab({"invalid.url"});
  EXPECT_TRUE(ForeignSessionHelper::ShouldSkipTab(*tab));
}

TEST(ForeignSessionHelperTest, ShouldSkipTabUnsyncableSchemes) {
  // WebUI URLs
  EXPECT_TRUE(ForeignSessionHelper::ShouldSkipTab(
      *MakeTab({chrome::kChromeUIVersionURL})));
  EXPECT_TRUE(
      ForeignSessionHelper::ShouldSkipTab(*MakeTab({"chrome://newtab"})));
  EXPECT_TRUE(
      ForeignSessionHelper::ShouldSkipTab(*MakeTab({"chrome://history"})));

  // Chrome native scheme
  EXPECT_TRUE(ForeignSessionHelper::ShouldSkipTab(
      *MakeTab({"chrome-native://newtab"})));
  EXPECT_TRUE(ForeignSessionHelper::ShouldSkipTab(
      *MakeTab({"chrome-native://bookmarks"})));

  // File scheme
  EXPECT_TRUE(ForeignSessionHelper::ShouldSkipTab(
      *MakeTab({"file:///sdcard/download/page.html"})));

  // Distiller scheme
  EXPECT_TRUE(ForeignSessionHelper::ShouldSkipTab(
      *MakeTab({"chrome-distiller://article"})));

  // Untrusted WebUI
  EXPECT_TRUE(ForeignSessionHelper::ShouldSkipTab(
      *MakeTab({"chrome-untrusted://terminal"})));
}

TEST(ForeignSessionHelperTest, ShouldSkipTabSyncableUrls) {
  // HTTP / HTTPS
  EXPECT_FALSE(ForeignSessionHelper::ShouldSkipTab(
      *MakeTab({"http://www.example.com"})));
  EXPECT_FALSE(ForeignSessionHelper::ShouldSkipTab(
      *MakeTab({"https://www.google.com/search?q=test"})));

  // Other schemes syncable in SessionSyncService
  EXPECT_FALSE(
      ForeignSessionHelper::ShouldSkipTab(*MakeTab({"other://anything"})));
  EXPECT_FALSE(ForeignSessionHelper::ShouldSkipTab(
      *MakeTab({"chrome-other://anything"})));
}

TEST(ForeignSessionHelperTest, ShouldSkipTabMultiNavigationHistory) {
  // Common scenario: Tab was opened from chrome-native://newtab, then navigated
  // to google.com. Current navigation (index 1) is valid and syncable.
  {
    auto tab = MakeTab({"chrome-native://newtab", "https://www.google.com"},
                       /*current_index=*/1);
    EXPECT_FALSE(ForeignSessionHelper::ShouldSkipTab(*tab));
  }

  // Common scenario: Tab was opened from chrome://newtab, then navigated to
  // example.com. Current navigation (index 1) is valid and syncable.
  {
    auto tab = MakeTab({"chrome://newtab", "https://example.com"},
                       /*current_index=*/1);
    EXPECT_FALSE(ForeignSessionHelper::ShouldSkipTab(*tab));
  }

  // Navigation index beyond bounds (e.g. 100) clamps to the last entry
  // (index 1).
  {
    auto tab = MakeTab({"chrome-native://newtab", "https://www.google.com"},
                       /*current_index=*/100);
    EXPECT_FALSE(ForeignSessionHelper::ShouldSkipTab(*tab));
  }

  // Navigation index -1 clamps to index 0 (which is chrome-native://newtab and
  // thus skipped).
  {
    auto tab = MakeTab({"chrome-native://newtab", "https://www.google.com"},
                       /*current_index=*/-1);
    EXPECT_TRUE(ForeignSessionHelper::ShouldSkipTab(*tab));
  }

  // Tab navigated from valid URL to an unsyncable URL (current navigation is
  // index 1).
  {
    auto tab = MakeTab({"https://www.google.com", "chrome://newtab"},
                       /*current_index=*/1);
    EXPECT_TRUE(ForeignSessionHelper::ShouldSkipTab(*tab));
  }

  // Tab navigated from valid URL to an unsyncable URL, but the user navigated
  // back (current navigation is index 0).
  {
    auto tab = MakeTab({"https://www.google.com", "chrome://newtab"},
                       /*current_index=*/0);
    EXPECT_FALSE(ForeignSessionHelper::ShouldSkipTab(*tab));
  }
}

TEST(ForeignSessionHelperTest, ShouldSkipWindow) {
  // Empty window
  {
    auto window = MakeWindow({});
    EXPECT_TRUE(ForeignSessionHelper::ShouldSkipWindow(*window));
  }

  // Window with only unsyncable tabs
  {
    std::vector<std::unique_ptr<sessions::SessionTab>> tabs;
    tabs.push_back(MakeTab({"chrome://newtab"}));
    tabs.push_back(MakeTab({"chrome-native://newtab"}));
    tabs.push_back(MakeTab({"file:///path/test.html"}));
    auto window = MakeWindow(std::move(tabs));
    EXPECT_TRUE(ForeignSessionHelper::ShouldSkipWindow(*window));
  }

  // Window with all syncable tabs
  {
    std::vector<std::unique_ptr<sessions::SessionTab>> tabs;
    tabs.push_back(MakeTab({"https://www.google.com"}));
    tabs.push_back(MakeTab({"http://www.example.com"}));
    auto window = MakeWindow(std::move(tabs));
    EXPECT_FALSE(ForeignSessionHelper::ShouldSkipWindow(*window));
  }

  // Window with mixed tabs (at least one syncable tab should keep the window)
  {
    std::vector<std::unique_ptr<sessions::SessionTab>> tabs;
    tabs.push_back(MakeTab({"chrome://newtab"}));
    tabs.push_back(MakeTab({"https://www.google.com"}));
    tabs.push_back(MakeTab({"file:///path/test.html"}));
    auto window = MakeWindow(std::move(tabs));
    EXPECT_FALSE(ForeignSessionHelper::ShouldSkipWindow(*window));
  }
}

TEST(ForeignSessionHelperTest, ShouldSkipSession) {
  // Empty session
  {
    auto session = MakeSession({});
    EXPECT_TRUE(ForeignSessionHelper::ShouldSkipSession(*session));
  }

  // Session where all windows have only unsyncable tabs
  {
    std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
    std::vector<std::unique_ptr<sessions::SessionTab>> tabs1;
    tabs1.push_back(MakeTab({"chrome://newtab"}));
    windows.push_back(MakeWindow(std::move(tabs1)));

    std::vector<std::unique_ptr<sessions::SessionTab>> tabs2;
    tabs2.push_back(MakeTab({"chrome-native://newtab"}));
    windows.push_back(MakeWindow(std::move(tabs2)));

    auto session = MakeSession(std::move(windows));
    EXPECT_TRUE(ForeignSessionHelper::ShouldSkipSession(*session));
  }

  // Session with one valid window
  {
    std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
    std::vector<std::unique_ptr<sessions::SessionTab>> tabs;
    tabs.push_back(MakeTab({"https://www.google.com"}));
    windows.push_back(MakeWindow(std::move(tabs)));

    auto session = MakeSession(std::move(windows));
    EXPECT_FALSE(ForeignSessionHelper::ShouldSkipSession(*session));
  }

  // Session with one skipped window and one valid window
  {
    std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
    std::vector<std::unique_ptr<sessions::SessionTab>> tabs1;
    tabs1.push_back(MakeTab({"chrome://newtab"}));
    windows.push_back(MakeWindow(std::move(tabs1)));

    std::vector<std::unique_ptr<sessions::SessionTab>> tabs2;
    tabs2.push_back(MakeTab({"https://www.google.com"}));
    windows.push_back(MakeWindow(std::move(tabs2)));

    auto session = MakeSession(std::move(windows));
    EXPECT_FALSE(ForeignSessionHelper::ShouldSkipSession(*session));
  }
}

constexpr int kAnchorTabId = 100;
constexpr int kUnknownSessionTabId = 999;
constexpr char kSessionTag[] = "session_tag";
constexpr char kUrl1[] = "https://www.example.com/1";
constexpr char kUrl2[] = "https://www.example.com/2";

class ForeignSessionHelperRestoreTest : public ChromeRenderViewHostTestHarness {
 protected:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        SessionSyncServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<sync_sessions::FakeSessionSyncService>();
        })}};
  }

  // Adds a synced tab with `url` and returns its session tab ID.
  int32_t AddForeignTab(const char* url) {
    auto* service = static_cast<sync_sessions::FakeSessionSyncService*>(
        SessionSyncServiceFactory::GetForProfile(profile()));
    return service->GetOpenTabsUIDelegate()
        ->AddTabToForeignSession(kSessionTag, GURL(url))
        ->tab_id.id();
  }

  std::vector<int32_t> Restore(TabAndroid* anchor_tab,
                               const std::vector<int32_t>& session_tab_ids,
                               bool all_in_background = false) {
    ForeignSessionHelper helper(profile());
    return helper.OpenForeignSessionTabsAsBackgroundTabs(
        anchor_tab, session_tab_ids, kSessionTag, all_in_background);
  }
};

TEST_F(ForeignSessionHelperRestoreTest, EmptyInputReturnsEmpty) {
  OwningTestTabModel tab_model(profile());
  TabAndroid* anchor_tab = tab_model.AddEmptyTab(0);
  EXPECT_TRUE(Restore(anchor_tab, {}).empty());
}

TEST_F(ForeignSessionHelperRestoreTest, NullAnchorReturnsAllInvalid) {
  EXPECT_THAT(Restore(nullptr, {AddForeignTab(kUrl1)}),
              ElementsAre(TabAndroid::kInvalidTabId));
}

TEST_F(ForeignSessionHelperRestoreTest, AnchorOutsideTabModelReturnsInvalid) {
  std::unique_ptr<TabAndroid> anchor_tab = TabAndroid::CreateForTesting(
      profile(), kAnchorTabId, CreateTestWebContents());
  EXPECT_THAT(Restore(anchor_tab.get(), {AddForeignTab(kUrl1)}),
              ElementsAre(TabAndroid::kInvalidTabId));
}

TEST_F(ForeignSessionHelperRestoreTest, ReturnsRestoredTabIdsInOrder) {
  OwningTestTabModel tab_model(profile());
  TabAndroid* anchor_tab = tab_model.AddEmptyTab(0);
  int32_t first = AddForeignTab(kUrl1);
  int32_t second = AddForeignTab(kUrl2);

  std::vector<int32_t> ids =
      Restore(anchor_tab, {first, kUnknownSessionTabId, second});

  ASSERT_EQ(3, tab_model.GetTabCount());
  EXPECT_THAT(ids, ElementsAre(tab_model.GetTabAt(1)->GetAndroidId(),
                               TabAndroid::kInvalidTabId,
                               tab_model.GetTabAt(2)->GetAndroidId()));
}

TEST_F(ForeignSessionHelperRestoreTest, AllInBackgroundReturnsIdsInOrder) {
  OwningTestTabModel tab_model(profile());
  TabAndroid* anchor_tab = tab_model.AddEmptyTab(0);
  int32_t first = AddForeignTab(kUrl1);
  int32_t second = AddForeignTab(kUrl2);

  std::vector<int32_t> ids =
      Restore(anchor_tab, {first, kUnknownSessionTabId, second},
              /*all_in_background=*/true);

  ASSERT_EQ(3, tab_model.GetTabCount());
  EXPECT_THAT(ids, ElementsAre(tab_model.GetTabAt(1)->GetAndroidId(),
                               TabAndroid::kInvalidTabId,
                               tab_model.GetTabAt(2)->GetAndroidId()));
}

TEST_F(ForeignSessionHelperRestoreTest, AllInBackgroundKeepsAnchorSelected) {
  OwningTestTabModel tab_model(profile());
  TabAndroid* anchor_tab = tab_model.AddEmptyTab(0, /*select=*/true);

  Restore(anchor_tab, {AddForeignTab(kUrl1), AddForeignTab(kUrl2)},
          /*all_in_background=*/true);

  EXPECT_EQ(anchor_tab, tab_model.GetTabAt(0));
  EXPECT_EQ(0, tab_model.GetActiveIndex());
}

TEST_F(ForeignSessionHelperRestoreTest, CreateTabFailureReturnsInvalid) {
  std::unique_ptr<TabAndroid> anchor_tab = TabAndroid::CreateForTesting(
      profile(), kAnchorTabId, CreateTestWebContents());
  // TestTabModel::CreateTab() always fails.
  TestTabModel tab_model(profile());
  tab_model.SetWebContentsList({anchor_tab->web_contents()});
  TabModelList::AddTabModel(&tab_model);

  EXPECT_THAT(Restore(anchor_tab.get(), {AddForeignTab(kUrl1)}),
              ElementsAre(TabAndroid::kInvalidTabId));
  TabModelList::RemoveTabModel(&tab_model);
}

TEST_F(ForeignSessionHelperRestoreTest,
       CreateTabFailureInBackgroundReturnsInvalid) {
  std::unique_ptr<TabAndroid> anchor_tab = TabAndroid::CreateForTesting(
      profile(), kAnchorTabId, CreateTestWebContents());
  // TestTabModel::CreateTab() always fails.
  TestTabModel tab_model(profile());
  tab_model.SetWebContentsList({anchor_tab->web_contents()});
  TabModelList::AddTabModel(&tab_model);

  EXPECT_THAT(Restore(anchor_tab.get(), {AddForeignTab(kUrl1)},
                      /*all_in_background=*/true),
              ElementsAre(TabAndroid::kInvalidTabId));
  TabModelList::RemoveTabModel(&tab_model);
}

TEST_F(ForeignSessionHelperRestoreTest, CrossProfileAnchorReturnsInvalid) {
  OwningTestTabModel tab_model(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  TabAndroid* anchor_tab = tab_model.AddEmptyTab(0);

  EXPECT_THAT(Restore(anchor_tab, {AddForeignTab(kUrl1)}),
              ElementsAre(TabAndroid::kInvalidTabId));
  EXPECT_EQ(1, tab_model.GetTabCount());
}

}  // namespace
