// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ash_session_restore_page_load_metrics_observer.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/app_restore/app_restore_test_util.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
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
class FirstInputDelayBarrier : public BrowserCollectionObserver,
                               public TabStripModelObserver {
 public:
  FirstInputDelayBarrier() {
    if (BrowserWindowInterface* const active_browser =
            GetLastActiveBrowserWindowInterfaceWithAnyProfile()) {
      OnBrowserCreatedInternal(active_browser);
    }
    browser_collection_observation_.Observe(
        GlobalBrowserCollection::GetInstance());
  }

  FirstInputDelayBarrier(const FirstInputDelayBarrier&) = delete;
  FirstInputDelayBarrier& operator=(const FirstInputDelayBarrier&) = delete;
  ~FirstInputDelayBarrier() override = default;

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
  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override {
    OnBrowserCreatedInternal(browser);
  }

  void OnBrowserCreatedInternal(BrowserWindowInterface* browser) {
    TabStripModel* const tab_strip_model = browser->GetTabStripModel();
    if (content::WebContents* const active_contents =
            tab_strip_model->GetActiveWebContents()) {
      ObserveWebContents(active_contents);
    }
    tab_strip_model->AddObserver(this);
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

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

// Lifetime manager for FirstInputDelayBarrier. Ensures that the
// BrowserCollectionObserver is created and destroyed at the correct times in
// browser process startup and shutdown.
class FirstInputDelayBarrierLifetimeManager
    : public ChromeBrowserMainExtraParts {
 public:
  FirstInputDelayBarrierLifetimeManager(
      base::OnceClosure create_barrier_callback,
      base::OnceClosure destroy_barrier_callback)
      : create_barrier_callback_(std::move(create_barrier_callback)),
        destroy_barrier_callback_(std::move(destroy_barrier_callback)) {}

  // ChromeBrowserMainExtraParts:
  void PreCreateThreads() override {
    // Runs after BrowserProcessImpl::Init() - ie, after
    // g_browser_process->features_ is created and initialized - but before a
    // Browser is created. This is important because FirstInputDelayBarrier
    // depends on the global GlobalBrowsersCollection instance, which is a
    // member of g_browser_process->features_, and we want
    // FirstInputDelayBarrier to be able to observe the first Browser creation.
    std::move(create_barrier_callback_).Run();
  }
  void PostMainMessageLoopRun() override {
    // Runs before g_browser_process shutdown begins (ie, before
    // g_browser_process->features_ teardown begins), and thus ensures that
    // ChromeBrowserMainExtraParts is cleaned up before its dependency,
    // GlobalBrowsersCollection.
    std::move(destroy_barrier_callback_).Run();
  }

 private:
  base::OnceClosure create_barrier_callback_;
  base::OnceClosure destroy_barrier_callback_;
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

  // InProcessBrowserTest:
  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    ChromeBrowserMainParts* chrome_browser_main_parts =
        static_cast<ChromeBrowserMainParts*>(parts);
    chrome_browser_main_parts->AddParts(
        std::make_unique<FirstInputDelayBarrierLifetimeManager>(
            base::BindOnce(
                &AshSessionRestorePageLoadMetricsObserverTest::create_barrier,
                base::Unretained(this)),
            base::BindOnce(
                &AshSessionRestorePageLoadMetricsObserverTest::destroy_barrier,
                base::Unretained(this))));
  }

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

  // Opens a browser window manually, navigates to a test url, and ensures
  // first input delay is not recorded since the browser window is not from a
  // session restore.
  void RunFirstInputDelaySetupTest() {
    ASSERT_TRUE(GlobalBrowserCollection::GetInstance()->IsEmpty());

    CreateBrowser(ProfileManager::GetActiveUserProfile());

    BrowserWindowInterface* const active_browser =
        GetLastActiveBrowserWindowInterfaceWithAnyProfile();
    ASSERT_TRUE(active_browser);
    content::WebContents* const web_contents =
        active_browser->GetTabStripModel()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        active_browser, embedded_test_server()->GetURL(kTestUrlRelativePath)));
    SimulateMouseClick(web_contents);

    ASSERT_TRUE(first_input_delay_barrier_->Wait(web_contents));

    histogram_tester_.ExpectTotalCount(
        AshSessionRestorePageLoadMetricsObserver::kFirstInputDelayName, 0);

    // Immediate save to full restore file to bypass the 2.5 second throttle.
    AppLaunchInfoSaveWaiter::Wait();
  }

  base::HistogramTester histogram_tester_;
  std::unique_ptr<FirstInputDelayBarrier> first_input_delay_barrier_;

 private:
  // Callbacks for `FirstInputDelayBarrierLifetimeManager` to create and
  // destroy `first_input_delay_barrier_` at the proper time in browser
  // process startup and teardown.
  void create_barrier() {
    first_input_delay_barrier_ = std::make_unique<FirstInputDelayBarrier>();
  }
  void destroy_barrier() { first_input_delay_barrier_.reset(); }
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
  BrowserWindowInterface* const restored_browser =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  ASSERT_TRUE(restored_browser);

  content::WebContents* const web_contents =
      restored_browser->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SimulateMouseClick(web_contents);
  ASSERT_TRUE(first_input_delay_barrier_->Wait(web_contents));

  histogram_tester_.ExpectTotalCount(
      AshSessionRestorePageLoadMetricsObserver::kFirstInputDelayName, 1);
}

