// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/atomic_flag.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/token.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/content/content_test_helper.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_types.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_state.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

using content::NavigationEntry;
using sessions::ContentTestHelper;
using sessions::SerializedNavigationEntry;
using sessions::SerializedNavigationEntryTestHelper;

class SessionServiceTest : public BrowserWithTestWindowTest {
 public:
  SessionServiceTest() : window_bounds(0, 1, 2, 3) {}

 protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    std::string b = base::NumberToString(base::Time::Now().ToInternalValue());
    TestingProfile* profile = profile_manager()->CreateTestingProfile(b);
    SessionService* session_service = new SessionService(profile);
    path_ = profile->GetPath();

    helper_.SetService(session_service);

    service()->SetWindowType(window_id, Browser::TYPE_NORMAL);
    service()->SetWindowBounds(window_id,
                               window_bounds,
                               ui::SHOW_STATE_NORMAL);
    service()->SetWindowWorkspace(window_id, window_workspace);
  }

  void TearDown() override {
    helper_.SetService(NULL);
    BrowserWithTestWindowTest::TearDown();
  }

  void UpdateNavigation(
      const SessionID& window_id,
      const SessionID& tab_id,
      const SerializedNavigationEntry& navigation,
      bool select) {
    service()->UpdateTabNavigation(window_id, tab_id, navigation);
    if (select) {
      service()->SetSelectedNavigationIndex(
          window_id, tab_id, navigation.index());
    }
  }

  SessionID CreateTabWithTestNavigationData(SessionID window_id,
                                            int visual_index) {
    const SessionID tab_id = SessionID::NewUnique();
    const SerializedNavigationEntry nav =
        SerializedNavigationEntryTestHelper::CreateNavigationForTest();
    helper_.PrepareTabInWindow(window_id, tab_id, visual_index, true);
    UpdateNavigation(window_id, tab_id, nav, true);
    return tab_id;
  }

  void ReadWindows(
      std::vector<std::unique_ptr<sessions::SessionWindow>>* windows,
      SessionID* active_window_id) {
    // Forces closing the file.
    helper_.SetService(NULL);

    SessionService* session_service = new SessionService(path_);
    helper_.SetService(session_service);

    SessionID* non_null_active_window_id = active_window_id;
    SessionID dummy_active_window_id = SessionID::InvalidValue();
    if (!non_null_active_window_id)
      non_null_active_window_id = &dummy_active_window_id;
    helper_.ReadWindows(windows, non_null_active_window_id);
  }

  // Configures the session service with one window with one tab and a single
  // navigation. If |pinned_state| is true or |write_always| is true, the
  // pinned state of the tab is updated. The session service is then recreated
  // and the pinned state of the read back tab is returned.
  bool CreateAndWriteSessionWithOneTab(bool pinned_state, bool write_always) {
    SessionID tab_id = SessionID::NewUnique();
    SerializedNavigationEntry nav1 =
        ContentTestHelper::CreateNavigation("http://google.com", "abc");

    helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
    UpdateNavigation(window_id, tab_id, nav1, true);

    if (pinned_state || write_always)
      helper_.service()->SetPinnedState(window_id, tab_id, pinned_state);

    std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
    ReadWindows(&windows, NULL);

    EXPECT_EQ(1U, windows.size());
    if (HasFatalFailure())
      return false;
    EXPECT_EQ(1U, windows[0]->tabs.size());
    if (HasFatalFailure())
      return false;

    sessions::SessionTab* tab = windows[0]->tabs[0].get();
    helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);

    return tab->pinned;
  }

  void CreateAndWriteSessionWithTwoWindows(
      const SessionID& window2_id,
      const SessionID& tab1_id,
      const SessionID& tab2_id,
      SerializedNavigationEntry* nav1,
      SerializedNavigationEntry* nav2) {
    *nav1 = ContentTestHelper::CreateNavigation("http://google.com", "abc");
    *nav2 = ContentTestHelper::CreateNavigation("http://google2.com", "abcd");

    helper_.PrepareTabInWindow(window_id, tab1_id, 0, true);
    UpdateNavigation(window_id, tab1_id, *nav1, true);

    const gfx::Rect window2_bounds(3, 4, 5, 6);
    service()->SetWindowType(window2_id, Browser::TYPE_NORMAL);
    service()->SetWindowBounds(window2_id,
                               window2_bounds,
                               ui::SHOW_STATE_MAXIMIZED);
    helper_.PrepareTabInWindow(window2_id, tab2_id, 0, true);
    UpdateNavigation(window2_id, tab2_id, *nav2, true);
  }

  SessionService* service() { return helper_.service(); }

  const gfx::Rect window_bounds;

  const std::string window_workspace = "abc";

  const SessionID window_id = SessionID::NewUnique();

  // Path used in testing.
  base::ScopedTempDir temp_dir_;
  base::FilePath path_;

  SessionServiceTestHelper helper_;
};

TEST_F(SessionServiceTest, Basic) {
  SessionID tab_id = SessionID::NewUnique();
  ASSERT_NE(window_id, tab_id);

  SerializedNavigationEntry nav1 =
      ContentTestHelper::CreateNavigation("http://google.com", "abc");
  SerializedNavigationEntryTestHelper::SetOriginalRequestURL(
      GURL("http://original.request.com"), &nav1);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  ASSERT_EQ(1U, windows.size());
  ASSERT_TRUE(window_bounds == windows[0]->bounds);
  ASSERT_EQ(window_workspace, windows[0]->workspace);
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id, windows[0]->window_id);
  ASSERT_EQ(1U, windows[0]->tabs.size());
  ASSERT_EQ(sessions::SessionWindow::TYPE_NORMAL, windows[0]->type);

  sessions::SessionTab* tab = windows[0]->tabs[0].get();
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);

  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

