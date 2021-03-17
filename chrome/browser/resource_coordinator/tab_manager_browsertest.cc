// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/util/memory_pressure/fake_memory_pressure_monitor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

using content::OpenURLParams;

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)

namespace resource_coordinator {

namespace {

constexpr base::TimeDelta kShortDelay = base::TimeDelta::FromSeconds(1);

bool IsTabDiscarded(content::WebContents* web_contents) {
  return TabLifecycleUnitExternal::FromWebContents(web_contents)->IsDiscarded();
}

class ExpectStateTransitionObserver : public LifecycleUnitObserver {
 public:
  ExpectStateTransitionObserver(LifecycleUnit* lifecyle_unit,
                                LifecycleUnitState expected_state)
      : lifecycle_unit_(lifecyle_unit), expected_state_(expected_state) {
    lifecycle_unit_->AddObserver(this);
  }

  ~ExpectStateTransitionObserver() override {
    lifecycle_unit_->RemoveObserver(this);
  }

  void AllowState(LifecycleUnitState allowed_state) {
    allowed_states_.insert(allowed_state);
  }

  void Wait() {
    EXPECT_NE(expected_state_, lifecycle_unit_->GetState());
    run_loop_.Run();
    EXPECT_EQ(expected_state_, lifecycle_unit_->GetState());
  }

 private:
  // LifecycleUnitObserver:
  void OnLifecycleUnitStateChanged(
      LifecycleUnit* lifecycle_unit,
      LifecycleUnitState last_state,
      LifecycleUnitStateChangeReason reason) override {
    EXPECT_EQ(lifecycle_unit, lifecycle_unit_);
    if (lifecycle_unit_->GetState() == expected_state_) {
      run_loop_.Quit();
    } else {
      LOG(ERROR) << "transition to state "
                 << static_cast<int>(lifecycle_unit_->GetState());
      EXPECT_TRUE(base::Contains(allowed_states_, lifecycle_unit_->GetState()));
    }
  }

  LifecycleUnit* const lifecycle_unit_;
  const LifecycleUnitState expected_state_;
  std::set<LifecycleUnitState> allowed_states_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ExpectStateTransitionObserver);
};

class DiscardWaiter : public TabLifecycleObserver {
 public:
  DiscardWaiter() { TabLifecycleUnitExternal::AddTabLifecycleObserver(this); }

  ~DiscardWaiter() override {
    TabLifecycleUnitExternal::RemoveTabLifecycleObserver(this);
  }

  void Wait() { run_loop_.Run(); }

 private:
  void OnDiscardedStateChange(content::WebContents* contents,
                              LifecycleUnitDiscardReason reason,
                              bool is_discarded) override {
    if (is_discarded)
      run_loop_.Quit();
  }

  base::RunLoop run_loop_;
};

}  // namespace

class TabManagerTest : public InProcessBrowserTest {
 public:
  TabManagerTest() : scoped_set_tick_clock_for_testing_(&test_clock_) {
    // Start with a non-null TimeTicks, as there is no discard protection for
    // a tab with a null focused timestamp.
    test_clock_.Advance(kShortDelay);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    // To avoid flakes when focus changes, set the active tab strip model
    // explicitly.
    GetTabLifecycleUnitSource()->SetFocusedTabStripModelForTesting(tsm());
  }

