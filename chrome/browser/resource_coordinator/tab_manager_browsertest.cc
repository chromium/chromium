// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
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
#include "chrome/test/base/ui_test_utils.h"
#include "components/memory_pressure/fake_memory_pressure_monitor.h"
#include "components/performance_manager/public/features.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "url/gurl.h"

using content::OpenURLParams;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)

namespace resource_coordinator {

namespace {

constexpr base::TimeDelta kShortDelay = base::Seconds(1);

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

  ExpectStateTransitionObserver(const ExpectStateTransitionObserver&) = delete;
  ExpectStateTransitionObserver& operator=(
      const ExpectStateTransitionObserver&) = delete;

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

  const raw_ptr<LifecycleUnit> lifecycle_unit_;
  const LifecycleUnitState expected_state_;
  std::set<LifecycleUnitState> allowed_states_;
  base::RunLoop run_loop_;
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

// Allows tests to wait for a renderer process host to exit.
class WindowedRenderProcessHostExitObserver
    : public content::RenderProcessHostObserver {
 public:
  WindowedRenderProcessHostExitObserver() {
    for (auto it = content::RenderProcessHost::AllHostsIterator();
         !it.IsAtEnd(); it.Advance()) {
      host_observation_.AddObservation(it.GetCurrentValue());
    }
  }

  void Wait() {
    if (!seen_) {
      run_loop_.Run();
    }
    EXPECT_TRUE(seen_);
  }

  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override {
    seen_ = true;
    host_observation_.RemoveObservation(host);
  }

 private:
  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      host_observation_{this};

  base::RunLoop run_loop_;
  bool seen_ = false;
};

}  // namespace

class TabManagerTest : public InProcessBrowserTest,
                       public ::testing::WithParamInterface<bool> {
 public:
  TabManagerTest()
      : scoped_set_clocks_for_testing_(&test_clock_, &test_tick_clock_) {
    scoped_feature_list_.InitWithFeatureState(features::kWebContentsDiscard,
                                              GetParam());
    // Start with a non-null TimeTicks, as there is no discard protection for
    // a tab with a null focused timestamp.
    test_tick_clock_.Advance(kShortDelay);
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
    content::TestNavigationObserver load1(tsm()->GetActiveWebContents(), 1);
    OpenURLParams open1(first_url, content::Referrer(),
                        WindowOpenDisposition::CURRENT_TAB,
                        ui::PAGE_TRANSITION_TYPED, false);
    browser()->OpenURL(open1, /*navigation_handle_callback=*/{});
    load1.Wait();

    OpenURLParams open2(second_url, content::Referrer(),
                        WindowOpenDisposition::NEW_BACKGROUND_TAB,
                        ui::PAGE_TRANSITION_TYPED, false);
    auto* tab2 = browser()->OpenURL(open2, /*navigation_handle_callback=*/{});
    content::WaitForLoadStop(tab2);

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

  memory_pressure::test::FakeMemoryPressureMonitor
      fake_memory_pressure_monitor_;

  base::SimpleTestClock test_clock_;
  base::SimpleTestTickClock test_tick_clock_;
  ScopedSetClocksForTesting scoped_set_clocks_for_testing_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class TabManagerTestWithTwoTabs : public TabManagerTest {
 public:
  TabManagerTestWithTwoTabs() {
    // Tests using two tabs assume that each tab has a dedicated process.
    feature_list_.InitAndEnableFeature(features::kDisableProcessReuse);
  }

  TabManagerTestWithTwoTabs(const TabManagerTestWithTwoTabs&) = delete;
  TabManagerTestWithTwoTabs& operator=(const TabManagerTestWithTwoTabs&) =
      delete;

  void SetUpOnMainThread() override {
    TabManagerTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    // Open 2 tabs with default URLs in a focused tab strip.
    OpenTwoTabs(embedded_test_server()->GetURL("/title2.html"),
                embedded_test_server()->GetURL("/title3.html"));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(TabManagerTest, TabManagerBasics) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("a.com", "/title2.html");
  const GURL url3 = embedded_test_server()->GetURL("a.com", "/title3.html");

  // Get three tabs open.

  test_tick_clock_.Advance(kShortDelay);
  NavigateToURLWithDisposition(browser(), url1,
                               WindowOpenDisposition::CURRENT_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  test_tick_clock_.Advance(kShortDelay);
  NavigateToURLWithDisposition(browser(), url1,
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  test_tick_clock_.Advance(kShortDelay);
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
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);

  // Discard a tab.
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));
  EXPECT_EQ(3, tsm()->count());

  // The first tab should be killed since it was the oldest and was not
  // selected.
  EXPECT_TRUE(IsTabDiscarded(GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(1)));

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
  tsm()->ActivateTabAt(1, TabStripUserGestureDetails(
                              TabStripUserGestureDetails::GestureType::kOther));

  // Advance time so everything is urgent discardable again.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);

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

IN_PROC_BROWSER_TEST_P(TabManagerTest, InvalidOrEmptyURL) {
  // Open two tabs. Wait for the foreground one to load but do not wait for the
  // background one.
  NavigateToURLWithDisposition(browser(), GURL(chrome::kChromeUIAboutURL),
                               WindowOpenDisposition::CURRENT_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  NavigateToURLWithDisposition(browser(), GURL(chrome::kChromeUICreditsURL),
                               WindowOpenDisposition::NEW_BACKGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_NO_WAIT);

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
IN_PROC_BROWSER_TEST_P(TabManagerTest, TabDiscardDoneCallback) {
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
IN_PROC_BROWSER_TEST_P(TabManagerTest, ProtectPDFPages) {
  // Start the embedded test server so we can get served the required PDF page.
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  embedded_test_server()->StartAcceptingConnections();

  // Get two tabs open, the first one being a PDF page and the second one being
  // the foreground tab.
  GURL url1 = embedded_test_server()->GetURL("/pdf/test.pdf");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

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
// These protections only apply on non-Ash desktop platforms. Check
// TabLifecycleUnit::CanDiscard for more details.
IN_PROC_BROWSER_TEST_P(TabManagerTest,
                       ProtectRecentlyUsedTabsFromUrgentDiscarding) {
  TabManager* tab_manager = g_browser_process->GetTabManager();

  auto* tsm = browser()->tab_strip_model();

  // Open 2 tabs, the second one being in the background.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL)));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIAboutURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(2, tsm->count());

  // Advance the clock for less than the protection time.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime / 2);

  // Should not be able to discard a tab.
  ASSERT_FALSE(tab_manager->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));

  // Advance the clock for more than the protection time.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime / 2 +
                           base::Seconds(1));

  // Should be able to discard the background tab now.
  EXPECT_TRUE(tab_manager->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));

  // Activate the 2nd tab.
  tsm->ActivateTabAt(1, TabStripUserGestureDetails(
                            TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(1, tsm->active_index());

  // Advance the clock for less than the protection time.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime / 2);

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
IN_PROC_BROWSER_TEST_P(TabManagerTest, ProtectVideoTabs) {
  // Open 2 tabs, the second one being in the background.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL)));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIAboutURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  auto* tab = GetWebContentsAt(1);

  // Simulate that a video stream is now being captured.
  blink::mojom::StreamDevices devices;
  blink::MediaStreamDevice video_device = blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, "fake_media_device",
      "fake_media_device");
  devices.video_device = video_device;
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaCaptureDevicesDispatcher::GetInstance();
  dispatcher->SetTestVideoCaptureDevices({video_device});
  std::unique_ptr<content::MediaStreamUI> video_stream_ui =
      dispatcher->GetMediaStreamCaptureIndicator()->RegisterMediaStream(
          tab, devices);
  video_stream_ui->OnStarted(base::RepeatingClosure(),
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
// TODO(crbug.com/40913262): Flaky on debug Linux.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_ProtectDevToolsTabsFromDiscarding \
  DISABLED_ProtectDevToolsTabsFromDiscarding
#else
#define MAYBE_ProtectDevToolsTabsFromDiscarding \
  ProtectDevToolsTabsFromDiscarding
#endif
IN_PROC_BROWSER_TEST_P(TabManagerTest,
                       MAYBE_ProtectDevToolsTabsFromDiscarding) {
  // Get two tabs open, the second one being the foreground tab.
  GURL test_page(ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("simple.html"))));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));
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

// TODO(crbug.com/336450782): Flaky on Lacros ASAN.
#if BUILDFLAG(IS_CHROMEOS_LACROS) && defined(ADDRESS_SANITIZER)
#define MAYBE_AutoDiscardable DISABLED_AutoDiscardable
#else
#define MAYBE_AutoDiscardable AutoDiscardable
#endif
IN_PROC_BROWSER_TEST_P(TabManagerTest, MAYBE_AutoDiscardable) {
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

IN_PROC_BROWSER_TEST_P(TabManagerTestWithTwoTabs,
                       UrgentFastShutdownSingleTabProcess) {
  // The Tab Manager should be able to fast-kill a process for the discarded tab
  // on all platforms, as each tab will be running in a separate process by
  // itself regardless of the discard reason.
  WindowedRenderProcessHostExitObserver observer;
  // Advance time so everything is urgent discardable.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));
  observer.Wait();
}

IN_PROC_BROWSER_TEST_P(TabManagerTest, UrgentFastShutdownSharedTabProcess) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Set max renderers to 1 before opening tabs to force running out of
  // processes and for both these tabs to share a renderer.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);
  OpenTwoTabs(embedded_test_server()->GetURL("a.com", "/title1.html"),
              embedded_test_server()->GetURL("a.com", "/title2.html"));
  EXPECT_EQ(tsm()->GetWebContentsAt(0)->GetPrimaryMainFrame()->GetProcess(),
            tsm()->GetWebContentsAt(1)->GetPrimaryMainFrame()->GetProcess());

  // Advance time so everything is urgent discardable.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);

  // The Tab Manager will not be able to fast-kill either of the tabs since they
  // share the same process regardless of the discard reason. An unsafe attempt
  // will be made on some platforms.
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));
}

IN_PROC_BROWSER_TEST_P(TabManagerTest, UrgentFastShutdownWithUnloadHandler) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Disable the protection of recent tabs.
  OpenTwoTabs(embedded_test_server()->GetURL("a.com", "/title1.html"),
              embedded_test_server()->GetURL("/unload.html"));

  // Advance time so everything is urgent discardable.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);

  // The Tab Manager will not be able to safely fast-kill either of the tabs as
  // one of them is current, and the other has an unload handler. An unsafe
  // attempt will be made on some platforms.
#if BUILDFLAG(IS_CHROMEOS)
  // The unsafe attempt for ChromeOS should succeed as ChromeOS ignores unload
  // handlers when in critical condition.
  WindowedRenderProcessHostExitObserver observer;
#endif  // BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));
#if BUILDFLAG(IS_CHROMEOS)
  observer.Wait();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

IN_PROC_BROWSER_TEST_P(TabManagerTest,
                       UrgentFastShutdownWithBeforeunloadHandler) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Disable the protection of recent tabs.
  OpenTwoTabs(embedded_test_server()->GetURL("a.com", "/title1.html"),
              embedded_test_server()->GetURL("/beforeunload.html"));

  // Advance time so everything is urgent discardable.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);

  // The Tab Manager will not be able to safely fast-kill either of the tabs as
  // one of them is current, and the other has a beforeunload handler. An unsafe
  // attempt will be made on some platforms.
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));
}

// Verifies the following state transitions for a tab:
// - Initial state: ACTIVE
// - Discard(kUrgent): ACTIVE->DISCARDED
// - Navigate: DISCARDED->ACTIVE
//             window.document.wasDiscarded is true
IN_PROC_BROWSER_TEST_P(TabManagerTestWithTwoTabs, TabUrgentDiscardAndNavigate) {
  const char kDiscardedStateJS[] = "window.document.wasDiscarded;";

  GURL test_page(ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("simple.html"))));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));

  // document.wasDiscarded is false initially.
  EXPECT_EQ(false, content::EvalJs(GetWebContentsAt(0), kDiscardedStateJS));

  // Discard the tab.
  EXPECT_EQ(LifecycleUnitState::ACTIVE, GetLifecycleUnitAt(0)->GetState());
  EXPECT_TRUE(
      GetLifecycleUnitAt(0)->Discard(LifecycleUnitDiscardReason::EXTERNAL));
  EXPECT_EQ(LifecycleUnitState::DISCARDED, GetLifecycleUnitAt(0)->GetState());

  // Here we simulate re-focussing the tab causing reload with navigation,
  // the navigation will reload the tab.
  // TODO(fdoray): Figure out why the test fails if a reload is done instead of
  // a navigation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));
  EXPECT_EQ(LifecycleUnitState::ACTIVE, GetLifecycleUnitAt(0)->GetState());

  // document.wasDiscarded is true on navigate after discard.
  EXPECT_EQ(true, content::EvalJs(GetWebContentsAt(0), kDiscardedStateJS));
}