// Make sure we persist post entries.
TEST_F(SessionServiceTest, PersistPostData) {
  SessionID tab_id = SessionID::NewUnique();
  ASSERT_NE(window_id, tab_id);

  SerializedNavigationEntry nav1 =
      ContentTestHelper::CreateNavigation("http://google.com", "abc");
  SerializedNavigationEntryTestHelper::SetHasPostData(true, &nav1);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  helper_.AssertSingleWindowWithSingleTab(windows, 1);
}

TEST_F(SessionServiceTest, ClosingTabStaysClosed) {
  SessionID tab_id = SessionID::NewUnique();
  SessionID tab2_id = SessionID::NewUnique();
  ASSERT_NE(tab_id, tab2_id);

  SerializedNavigationEntry nav1 =
      ContentTestHelper::CreateNavigation("http://google.com", "abc");
  SerializedNavigationEntry nav2 =
      ContentTestHelper::CreateNavigation("http://google2.com", "abcd");

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  helper_.PrepareTabInWindow(window_id, tab2_id, 1, false);
  UpdateNavigation(window_id, tab2_id, nav2, true);
  service()->TabClosed(window_id, tab2_id, false);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id, windows[0]->window_id);
  ASSERT_EQ(1U, windows[0]->tabs.size());

  sessions::SessionTab* tab = windows[0]->tabs[0].get();
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);

  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

TEST_F(SessionServiceTest, Pruning) {
  SessionID tab_id = SessionID::NewUnique();

  SerializedNavigationEntry nav1 =
      ContentTestHelper::CreateNavigation("http://google.com", "abc");
  SerializedNavigationEntry nav2 =
      ContentTestHelper::CreateNavigation("http://google2.com", "abcd");

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  for (int i = 0; i < 6; ++i) {
    SerializedNavigationEntry* nav = (i % 2) == 0 ? &nav1 : &nav2;
    nav->set_index(i);
    UpdateNavigation(window_id, tab_id, *nav, true);
  }

  // Set available range for testing.
  helper_.SetAvailableRange(tab_id, std::pair<int, int>(0, 5));

  service()->TabNavigationPathPruned(window_id, tab_id, 3 /* index */,
                                     3 /* count */);

  std::pair<int, int> available_range;
  EXPECT_TRUE(helper_.GetAvailableRange(tab_id, &available_range));
  EXPECT_EQ(0, available_range.first);
  EXPECT_EQ(2, available_range.second);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(1U, windows[0]->tabs.size());

  sessions::SessionTab* tab = windows[0]->tabs[0].get();
  // We left the selected index at 5, then pruned. When rereading the
  // index should get reset to last valid navigation, which is 2.
  helper_.AssertTabEquals(window_id, tab_id, 0, 2, 3, *tab);

  ASSERT_EQ(3u, tab->navigations.size());
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
  helper_.AssertNavigationEquals(nav2, tab->navigations[1]);
  helper_.AssertNavigationEquals(nav1, tab->navigations[2]);
}