  void OpenTwoTabs(const GURL& first_url, const GURL& second_url) {
    // Open two tabs. Wait for both of them to load.
    content::WindowedNotificationObserver load1(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    OpenURLParams open1(first_url, content::Referrer(),
                        WindowOpenDisposition::CURRENT_TAB,
                        ui::PAGE_TRANSITION_TYPED, false);
    content::WebContents* web_contents = browser()->OpenURL(open1);
    load1.Wait();

    content::WindowedNotificationObserver load2(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    OpenURLParams open2(second_url, content::Referrer(),
                        WindowOpenDisposition::NEW_BACKGROUND_TAB,
                        ui::PAGE_TRANSITION_TYPED, false);
    web_contents = browser()->OpenURL(open2);
    load2.Wait();

    ASSERT_EQ(2, tsm()->count());
  }

  TabManager* tab_manager() { return g_browser_process->GetTabManager(); }
  TabStripModel* tsm() { return browser()->tab_strip_model(); }

  content::WebContents* GetWebContentsAt(int index) {
    return tsm()->GetWebContentsAt(index);
  }

  LifecycleUnit* GetLifecycleUnitAt(int index) {
    return GetTabLifecycleUnitSource()->GetTabLifecycleUnit(
        GetWebContentsAt(index));
  }

  util::test::FakeMemoryPressureMonitor fake_memory_pressure_monitor_;
  base::SimpleTestTickClock test_clock_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;
};

class TabManagerTestWithTwoTabs : public TabManagerTest {
 public:
  TabManagerTestWithTwoTabs() = default;

  void SetUpOnMainThread() override {
    TabManagerTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    // Open 2 tabs with default URLs in a focused tab strip.
    OpenTwoTabs(embedded_test_server()->GetURL("/title2.html"),
                embedded_test_server()->GetURL("/title3.html"));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabManagerTestWithTwoTabs);
};

IN_PROC_BROWSER_TEST_F(TabManagerTest, TabManagerBasics) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("a.com", "/title2.html");
  const GURL url3 = embedded_test_server()->GetURL("a.com", "/title3.html");

  // Get three tabs open.

  test_clock_.Advance(kShortDelay);
  NavigateToURLWithDisposition(browser(), url1,
                               WindowOpenDisposition::CURRENT_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  test_clock_.Advance(kShortDelay);
  NavigateToURLWithDisposition(browser(), url1,
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  test_clock_.Advance(kShortDelay);
  NavigateToURLWithDisposition(browser(), url1,
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(3, tsm()->count());

  // Navigate the current (third) tab to a different URL, so we can test
  // back/forward later.
  NavigateToURLWithDisposition(browser(), url2,
                               WindowOpenDisposition::CURRENT_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Navigate the third tab again, such that we have three navigation entries.
  NavigateToURLWithDisposition(browser(), url3,
                               WindowOpenDisposition::CURRENT_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(3, tsm()->count());

  // Advance time so everything is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  // Discard a tab.
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));
  EXPECT_EQ(3, tsm()->count());
  if (base::FeatureList::IsEnabled(features::kTabRanker)) {
    // In testing configs with TabRanker enabled, we don't always know which tab
    // it will kill. But exactly one of the first two tabs should be killed.
    EXPECT_TRUE(IsTabDiscarded(GetWebContentsAt(0)) ^
                IsTabDiscarded(GetWebContentsAt(1)));
  } else {
    // With TabRanker disabled, it should kill the first tab, since it was the
    // oldest and was not selected.
    EXPECT_TRUE(IsTabDiscarded(GetWebContentsAt(0)));
    EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(1)));
  }
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(2)));

  // Run discard again. Both unselected tabs should now be killed.
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));
  EXPECT_EQ(3, tsm()->count());
  EXPECT_TRUE(IsTabDiscarded(GetWebContentsAt(0)));
  EXPECT_TRUE(IsTabDiscarded(GetWebContentsAt(1)));
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(2)));

  // Run discard again. It should not kill the last tab, since it is active.
  EXPECT_FALSE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));
  EXPECT_TRUE(IsTabDiscarded(GetWebContentsAt(0)));
  EXPECT_TRUE(IsTabDiscarded(GetWebContentsAt(1)));
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(2)));

  // Kill the third tab after making second tab active.
  tsm()->ActivateTabAt(1, {TabStripModel::GestureType::kOther});

  // Advance time so everything is urgent discardable again.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  EXPECT_EQ(1, tsm()->active_index());
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(1)));
  tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT);
  EXPECT_TRUE(IsTabDiscarded(GetWebContentsAt(2)));

  // Force creation of the FindBarController.
  browser()->GetFindBarController();

  // Select the first tab.  It should reload.
  chrome::SelectNumberedTab(browser(), 0);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  // Make sure the FindBarController gets the right WebContents.
  EXPECT_EQ(browser()->GetFindBarController()->web_contents(),
            tsm()->GetActiveWebContents());
  EXPECT_EQ(0, tsm()->active_index());
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(1)));
  EXPECT_TRUE(IsTabDiscarded(GetWebContentsAt(2)));

  // Select the third tab. It should reload.
  chrome::SelectNumberedTab(browser(), 2);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(2, tsm()->active_index());
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(1)));
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(2)));

  // Navigate the third tab back twice.  We used to crash here due to
  // crbug.com/121373.
  EXPECT_TRUE(chrome::CanGoBack(browser()));
  EXPECT_FALSE(chrome::CanGoForward(browser()));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(chrome::CanGoBack(browser()));
  EXPECT_TRUE(chrome::CanGoForward(browser()));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_FALSE(chrome::CanGoBack(browser()));
  EXPECT_TRUE(chrome::CanGoForward(browser()));
}

