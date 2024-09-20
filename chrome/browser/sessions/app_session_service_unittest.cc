// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/app_session_service.h"

#include <stddef.h>

#include <vector>

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/sessions/app_session_service_test_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/content/content_test_helper.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_show_state.mojom.h"

using content::NavigationEntry;
using sessions::ContentTestHelper;
using sessions::SerializedNavigationEntry;
using sessions::SerializedNavigationEntryTestHelper;

// Since AppSessionService is mostly based on SessionServiceBase,
// AppSessionService unit tests will aim to test things unique to
// AppSessionService.

// This unit_test suite is relatively spartan compared to SessionService
// unittests because a large portion of SessionService unit tests test
// SessionServiceBase and SessionService together.

// Actual app restoration testing will be in
// app_session_service_browsertests.cc
class AppSessionServiceTest : public BrowserWithTestWindowTest {
 public:
  AppSessionServiceTest() : window_bounds_(0, 1, 2, 3) {}

 protected:
  // SetUp() opens 1 normal window and 1 app window to each of the respective
  // [app]SessionService classes.
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    app_session_service_ =
        std::make_unique<AppSessionService>(browser()->profile());
    app_helper_.SetService(app_session_service_.get());

    app_service()->SetWindowType(app_window_id, Browser::TYPE_APP);
    app_service()->SetWindowBounds(app_window_id, window_bounds_,
                                   ui::mojom::WindowShowState::kNormal);
    app_service()->SetWindowAppName(app_window_id, "TestApp");
    app_service()->SetWindowWorkspace(app_window_id, window_workspace);

    app_nav = ContentTestHelper::CreateNavigation("http://google2.com", "abcd");
    app_helper_.PrepareTabInWindow(app_window_id, app_tab_id, 0, true);
    AppUpdateNavigation(app_window_id, app_tab_id, app_nav, true);
  }

  void TearDown() override {
    DestroyAppSessionService();

    BrowserWithTestWindowTest::TearDown();
  }

  void DestroyAppSessionService() {
    // Destroy the AppSessionService first as it may post tasks.
    ASSERT_TRUE(app_session_service_);
    app_session_service_.reset();
    // This flushes tasks.
    app_helper_.SetService(nullptr);
  }

  void AppUpdateNavigation(SessionID window_session_id,
                           SessionID tab_id,
                           const SerializedNavigationEntry& navigation,
                           bool select) {
    app_service()->UpdateTabNavigation(window_session_id, tab_id, navigation);
    if (select) {
      app_service()->SetSelectedNavigationIndex(window_session_id, tab_id,
                                                navigation.index());
    }
  }

  void AppReadWindows(
      std::vector<std::unique_ptr<sessions::SessionWindow>>* windows,
      SessionID* active_window_id) {
    DestroyAppSessionService();

    app_session_service_ =
        std::make_unique<AppSessionService>(browser()->profile());
    app_helper_.SetService(app_session_service_.get());

    SessionID* non_null_active_window_id = active_window_id;
    SessionID dummy_active_window_id = SessionID::InvalidValue();
    if (!non_null_active_window_id)
      non_null_active_window_id = &dummy_active_window_id;
    app_helper_.ReadWindows(windows, non_null_active_window_id);
  }

  // Gets the pinned state of the app set up in SetUp()
  bool GetPinnedStateOfDefaultApp() {
    std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
    AppReadWindows(&windows, nullptr);

    EXPECT_EQ(1U, windows.size());
    if (HasFatalFailure())
      return false;
    EXPECT_EQ(1U, windows[0]->tabs.size());
    if (HasFatalFailure())
      return false;

    sessions::SessionTab* tab = windows[0]->tabs[0].get();
    return tab->pinned;
  }

  // SetUp already includes one add, add one more.
  void CreateAndWriteSessionWithTwoApps(SessionID app2_id,
                                        SessionID tab1_id,
                                        SerializedNavigationEntry* nav1) {
    *nav1 = ContentTestHelper::CreateNavigation("http://google.com", "abc");

    app_service()->SetWindowType(app2_id, Browser::TYPE_APP);
    app_service()->SetWindowBounds(app2_id, window_bounds_,
                                   ui::mojom::WindowShowState::kNormal);
    app_service()->SetWindowAppName(app2_id, "TestApp");
    app_service()->SetWindowWorkspace(app2_id, window_workspace);

    SerializedNavigationEntry nav =
        ContentTestHelper::CreateNavigation("http://google2.com", "abcd");
    app_helper_.PrepareTabInWindow(app2_id, tab1_id, 0, true);
    AppUpdateNavigation(app2_id, tab1_id, *nav1, true);
  }

  AppSessionService* app_service() { return app_helper_.service(); }

  const gfx::Rect window_bounds_;

  const std::string window_workspace = "abc";

  const SessionID window_id = SessionID::NewUnique();
  const SessionID app_window_id = SessionID::NewUnique();
  const SessionID app_tab_id = SessionID::NewUnique();
  SerializedNavigationEntry app_nav;

  std::unique_ptr<AppSessionService> app_session_service_;

  AppSessionServiceTestHelper app_helper_;
};

