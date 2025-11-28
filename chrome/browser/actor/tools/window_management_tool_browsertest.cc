// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/actor/tools/window_management_tool_request.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ozone_buildflags.h"

using base::test::TestFuture;

namespace actor {

namespace {

class ActorWindowManagementToolBrowserTest : public ActorToolsTest {
 public:
  ActorWindowManagementToolBrowserTest() = default;
  ~ActorWindowManagementToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  std::unique_ptr<ToolRequest> MakeCreateWindowRequest() {
    return std::make_unique<CreateWindowToolRequest>();
  }

  std::unique_ptr<ToolRequest> MakeCloseWindowRequest(int32_t window_id) {
    return std::make_unique<CloseWindowToolRequest>(window_id);
  }

  std::unique_ptr<ToolRequest> MakeActivateWindowRequest(int32_t window_id) {
    return std::make_unique<ActivateWindowToolRequest>(window_id);
  }
};

class NewWindowObserver : public BrowserCollectionObserver {
 public:
  explicit NewWindowObserver(Profile* profile) {
    profile_browser_collection_observation_.Observe(
        ProfileBrowserCollection::GetForProfile(profile));
  }
  ~NewWindowObserver() override = default;

  void OnBrowserCreated(BrowserWindowInterface* browser) override {
    created_browser_ = browser;
  }

  BrowserWindowInterface* created_browser() const { return created_browser_; }

 private:
  raw_ptr<BrowserWindowInterface> created_browser_ = nullptr;
  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      profile_browser_collection_observation_{this};
};

// Ensure CreateWindow creates a new window and makes it the active window.
IN_PROC_BROWSER_TEST_F(ActorWindowManagementToolBrowserTest, CreateWindow) {
  const size_t initial_browser_count = GetAllBrowserWindowInterfaces().size();

  NewWindowObserver new_window_observer(GetProfile());

  std::unique_ptr<ToolRequest> action = MakeCreateWindowRequest();
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  BrowserWindowInterface* new_window = new_window_observer.created_browser();
  EXPECT_TRUE(new_window);

  EXPECT_EQ(initial_browser_count + 1, GetAllBrowserWindowInterfaces().size());
  ui_test_utils::WaitForBrowserSetLastActive(
      new_window->GetBrowserForMigrationOnly());
  EXPECT_EQ(new_window, GetLastActiveBrowserWindowInterfaceWithAnyProfile());
}

// Ensure a created window includes a new tab and that tab is added to the
// acting tab set.
IN_PROC_BROWSER_TEST_F(ActorWindowManagementToolBrowserTest,
                       CreateWindowAddsTab) {
  NewWindowObserver new_window_observer(GetProfile());

  std::unique_ptr<ToolRequest> action = MakeCreateWindowRequest();
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(new_window_observer.created_browser()
                ->GetFeatures()
                .tab_strip_model()
                ->count(),
            1);
  EXPECT_EQ(actor_task().GetTabs().size(), 1ul);
}

// Ensure a created window only adds the new window's tab to the acting set if
// the acting set is empty.
// TODO(crbug.com/420669167): This can be updated to add multiple tabs once
// multi-tab is supported.
IN_PROC_BROWSER_TEST_F(ActorWindowManagementToolBrowserTest,
                       CreateWindowAddsOnlyOneActingTab) {
  tabs::TabInterface* first_new_window_tab = nullptr;

  // Create a new window and so its tab is added to the acting set.
  {
    NewWindowObserver new_window_observer(GetProfile());

    std::unique_ptr<ToolRequest> action = MakeCreateWindowRequest();
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);

    first_new_window_tab = new_window_observer.created_browser()
                               ->GetFeatures()
                               .tab_strip_model()
                               ->GetActiveTab();
  }

