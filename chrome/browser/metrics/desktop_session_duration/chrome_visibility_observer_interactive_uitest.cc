// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/chrome_visibility_observer.h"

#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

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
// Disabled for being flaky. crbug.com/1311773
IN_PROC_BROWSER_TEST_F(ChromeVisibilityObserverInteractiveTest,
                       DISABLED_VisibilityTest) {
  // Observer should now be active as there is one active browser.
  EXPECT_TRUE(is_active());

// BrowserWindow::Deactivate() not implemented on Mac (https://crbug.com/51364).
#if !BUILDFLAG(IS_MAC)
  // Deactivating and activating the browser should affect the observer
  // accordingly.
  browser()->window()->Deactivate();
  EXPECT_FALSE(is_active());
  browser()->window()->Activate();
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  EXPECT_TRUE(is_active());
#endif  // !BUILDFLAG(IS_MAC)

  // Creating and closing new browsers should keep the observer active.
  Browser* new_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(new_browser));
  EXPECT_TRUE(is_active());

  Browser* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(incognito_browser));
  EXPECT_TRUE(is_active());

  CloseBrowserSynchronously(incognito_browser);
  EXPECT_TRUE(is_active());

  CloseBrowserSynchronously(new_browser);
  EXPECT_TRUE(is_active());

  // Closing last browser should set observer not active.
  CloseBrowserSynchronously(browser());
  EXPECT_FALSE(is_active());
}

}  // namespace metrics
