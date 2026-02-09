// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/protocol/browser_handler.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/display/types/display_constants.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

class DevToolsManagerDelegateTest : public InProcessBrowserTest {
 public:
  void SendCommand(const std::string& state) {
    auto window_bounds =
        protocol::Browser::Bounds::Create().SetWindowState(state).Build();
    BrowserHandler handler(nullptr, "");
    handler.SetWindowBounds(browser()->session_id().id(),
                            std::move(window_bounds));
  }

  void UpdateBounds() {
    auto window_bounds = protocol::Browser::Bounds::Create()
                             .SetWindowState("normal")
                             .SetLeft(200)
                             .SetHeight(400)
                             .Build();
    BrowserHandler handler(nullptr, "");
    handler.SetWindowBounds(browser()->session_id().id(),
                            std::move(window_bounds));
  }

  void CheckIsMaximized(bool maximized) {
    ui_test_utils::CheckWaiter(
        base::BindRepeating(&BrowserWindow::IsMaximized,
                            base::Unretained(browser()->window())),
        maximized, base::Seconds(1))
        .Wait();
    EXPECT_EQ(maximized, browser()->window()->IsMaximized());
  }

  void CheckIsMinimized(bool minimized) {
    ui_test_utils::CheckWaiter(
        base::BindRepeating(&BrowserWindow::IsMinimized,
                            base::Unretained(browser()->window())),
        minimized, base::Seconds(1))
        .Wait();
    EXPECT_EQ(minimized, browser()->window()->IsMinimized());
  }

  void CheckIsFullscreen(bool fullscreen) {
    ui_test_utils::CheckWaiter(
        base::BindRepeating(&BrowserWindow::IsFullscreen,
                            base::Unretained(browser()->window())),
        fullscreen, base::Seconds(1))
        .Wait();
    EXPECT_EQ(fullscreen, browser()->window()->IsFullscreen());
  }

  bool IsWindowBoundsEqual(gfx::Rect expected) {
    return browser()->window()->GetBounds() == expected;
  }

  void CheckWindowBounds(gfx::Rect expected) {
    ui_test_utils::CheckWaiter(
        base::BindRepeating(&DevToolsManagerDelegateTest::IsWindowBoundsEqual,
                            base::Unretained(this), expected),
        true, base::Seconds(1))
        .Wait();
    EXPECT_EQ(expected, browser()->window()->GetBounds());
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest, NormalWindowChangeBounds) {
  browser()->window()->SetBounds(gfx::Rect(100, 100, 600, 600));
  CheckWindowBounds(gfx::Rect(100, 100, 600, 600));
  UpdateBounds();
  CheckWindowBounds(gfx::Rect(200, 100, 600, 400));
}

#if BUILDFLAG(IS_MAC)
// MacViews does not yet implement maximized windows: https://crbug.com/41385204
#define MAYBE_NormalToMaximizedWindow DISABLED_NormalToMaximizedWindow
#else
#define MAYBE_NormalToMaximizedWindow NormalToMaximizedWindow
#endif
IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest,
                       MAYBE_NormalToMaximizedWindow) {
  CheckIsMaximized(false);
  SendCommand("maximized");
  CheckIsMaximized(true);
}

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest, NormalToMinimizedWindow) {
  CheckIsMinimized(false);
  SendCommand("minimized");
  CheckIsMinimized(true);
}

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest, NormalToFullscreenWindow) {
#if BUILDFLAG(IS_MAC)
  ui::test::ScopedFakeNSWindowFullscreen faker;
#endif
  CheckIsFullscreen(false);
  SendCommand("fullscreen");
  CheckIsFullscreen(true);
}

#if BUILDFLAG(IS_MAC)
// MacViews does not yet implement maximized windows: https://crbug.com/41385204
#define MAYBE_MaximizedToMinimizedWindow DISABLED_MaximizedToMinimizedWindow
#else
#define MAYBE_MaximizedToMinimizedWindow MaximizedToMinimizedWindow
#endif
IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest,
                       MAYBE_MaximizedToMinimizedWindow) {
  browser()->window()->Maximize();
  CheckIsMaximized(true);

  CheckIsMinimized(false);
  SendCommand("minimized");
  CheckIsMinimized(true);
}