IN_PROC_BROWSER_TEST_P(TabManagerTest, DiscardedTabHasNoProcess) {
  GURL test_page(ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("simple.html"))));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));
  content::WebContents* web_contents = tsm()->GetActiveWebContents();

  // The renderer process should be alive at this point.
  content::RenderProcessHost* process =
      web_contents->GetPrimaryMainFrame()->GetProcess();
  ASSERT_TRUE(process);
  EXPECT_TRUE(process->IsInitializedAndNotDead());
  EXPECT_NE(base::kNullProcessHandle, process->GetProcess().Handle());
  const int initial_renderer_id = process->GetID();

  // Discard the tab. This simulates a tab discard.
  TabLifecycleUnitExternal::FromWebContents(web_contents)
      ->DiscardTab(LifecycleUnitDiscardReason::URGENT);

  // Replacing the WebContents for the discard operation should result in
  // assignment of a new RenderProcessHost.
  if (!base::FeatureList::IsEnabled(features::kWebContentsDiscard)) {
    content::WebContents* new_web_contents = tsm()->GetActiveWebContents();
    EXPECT_NE(new_web_contents, web_contents);
    web_contents = new_web_contents;
    content::RenderProcessHost* new_process =
        web_contents->GetPrimaryMainFrame()->GetProcess();
    EXPECT_NE(new_process, process);
    EXPECT_NE(new_process->GetID(), initial_renderer_id);
    process = new_process;
  }

  // The renderer process should be dead after a discard.
  EXPECT_EQ(process, web_contents->GetPrimaryMainFrame()->GetProcess());
  EXPECT_FALSE(process->IsInitializedAndNotDead());
  EXPECT_EQ(base::kNullProcessHandle, process->GetProcess().Handle());

  // Here we simulate re-focussing the tab causing reload with navigation,
  // the navigation will reload the tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));

  // Reload should mean that the renderer process is alive now.
  EXPECT_EQ(process, web_contents->GetPrimaryMainFrame()->GetProcess());
  EXPECT_TRUE(process->IsInitializedAndNotDead());
  EXPECT_NE(base::kNullProcessHandle, process->GetProcess().Handle());
}