TEST_F(SessionServiceTest, TwoWindows) {
  SessionID window2_id = SessionID::NewUnique();
  SessionID tab1_id = SessionID::NewUnique();
  SessionID tab2_id = SessionID::NewUnique();
  SerializedNavigationEntry nav1;
  SerializedNavigationEntry nav2;

  CreateAndWriteSessionWithTwoWindows(
      window2_id, tab1_id, tab2_id, &nav1, &nav2);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  ASSERT_EQ(2U, windows.size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(0, windows[1]->selected_tab_index);
  ASSERT_EQ(1U, windows[0]->tabs.size());
  ASSERT_EQ(1U, windows[1]->tabs.size());

  sessions::SessionTab* rt1;
  sessions::SessionTab* rt2;
  if (windows[0]->window_id == window_id) {
    ASSERT_EQ(window2_id, windows[1]->window_id);
    ASSERT_EQ(ui::SHOW_STATE_NORMAL, windows[0]->show_state);
    ASSERT_EQ(ui::SHOW_STATE_MAXIMIZED, windows[1]->show_state);
    rt1 = windows[0]->tabs[0].get();
    rt2 = windows[1]->tabs[0].get();
  } else {
    ASSERT_EQ(window2_id, windows[0]->window_id);
    ASSERT_EQ(window_id, windows[1]->window_id);
    ASSERT_EQ(ui::SHOW_STATE_MAXIMIZED, windows[0]->show_state);
    ASSERT_EQ(ui::SHOW_STATE_NORMAL, windows[1]->show_state);
    rt1 = windows[1]->tabs[0].get();
    rt2 = windows[0]->tabs[0].get();
  }
  sessions::SessionTab* tab = rt1;
  helper_.AssertTabEquals(window_id, tab1_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);

  tab = rt2;
  helper_.AssertTabEquals(window2_id, tab2_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav2, tab->navigations[0]);
}

TEST_F(SessionServiceTest, WindowWithNoTabsGetsPruned) {
  SessionID window2_id = SessionID::NewUnique();
  SessionID tab1_id = SessionID::NewUnique();
  SessionID tab2_id = SessionID::NewUnique();

  SerializedNavigationEntry nav1 =
      ContentTestHelper::CreateNavigation("http://google.com", "abc");

  helper_.PrepareTabInWindow(window_id, tab1_id, 0, true);
  UpdateNavigation(window_id, tab1_id, nav1, true);

  const gfx::Rect window2_bounds(3, 4, 5, 6);
  service()->SetWindowType(window2_id, Browser::TYPE_NORMAL);
  service()->SetWindowBounds(window2_id,
                             window2_bounds,
                             ui::SHOW_STATE_NORMAL);
  helper_.PrepareTabInWindow(window2_id, tab2_id, 0, true);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(1U, windows[0]->tabs.size());
  ASSERT_EQ(window_id, windows[0]->window_id);

  sessions::SessionTab* tab = windows[0]->tabs[0].get();
  helper_.AssertTabEquals(window_id, tab1_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

TEST_F(SessionServiceTest, ClosingWindowDoesntCloseTabs) {
  SessionID tab_id = SessionID::NewUnique();
  SessionID tab2_id = SessionID::NewUnique();
  ASSERT_NE(tab_id, tab2_id);

  SerializedNavigationEntry nav1 =
      ContentTestHelper::CreateNavigation("http://google.com", "abc");
  SerializedNavigationEntry nav2 =
      ContentTestHelper::CreateNavigation("http://google2.com", "abcd");

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  helper_.PrepareTabInWindow(window_id, tab2_id, 1, false);
  UpdateNavigation(window_id, tab2_id, nav2, true);

  service()->WindowClosing(window_id);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id, windows[0]->window_id);
  ASSERT_EQ(2U, windows[0]->tabs.size());

  sessions::SessionTab* tab = windows[0]->tabs[0].get();
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);

  tab = windows[0]->tabs[1].get();
  helper_.AssertTabEquals(window_id, tab2_id, 1, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav2, tab->navigations[0]);
}

TEST_F(SessionServiceTest, LockingWindowRemembersAll) {
  SessionID window2_id = SessionID::NewUnique();
  SessionID tab1_id = SessionID::NewUnique();
  SessionID tab2_id = SessionID::NewUnique();
  SerializedNavigationEntry nav1;
  SerializedNavigationEntry nav2;

  CreateAndWriteSessionWithTwoWindows(
      window2_id, tab1_id, tab2_id, &nav1, &nav2);

  ASSERT_TRUE(service()->profile());
  ProfileManager* manager = g_browser_process->profile_manager();
  ASSERT_TRUE(manager);
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(manager->GetProfileAttributesStorage().
      GetProfileAttributesWithPath(service()->profile()->GetPath(), &entry));
  entry->SetIsSigninRequired(true);

  service()->WindowClosing(window_id);
  service()->WindowClosed(window_id);
  service()->WindowClosing(window2_id);
  service()->WindowClosed(window2_id);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  ASSERT_EQ(2U, windows.size());
  ASSERT_EQ(1U, windows[0]->tabs.size());
  ASSERT_EQ(1U, windows[1]->tabs.size());
}

TEST_F(SessionServiceTest, WindowCloseCommittedAfterNavigate) {
  SessionID window2_id = SessionID::NewUnique();
  SessionID tab_id = SessionID::NewUnique();
  SessionID tab2_id = SessionID::NewUnique();
  ASSERT_NE(window2_id, window_id);

  service()->SetWindowType(window2_id, Browser::TYPE_NORMAL);
  service()->SetWindowBounds(window2_id,
                             window_bounds,
                             ui::SHOW_STATE_NORMAL);

  SerializedNavigationEntry nav1 =
      ContentTestHelper::CreateNavigation("http://google.com", "abc");
  SerializedNavigationEntry nav2 =
      ContentTestHelper::CreateNavigation("http://google2.com", "abcd");

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  helper_.PrepareTabInWindow(window2_id, tab2_id, 0, false);
  UpdateNavigation(window2_id, tab2_id, nav2, true);

  service()->WindowClosing(window2_id);
  service()->TabClosed(window2_id, tab2_id, false);
  service()->WindowClosed(window2_id);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id, windows[0]->window_id);
  ASSERT_EQ(1U, windows[0]->tabs.size());

  sessions::SessionTab* tab = windows[0]->tabs[0].get();
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

TEST_F(SessionServiceTest, RemoveUnusedRestoreWindowsTest) {
  std::vector<std::unique_ptr<sessions::SessionWindow>> windows_list;
  windows_list.push_back(std::make_unique<sessions::SessionWindow>());
  windows_list.back()->type = sessions::SessionWindow::TYPE_NORMAL;
  windows_list.push_back(std::make_unique<sessions::SessionWindow>());
  windows_list.back()->type = sessions::SessionWindow::TYPE_DEVTOOLS;

  service()->RemoveUnusedRestoreWindows(&windows_list);
  ASSERT_EQ(1U, windows_list.size());
  EXPECT_EQ(sessions::SessionWindow::TYPE_NORMAL, windows_list[0]->type);
}

#if defined (OS_CHROMEOS)
// Makes sure we track apps. Only applicable on chromeos.
TEST_F(SessionServiceTest, RestoreApp) {
  SessionID window2_id = SessionID::NewUnique();
  SessionID tab_id = SessionID::NewUnique();
  SessionID tab2_id = SessionID::NewUnique();
  ASSERT_NE(window2_id, window_id);

  service()->SetWindowType(window2_id, Browser::TYPE_APP);
  service()->SetWindowBounds(window2_id,
                             window_bounds,
                             ui::SHOW_STATE_NORMAL);
  service()->SetWindowAppName(window2_id, "TestApp");

  SerializedNavigationEntry nav1 =
      ContentTestHelper::CreateNavigation("http://google.com", "abc");
  SerializedNavigationEntry nav2 =
      ContentTestHelper::CreateNavigation("http://google2.com", "abcd");

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  helper_.PrepareTabInWindow(window2_id, tab2_id, 0, false);
  UpdateNavigation(window2_id, tab2_id, nav2, true);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  ASSERT_EQ(2U, windows.size());
  int tabbed_index =
      windows[0]->type == sessions::SessionWindow::TYPE_NORMAL ? 0 : 1;
  int app_index = tabbed_index == 0 ? 1 : 0;
  ASSERT_EQ(0, windows[tabbed_index]->selected_tab_index);
  ASSERT_EQ(window_id, windows[tabbed_index]->window_id);
  ASSERT_EQ(1U, windows[tabbed_index]->tabs.size());

  sessions::SessionTab* tab = windows[tabbed_index]->tabs[0].get();
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);

  ASSERT_EQ(0, windows[app_index]->selected_tab_index);
  ASSERT_EQ(window2_id, windows[app_index]->window_id);
  ASSERT_EQ(1U, windows[app_index]->tabs.size());
  ASSERT_TRUE(windows[app_index]->type == sessions::SessionWindow::TYPE_APP);
  ASSERT_EQ("TestApp", windows[app_index]->app_name);

  tab = windows[app_index]->tabs[0].get();
  helper_.AssertTabEquals(window2_id, tab2_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav2, tab->navigations[0]);
}