IN_PROC_BROWSER_TEST_F(TabManagerTest, InvalidOrEmptyURL) {
  // Open two tabs. Wait for the foreground one to load but do not wait for the
  // background one.
  NavigateToURLWithDisposition(browser(), GURL(chrome::kChromeUIAboutURL),
                               WindowOpenDisposition::CURRENT_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  NavigateToURLWithDisposition(browser(), GURL(chrome::kChromeUICreditsURL),
                               WindowOpenDisposition::NEW_BACKGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_NONE);

  ASSERT_EQ(2, tsm()->count());

  // This shouldn't be able to discard a tab as the background tab has not yet
  // started loading (its URL is not committed).
  EXPECT_FALSE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::EXTERNAL));

  // Wait for the background tab to load which then allows it to be discarded.
  content::WaitForLoadStop(browser()->tab_strip_model()->GetWebContentsAt(1));
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::EXTERNAL));
}

// Makes sure that the TabDiscardDoneCB callback is called after
// DiscardTabImpl() returns.
IN_PROC_BROWSER_TEST_F(TabManagerTest, TabDiscardDoneCallback) {
  // Open two tabs.
  NavigateToURLWithDisposition(browser(), GURL(chrome::kChromeUIAboutURL),
                               WindowOpenDisposition::CURRENT_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  NavigateToURLWithDisposition(browser(), GURL(chrome::kChromeUICreditsURL),
                               WindowOpenDisposition::NEW_BACKGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_EQ(2, tsm()->count());

  struct CallbackState {
    bool called_ = false;
    void Run() { called_ = true; }
  } callback_state;

  TabManager::TabDiscardDoneCB callback{
      base::BindOnce(&CallbackState::Run, base::Unretained(&callback_state))};
  EXPECT_TRUE(tab_manager()->DiscardTabImpl(
      LifecycleUnitDiscardReason::EXTERNAL, std::move(callback)));
  EXPECT_TRUE(callback_state.called_);
}

// Makes sure that PDF pages are protected.
IN_PROC_BROWSER_TEST_F(TabManagerTest, ProtectPDFPages) {
  // Start the embedded test server so we can get served the required PDF page.
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  embedded_test_server()->StartAcceptingConnections();

  // Get two tabs open, the first one being a PDF page and the second one being
  // the foreground tab.
  GURL url1 = embedded_test_server()->GetURL("/pdf/test.pdf");
  ui_test_utils::NavigateToURL(browser(), url1);

  GURL url2(chrome::kChromeUIAboutURL);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // No discarding should be possible as the only background tab is displaying a
  // PDF page, hence protected.
  EXPECT_FALSE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::EXTERNAL));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Makes sure that recently opened or used tabs are protected.
IN_PROC_BROWSER_TEST_F(TabManagerTest,
                       ProtectRecentlyUsedTabsFromUrgentDiscarding) {
  TabManager* tab_manager = g_browser_process->GetTabManager();

  auto* tsm = browser()->tab_strip_model();

  // Open 2 tabs, the second one being in the background.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIAboutURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(2, tsm->count());

  // Advance the clock for less than the protection time.
  test_clock_.Advance(kBackgroundUrgentProtectionTime / 2);

  // Should not be able to discard a tab.
  ASSERT_FALSE(tab_manager->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));

  // Advance the clock for more than the protection time.
  test_clock_.Advance(kBackgroundUrgentProtectionTime / 2 +
                      base::TimeDelta::FromSeconds(1));

  // Should be able to discard the background tab now.
  EXPECT_TRUE(tab_manager->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));

  // Activate the 2nd tab.
  tsm->ActivateTabAt(1, {TabStripModel::GestureType::kOther});
  EXPECT_EQ(1, tsm->active_index());

  // Advance the clock for less than the protection time.
  test_clock_.Advance(kBackgroundUrgentProtectionTime / 2);

  // Should not be able to urgent discard the tab.
  ASSERT_FALSE(tab_manager->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));

  // But should be able to externally discard the tab.
  EXPECT_TRUE(
      tab_manager->DiscardTabImpl(LifecycleUnitDiscardReason::EXTERNAL));

  // This is necessary otherwise the test crashes in
  // WebContentsData::WebContentsDestroyed.
  tsm->CloseAllTabs();
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Makes sure that tabs using media devices are protected.
IN_PROC_BROWSER_TEST_F(TabManagerTest, ProtectVideoTabs) {
  // Open 2 tabs, the second one being in the background.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIAboutURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  auto* tab = GetWebContentsAt(1);

  // Simulate that a video stream is now being captured.
  blink::MediaStreamDevices video_devices(1);
  video_devices[0] = blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, "fake_media_device",
      "fake_media_device");
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaCaptureDevicesDispatcher::GetInstance();
  dispatcher->SetTestVideoCaptureDevices(video_devices);
  std::unique_ptr<content::MediaStreamUI> video_stream_ui =
      dispatcher->GetMediaStreamCaptureIndicator()->RegisterMediaStream(
          tab, video_devices);
  video_stream_ui->OnStarted(base::OnceClosure(),
                             content::MediaStreamUI::SourceCallback(),
                             /*label=*/std::string(), /*screen_capture_ids=*/{},
                             content::MediaStreamUI::StateChangeCallback());

  // Should not be able to discard a tab.
  ASSERT_FALSE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::EXTERNAL));

  // Remove the video stream.
  video_stream_ui.reset();

  // Should be able to discard the background tab now.
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::EXTERNAL));
}