IN_PROC_BROWSER_TEST_P(TabManagerTest,
                       TabManagerWasDiscardedCrossSiteSubFrame) {
  const char kDiscardedStateJS[] = "window.document.wasDiscarded;";
  // Navigate to a page with a cross-site frame.
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/iframe_cross_site.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Grab the original frames.
  content::WebContents* contents = tsm()->GetActiveWebContents();
  content::RenderFrameHost* main_frame = contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child_frame = ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(child_frame);

  // Sanity check that in this test page the main frame and the
  // subframe are cross-site.
  EXPECT_NE(main_frame->GetLastCommittedURL().DeprecatedGetOriginAsURL(),
            child_frame->GetLastCommittedURL().DeprecatedGetOriginAsURL());
  if (content::AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(main_frame->GetSiteInstance(), child_frame->GetSiteInstance());
    EXPECT_NE(main_frame->GetProcess()->GetID(),
              child_frame->GetProcess()->GetID());
  }

  // document.wasDiscarded is false before discard, on main frame and child
  // frame.
  EXPECT_EQ(false, content::EvalJs(main_frame, kDiscardedStateJS));

  EXPECT_EQ(false, content::EvalJs(child_frame, kDiscardedStateJS));

  // Discard the tab. This simulates a tab discard.
  TabLifecycleUnitExternal::FromWebContents(contents)->DiscardTab(
      LifecycleUnitDiscardReason::URGENT);

  // Here we simulate re-focussing the tab causing reload with navigation,
  // the navigation will reload the tab.
  // TODO(panicker): Consider adding a test hook on LifecycleUnit when ready.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Re-assign pointers after discarding, as they've changed.
  contents = tsm()->GetActiveWebContents();
  main_frame = contents->GetPrimaryMainFrame();
  child_frame = ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(child_frame);

  // document.wasDiscarded is true after discard, on mainframe and childframe.
  EXPECT_EQ(true, content::EvalJs(main_frame, kDiscardedStateJS));

  EXPECT_EQ(true, content::EvalJs(child_frame, kDiscardedStateJS));

  // Navigate the child frame, wasDiscarded is not set anymore.
  GURL childframe_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(contents, "frame1", childframe_url));
  EXPECT_EQ(false,
            content::EvalJs(ChildFrameAt(contents, 0), kDiscardedStateJS));

  // Navigate second child frame cross site.
  GURL second_childframe_url(
      embedded_test_server()->GetURL("d.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(contents, "frame2", second_childframe_url));
  EXPECT_EQ(false,
            content::EvalJs(ChildFrameAt(contents, 1), kDiscardedStateJS));

  // Navigate the main frame (same site) again, wasDiscarded is not set anymore.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  main_frame = contents->GetPrimaryMainFrame();
  EXPECT_EQ(false, content::EvalJs(main_frame, kDiscardedStateJS));

  // Go back in history and ensure wasDiscarded is still false.
  content::TestNavigationObserver observer(contents);
  contents->GetController().GoBack();
  observer.Wait();
  main_frame = contents->GetPrimaryMainFrame();
  EXPECT_EQ(false, content::EvalJs(main_frame, kDiscardedStateJS));
}

class TabManagerFencedFrameTest : public TabManagerTest {
 public:
  TabManagerFencedFrameTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
  }
  ~TabManagerFencedFrameTest() override = default;

 protected:
  net::EmbeddedTestServer& https_server() { return https_server_; }
  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

 private:
  net::EmbeddedTestServer https_server_;
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
};

