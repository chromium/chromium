// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/actor/tools/window_management_tool_request.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/common/actor.mojom.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/base_window.h"
#include "ui/base/ozone_buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/command_line.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#else
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

using base::test::TestFuture;

namespace actor {

namespace {

class ActorWindowManagementToolBrowserTest : public ActorToolsTest {
 public:
  ActorWindowManagementToolBrowserTest() = default;
  ~ActorWindowManagementToolBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ActorToolsTest::SetUpCommandLine(command_line);
#if BUILDFLAG(IS_ANDROID)
    // Opening a second window launches ChromeTabbedActivity, which would show
    // the first-run experience instead of a browser window.
    command_line->AppendSwitch("disable-fre");
#endif
  }

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
    // Navigate to an actual web page so that on Android the tab and view
    // hierarchy are attached and interactable before exercising fullscreen.
    const GURL url =
        embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
    ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
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

  std::unique_ptr<ToolRequest> MakeEnterFullscreenRequest(int32_t window_id) {
    return std::make_unique<EnterFullscreenToolRequest>(window_id);
  }

  std::unique_ptr<ToolRequest> MakeExitFullscreenRequest(int32_t window_id) {
    return std::make_unique<ExitFullscreenToolRequest>(window_id);
  }

  // Whether `browser_window` is fullscreen. On desktop this is browser
  // fullscreen; on Android it is the active tab's immersive fullscreen, which
  // is what the tool drives there.
  bool IsFullscreen(BrowserWindowInterface* browser_window) {
#if BUILDFLAG(IS_ANDROID)
    return web_contents()->IsFullscreen();
#else
    return browser_window->GetWindow()->IsFullscreen();
#endif
  }

  // Fullscreen transitions can complete asynchronously, so poll.
  void WaitForFullscreen(BrowserWindowInterface* browser_window,
                         bool fullscreen) {
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return IsFullscreen(browser_window) == fullscreen; }));
  }

  // Waits until `browser_window` is the most recently activated window.
  void WaitForWindowActive(BrowserWindowInterface* browser_window) {
#if BUILDFLAG(IS_ANDROID)
    // Bringing a task to the front is asynchronous in the OS; the window's
    // activation time is recorded when its Activity becomes top-resumed.
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return GetLastActiveBrowserWindowInterfaceWithAnyProfile() ==
             browser_window;
    }));
#else
    ui_test_utils::WaitForBrowserSetLastActive(browser_window);
#endif
  }

  // Opens a second window with one tab and waits for it to become active.
  BrowserWindowInterface* OpenSecondWindow();
};

#if !BUILDFLAG(IS_ANDROID)
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
#endif  // !BUILDFLAG(IS_ANDROID)

BrowserWindowInterface*
ActorWindowManagementToolBrowserTest::OpenSecondWindow() {
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/470475787): Use the CreateWindow tool once it's ported.
  // Window creation launches a new Activity asynchronously; the callback
  // runs once the window exists and has a tab. A popup is used because a
  // normal window launches adjacent to the test Activity in split-screen
  // mode, and leaving split-screen when it closes triggers a configuration
  // change that recreates the test Activity, which browser tests forbid
  // (crbug.com/40237216). A popup opens in its own full-screen task.
  BrowserWindowCreateParams params(BrowserWindowInterface::TYPE_POPUP,
                                   *GetProfile(),
                                   /*from_user_gesture=*/false);
  TestFuture<BrowserWindowInterface*> future;
  CreateBrowserWindow(std::move(params), future.GetCallback());
  BrowserWindowInterface* new_window = future.Get();
#else
  NewWindowObserver new_window_observer(GetProfile());
  std::unique_ptr<ToolRequest> action = MakeCreateWindowRequest();
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
  BrowserWindowInterface* new_window = new_window_observer.created_browser();
#endif
  EXPECT_TRUE(new_window);
  if (new_window) {
    WaitForWindowActive(new_window);
  }
  return new_window;
}