IN_PROC_BROWSER_TEST_F(AshSessionRestorePageLoadMetricsObserverTest,
                       PRE_DoesNotRecordFirstInputDelayForManualWindow) {
  RunFirstInputDelaySetupTest();
}

// Browser window is restored during test setup, user manually opens up
// another window and interacts with it. First input delay should not be
// recorded since it wasn't for the restored window.
IN_PROC_BROWSER_TEST_F(AshSessionRestorePageLoadMetricsObserverTest,
                       DoesNotRecordFirstInputDelayForManualWindow) {
  BrowserWindowInterface* const restored_browser =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  ASSERT_TRUE(restored_browser);

  content::WebContents* const restored_web_contents =
      restored_browser->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(restored_web_contents);
  ASSERT_EQ(restored_web_contents->GetVisibleURL().GetPath(),
            kTestUrlRelativePath);

  Browser* const manual_browser =
      CreateBrowser(ProfileManager::GetActiveUserProfile());
  ASSERT_TRUE(manual_browser);
  ASSERT_EQ(GetLastActiveBrowserWindowInterfaceWithAnyProfile(),
            manual_browser);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      manual_browser, embedded_test_server()->GetURL(kTestUrlRelativePath)));

  content::WebContents* const manual_web_contents =
      manual_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(manual_web_contents);

  SimulateMouseClick(manual_web_contents);
  ASSERT_TRUE(first_input_delay_barrier_->Wait(manual_web_contents));

  // Even after switching back to the restored window, first input delay
  // should not be recorded since the first input went to the manually opened
  // window.
  restored_browser->GetWindow()->Activate();
  SimulateMouseClick(restored_web_contents);
  ASSERT_TRUE(first_input_delay_barrier_->Wait(restored_web_contents));

  histogram_tester_.ExpectTotalCount(
      AshSessionRestorePageLoadMetricsObserver::kFirstInputDelayName, 0);
}

IN_PROC_BROWSER_TEST_F(AshSessionRestorePageLoadMetricsObserverTest,
                       PRE_DoesNotRecordFirstInputDelayForManualTab) {
  RunFirstInputDelaySetupTest();
}

// Browser window is restored during test setup, user manually opens up
// another tab and interacts with it. First input delay should not be recorded
// since it wasn't for the restored window.
IN_PROC_BROWSER_TEST_F(AshSessionRestorePageLoadMetricsObserverTest,
                       DoesNotRecordFirstInputDelayForManualTab) {
  BrowserWindowInterface* const browser =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  ASSERT_TRUE(browser);

  TabStripModel* const tab_strip_model = browser->GetTabStripModel();
  ASSERT_TRUE(tab_strip_model);
  content::WebContents* const restored_web_contents =
      tab_strip_model->GetActiveWebContents();
  ASSERT_TRUE(restored_web_contents);
  ASSERT_EQ(restored_web_contents->GetVisibleURL().GetPath(),
            kTestUrlRelativePath);
  const int restored_tab_index = tab_strip_model->active_index();
  ASSERT_TRUE(AddTabAtIndexToBrowser(
      browser->GetBrowserForMigrationOnly(), restored_tab_index + 1,
      embedded_test_server()->GetURL(kTestUrlRelativePath),
      ui::PAGE_TRANSITION_TYPED));

  content::WebContents* const manual_web_contents =
      tab_strip_model->GetActiveWebContents();
  ASSERT_TRUE(manual_web_contents);
  ASSERT_NE(manual_web_contents, restored_web_contents);

  SimulateMouseClick(manual_web_contents);
  ASSERT_TRUE(first_input_delay_barrier_->Wait(manual_web_contents));

  // Even after switching back to the restored tab, first input delay should
  // not be recorded since the first input went to the manually opened tab.
  tab_strip_model->ActivateTabAt(restored_tab_index);
  ASSERT_EQ(tab_strip_model->GetActiveWebContents(), restored_web_contents);
  SimulateMouseClick(restored_web_contents);
  ASSERT_TRUE(first_input_delay_barrier_->Wait(restored_web_contents));

  histogram_tester_.ExpectTotalCount(
      AshSessionRestorePageLoadMetricsObserver::kFirstInputDelayName, 0);
}

}  // namespace
}  // namespace ash