// Makes sure that tabs using DevTools are protected from discarding.
IN_PROC_BROWSER_TEST_F(TabManagerTest, ProtectDevToolsTabsFromDiscarding) {
  // Get two tabs open, the second one being the foreground tab.
  GURL test_page(ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("simple.html"))));
  ui_test_utils::NavigateToURL(browser(), test_page);
  // Open a DevTools window for the first.
  DevToolsWindow* devtool = DevToolsWindowTesting::OpenDevToolsWindowSync(
      GetWebContentsAt(0), true /* is_docked */);
  EXPECT_TRUE(devtool);

  GURL url2(chrome::kChromeUIAboutURL);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIAboutURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // No discarding should be possible as the only background tab is currently
  // using DevTools.
  EXPECT_FALSE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::EXTERNAL));

  // Close the DevTools window and repeat the test, this time use a non-docked
  // window.
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtool);
  devtool = DevToolsWindowTesting::OpenDevToolsWindowSync(
      GetWebContentsAt(0), false /* is_docked */);
  EXPECT_TRUE(devtool);
  EXPECT_FALSE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::EXTERNAL));

  // Close the DevTools window, ensure that the tab can be discarded.
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtool);
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::EXTERNAL));
}

IN_PROC_BROWSER_TEST_F(TabManagerTest, AutoDiscardable) {
  // Get two tabs open.
  NavigateToURLWithDisposition(browser(), GURL(chrome::kChromeUIAboutURL),
                               WindowOpenDisposition::CURRENT_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  NavigateToURLWithDisposition(browser(), GURL(chrome::kChromeUICreditsURL),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(2, tsm()->count());

  // Set the auto-discardable state of the first tab to false.
  TabLifecycleUnitExternal::FromWebContents(GetWebContentsAt(0))
      ->SetAutoDiscardable(false);

  // Shouldn't discard the tab, since auto-discardable is deactivated.
  EXPECT_FALSE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::EXTERNAL));

  // Reset auto-discardable state to true.
  TabLifecycleUnitExternal::FromWebContents(GetWebContentsAt(0))
      ->SetAutoDiscardable(true);

  // Now it should be able to discard the tab.
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::EXTERNAL));
  EXPECT_TRUE(IsTabDiscarded(GetWebContentsAt(0)));
}

IN_PROC_BROWSER_TEST_F(TabManagerTestWithTwoTabs,
                       UrgentFastShutdownSingleTabProcess) {
  // The Tab Manager should be able to fast-kill a process for the discarded tab
  // on all platforms, as each tab will be running in a separate process by
  // itself regardless of the discard reason.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::NotificationService::AllSources());
  // Advance time so everything is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);
  base::HistogramTester tester;
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));
  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldFastShutdown", true, 1);
  observer.Wait();
}

IN_PROC_BROWSER_TEST_F(TabManagerTest, UrgentFastShutdownSharedTabProcess) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Set max renderers to 1 before opening tabs to force running out of
  // processes and for both these tabs to share a renderer.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);
  OpenTwoTabs(embedded_test_server()->GetURL("a.com", "/title1.html"),
              embedded_test_server()->GetURL("a.com", "/title2.html"));
  EXPECT_EQ(tsm()->GetWebContentsAt(0)->GetMainFrame()->GetProcess(),
            tsm()->GetWebContentsAt(1)->GetMainFrame()->GetProcess());

  // Advance time so everything is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  // The Tab Manager will not be able to fast-kill either of the tabs since they
  // share the same process regardless of the discard reason. An unsafe attempt
  // will be made on some platforms.
  base::HistogramTester tester;
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The unsafe killing attempt will fail for the same reason.
  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldUnsafeFastShutdown", false, 1);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldFastShutdown", false, 1);
}