  // Create another new window; its tab should not be added to the acting set
  // since the acting set is not empty.
  {
    std::unique_ptr<ToolRequest> action = MakeCreateWindowRequest();
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(actor_task().GetTabs().size(), 1ul);
  EXPECT_EQ(*actor_task().GetTabs().begin(), first_new_window_tab->GetHandle());
}

// Ensure CloseWindow closes the window with the given ID.
IN_PROC_BROWSER_TEST_F(ActorWindowManagementToolBrowserTest, CloseWindow) {
  const size_t initial_browser_count = GetAllBrowserWindowInterfaces().size();
  BrowserWindowInterface* initial_active_browser =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();

  // Create a new window to close.
  std::unique_ptr<ToolRequest> create_action = MakeCreateWindowRequest();
  ActResultFuture create_result;
  actor_task().Act(ToRequestList(create_action), create_result.GetCallback());
  ExpectOkResult(create_result);
  ASSERT_EQ(initial_browser_count + 1, GetAllBrowserWindowInterfaces().size());

  // Close the new window.
  const int32_t window_id_to_close =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile()->GetSessionID().id();
  std::unique_ptr<ToolRequest> close_action =
      MakeCloseWindowRequest(window_id_to_close);
  ActResultFuture close_result;
  actor_task().Act(ToRequestList(close_action), close_result.GetCallback());
  ExpectOkResult(close_result);

  EXPECT_EQ(initial_browser_count, GetAllBrowserWindowInterfaces().size());
  ui_test_utils::WaitForBrowserSetLastActive(
      initial_active_browser->GetBrowserForMigrationOnly());
  EXPECT_EQ(initial_active_browser,
            GetLastActiveBrowserWindowInterfaceWithAnyProfile());
}

// Ensure CloseWindow removes the window's tab from the acting set.
IN_PROC_BROWSER_TEST_F(ActorWindowManagementToolBrowserTest,
                       CloseWindowRemovesTabFromActingSet) {
  BrowserWindowInterface* new_window = nullptr;

  // Create a new window to close.
  {
    NewWindowObserver new_window_observer(GetProfile());
    std::unique_ptr<ToolRequest> create_action = MakeCreateWindowRequest();
    ActResultFuture create_result;
    actor_task().Act(ToRequestList(create_action), create_result.GetCallback());
    ExpectOkResult(create_result);
    new_window = new_window_observer.created_browser();
  }

  ASSERT_EQ(actor_task().GetTabs().size(), 1ul);

  // Close the new window.
  {
    std::unique_ptr<ToolRequest> close_action =
        MakeCloseWindowRequest(new_window->GetSessionID().id());
    ActResultFuture close_result;
    actor_task().Act(ToRequestList(close_action), close_result.GetCallback());
    ExpectOkResult(close_result);
  }

  EXPECT_TRUE(actor_task().GetTabs().empty());
}

#if BUILDFLAG(IS_OZONE_WAYLAND)
// Wayland doesn't support programmatic window activation at all so this test
// (and functionality?) isn't relevant.
#define MAYBE_ActivateWindow DISABLED_ActivateWindow
#else
#define MAYBE_ActivateWindow ActivateWindow
#endif
// Ensure ActivateWindow activates the window with the given ID.
IN_PROC_BROWSER_TEST_F(ActorWindowManagementToolBrowserTest,
                       MAYBE_ActivateWindow) {
  BrowserWindowInterface* initial_window =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();

  // Create a new window, which should become active.
  NewWindowObserver new_window_observer(GetProfile());
  std::unique_ptr<ToolRequest> create_action = MakeCreateWindowRequest();
  ActResultFuture create_result;
  actor_task().Act(ToRequestList(create_action), create_result.GetCallback());
  ExpectOkResult(create_result);

  BrowserWindowInterface* new_window = new_window_observer.created_browser();
  ASSERT_NE(new_window, initial_window);
  ui_test_utils::WaitForBrowserSetLastActive(
      new_window->GetBrowserForMigrationOnly());

  // Activate the original window.
  std::unique_ptr<ToolRequest> activate_action =
      MakeActivateWindowRequest(initial_window->GetSessionID().id());
  ActResultFuture activate_result;
  actor_task().Act(ToRequestList(activate_action),
                   activate_result.GetCallback());
  ExpectOkResult(activate_result);

  ui_test_utils::WaitForBrowserSetLastActive(
      initial_window->GetBrowserForMigrationOnly());
  EXPECT_EQ(initial_window,
            GetLastActiveBrowserWindowInterfaceWithAnyProfile());
}

}  // namespace
}  // namespace actor