#if BUILDFLAG(IS_MAC)
// MacViews does not yet implement maximized windows: https://crbug.com/41385204
#define MAYBE_MaximizedToFullscreenWindow DISABLED_MaximizedToFullscreenWindow
#else
#define MAYBE_MaximizedToFullscreenWindow MaximizedToFullscreenWindow
#endif
IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest,
                       MAYBE_MaximizedToFullscreenWindow) {
  browser()->window()->Maximize();
  CheckIsMaximized(true);

  CheckIsFullscreen(false);
  SendCommand("fullscreen");
  CheckIsFullscreen(true);
}

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest, ShowMinimizedWindow) {
  browser()->window()->Minimize();
  CheckIsMinimized(true);
  SendCommand("normal");
  CheckIsMinimized(false);
}

#if BUILDFLAG(IS_MAC)
// MacViews does not yet implement maximized windows: https://crbug.com/41385204
#define MAYBE_RestoreMaximizedWindow DISABLED_RestoreMaximizedWindow
#else
#define MAYBE_RestoreMaximizedWindow RestoreMaximizedWindow
#endif
IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest,
                       MAYBE_RestoreMaximizedWindow) {
  browser()->window()->Maximize();
  CheckIsMaximized(true);
  SendCommand("normal");
  CheckIsMaximized(false);
}

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest, ExitFullscreenWindow) {
#if BUILDFLAG(IS_MAC)
  ui::test::ScopedFakeNSWindowFullscreen faker;
#endif
  browser()->window()->GetExclusiveAccessContext()->EnterFullscreen(
      url::Origin(), EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE, FullscreenTabParams());
  CheckIsFullscreen(true);
  SendCommand("normal");
  CheckIsFullscreen(false);
}

#if BUILDFLAG(IS_MAC)
// Test that closing undocked DevTools from a PWA window returns focus to the
// PWA window instead of the main browser window.
class DevToolsPWAFocusTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    // Install a simple PWA.
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
            embedded_test_server()->GetURL("/simple.html"));
    web_app_info->title = u"Test PWA";
    web_app_info->scope = embedded_test_server()->GetURL("/");
    app_id_ = web_app::test::InstallWebApp(browser()->profile(),
                                           std::move(web_app_info));
  }

  void TearDownOnMainThread() override {
    if (!app_id_.empty()) {
      web_app::test::UninstallWebApp(browser()->profile(), app_id_);
    }
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  webapps::AppId app_id_;
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

IN_PROC_BROWSER_TEST_F(DevToolsPWAFocusTest,
                       ClosingUndockedDevToolsFocusesPWAWindow) {
  BrowserWindowInterface* const pwa_browser =
      web_app::LaunchWebAppBrowserAndWait(browser()->profile(), app_id_);
  ASSERT_TRUE(pwa_browser);
  ASSERT_TRUE(pwa_browser->GetWindow());

  ui_test_utils::WaitUntilBrowserBecomeActive(pwa_browser);
  EXPECT_TRUE(pwa_browser->GetWindow()->IsActive());

  content::WebContents* pwa_web_contents =
      pwa_browser->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(pwa_web_contents);

  DevToolsWindow* devtools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(pwa_web_contents,
                                                    /*is_docked=*/false);
  ASSERT_TRUE(devtools_window);

  BrowserWindowInterface* devtools_browser =
      DevToolsWindowTesting::Get(devtools_window)->browser();
  ASSERT_TRUE(devtools_browser);
  ui_test_utils::WaitUntilBrowserBecomeActive(devtools_browser);

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);

  // After closing DevTools, the PWA window should become active again,
  // not the main browser window.
  ui_test_utils::WaitUntilBrowserBecomeActive(pwa_browser);
  EXPECT_TRUE(pwa_browser->GetWindow()->IsActive());
  EXPECT_FALSE(browser()->window()->IsActive());
}
#endif  // BUILDFLAG(IS_MAC)