// Tests that `window.document.wasDiscarded` is updated for a fenced frame.
IN_PROC_BROWSER_TEST_P(TabManagerFencedFrameTest, TabManagerWasDiscarded) {
  const char kDiscardedStateJS[] = "window.document.wasDiscarded;";

  // Navigate to a page with a fenced frame.
  ASSERT_TRUE(https_server().Start());
  GURL main_url(
      https_server().GetURL("c.test", "/fenced_frames/basic_title.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Grab the original frames.
  content::WebContents* contents = tsm()->GetActiveWebContents();
  content::RenderFrameHost* primary_main_frame =
      contents->GetPrimaryMainFrame();

  content::RenderFrameHost* fenced_frame =
      fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
          primary_main_frame);
  ASSERT_TRUE(fenced_frame);

  // document.wasDiscarded is false before discard, on a main frame and fenced
  // frame.
  EXPECT_EQ(false, content::EvalJs(primary_main_frame, kDiscardedStateJS));

  EXPECT_EQ(false, content::EvalJs(fenced_frame, kDiscardedStateJS));

  // Discard the tab. This simulates a tab discard.
  TabLifecycleUnitExternal::FromWebContents(contents)->DiscardTab(
      LifecycleUnitDiscardReason::URGENT);

  // Here we simulate re-focussing the tab causing reload with navigation,
  // the navigation will reload the tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Re-assign pointers after discarding, as they've changed.
  contents = tsm()->GetActiveWebContents();
  primary_main_frame = contents->GetPrimaryMainFrame();
  fenced_frame = fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
      primary_main_frame);
  ASSERT_TRUE(fenced_frame);

  // document.wasDiscarded is true after discard, on a main frame and fenced
  // frame.
  EXPECT_EQ(true, content::EvalJs(primary_main_frame, kDiscardedStateJS));

  EXPECT_EQ(true, content::EvalJs(fenced_frame, kDiscardedStateJS));
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
  ui_test_utils::BrowserChangeObserver new_browser_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  chrome::NewWindow(current_browser);
  ui_test_utils::WaitForBrowserSetLastActive(new_browser_observer.Wait());
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
IN_PROC_BROWSER_TEST_P(TabManagerTest, MAYBE_DiscardTabsWithMinimizedWindow) {
  // Do not override the focused TabStripModel.
  GetTabLifecycleUnitSource()->SetFocusedTabStripModelForTesting(nullptr);

  // Minimized browser.
  EnsureTabsInBrowser(browser(), 2);
  browser()->window()->Minimize();

  // Advance time so everything is urgent discardable.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);

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
IN_PROC_BROWSER_TEST_P(TabManagerTest, MAYBE_DiscardTabsWithOccludedWindow) {
  // Occluded browser.
  EnsureTabsInBrowser(browser(), 2);
  browser()->window()->SetBounds(gfx::Rect(10, 10, 10, 10));
  // Other browser that covers the occluded browser.
  Browser* other_browser = CreateBrowserWithTabs(1);
  EXPECT_NE(other_browser, browser());
  other_browser->window()->SetBounds(gfx::Rect(0, 0, 100, 100));

  // Advance time so everything is urgent discardable.
  test_tick_clock_.Advance(kBackgroundUrgentProtectionTime);

  for (int i = 0; i < 3; ++i)
    tab_manager()->DiscardTab(LifecycleUnitDiscardReason::URGENT);

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(
      IsTabDiscarded(browser()->tab_strip_model()->GetWebContentsAt(0)));

  // Non-active tabs can be discarded on all platforms.
  EXPECT_TRUE(
      IsTabDiscarded(browser()->tab_strip_model()->GetWebContentsAt(1)));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    TabManagerTest,
    ::testing::Values(false, true),
    [](const ::testing::TestParamInfo<TabManagerTest::ParamType>& info) {
      return info.param ? "RetainedWebContents" : "UnretainedWebContents";
    });

INSTANTIATE_TEST_SUITE_P(
    ,
    TabManagerTestWithTwoTabs,
    ::testing::Values(false, true),
    [](const ::testing::TestParamInfo<TabManagerTestWithTwoTabs::ParamType>&
           info) {
      return info.param ? "RetainedWebContents" : "UnretainedWebContents";
    });

INSTANTIATE_TEST_SUITE_P(
    ,
    TabManagerFencedFrameTest,
    ::testing::Values(false, true),
    [](const ::testing::TestParamInfo<TabManagerFencedFrameTest::ParamType>&
           info) {
      return info.param ? "RetainedWebContents" : "UnretainedWebContents";
    });

}  // namespace resource_coordinator

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