// TODO(crbug.com/470475787): Port CreateWindow to Android.
#if !BUILDFLAG(IS_ANDROID)
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
  ui_test_utils::WaitForBrowserSetLastActive(new_window);
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

  EXPECT_EQ(new_window_observer.created_browser()->GetTabStripModel()->count(),
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
                               ->GetTabStripModel()
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

#endif  // !BUILDFLAG(IS_ANDROID)

// Ensure CloseWindow closes the window with the given ID.
IN_PROC_BROWSER_TEST_F(ActorWindowManagementToolBrowserTest, CloseWindow) {
  const size_t initial_browser_count = GetAllBrowserWindowInterfaces().size();
  BrowserWindowInterface* initial_active_browser =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();

  // Open a new window to close.
  BrowserWindowInterface* new_window = OpenSecondWindow();
  ASSERT_TRUE(new_window);
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
  WaitForWindowActive(initial_active_browser);
  EXPECT_EQ(initial_active_browser,
            GetLastActiveBrowserWindowInterfaceWithAnyProfile());
}

// Ensure CloseWindow removes the window's tab from the acting set.
IN_PROC_BROWSER_TEST_F(ActorWindowManagementToolBrowserTest,
                       CloseWindowRemovesTabFromActingSet) {
  BrowserWindowInterface* new_window = OpenSecondWindow();
  ASSERT_TRUE(new_window);
#if BUILDFLAG(IS_ANDROID)
  // On desktop the CreateWindow tool adds the new window's tab to the acting
  // set. That tool isn't available on Android, so add the tab directly.
  tabs::TabInterface* new_tab =
      TabListInterface::From(new_window)->GetActiveTab();
  ASSERT_TRUE(new_tab);
  AddTabToTask(*new_tab, actor_task());
#endif

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

// Ensure ActivateWindow activates the window with the given ID.
IN_PROC_BROWSER_TEST_F(ActorWindowManagementToolBrowserTest, ActivateWindow) {
#if BUILDFLAG(IS_OZONE)
  if (::ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP() << "Wayland doesn't support programmatic window activation";
  }
#endif
  BrowserWindowInterface* initial_window =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();

  // Open a new window, which becomes active.
  BrowserWindowInterface* new_window = OpenSecondWindow();
  ASSERT_TRUE(new_window);
  ASSERT_NE(new_window, initial_window);

  // Activate the original window.
  std::unique_ptr<ToolRequest> activate_action =
      MakeActivateWindowRequest(initial_window->GetSessionID().id());
  ActResultFuture activate_result;
  actor_task().Act(ToRequestList(activate_action),
                   activate_result.GetCallback());
  ExpectOkResult(activate_result);

  WaitForWindowActive(initial_window);
  EXPECT_EQ(initial_window,
            GetLastActiveBrowserWindowInterfaceWithAnyProfile());
}

// Ensure EnterFullscreen enters fullscreen for a specific window ID and is a
// no-op if already in fullscreen.
IN_PROC_BROWSER_TEST_F(ActorWindowManagementToolBrowserTest, EnterFullscreen) {
  BrowserWindowInterface* browser_window =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  ASSERT_TRUE(browser_window);
  const int32_t window_id = browser_window->GetSessionID().id();
  ASSERT_FALSE(IsFullscreen(browser_window));

  // Enter fullscreen.
  {
    std::unique_ptr<ToolRequest> action = MakeEnterFullscreenRequest(window_id);
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    WaitForFullscreen(browser_window, true);
  }

  // Already in fullscreen. Should be no-op.
  {
    std::unique_ptr<ToolRequest> action = MakeEnterFullscreenRequest(window_id);
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_TRUE(IsFullscreen(browser_window));
  }
}

// Ensure ExitFullscreen exits fullscreen for a specific window ID and is a
// no-op if already out of fullscreen.
IN_PROC_BROWSER_TEST_F(ActorWindowManagementToolBrowserTest, ExitFullscreen) {
  BrowserWindowInterface* browser_window =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  ASSERT_TRUE(browser_window);
  const int32_t window_id = browser_window->GetSessionID().id();

  // Enter fullscreen first.
  {
    std::unique_ptr<ToolRequest> action = MakeEnterFullscreenRequest(window_id);
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    WaitForFullscreen(browser_window, true);
  }

  // Exit fullscreen.
  {
    std::unique_ptr<ToolRequest> action = MakeExitFullscreenRequest(window_id);
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    WaitForFullscreen(browser_window, false);
  }

  // Already out of fullscreen. Should be no-op.
  {
    std::unique_ptr<ToolRequest> action = MakeExitFullscreenRequest(window_id);
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_FALSE(IsFullscreen(browser_window));
  }
}

// Ensure EnterFullscreen fails with kWindowWentAway for an invalid window ID.
IN_PROC_BROWSER_TEST_F(ActorWindowManagementToolBrowserTest,
                       EnterFullscreen_FailsWithInvalidWindowId) {
  std::unique_ptr<ToolRequest> action =
      MakeEnterFullscreenRequest(/*window_id=*/999999);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kWindowWentAway);
}

// Ensure ExitFullscreen fails with kWindowWentAway for an invalid window ID.
IN_PROC_BROWSER_TEST_F(ActorWindowManagementToolBrowserTest,
                       ExitFullscreen_FailsWithInvalidWindowId) {
  std::unique_ptr<ToolRequest> action =
      MakeExitFullscreenRequest(/*window_id=*/999999);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kWindowWentAway);
}

}  // namespace
}  // namespace actor
