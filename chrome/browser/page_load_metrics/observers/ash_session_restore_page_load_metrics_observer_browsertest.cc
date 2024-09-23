// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ash_session_restore_page_load_metrics_observer.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/app_restore/app_restore_test_util.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace ash {
namespace {

// Uses //chrome/test/data/simple.html as a sample web-page to load, restore,
// and test first input delay.
constexpr char kTestUrlRelativePath[] = "/simple.html";

// Blocks until first input delay is received for a given `WebContents`. Used
// after simulating a user input (ex: a mouse click), and before checking that
// the user input had the intended effect.
class FirstInputDelayBarrier : public BrowserListObserver,
                               public TabStripModelObserver {
 public:
  FirstInputDelayBarrier() {
    if (BrowserList::GetInstance()->GetLastActive()) {
      OnBrowserAdded(BrowserList::GetInstance()->GetLastActive());
    }
    BrowserList::AddObserver(this);
  }

  FirstInputDelayBarrier(const FirstInputDelayBarrier&) = delete;
  FirstInputDelayBarrier& operator=(const FirstInputDelayBarrier&) = delete;

  ~FirstInputDelayBarrier() override { BrowserList::RemoveObserver(this); }

  // The incoming `web_contents` must have been activated in the past, or this
  // will fail and return `false`.
  bool Wait(content::WebContents* web_contents) {
    const uintptr_t web_contents_ptr = AsMapKey(web_contents);
    if (!waiters_.contains(web_contents_ptr)) {
      return false;
    }
    waiters_.at(web_contents_ptr)->Wait();
    // In case the `PageLoadMetricsTestWaiter` receives the first input delay
    // signal before `AshSessionRestorePageLoadMetricsObserver` (since both
    // are listening for it), run the message loop to ensure
    // `AshSessionRestorePageLoadMetricsObserver` has received the notification
    // before returning control to the caller.
    base::RunLoop().RunUntilIdle();
    return true;
  }

 private:
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    if (browser->tab_strip_model()->GetActiveWebContents()) {
      ObserveWebContents(browser->tab_strip_model()->GetActiveWebContents());
    }
    browser->tab_strip_model()->AddObserver(this);
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (selection.new_contents) {
      ObserveWebContents(selection.new_contents);
    }
  }

  void ObserveWebContents(content::WebContents* web_contents) {
    const uintptr_t web_contents_ptr = AsMapKey(web_contents);
    if (waiters_.contains(web_contents_ptr)) {
      return;
    }
    waiters_[web_contents_ptr] =
        std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
            web_contents);
    waiters_.at(web_contents_ptr)
        ->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstInputDelay);
  }

  uintptr_t AsMapKey(content::WebContents* web_contents) const {
    return reinterpret_cast<uintptr_t>(web_contents);
  }

  base::flat_map<uintptr_t,
                 std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>>
      waiters_;
};

class AshSessionRestorePageLoadMetricsObserverTest
    : public InProcessBrowserTest {
 protected:
  AshSessionRestorePageLoadMetricsObserverTest() {
    set_launch_browser_for_testing(nullptr);
    // Prevents an "about:blank" tab being opened over the restored browser
    // tab and incorrectly receiving the first input delay signal.
    set_open_about_blank_on_browser_launch(false);
  }
  AshSessionRestorePageLoadMetricsObserverTest(
      const AshSessionRestorePageLoadMetricsObserverTest&) = delete;
  AshSessionRestorePageLoadMetricsObserverTest& operator=(
      const AshSessionRestorePageLoadMetricsObserverTest&) = delete;
  ~AshSessionRestorePageLoadMetricsObserverTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    auto* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
    prefs->SetInteger(prefs::kRestoreAppsAndPagesPrefName,
                      static_cast<int>(full_restore::RestoreOption::kAlways));

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SimulateMouseClick(content::WebContents* web_contents) {
    // We should wait for the main frame's hit-test data to be ready before
    // sending the click event below to avoid flakiness.
    content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
    // Ensure the compositor thread is ready for mouse events.
    content::MainThreadFrameObserver frame_observer(
        web_contents->GetRenderWidgetHostView()->GetRenderWidgetHost());
    frame_observer.Wait();
    content::SimulateMouseClick(web_contents, 0,
                                blink::WebMouseEvent::Button::kLeft);
  }

  // Opens a browser window manually, navigates to a test url, and ensures first
  // input delay is not recorded since the browser window is not from a session
  // restore.
  void RunFirstInputDelaySetupTest() {
    ASSERT_TRUE(BrowserList::GetInstance()->empty());

    CreateBrowser(ProfileManager::GetActiveUserProfile());

    ASSERT_TRUE(BrowserList::GetInstance()->GetLastActive());
    content::WebContents* const web_contents = BrowserList::GetInstance()
                                                   ->GetLastActive()
                                                   ->tab_strip_model()
                                                   ->GetActiveWebContents();
    ASSERT_TRUE(web_contents);

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        BrowserList::GetInstance()->GetLastActive(),
        embedded_test_server()->GetURL(kTestUrlRelativePath)));
    SimulateMouseClick(web_contents);

    ASSERT_TRUE(first_input_delay_barrier_.Wait(web_contents));

    histogram_tester_.ExpectTotalCount(
        AshSessionRestorePageLoadMetricsObserver::kFirstInputDelayName, 0);

    // Immediate save to full restore file to bypass the 2.5 second throttle.
    AppLaunchInfoSaveWaiter::Wait();
  }

  base::HistogramTester histogram_tester_;
  FirstInputDelayBarrier first_input_delay_barrier_;
};