IN_PROC_BROWSER_TEST_F(TabManagerTest, UrgentFastShutdownWithUnloadHandler) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Disable the protection of recent tabs.
  OpenTwoTabs(embedded_test_server()->GetURL("a.com", "/title1.html"),
              embedded_test_server()->GetURL("/unload.html"));

  // Advance time so everything is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  // The Tab Manager will not be able to safely fast-kill either of the tabs as
  // one of them is current, and the other has an unload handler. An unsafe
  // attempt will be made on some platforms.
  base::HistogramTester tester;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The unsafe attempt for ChromeOS should succeed as ChromeOS ignores unload
  // handlers when in critical condition.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::NotificationService::AllSources());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldUnsafeFastShutdown", true, 1);
  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldFastShutdown", true, 1);
  observer.Wait();
#else
  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldFastShutdown", false, 1);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

IN_PROC_BROWSER_TEST_F(TabManagerTest,
                       UrgentFastShutdownWithBeforeunloadHandler) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Disable the protection of recent tabs.
  OpenTwoTabs(embedded_test_server()->GetURL("a.com", "/title1.html"),
              embedded_test_server()->GetURL("/beforeunload.html"));

  // Advance time so everything is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  // The Tab Manager will not be able to safely fast-kill either of the tabs as
  // one of them is current, and the other has a beforeunload handler. An unsafe
  // attempt will be made on some platforms.
  base::HistogramTester tester;
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The unsafe killing attempt will fail as ChromeOS does not ignore
  // beforeunload handlers.
  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldUnsafeFastShutdown", false, 1);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldFastShutdown", false, 1);
}

// Verifies the following state transitions for a tab:
// - Initial state: ACTIVE
// - Discard(kUrgent): ACTIVE->DISCARDED
// - Navigate: DISCARDED->ACTIVE
//             window.document.wasDiscarded is true
IN_PROC_BROWSER_TEST_F(TabManagerTestWithTwoTabs, TabUrgentDiscardAndNavigate) {
  const char kDiscardedStateJS[] =
      "window.domAutomationController.send("
      "window.document.wasDiscarded);";

  GURL test_page(ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("simple.html"))));
  ui_test_utils::NavigateToURL(browser(), test_page);

  // document.wasDiscarded is false initially.
  bool not_discarded_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContentsAt(0), kDiscardedStateJS, &not_discarded_result));
  EXPECT_FALSE(not_discarded_result);

  // Discard the tab.
  EXPECT_EQ(LifecycleUnitState::ACTIVE, GetLifecycleUnitAt(0)->GetState());
  EXPECT_TRUE(
      GetLifecycleUnitAt(0)->Discard(LifecycleUnitDiscardReason::EXTERNAL));
  EXPECT_EQ(LifecycleUnitState::DISCARDED, GetLifecycleUnitAt(0)->GetState());

  // Here we simulate re-focussing the tab causing reload with navigation,
  // the navigation will reload the tab.
  // TODO(fdoray): Figure out why the test fails if a reload is done instead of
  // a navigation.
  ui_test_utils::NavigateToURL(browser(), test_page);
  EXPECT_EQ(LifecycleUnitState::ACTIVE, GetLifecycleUnitAt(0)->GetState());

  // document.wasDiscarded is true on navigate after discard.
  bool discarded_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContentsAt(0), kDiscardedStateJS, &discarded_result));
  EXPECT_TRUE(discarded_result);
}

IN_PROC_BROWSER_TEST_F(TabManagerTest, DiscardedTabHasNoProcess) {
  GURL test_page(ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("simple.html"))));
  ui_test_utils::NavigateToURL(browser(), test_page);
  content::WebContents* web_contents = tsm()->GetActiveWebContents();

  // The renderer process should be alive at this point.
  content::RenderProcessHost* process =
      web_contents->GetMainFrame()->GetProcess();
  ASSERT_TRUE(process);
  EXPECT_TRUE(process->IsInitializedAndNotDead());
  EXPECT_NE(base::kNullProcessHandle, process->GetProcess().Handle());
  int renderer_id = process->GetID();

  // Discard the tab. This simulates a tab discard.
  TabLifecycleUnitExternal::FromWebContents(web_contents)
      ->DiscardTab(LifecycleUnitDiscardReason::URGENT);
  content::WebContents* new_web_contents = tsm()->GetActiveWebContents();
  EXPECT_NE(new_web_contents, web_contents);
  web_contents = new_web_contents;
  content::RenderProcessHost* new_process =
      web_contents->GetMainFrame()->GetProcess();
  EXPECT_NE(new_process, process);
  EXPECT_NE(new_process->GetID(), renderer_id);
  process = new_process;

  // The renderer process should be dead after a discard.
  EXPECT_EQ(process, web_contents->GetMainFrame()->GetProcess());
  EXPECT_FALSE(process->IsInitializedAndNotDead());
  EXPECT_EQ(base::kNullProcessHandle, process->GetProcess().Handle());

  // Here we simulate re-focussing the tab causing reload with navigation,
  // the navigation will reload the tab.
  ui_test_utils::NavigateToURL(browser(), test_page);

  // Reload should mean that the renderer process is alive now.
  EXPECT_EQ(process, web_contents->GetMainFrame()->GetProcess());
  EXPECT_TRUE(process->IsInitializedAndNotDead());
  EXPECT_NE(base::kNullProcessHandle, process->GetProcess().Handle());
}