// Don't track Crostini apps. Only applicable on Chrome OS.
TEST_F(SessionServiceTest, IgnoreCrostiniApps) {
  SessionID window2_id = SessionID::NewUnique();
  ASSERT_NE(window2_id, window_id);

  service()->SetWindowType(window2_id, Browser::TYPE_APP);
  service()->SetWindowBounds(window2_id, window_bounds, ui::SHOW_STATE_NORMAL);
  service()->SetWindowAppName(window2_id, "_crostini_fakeappid");

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, nullptr);

  for (auto& window : windows)
    ASSERT_NE(window2_id, window->window_id);
}

#endif  // defined (OS_CHROMEOS)

// Tests pruning from the front.
TEST_F(SessionServiceTest, PruneFromFront) {
  const std::string base_url("http://google.com/");
  SessionID tab_id = SessionID::NewUnique();

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);

  // Add 5 navigations, with the 4th selected.
  for (int i = 0; i < 5; ++i) {
    SerializedNavigationEntry nav = ContentTestHelper::CreateNavigation(
        base_url + base::NumberToString(i), "a");
    nav.set_index(i);
    UpdateNavigation(window_id, tab_id, nav, (i == 3));
  }

  // Set available range for testing.
  helper_.SetAvailableRange(tab_id, std::pair<int, int>(0, 4));

  // Prune the first two navigations from the front.
  helper_.service()->TabNavigationPathPruned(window_id, tab_id, 0 /* index */,
                                             2 /* count */);

  std::pair<int, int> available_range;
  EXPECT_TRUE(helper_.GetAvailableRange(tab_id, &available_range));
  EXPECT_EQ(0, available_range.first);
  EXPECT_EQ(2, available_range.second);

  // Read back in.
  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id, windows[0]->window_id);
  ASSERT_EQ(1U, windows[0]->tabs.size());

  // There shouldn't be an app id.
  EXPECT_TRUE(windows[0]->tabs[0]->extension_app_id.empty());

  // We should be left with three navigations, the 2nd selected.
  sessions::SessionTab* tab = windows[0]->tabs[0].get();
  ASSERT_EQ(1, tab->current_navigation_index);
  EXPECT_EQ(3U, tab->navigations.size());
  EXPECT_TRUE(GURL(base_url + base::NumberToString(2)) ==
              tab->navigations[0].virtual_url());
  EXPECT_TRUE(GURL(base_url + base::NumberToString(3)) ==
              tab->navigations[1].virtual_url());
  EXPECT_TRUE(GURL(base_url + base::NumberToString(4)) ==
              tab->navigations[2].virtual_url());
}

// Tests pruning from the middle.
TEST_F(SessionServiceTest, PruneFromMiddle) {
  const std::string base_url("http://google.com/");
  SessionID tab_id = SessionID::NewUnique();

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);

  // Add 5 navigations, with the 4th selected.
  for (int i = 0; i < 5; ++i) {
    SerializedNavigationEntry nav = ContentTestHelper::CreateNavigation(
        base_url + base::NumberToString(i), "a");
    nav.set_index(i);
    UpdateNavigation(window_id, tab_id, nav, (i == 3));
  }

  // Set available range for testing.
  helper_.SetAvailableRange(tab_id, std::pair<int, int>(0, 4));

  // Prune two navigations starting from second.
  helper_.service()->TabNavigationPathPruned(window_id, tab_id, 1 /* index */,
                                             2 /* count */);

  std::pair<int, int> available_range;
  EXPECT_TRUE(helper_.GetAvailableRange(tab_id, &available_range));
  EXPECT_EQ(0, available_range.first);
  EXPECT_EQ(2, available_range.second);

  // Read back in.
  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, nullptr);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id, windows[0]->window_id);
  ASSERT_EQ(1U, windows[0]->tabs.size());

  // There shouldn't be an app id.
  EXPECT_TRUE(windows[0]->tabs[0]->extension_app_id.empty());

  // We should be left with three navigations, the 2nd selected.
  sessions::SessionTab* tab = windows[0]->tabs[0].get();
  ASSERT_EQ(1, tab->current_navigation_index);
  EXPECT_EQ(3U, tab->navigations.size());
  EXPECT_EQ(GURL(base_url + base::NumberToString(0)),
            tab->navigations[0].virtual_url());
  EXPECT_EQ(GURL(base_url + base::NumberToString(3)),
            tab->navigations[1].virtual_url());
  EXPECT_EQ(GURL(base_url + base::NumberToString(4)),
            tab->navigations[2].virtual_url());
}

