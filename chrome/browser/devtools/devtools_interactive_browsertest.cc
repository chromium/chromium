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
#include "content/public/test/browser_test.h"
#include "ui/display/types/display_constants.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

// Encapsulates waiting for the browser window to change state. This is
// needed for example on Chrome desktop linux, where window state change is done
// asynchronously as an event received from a different process.
class CheckWaiter {
 public:
  CheckWaiter(base::RepeatingCallback<bool()> callback, bool expected)
      : callback_(callback),
        expected_(expected),
        timeout_(base::TimeTicks::Now() + base::Seconds(1)) {}

  CheckWaiter(const CheckWaiter&) = delete;
  CheckWaiter& operator=(const CheckWaiter&) = delete;

  ~CheckWaiter() = default;

  // Blocks until the browser window becomes maximized.
  void Wait() {
    if (Check())
      return;

    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  bool Check() {
    if (callback_.Run() != expected_ && base::TimeTicks::Now() < timeout_) {
      // Check again after a short timeout. Important: Don't use an immediate
      // task to check again, because the pump would be allowed to run it
      // immediately without processing system events (system events are
      // required for the state to change).
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(base::IgnoreResult(&CheckWaiter::Check),
                         base::Unretained(this)),
          TestTimeouts::tiny_timeout());
      return false;
    }

    // Quit the run_loop to end the wait.
    if (!quit_.is_null())
      std::move(quit_).Run();
    return true;
  }

  base::RepeatingCallback<bool()> callback_;
  bool expected_;
  const base::TimeTicks timeout_;
  // The waiter's RunLoop quit closure.
  base::RepeatingClosure quit_;
};

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
    CheckWaiter(base::BindRepeating(&BrowserWindow::IsMaximized,
                                    base::Unretained(browser()->window())),
                maximized)
        .Wait();
    EXPECT_EQ(maximized, browser()->window()->IsMaximized());
  }

  void CheckIsMinimized(bool minimized) {
    CheckWaiter(base::BindRepeating(&BrowserWindow::IsMinimized,
                                    base::Unretained(browser()->window())),
                minimized)
        .Wait();
    EXPECT_EQ(minimized, browser()->window()->IsMinimized());
  }

  void CheckIsFullscreen(bool fullscreen) {
    CheckWaiter(base::BindRepeating(&BrowserWindow::IsFullscreen,
                                    base::Unretained(browser()->window())),
                fullscreen)
        .Wait();
    EXPECT_EQ(fullscreen, browser()->window()->IsFullscreen());
  }

  bool IsWindowBoundsEqual(gfx::Rect expected) {
    return browser()->window()->GetBounds() == expected;
  }

  void CheckWindowBounds(gfx::Rect expected) {
    CheckWaiter(
        base::BindRepeating(&DevToolsManagerDelegateTest::IsWindowBoundsEqual,
                            base::Unretained(this), expected),
        true)
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
// MacViews does not yet implement maximized windows: https://crbug.com/836327
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
// MacViews does not yet implement maximized windows: https://crbug.com/836327
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
// MacViews does not yet implement maximized windows: https://crbug.com/836327
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
// MacViews does not yet implement maximized windows: https://crbug.com/836327
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
      GURL(), EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE, display::kInvalidDisplayId);
  CheckIsFullscreen(true);
  SendCommand("normal");
  CheckIsFullscreen(false);
}