IN_PROC_BROWSER_TEST_F(TabManagerTest,
                       TabManagerWasDiscardedCrossSiteSubFrame) {
  const char kDiscardedStateJS[] =
      "window.domAutomationController.send("
      "window.document.wasDiscarded);";
  // Navigate to a page with a cross-site frame.
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/iframe_cross_site.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  // Grab the original frames.
  content::WebContents* contents = tsm()->GetActiveWebContents();
  content::RenderFrameHost* main_frame = contents->GetMainFrame();
  ASSERT_EQ(3u, contents->GetAllFrames().size());
  content::RenderFrameHost* child_frame = contents->GetAllFrames()[1];

  // Sanity check that in this test page the main frame and the
  // subframe are cross-site.
  EXPECT_NE(main_frame->GetLastCommittedURL().GetOrigin(),
            child_frame->GetLastCommittedURL().GetOrigin());
  if (content::AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(main_frame->GetSiteInstance(), child_frame->GetSiteInstance());
    EXPECT_NE(main_frame->GetProcess()->GetID(),
              child_frame->GetProcess()->GetID());
  }

  // document.wasDiscarded is false before discard, on main frame and child
  // frame.
  bool before_discard_mainframe_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      main_frame, kDiscardedStateJS, &before_discard_mainframe_result));
  EXPECT_FALSE(before_discard_mainframe_result);

  bool before_discard_childframe_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      child_frame, kDiscardedStateJS, &before_discard_childframe_result));
  EXPECT_FALSE(before_discard_childframe_result);

  // Discard the tab. This simulates a tab discard.
  TabLifecycleUnitExternal::FromWebContents(contents)->DiscardTab(
      LifecycleUnitDiscardReason::URGENT);

  // Here we simulate re-focussing the tab causing reload with navigation,
  // the navigation will reload the tab.
  // TODO(panicker): Consider adding a test hook on LifecycleUnit when ready.
  ui_test_utils::NavigateToURL(browser(), main_url);

  // Re-assign pointers after discarding, as they've changed.
  contents = tsm()->GetActiveWebContents();
  main_frame = contents->GetMainFrame();
  ASSERT_LE(2u, contents->GetAllFrames().size());
  child_frame = contents->GetAllFrames()[1];

  // document.wasDiscarded is true after discard, on mainframe and childframe.
  bool discarded_mainframe_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      main_frame, kDiscardedStateJS, &discarded_mainframe_result));
  EXPECT_TRUE(discarded_mainframe_result);

  bool discarded_childframe_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      child_frame, kDiscardedStateJS, &discarded_childframe_result));
  EXPECT_TRUE(discarded_childframe_result);

  // Navigate the child frame, wasDiscarded is not set anymore.
  GURL childframe_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(contents, "frame1", childframe_url));
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      contents->GetAllFrames()[1], kDiscardedStateJS,
      &discarded_childframe_result));
  EXPECT_FALSE(discarded_childframe_result);

  // Navigate second child frame cross site.
  GURL second_childframe_url(
      embedded_test_server()->GetURL("d.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(contents, "frame2", second_childframe_url));
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      contents->GetAllFrames()[2], kDiscardedStateJS,
      &discarded_childframe_result));
  EXPECT_FALSE(discarded_childframe_result);

  // Navigate the main frame (same site) again, wasDiscarded is not set anymore.
  ui_test_utils::NavigateToURL(browser(), main_url);
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      main_frame, kDiscardedStateJS, &discarded_mainframe_result));
  EXPECT_FALSE(discarded_mainframe_result);

  // Go back in history and ensure wasDiscarded is still false.
  content::TestNavigationObserver observer(contents);
  contents->GetController().GoBack();
  observer.Wait();
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      main_frame, kDiscardedStateJS, &discarded_mainframe_result));
  EXPECT_FALSE(discarded_mainframe_result);
}