TEST_F(AppSessionServiceTest, Basic) {
  SessionID tab_id = SessionID::NewUnique();
  ASSERT_NE(window_id, tab_id);

  // Now verify AppSessionService
  std::vector<std::unique_ptr<sessions::SessionWindow>> app_windows;
  AppReadWindows(&app_windows, nullptr);

  ASSERT_EQ(1U, app_windows.size());
  ASSERT_TRUE(window_bounds_ == app_windows[0]->bounds);
  ASSERT_EQ(window_workspace, app_windows[0]->workspace);
  ASSERT_EQ(0, app_windows[0]->selected_tab_index);
  ASSERT_EQ(app_window_id, app_windows[0]->window_id);
  ASSERT_EQ(1U, app_windows[0]->tabs.size());
  ASSERT_EQ(sessions::SessionWindow::TYPE_APP, app_windows[0]->type);
}

TEST_F(AppSessionServiceTest, BasicRelevancyTest) {
  ASSERT_TRUE(app_service()->ShouldRestoreWindowOfType(
      sessions::SessionWindow::TYPE_APP));
  ASSERT_FALSE(app_service()->ShouldRestoreWindowOfType(
      sessions::SessionWindow::TYPE_NORMAL));
}

// SetUp has one app window written. Add one more and ensure the nav data
// stored matches expectations.
TEST_F(AppSessionServiceTest, TwoApps) {
  SessionID window2_id = SessionID::NewUnique();
  SessionID tab1_id = SessionID::NewUnique();
  SerializedNavigationEntry nav1;

  CreateAndWriteSessionWithTwoApps(window2_id, tab1_id, &nav1);

  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  AppReadWindows(&windows, nullptr);

  ASSERT_EQ(2U, windows.size());
  ASSERT_EQ(1U, windows[0]->tabs.size());
  ASSERT_EQ(1U, windows[1]->tabs.size());

  if (windows[0]->window_id == app_window_id) {
    ASSERT_EQ(window2_id, windows[1]->window_id);
  } else {
    ASSERT_EQ(window2_id, windows[0]->window_id);
    ASSERT_EQ(window_id, windows[1]->window_id);
    ASSERT_EQ(ui::mojom::WindowShowState::kMaximized, windows[0]->show_state);
    ASSERT_EQ(ui::mojom::WindowShowState::kNormal, windows[1]->show_state);
  }
}

// Don't set the pinned state and make sure the pinned value is false.
TEST_F(AppSessionServiceTest, PinnedDefaultsToFalse) {
  EXPECT_FALSE(GetPinnedStateOfDefaultApp());
}

TEST_F(AppSessionServiceTest, RestoreAppWithAppSessionService) {
  SessionID window2_id = SessionID::NewUnique();
  SessionID tab2_id = SessionID::NewUnique();
  ASSERT_NE(window2_id, window_id);

  // This unit test checks that the two instances of SessionService
  // do not interfer and are isolated.
  app_helper_.service()->SetWindowType(window2_id, Browser::TYPE_APP);
  app_helper_.service()->SetWindowBounds(window2_id, window_bounds_,
                                         ui::mojom::WindowShowState::kNormal);
  app_helper_.service()->SetWindowAppName(window2_id, "TestApp");

  SerializedNavigationEntry nav1 =
      ContentTestHelper::CreateNavigation("http://google.com", "abc");
  SerializedNavigationEntry nav2 =
      ContentTestHelper::CreateNavigation("http://google2.com", "abcd");

  app_helper_.PrepareTabInWindow(window2_id, tab2_id, 0, false);
  AppUpdateNavigation(window2_id, tab2_id, nav2, true);

  std::vector<std::unique_ptr<sessions::SessionWindow>> app_windows;
  AppReadWindows(&app_windows, nullptr);

  // Now check all the state from AppSessionService
  // We can't predict if app_windows[0] or [1] is the one we opened,
  // so try to figure that out first.
  int our_window_index = 0;
  if (app_windows[0]->window_id != window2_id) {
    // by deduction, it should be [1].
    our_window_index = 1;
  }

  ASSERT_EQ(0, app_windows[our_window_index]->selected_tab_index);
  ASSERT_EQ(window2_id, app_windows[our_window_index]->window_id);
  ASSERT_EQ(1U, app_windows[our_window_index]->tabs.size());
  ASSERT_TRUE(app_windows[our_window_index]->type ==
              sessions::SessionWindow::TYPE_APP);
  ASSERT_EQ("TestApp", app_windows[our_window_index]->app_name);

  auto* tab = app_windows[our_window_index]->tabs[0].get();
  app_helper_.AssertTabEquals(window2_id, tab2_id, 0, 0, 1, *tab);
}