// Tests possible computations of available ranges.
TEST_F(SessionServiceTest, AvailableRanges) {
  const std::string base_url("http://google.com/");
  SessionID tab_id = SessionID::NewUnique();

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);

  // Set available range to a subset for testing.
  helper_.SetAvailableRange(tab_id, std::pair<int, int>(4, 7));

  // 1. Test when range starts after the pruned entries.
  helper_.service()->TabNavigationPathPruned(window_id, tab_id, 1 /* index */,
                                             2 /* count */);

  std::pair<int, int> available_range;
  EXPECT_TRUE(helper_.GetAvailableRange(tab_id, &available_range));
  EXPECT_EQ(2, available_range.first);
  EXPECT_EQ(5, available_range.second);

  // Set back available range.
  helper_.SetAvailableRange(tab_id, std::pair<int, int>(4, 7));

  // 2. Test when range is before the pruned entries.
  helper_.service()->TabNavigationPathPruned(window_id, tab_id, 8 /* index */,
                                             2 /* count */);
  EXPECT_TRUE(helper_.GetAvailableRange(tab_id, &available_range));
  EXPECT_EQ(4, available_range.first);
  EXPECT_EQ(7, available_range.second);

  // Set back available range.
  helper_.SetAvailableRange(tab_id, std::pair<int, int>(4, 7));

  // 3. Test when range is within the pruned entries.
  helper_.service()->TabNavigationPathPruned(window_id, tab_id, 3 /* index */,
                                             5 /* count */);
  EXPECT_TRUE(helper_.GetAvailableRange(tab_id, &available_range));
  EXPECT_EQ(0, available_range.first);
  EXPECT_EQ(0, available_range.second);

  // Set back available range.
  helper_.SetAvailableRange(tab_id, std::pair<int, int>(4, 7));

  // 4. Test when only range.first is within the pruned entries.
  helper_.service()->TabNavigationPathPruned(window_id, tab_id, 3 /* index */,
                                             3 /* count */);
  EXPECT_TRUE(helper_.GetAvailableRange(tab_id, &available_range));
  EXPECT_EQ(3, available_range.first);
  EXPECT_EQ(4, available_range.second);

  // Set back available range.
  helper_.SetAvailableRange(tab_id, std::pair<int, int>(4, 7));

  // 4. Test when only range.second is within the pruned entries.
  helper_.service()->TabNavigationPathPruned(window_id, tab_id, 5 /* index */,
                                             3 /* count */);
  EXPECT_TRUE(helper_.GetAvailableRange(tab_id, &available_range));
  EXPECT_EQ(4, available_range.first);
  EXPECT_EQ(4, available_range.second);

  // Set back available range.
  helper_.SetAvailableRange(tab_id, std::pair<int, int>(4, 7));

  // 4. Test when only range contains all the pruned entries.
  helper_.service()->TabNavigationPathPruned(window_id, tab_id, 5 /* index */,
                                             2 /* count */);
  EXPECT_TRUE(helper_.GetAvailableRange(tab_id, &available_range));
  EXPECT_EQ(4, available_range.first);
  EXPECT_EQ(5, available_range.second);
}

// Prunes from front so that we have no entries.
TEST_F(SessionServiceTest, PruneToEmpty) {
  const std::string base_url("http://google.com/");
  SessionID tab_id = SessionID::NewUnique();

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);

  // Add 5 navigations, with the 4th selected.
  for (int i = 0; i < 5; ++i) {
    SerializedNavigationEntry nav = ContentTestHelper::CreateNavigation(
        base_url + base::NumberToString(i), "a");
    nav.set_index(i);
    UpdateNavigation(window_id, tab_id, nav, (i == 3));
  }

  // Set available range for testing.
  helper_.SetAvailableRange(tab_id, std::pair<int, int>(0, 4));

  // Prune all navigations from the front.
  helper_.service()->TabNavigationPathPruned(window_id, tab_id, 0 /* index */,
                                             5 /* count */);

  std::pair<int, int> available_range;
  EXPECT_TRUE(helper_.GetAvailableRange(tab_id, &available_range));
  EXPECT_EQ(0, available_range.first);
  EXPECT_EQ(0, available_range.second);

  // Read back in.
  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  ASSERT_EQ(0U, windows.size());
}

// Don't set the pinned state and make sure the pinned value is false.
TEST_F(SessionServiceTest, PinnedDefaultsToFalse) {
  EXPECT_FALSE(CreateAndWriteSessionWithOneTab(false, false));
}

// Explicitly set the pinned state to false and make sure we get back false.
TEST_F(SessionServiceTest, PinnedFalseWhenSetToFalse) {
  EXPECT_FALSE(CreateAndWriteSessionWithOneTab(false, true));
}

// Explicitly set the pinned state to true and make sure we get back true.
TEST_F(SessionServiceTest, PinnedTrue) {
  EXPECT_TRUE(CreateAndWriteSessionWithOneTab(true, true));
}