// Creates browser window that will be restored in the main test.
IN_PROC_BROWSER_TEST_F(AshSessionRestorePageLoadMetricsObserverTest,
                       PRE_RecordsFirstInputDelay) {
  RunFirstInputDelaySetupTest();
}

// Browser window is restored during test setup, so first input delay should
// be recorded.
IN_PROC_BROWSER_TEST_F(AshSessionRestorePageLoadMetricsObserverTest,
                       RecordsFirstInputDelay) {
  Browser* const restored_browser = BrowserList::GetInstance()->GetLastActive();
  ASSERT_TRUE(restored_browser);

  content::WebContents* const web_contents =
      restored_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SimulateMouseClick(web_contents);
  ASSERT_TRUE(first_input_delay_barrier_.Wait(web_contents));

  histogram_tester_.ExpectTotalCount(
      AshSessionRestorePageLoadMetricsObserver::kFirstInputDelayName, 1);
}

IN_PROC_BROWSER_TEST_F(AshSessionRestorePageLoadMetricsObserverTest,
                       PRE_DoesNotRecordFirstInputDelayForManualWindow) {
  RunFirstInputDelaySetupTest();
}

// Browser window is restored during test setup, user manually opens up another
// window and interacts with it. First input delay should not be recorded since
// it wasn't for the restored window.
IN_PROC_BROWSER_TEST_F(AshSessionRestorePageLoadMetricsObserverTest,
                       DoesNotRecordFirstInputDelayForManualWindow) {
  Browser* const restored_browser = BrowserList::GetInstance()->GetLastActive();
  ASSERT_TRUE(restored_browser);

  content::WebContents* const restored_web_contents =
      restored_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(restored_web_contents);
  ASSERT_EQ(restored_web_contents->GetVisibleURL().path(),
            kTestUrlRelativePath);

  Browser* const manual_browser =
      CreateBrowser(ProfileManager::GetActiveUserProfile());
  ASSERT_TRUE(manual_browser);
  ASSERT_EQ(BrowserList::GetInstance()->GetLastActive(), manual_browser);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      manual_browser, embedded_test_server()->GetURL(kTestUrlRelativePath)));

  content::WebContents* const manual_web_contents =
      manual_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(manual_web_contents);

  SimulateMouseClick(manual_web_contents);
  ASSERT_TRUE(first_input_delay_barrier_.Wait(manual_web_contents));

  // Even after switching back to the restored window, first input delay should
  // not be recorded since the first input went to the manually opened window.
  restored_browser->window()->Activate();
  SimulateMouseClick(restored_web_contents);
  ASSERT_TRUE(first_input_delay_barrier_.Wait(restored_web_contents));

  histogram_tester_.ExpectTotalCount(
      AshSessionRestorePageLoadMetricsObserver::kFirstInputDelayName, 0);
}

IN_PROC_BROWSER_TEST_F(AshSessionRestorePageLoadMetricsObserverTest,
                       PRE_DoesNotRecordFirstInputDelayForManualTab) {
  RunFirstInputDelaySetupTest();
}

// Browser window is restored during test setup, user manually opens up another
// tab and interacts with it. First input delay should not be recorded since
// it wasn't for the restored window.
IN_PROC_BROWSER_TEST_F(AshSessionRestorePageLoadMetricsObserverTest,
                       DoesNotRecordFirstInputDelayForManualTab) {
  Browser* const browser = BrowserList::GetInstance()->GetLastActive();
  ASSERT_TRUE(browser);

  content::WebContents* const restored_web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(restored_web_contents);
  ASSERT_EQ(restored_web_contents->GetVisibleURL().path(),
            kTestUrlRelativePath);
  const int restored_tab_index = browser->tab_strip_model()->active_index();
  ASSERT_TRUE(AddTabAtIndexToBrowser(
      browser, restored_tab_index + 1,
      embedded_test_server()->GetURL(kTestUrlRelativePath),
      ui::PAGE_TRANSITION_TYPED));

  content::WebContents* const manual_web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(manual_web_contents);
  ASSERT_NE(manual_web_contents, restored_web_contents);

  SimulateMouseClick(manual_web_contents);
  ASSERT_TRUE(first_input_delay_barrier_.Wait(manual_web_contents));

  // Even after switching back to the restored tab, first input delay should
  // not be recorded since the first input went to the manually opened tab.
  browser->tab_strip_model()->ActivateTabAt(restored_tab_index);
  ASSERT_EQ(browser->tab_strip_model()->GetActiveWebContents(),
            restored_web_contents);
  SimulateMouseClick(restored_web_contents);
  ASSERT_TRUE(first_input_delay_barrier_.Wait(restored_web_contents));

  histogram_tester_.ExpectTotalCount(
      AshSessionRestorePageLoadMetricsObserver::kFirstInputDelayName, 0);
}

}  // namespace
}  // namespace ash
