// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/chrome_visibility_observer.h"

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace metrics {
// Test class for |ChromeVisibilityObserver|.
class ChromeVisibilityObserverInteractiveTest
    : public metrics::ChromeVisibilityObserver,
      public InProcessBrowserTest {
 public:
  ChromeVisibilityObserverInteractiveTest() : is_active_(false) {
    SetVisibilityGapTimeoutForTesting(base::TimeDelta());
  }

  ChromeVisibilityObserverInteractiveTest(
      const ChromeVisibilityObserverInteractiveTest&) = delete;
  ChromeVisibilityObserverInteractiveTest& operator=(
      const ChromeVisibilityObserverInteractiveTest&) = delete;

  bool is_active() const { return is_active_; }

  // Waits for |is_active_| to be equal to |active| by processing tasks in 1
  // second intervals, up to 3 seconds. This is to account for scenarios when
  // two tasks are sent out very quickly, but the test only had time to process
  // the first one. For example, when switching between two windows, two tasks
  // are sent out very quickly: one that sets |is_active_| to false, and then
  // one that sets it back to true. In production, this is accounted for with
  // ChromeVisibilityObserver::visibility_gap_timeout_.
  void WaitForActive(bool active) {
    for (size_t i = 0; is_active_ != active && i < 3; ++i) {
      base::RunLoop run_loop;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), base::Seconds(1));
      run_loop.Run();
    }
    EXPECT_EQ(is_active_, active);
  }

 private:
  // metrics::ChromeVisibilityObserver:
  void SendVisibilityChangeEvent(bool active,
                                 base::TimeDelta time_ago) override {
    is_active_ = active;
  }

  bool is_active_;
};

// This test doesn't check whether switching between browser windows results in
// separate sessions or not.
IN_PROC_BROWSER_TEST_F(ChromeVisibilityObserverInteractiveTest,
                       VisibilityTest) {
#if BUILDFLAG(IS_MAC)
  if (base::mac::MacOSMajorVersion() >= 13) {
    GTEST_SKIP() << "Broken on macOS 13: https://crbug.com/1447844";
  }
#endif

  // Observer should now be active as there is one active browser.
  WaitForActive(/*active=*/true);

// BrowserWindow::Deactivate() not implemented on Mac (https://crbug.com/51364).
#if !BUILDFLAG(IS_MAC)
  // Deactivating and activating the browser should affect the observer
  // accordingly.
  browser()->window()->Deactivate();
  WaitForActive(/*active=*/false);
  browser()->window()->Activate();
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  WaitForActive(/*active=*/true);
#endif  // !BUILDFLAG(IS_MAC)

  // Creating and closing new browsers should keep the observer active.
  Browser* new_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(new_browser));
  WaitForActive(/*active=*/true);

  Browser* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(incognito_browser));
  WaitForActive(/*active=*/true);

  CloseBrowserSynchronously(incognito_browser);
  WaitForActive(/*active=*/true);

  CloseBrowserSynchronously(new_browser);
  WaitForActive(/*active=*/true);

  // Closing last browser should set observer not active.
  CloseBrowserSynchronously(browser());
  WaitForActive(/*active=*/false);
}

}  // namespace metrics