// Make sure application extension ids are persisted.
TEST_F(SessionServiceTest, PersistApplicationExtensionID) {
  SessionID tab_id = SessionID::NewUnique();
  ASSERT_NE(window_id, tab_id);
  std::string app_id("foo");

  SerializedNavigationEntry nav1 =
      ContentTestHelper::CreateNavigation("http://google.com", "abc");

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);
  helper_.SetTabExtensionAppID(window_id, tab_id, app_id);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  helper_.AssertSingleWindowWithSingleTab(windows, 1);
  EXPECT_TRUE(app_id == windows[0]->tabs[0]->extension_app_id);
}

// Check that user agent overrides are persisted.
TEST_F(SessionServiceTest, PersistUserAgentOverrides) {
  SessionID tab_id = SessionID::NewUnique();
  ASSERT_NE(window_id, tab_id);
  std::string user_agent_override = "Mozilla/5.0 (X11; Linux x86_64) "
      "AppleWebKit/535.19 (KHTML, like Gecko) Chrome/18.0.1025.45 "
      "Safari/535.19";

  SerializedNavigationEntry nav1 =
      ContentTestHelper::CreateNavigation("http://google.com", "abc");
  SerializedNavigationEntryTestHelper::SetIsOverridingUserAgent(true, &nav1);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);
  helper_.SetTabUserAgentOverride(window_id, tab_id, user_agent_override);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);
  helper_.AssertSingleWindowWithSingleTab(windows, 1);

  sessions::SessionTab* tab = windows[0]->tabs[0].get();
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
  EXPECT_TRUE(user_agent_override == tab->user_agent_override);
}

// Makes sure a tab closed by a user gesture is not restored.
TEST_F(SessionServiceTest, CloseTabUserGesture) {
  SessionID tab_id = SessionID::NewUnique();
  ASSERT_NE(window_id, tab_id);

  SerializedNavigationEntry nav1 =
      ContentTestHelper::CreateNavigation("http://google.com", "abc");

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);
  service()->TabClosed(window_id, tab_id, true);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  ASSERT_TRUE(windows.empty());
}

// Verifies SetWindowBounds maps SHOW_STATE_DEFAULT to SHOW_STATE_NORMAL.
TEST_F(SessionServiceTest, DontPersistDefault) {
  SessionID tab_id = SessionID::NewUnique();
  ASSERT_NE(window_id, tab_id);
  SerializedNavigationEntry nav1 =
      ContentTestHelper::CreateNavigation("http://google.com", "abc");
  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);
  service()->SetWindowBounds(window_id,
                             window_bounds,
                             ui::SHOW_STATE_DEFAULT);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);
  ASSERT_EQ(1U, windows.size());
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, windows[0]->show_state);
}

TEST_F(SessionServiceTest, KeepPostDataWithoutPasswords) {
  SessionID tab_id = SessionID::NewUnique();
  ASSERT_NE(window_id, tab_id);

  // Create a TabNavigation containing page_state and representing a POST
  // request.
  std::string post_data = "data";
  std::unique_ptr<content::NavigationEntry> entry1 =
      content::NavigationEntry::Create();
  entry1->SetURL(GURL("http://google.com"));
  entry1->SetTitle(base::UTF8ToUTF16("title1"));
  entry1->SetHasPostData(true);
  entry1->SetPostData(network::ResourceRequestBody::CreateFromBytes(
      post_data.data(), post_data.size()));
  SerializedNavigationEntry nav1 =
      sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
          0 /* == index*/, entry1.get());

  // Create a TabNavigation containing page_state and representing a normal
  // request.
  std::unique_ptr<content::NavigationEntry> entry2 =
      content::NavigationEntry::Create();
  entry2->SetURL(GURL("http://google.com/nopost"));
  entry2->SetTitle(base::UTF8ToUTF16("title2"));
  entry2->SetHasPostData(false);
  SerializedNavigationEntry nav2 =
      sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
          1 /* == index*/, entry2.get());

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);
  UpdateNavigation(window_id, tab_id, nav2, true);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  helper_.AssertSingleWindowWithSingleTab(windows, 2);

  // Expected: the page state of both navigations was saved and restored.
  ASSERT_EQ(2u, windows[0]->tabs[0]->navigations.size());
  {
    SCOPED_TRACE("Comparing |nav1| and |navigations[0]|");
    helper_.AssertNavigationEquals(nav1, windows[0]->tabs[0]->navigations[0]);
  }
  {
    SCOPED_TRACE("Comparing |nav2| and |navigations[1]|");
    helper_.AssertNavigationEquals(nav2, windows[0]->tabs[0]->navigations[1]);
  }
}

TEST_F(SessionServiceTest, RemovePostDataWithPasswords) {
  SessionID tab_id = SessionID::NewUnique();
  ASSERT_NE(window_id, tab_id);

  // Create a page state representing a HTTP body with posted passwords.
  content::PageState page_state =
      content::PageState::CreateForTesting(GURL(), true, "data", NULL);

  // Create a TabNavigation containing page_state and representing a POST
  // request with passwords.
  SerializedNavigationEntry nav1 =
      ContentTestHelper::CreateNavigation("http://google.com", "title");
  SerializedNavigationEntryTestHelper::SetEncodedPageState(
      page_state.ToEncodedData(), &nav1);
  SerializedNavigationEntryTestHelper::SetHasPostData(true, &nav1);
  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  helper_.AssertSingleWindowWithSingleTab(windows, 1);

  // Expected: the HTTP body was removed from the page state of the POST
  // navigation with passwords.
  EXPECT_NE(page_state.ToEncodedData(),
            windows[0]->tabs[0]->navigations[0].encoded_page_state());
}