namespace {

// Ensures that |browser| has |num_tabs| open tabs.
void EnsureTabsInBrowser(Browser* browser, int num_tabs) {
  for (int i = 0; i < num_tabs; ++i) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser, GURL(chrome::kChromeUICreditsURL),
        i == 0 ? WindowOpenDisposition::CURRENT_TAB
               : WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  EXPECT_EQ(num_tabs, browser->tab_strip_model()->count());
}

// Creates a browser with |num_tabs| tabs.
Browser* CreateBrowserWithTabs(int num_tabs) {
  Browser* current_browser = BrowserList::GetInstance()->GetLastActive();
  chrome::NewWindow(current_browser);
  Browser* new_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_NE(new_browser, current_browser);

  // To avoid flakes when focus changes, set the active tab strip model
  // explicitly.
  GetTabLifecycleUnitSource()->SetFocusedTabStripModelForTesting(
      new_browser->tab_strip_model());

  EnsureTabsInBrowser(new_browser, num_tabs);
  return new_browser;
}

}  // namespace

// Do not run in debug or ASAN builds to avoid timeouts due to multiple
// navigations. https://crbug.com/1106485
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER)
#define MAYBE_DiscardTabsWithMinimizedWindow \
  DISABLED_DiscardTabsWithMinimizedWindow
#else
#define MAYBE_DiscardTabsWithMinimizedWindow DiscardTabsWithMinimizedWindow
#endif
IN_PROC_BROWSER_TEST_F(TabManagerTest, MAYBE_DiscardTabsWithMinimizedWindow) {
  // Do not override the focused TabStripModel.
  GetTabLifecycleUnitSource()->SetFocusedTabStripModelForTesting(nullptr);

  // Minimized browser.
  EnsureTabsInBrowser(browser(), 2);
  browser()->window()->Minimize();

  // Advance time so everything is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  for (int i = 0; i < 8; ++i)
    tab_manager()->DiscardTab(LifecycleUnitDiscardReason::EXTERNAL);

  base::RunLoop().RunUntilIdle();

// On ChromeOS, active tabs are discarded if their window is non-visible. On
// other platforms, they are never discarded.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(
      IsTabDiscarded(browser()->tab_strip_model()->GetWebContentsAt(0)));
#else
  EXPECT_FALSE(
      IsTabDiscarded(browser()->tab_strip_model()->GetWebContentsAt(0)));
#endif

  // Non-active tabs can be discarded on all platforms.
  EXPECT_TRUE(
      IsTabDiscarded(browser()->tab_strip_model()->GetWebContentsAt(1)));

  // Showing the browser again should reload the active tab.
  browser()->window()->Show();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      IsTabDiscarded(browser()->tab_strip_model()->GetWebContentsAt(0)));
}

// Do not run in debug or ASAN builds to avoid timeouts due to multiple
// navigations. https://crbug.com/1106485
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER)
#define MAYBE_DiscardTabsWithOccludedWindow \
  DISABLED_DiscardTabsWithOccludedWindow
#else
#define MAYBE_DiscardTabsWithOccludedWindow DiscardTabsWithOccludedWindow
#endif
IN_PROC_BROWSER_TEST_F(TabManagerTest, MAYBE_DiscardTabsWithOccludedWindow) {
  // Occluded browser.
  EnsureTabsInBrowser(browser(), 2);
  browser()->window()->SetBounds(gfx::Rect(10, 10, 10, 10));
  // Other browser that covers the occluded browser.
  Browser* other_browser = CreateBrowserWithTabs(1);
  EXPECT_NE(other_browser, browser());
  other_browser->window()->SetBounds(gfx::Rect(0, 0, 100, 100));

  // Advance time so everything is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  for (int i = 0; i < 3; ++i)
    tab_manager()->DiscardTab(LifecycleUnitDiscardReason::URGENT);

  base::RunLoop().RunUntilIdle();

// On ChromeOS, active tabs are discarded if their window is non-visible. On
// other platforms, they are never discarded.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(
      IsTabDiscarded(browser()->tab_strip_model()->GetWebContentsAt(0)));
#else
  EXPECT_FALSE(
      IsTabDiscarded(browser()->tab_strip_model()->GetWebContentsAt(0)));
