// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/metrics/desktop_session_duration/chrome_visibility_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace metrics {

// ChromeVisibilityObserver implementation for the unit test below. This needs
// to be a separate class because it has a different lifetime than
// InProcessBrowserTest.
class ChromeVisibilityObserverInteractiveTestImpl
    : public ChromeVisibilityObserver {
 public:
  ChromeVisibilityObserverInteractiveTestImpl() {
    SetVisibilityGapTimeoutForTesting(base::TimeDelta());
  }

  bool is_active() const { return is_active_; }

 private:
  // ChromeVisibilityObserver:
  void SendVisibilityChangeEvent(bool active,
                                 base::TimeDelta time_ago) override {
    is_active_ = active;
  }

  bool is_active_ = false;
};

// Lifetime manager for ChromeVisibilityObserverInteractiveTestImpl. Ensures
// that ChromeVisibilityObserver is created and destroyed at the correct times
// in browser process startup and shutdown.
class ChromeVisibilityObserverInteractiveTestImplLifetimeManager
    : public ChromeBrowserMainExtraParts {
 public:
  ChromeVisibilityObserverInteractiveTestImplLifetimeManager(
      base::OnceClosure create_impl_callback,
      base::OnceClosure destroy_impl_callback)
      : create_impl_callback_(std::move(create_impl_callback)),
        destroy_impl_callback_(std::move(destroy_impl_callback)) {}

  // ChromeBrowserMainExtraParts:
  void PreCreateThreads() override {
    // Runs after BrowserProcessImpl::Init() - ie, after
    // g_browser_process->features_ is created and initialized - but before a
    // Browser is created. This is important because ChromeVisibilityObserver
    // depends on the global GlobalBrowsersCollection instance, which is a
    // member of g_browser_process->features_, and we want
    // ChromeVisibilityObserver to be able to observe the first Browser
    // creation.
    std::move(create_impl_callback_).Run();
  }
  void PostMainMessageLoopRun() override {
    // Runs before g_browser_process shutdown begins (ie, before
    // g_browser_process->features_ teardown begins), and thus ensures that
    // ChromeBrowserMainExtraParts is cleaned up before its dependency,
    // GlobalBrowsersCollection.
    std::move(destroy_impl_callback_).Run();
  }

 private:
  base::OnceClosure create_impl_callback_;
  base::OnceClosure destroy_impl_callback_;
};

// Test class for |ChromeVisibilityObserver|.
class ChromeVisibilityObserverInteractiveTest : public InProcessBrowserTest {
 public:
  ChromeVisibilityObserverInteractiveTest() = default;

  ChromeVisibilityObserverInteractiveTest(
      const ChromeVisibilityObserverInteractiveTest&) = delete;
  ChromeVisibilityObserverInteractiveTest& operator=(
      const ChromeVisibilityObserverInteractiveTest&) = delete;

  // InProcessBrowserTest:
  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    ChromeBrowserMainParts* chrome_browser_main_parts =
        static_cast<ChromeBrowserMainParts*>(parts);
    chrome_browser_main_parts->AddParts(
        std::make_unique<
            ChromeVisibilityObserverInteractiveTestImplLifetimeManager>(
            base::BindOnce(
                &ChromeVisibilityObserverInteractiveTest::create_impl,
                base::Unretained(this)),
            base::BindOnce(
                &ChromeVisibilityObserverInteractiveTest::destroy_impl,
                base::Unretained(this))));
  }

  bool is_active() const { return impl_->is_active(); }

  // Waits for |is_active_| to be equal to |active| by processing tasks in 1
  // second intervals, up to 3 seconds. This is to account for scenarios when
  // two tasks are sent out very quickly, but the test only had time to process
  // the first one. For example, when switching between two windows, two tasks
  // are sent out very quickly: one that sets |is_active_| to false, and then
  // one that sets it back to true. In production, this is accounted for with
  // ChromeVisibilityObserver::visibility_gap_timeout_.
  void WaitForActive(bool active) {
#if BUILDFLAG(IS_MAC)
    content::HandleMissingKeyWindow();
#endif

    for (size_t i = 0; is_active() != active && i < 3; ++i) {
      base::RunLoop run_loop;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), base::Seconds(1));
      run_loop.Run();
    }
    EXPECT_EQ(is_active(), active);
  }

 private:
  // Callbacks for `ChromeVisibilityObserverInteractiveTestImplLifetimeManager`
  // to create and destroy `impl_` at the proper time in browser process
  // startup and teardown.
  void create_impl() {
    impl_ = std::make_unique<ChromeVisibilityObserverInteractiveTestImpl>();
  }
  void destroy_impl() { impl_.reset(); }

  std::unique_ptr<ChromeVisibilityObserverInteractiveTestImpl> impl_;
};

// This test doesn't check whether switching between browser windows results in
// separate sessions or not.
IN_PROC_BROWSER_TEST_F(ChromeVisibilityObserverInteractiveTest,
                       VisibilityTest) {
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