TEST_F(SessionServiceTest, ReplacePendingNavigation) {
  const std::string base_url("http://google.com/");
  SessionID tab_id = SessionID::NewUnique();

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);

  // Add 5 navigations, some with the same index
  for (int i = 0; i < 5; ++i) {
    SerializedNavigationEntry nav = ContentTestHelper::CreateNavigation(
        base_url + base::NumberToString(i), "a");
    nav.set_index(i / 2);
    UpdateNavigation(window_id, tab_id, nav, true);
  }

  // Read back in.
  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  // The ones with index 0, and 2 should have been replaced by 1 and 3.
  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(1U, windows[0]->tabs.size());
  EXPECT_EQ(3U, windows[0]->tabs[0]->navigations.size());
  EXPECT_EQ(GURL(base_url + base::NumberToString(1)),
            windows[0]->tabs[0]->navigations[0].virtual_url());
  EXPECT_EQ(GURL(base_url + base::NumberToString(3)),
            windows[0]->tabs[0]->navigations[1].virtual_url());
  EXPECT_EQ(GURL(base_url + base::NumberToString(4)),
            windows[0]->tabs[0]->navigations[2].virtual_url());
}

TEST_F(SessionServiceTest, ReplacePendingNavigationAndPrune) {
  const std::string base_url("http://google.com/");
  SessionID tab_id = SessionID::NewUnique();

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);

  for (int i = 0; i < 5; ++i) {
    SerializedNavigationEntry nav = ContentTestHelper::CreateNavigation(
        base_url + base::NumberToString(i), "a");
    nav.set_index(i);
    UpdateNavigation(window_id, tab_id, nav, true);
  }

  // Set available range for testing.
  helper_.SetAvailableRange(tab_id, std::pair<int, int>(0, 4));

  // Prune all those navigations.
  helper_.service()->TabNavigationPathPruned(window_id, tab_id, 0 /* index */,
                                             5 /* count */);

  std::pair<int, int> available_range;
  EXPECT_TRUE(helper_.GetAvailableRange(tab_id, &available_range));
  EXPECT_EQ(0, available_range.first);
  EXPECT_EQ(0, available_range.second);

  // Add another navigation to replace the last one.
  SerializedNavigationEntry nav = ContentTestHelper::CreateNavigation(
      base_url + base::NumberToString(5), "a");
  nav.set_index(4);
  UpdateNavigation(window_id, tab_id, nav, true);

  // Read back in.
  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  // We should still have that last navigation at the end,
  // even though it replaced one that was set before the prune.
  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(1U, windows[0]->tabs.size());
  ASSERT_EQ(1U, windows[0]->tabs[0]->navigations.size());
  EXPECT_EQ(GURL(base_url + base::NumberToString(5)),
            windows[0]->tabs[0]->navigations[0].virtual_url());
}

TEST_F(SessionServiceTest, RestoreActivation1) {
  SessionID window2_id = SessionID::NewUnique();
  SessionID tab1_id = SessionID::NewUnique();
  SessionID tab2_id = SessionID::NewUnique();
  SerializedNavigationEntry nav1;
  SerializedNavigationEntry nav2;

  CreateAndWriteSessionWithTwoWindows(
      window2_id, tab1_id, tab2_id, &nav1, &nav2);

  service()->ScheduleCommand(
      sessions::CreateSetActiveWindowCommand(window2_id));
  service()->ScheduleCommand(sessions::CreateSetActiveWindowCommand(window_id));

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  SessionID active_window_id = SessionID::InvalidValue();
  ReadWindows(&windows, &active_window_id);
  EXPECT_EQ(window_id, active_window_id);
}

// It's easier to have two separate tests with setup/teardown than to manualy
// reset the state for the different flavors of the test.
TEST_F(SessionServiceTest, RestoreActivation2) {
  SessionID window2_id = SessionID::NewUnique();
  SessionID tab1_id = SessionID::NewUnique();
  SessionID tab2_id = SessionID::NewUnique();
  SerializedNavigationEntry nav1;
  SerializedNavigationEntry nav2;

  CreateAndWriteSessionWithTwoWindows(
      window2_id, tab1_id, tab2_id, &nav1, &nav2);

  service()->ScheduleCommand(
      sessions::CreateSetActiveWindowCommand(window2_id));
  service()->ScheduleCommand(sessions::CreateSetActiveWindowCommand(window_id));
  service()->ScheduleCommand(
      sessions::CreateSetActiveWindowCommand(window2_id));

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  SessionID active_window_id = SessionID::InvalidValue();
  ReadWindows(&windows, &active_window_id);
  EXPECT_EQ(window2_id, active_window_id);
}

// Makes sure we don't track blacklisted URLs.
TEST_F(SessionServiceTest, IgnoreBlacklistedUrls) {
  SessionID tab_id = SessionID::NewUnique();

  SerializedNavigationEntry nav1 =
      ContentTestHelper::CreateNavigation("http://google.com", "abc");
  SerializedNavigationEntry nav2 =
      ContentTestHelper::CreateNavigation(chrome::kChromeUIQuitURL, "quit");
  SerializedNavigationEntry nav3 = ContentTestHelper::CreateNavigation(
      chrome::kChromeUIRestartURL, "restart");

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);
  UpdateNavigation(window_id, tab_id, nav2, true);
  UpdateNavigation(window_id, tab_id, nav3, true);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, NULL);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id, windows[0]->window_id);
  ASSERT_EQ(1U, windows[0]->tabs.size());

  sessions::SessionTab* tab = windows[0]->tabs[0].get();
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