#endif

  // Non-active tabs can be discarded on all platforms.
  EXPECT_TRUE(
      IsTabDiscarded(browser()->tab_strip_model()->GetWebContentsAt(1)));
}

// On Linux, memory pressure listener is not implemented yet.
#if !defined(OS_LINUX) && !defined(OS_CHROMEOS)

class TabManagerMemoryPressureTest : public TabManagerTest {
 public:
  TabManagerMemoryPressureTest() {
    scoped_feature_list_.InitAndDisableFeature(
        performance_manager::features::kUrgentDiscardingFromPerformanceManager);
  }
  ~TabManagerMemoryPressureTest() override = default;
  TabManagerMemoryPressureTest(const TabManagerMemoryPressureTest& other) =
      delete;
  TabManagerMemoryPressureTest& operator=(const TabManagerMemoryPressureTest&) =
      delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the MemoryPressureListener event is properly triggering a tab
// discard upon |MEMORY_PRESSURE_LEVEL_CRITICAL| event.
IN_PROC_BROWSER_TEST_F(TabManagerMemoryPressureTest, OomPressureListener) {
  // Get two tabs open.
  NavigateToURLWithDisposition(browser(), GURL(chrome::kChromeUIAboutURL),
                               WindowOpenDisposition::CURRENT_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  NavigateToURLWithDisposition(browser(), GURL(chrome::kChromeUICreditsURL),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_EQ(tsm()->count(), 2);

  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(1)));

  // Advance time so everything is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  // Nothing should happen with a moderate memory pressure event.
  fake_memory_pressure_monitor_.SetAndNotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(1)));

  // A critical memory pressure event should discard a tab.
  fake_memory_pressure_monitor_.SetAndNotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  // Coming here, an asynchronous operation will collect system stats. Once in,
  // a tab should get discarded. As such we need to give it 10s time to discard.
  const int kTimeoutTimeInMS = 10000;
  const int kIntervalTimeInMS = 5;
  int timeout = kTimeoutTimeInMS / kIntervalTimeInMS;
  while (--timeout) {
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(kIntervalTimeInMS));
    base::RunLoop().RunUntilIdle();
    if (IsTabDiscarded(GetWebContentsAt(0)))
      break;
  }
  EXPECT_TRUE(IsTabDiscarded(GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(1)));
}

IN_PROC_BROWSER_TEST_F(TabManagerMemoryPressureTest,
                       RegisterMemoryPressureListener) {
  // Open two tabs.
  NavigateToURLWithDisposition(browser(), GURL(chrome::kChromeUIAboutURL),
                               WindowOpenDisposition::CURRENT_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  NavigateToURLWithDisposition(browser(), GURL(chrome::kChromeUICreditsURL),
                               WindowOpenDisposition::NEW_BACKGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_EQ(2, tsm()->count());
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(1)));

  // Advance time so everything is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  // After receiving memory pressure, the background tab should be discarded.
  // A critical memory pressure event should discard a tab.
  fake_memory_pressure_monitor_.SetAndNotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  if (!IsTabDiscarded(GetWebContentsAt(1))) {
    DiscardWaiter discard_waiter;
    discard_waiter.Wait();
  }

  // Critical memory pressure should make the background tab discarded.
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(0)));
  EXPECT_TRUE(IsTabDiscarded(GetWebContentsAt(1)));

  // Open another background tab. Wait for navigation to commit, but do not wait
  // for the load to finish as the tab might be discarded before that.
  NavigateToURLWithDisposition(browser(), GURL(chrome::kChromeUICreditsURL),
                               WindowOpenDisposition::NEW_BACKGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_EQ(3, tsm()->count());

  // Advance time again so the new background tab is discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  // After receiving another critical memory pressure, the new background tab
  // should be discarded. This verifies that the memory pressure listener is
  // registered back when the previous discard finished.
  fake_memory_pressure_monitor_.SetAndNotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  if (!IsTabDiscarded(GetWebContentsAt(2))) {
    DiscardWaiter discard_waiter;
    discard_waiter.Wait();
  }

  // Critical memory pressure should make the background tab discarded.
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(0)));
  EXPECT_TRUE(IsTabDiscarded(GetWebContentsAt(1)));
  EXPECT_TRUE(IsTabDiscarded(GetWebContentsAt(2)));
}

#endif  // !defined(OS_LINUX) && !defined(OS_CHROMEOS)

}  // namespace resource_coordinator

#endif  // OS_WIN || OS_MAXOSX || OS_LINUX || BUILDFLAG(IS_CHROMEOS_ASH)