TEST_F(SessionServiceTest, TabGroupDefaultsToNone) {
  CreateTabWithTestNavigationData(window_id, 0);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, nullptr);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(1U, windows[0]->tabs.size());
  ASSERT_EQ(0U, windows[0]->tab_groups.size());

  // Verify that the recorded tab has no group.
  sessions::SessionTab* tab = windows[0]->tabs[0].get();
  EXPECT_EQ(base::nullopt, tab->group);
}

TEST_F(SessionServiceTest, TabGroupsSaved) {
  const auto group1_token = base::Token::CreateRandom();
  const auto group2_token = base::Token::CreateRandom();
  constexpr int kNumTabs = 5;
  const std::array<base::Optional<base::Token>, kNumTabs> groups = {
      base::nullopt, group1_token, group1_token, base::nullopt, group2_token};

  // Create |kNumTabs| tabs with group IDs in |groups|.
  for (int tab_ndx = 0; tab_ndx < kNumTabs; ++tab_ndx) {
    const SessionID tab_id =
        CreateTabWithTestNavigationData(window_id, tab_ndx);
    service()->SetTabGroup(window_id, tab_id, groups[tab_ndx]);
  }

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, nullptr);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(kNumTabs, static_cast<int>(windows[0]->tabs.size()));
  ASSERT_EQ(2U, windows[0]->tab_groups.size());

  for (int tab_ndx = 0; tab_ndx < kNumTabs; ++tab_ndx) {
    sessions::SessionTab* tab = windows[0]->tabs[tab_ndx].get();
    EXPECT_EQ(groups[tab_ndx], tab->group);
  }
}

TEST_F(SessionServiceTest, TabGroupMetadataSaved) {
  constexpr int kNumGroups = 2;
  const std::array<base::Token, kNumGroups> group_ids = {
      base::Token::CreateRandom(), base::Token::CreateRandom()};
  const std::array<base::string16, kNumGroups> titles = {
      base::ASCIIToUTF16("Foo"), base::ASCIIToUTF16("Bar")};
  const std::array<SkColor, kNumGroups> colors = {SK_ColorBLUE, SK_ColorGREEN};

  // Create |kNumGroups| tab groups, each with one tab.
  for (int group_ndx = 0; group_ndx < kNumGroups; ++group_ndx) {
    const SessionID tab_id =
        CreateTabWithTestNavigationData(window_id, group_ndx);
    service()->SetTabGroup(window_id, tab_id, group_ids[group_ndx]);
    service()->SetTabGroupMetadata(window_id, group_ids[group_ndx],
                                   titles[group_ndx], colors[group_ndx]);
  }

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  ReadWindows(&windows, nullptr);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(2U, windows[0]->tabs.size());
  ASSERT_EQ(2U, windows[0]->tab_groups.size());

  // There's no guaranteed order in |SessionWindow::tab_groups|, so use a map.
  base::flat_map<base::Token, sessions::SessionTabGroup*> tab_groups;
  for (int group_ndx = 0; group_ndx < kNumGroups; ++group_ndx) {
    tab_groups.emplace(windows[0]->tab_groups[group_ndx]->group_id,
                       windows[0]->tab_groups[group_ndx].get());
  }

  for (int group_ndx = 0; group_ndx < kNumGroups; ++group_ndx) {
    const base::Token group_id = group_ids[group_ndx];
    ASSERT_TRUE(base::Contains(tab_groups, group_id));
    EXPECT_EQ(titles[group_ndx], tab_groups[group_id]->metadata.title);
    EXPECT_EQ(colors[group_ndx], tab_groups[group_id]->metadata.color);
  }
}

// Functions used by GetSessionsAndDestroy.
namespace {

void OnGotPreviousSession(
    std::vector<std::unique_ptr<sessions::SessionWindow>> windows,
    SessionID ignored_active_window) {
  FAIL() << "SessionService was destroyed, this shouldn't be reached.";
}

// Blocks until |keep_waiting| is false.
void SimulateWaitForTesting(const base::AtomicFlag* flag) {
  // Ideally this code would use WaitableEvent, but that triggers a DCHECK in
  // thread_restrictions. Rather than inject a trait only for the test this
  // code uses yield.
  while (!flag->IsSet())
    base::PlatformThread::YieldCurrentThread();
}

}  // namespace

// Verifies that SessionService::GetLastSession() works correctly if the
// SessionService is deleted during processing. To verify the problematic case
// does the following:
// 1. Sends a task to the background thread that blocks.
// 2. Asks SessionService for the last session commands. This is blocked by 1.
// 3. Posts another task to the background thread, this too is blocked by 1.
// 4. Deletes SessionService.
// 5. Signals the semaphore that 2 and 3 are waiting on, allowing
//    GetLastSession() to continue.
// 6. runs the message loop, this is quit when the task scheduled in 3 posts
//    back to the ui thread to quit the run loop.
// The call to get the previous session should never be invoked because the
// SessionService was destroyed before SessionService could process the results.
TEST_F(SessionServiceTest, GetSessionsAndDestroy) {
  base::AtomicFlag flag;
  base::CancelableTaskTracker cancelable_task_tracker;
  base::RunLoop run_loop;
  helper_.RunTaskOnBackendThread(
      FROM_HERE, base::Bind(&SimulateWaitForTesting, base::Unretained(&flag)));
  service()->GetLastSession(base::Bind(&OnGotPreviousSession),
                            &cancelable_task_tracker);
  helper_.RunTaskOnBackendThread(FROM_HERE, run_loop.QuitClosure());
  delete helper_.ReleaseService();
  flag.Set();
  run_loop.Run();
}
