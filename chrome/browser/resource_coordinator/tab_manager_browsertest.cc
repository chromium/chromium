// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/fake_memory_pressure_monitor.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_unittest_utils.h"
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
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

using content::OpenURLParams;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)

namespace resource_coordinator {

namespace {

constexpr char kBlinkPageLifecycleFeature[] = "PageLifecycle";
constexpr base::TimeDelta kShortDelay = base::TimeDelta::FromSeconds(1);

constexpr char kMainFrameFrozenStateJS[] =
    "window.domAutomationController.send(mainFrameFreezeCount);";
constexpr char kChildFrameFrozenStateJS[] =
    "window.domAutomationController.send(childFrameFreezeCount);";

bool ObserveNavEntryCommitted(const GURL& expected_url,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  return content::Details<content::LoadCommittedDetails>(details)
             ->entry->GetURL() == expected_url;
}

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
      EXPECT_TRUE(
          base::ContainsKey(allowed_states_, lifecycle_unit_->GetState()));
    }
  }

  LifecycleUnit* const lifecycle_unit_;
  const LifecycleUnitState expected_state_;
  std::set<LifecycleUnitState> allowed_states_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ExpectStateTransitionObserver);
};

}  // namespace

class TabManagerTest : public InProcessBrowserTest {
 public:
  TabManagerTest() : scoped_set_tick_clock_for_testing_(&test_clock_) {
    // Start with a non-null TimeTicks, as there is no discard protection for
    // a tab with a null focused timestamp.
    test_clock_.Advance(kShortDelay);
    scoped_feature_list_.InitAndEnableFeature(
        features::kSiteCharacteristicsDatabase);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    kBlinkPageLifecycleFeature);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
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
    if (URLShouldBeStoredInLocalDatabase(first_url))
      testing::ExpireLocalDBObservationWindows(web_contents);

    content::WindowedNotificationObserver load2(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    OpenURLParams open2(second_url, content::Referrer(),
                        WindowOpenDisposition::NEW_BACKGROUND_TAB,
                        ui::PAGE_TRANSITION_TYPED, false);
    web_contents = browser()->OpenURL(open2);
    load2.Wait();
    // Expire all the observation windows to prevent the discarding and freezing
    // interventions to fail because of a lack of observations.
    if (URLShouldBeStoredInLocalDatabase(second_url))
      testing::ExpireLocalDBObservationWindows(web_contents);

    ASSERT_EQ(2, tsm()->count());
  }

  // Opens 2 tabs. Calls Freeze() on the background tab. Verifies that it
  // transitions to the PENDING_FREEZE, and that onfreeze callbacks runs on the
  // page. The background tab is PENDING_FREEZE when this returns.
  void TestTransitionFromActiveToPendingFreeze() {
    // Setup the embedded_test_server to serve a cross-site frame.
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());

    // Opening two tabs, where the second tab is backgrounded.
    GURL main_url(
        embedded_test_server()->GetURL("a.com", "/iframe_cross_site.html"));
    OpenTwoTabs(GURL(chrome::kChromeUIAboutURL), main_url);
    constexpr int kFreezingIndex = 1;
    LifecycleUnit* const lifecycle_unit = GetLifecycleUnitAt(kFreezingIndex);
    content::WebContents* const content = GetWebContentsAt(kFreezingIndex);

    // Grab the frames.
    content::RenderFrameHost* main_frame = content->GetMainFrame();
    ASSERT_EQ(3u, content->GetAllFrames().size());
    // The page has 2 iframes, we will use the first one.
    content::RenderFrameHost* child_frame = content->GetAllFrames()[1];
    // Verify that the main frame and subframe are cross-site.
    EXPECT_FALSE(content::SiteInstance::IsSameWebSite(
        browser()->profile(), main_frame->GetLastCommittedURL(),
        child_frame->GetLastCommittedURL()));
    if (content::AreAllSitesIsolatedForTesting()) {
      EXPECT_NE(main_frame->GetProcess()->GetID(),
                child_frame->GetProcess()->GetID());
    }

    // Ensure that the tab is hidden or backgrounded.
    bool hidden_state_result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        main_frame,
        "window.domAutomationController.send("
        "window.document.hidden);",
        &hidden_state_result));
    EXPECT_TRUE(hidden_state_result);

    EXPECT_TRUE(content::ExecuteScript(
        main_frame,
        "if (window.location.pathname != '/iframe_cross_site.html')"
        "  throw 'Incorrect frame';"
        "mainFrameFreezeCount = 0;"
        "window.document.onfreeze = function(){ mainFrameFreezeCount++; };"));

    EXPECT_TRUE(content::ExecuteScript(
        child_frame,
        "if (window.location.pathname != '/title1.html') throw 'Incorrect "
        "frame';"
        "childFrameFreezeCount = 0;"
        "window.document.onfreeze = function(){ childFrameFreezeCount++; };"));

    // freeze_count_result should be 0 for both frames, if it is undefined then
    // we are in the wrong frame/tab.
    int freeze_count_result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        main_frame, kMainFrameFrozenStateJS, &freeze_count_result));
    EXPECT_EQ(0, freeze_count_result);
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        child_frame, kChildFrameFrozenStateJS, &freeze_count_result));
    EXPECT_EQ(0, freeze_count_result);

    // Freeze the tab. If it fails then we might be freezing a visible tab.
    EXPECT_EQ(LifecycleUnitState::ACTIVE, lifecycle_unit->GetState());
    EXPECT_TRUE(lifecycle_unit->Freeze());
    EXPECT_EQ(LifecycleUnitState::PENDING_FREEZE, lifecycle_unit->GetState());
  }

  // Opens 2 tabs. Calls Freeze() on the background tab. Verifies that it
  // transitions to the PENDING_FREEZE and FROZEN states, and that onfreeze
  // callbacks runs on the page. The background tabs is FROZEN when this
  // returns.
  void TestTransitionFromActiveToFrozen() {
    TestTransitionFromActiveToPendingFreeze();

    {
      ExpectStateTransitionObserver expect_state_transition(
          GetLifecycleUnitAt(1), LifecycleUnitState::FROZEN);
      expect_state_transition.Wait();
    }

    content::WebContents* const content = GetWebContentsAt(1);
    content::RenderFrameHost* main_frame = content->GetMainFrame();
    content::RenderFrameHost* child_frame = content->GetAllFrames()[1];

    // freeze_count_result should be exactly 1 for both frames. The value is
    // incremented in the onfreeze callback. If it is >1, then the callback was
    // called more than once.
    int freeze_count_result = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        main_frame, kMainFrameFrozenStateJS, &freeze_count_result));
    EXPECT_EQ(1, freeze_count_result);
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        child_frame, kChildFrameFrozenStateJS, &freeze_count_result));
    EXPECT_EQ(1, freeze_count_result);
  }

  // Gets the TabLifecycleUnit from |contents| and sends the signal that
  // indicates that the page is frozen. In production, this is sent by the
  // renderer process. This is done to finish a proactive tab discard.
  void SimulateFreezeSignal(content::WebContents* contents) {
    TabLifecycleUnitSource::GetInstance()
        ->GetTabLifecycleUnit(contents)
        ->UpdateLifecycleState(mojom::LifecycleState::kFrozen);
  }

  TabManager* tab_manager() { return g_browser_process->GetTabManager(); }
  TabStripModel* tsm() { return browser()->tab_strip_model(); }

  content::WebContents* GetWebContentsAt(int index) {
    return tsm()->GetWebContentsAt(index);
  }

  LifecycleUnit* GetLifecycleUnitAt(int index) {
    return TabLifecycleUnitSource::GetInstance()->GetTabLifecycleUnit(
        GetWebContentsAt(index));
  }

  base::test::FakeMemoryPressureMonitor fake_memory_pressure_monitor_;
  base::SimpleTestTickClock test_clock_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class TabManagerTestWithTwoTabs : public TabManagerTest {
 public:
  TabManagerTestWithTwoTabs() = default;

  void SetUpOnMainThread() override {
    TabManagerTest::SetUpOnMainThread();

    // Open 2 tabs with default URLs in a focused tab strip.
    TabLifecycleUnitSource::GetInstance()->SetFocusedTabStripModelForTesting(
        tsm());
    OpenTwoTabs(GURL(chrome::kChromeUIAboutURL),
                GURL(chrome::kChromeUICreditsURL));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabManagerTestWithTwoTabs);
};

// Flaky on Mac. http://crbug.com/857418
#if defined(OS_MACOSX)
#define MAYBE_TabManagerBasics DISABLED_TabManagerBasics
#else
#define MAYBE_TabManagerBasics TabManagerBasics
#endif
IN_PROC_BROWSER_TEST_F(TabManagerTest, MAYBE_TabManagerBasics) {
  using content::WindowedNotificationObserver;

  // Get three tabs open.

  test_clock_.Advance(kShortDelay);
  WindowedNotificationObserver load1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open1(GURL(chrome::kChromeUIAboutURL), content::Referrer(),
                      WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open1);
  load1.Wait();

  test_clock_.Advance(kShortDelay);
  WindowedNotificationObserver load2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open2(GURL(chrome::kChromeUICreditsURL), content::Referrer(),
                      WindowOpenDisposition::NEW_FOREGROUND_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open2);
  load2.Wait();

  test_clock_.Advance(kShortDelay);
  WindowedNotificationObserver load3(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open3(GURL(chrome::kChromeUITermsURL), content::Referrer(),
                      WindowOpenDisposition::NEW_FOREGROUND_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open3);
  load3.Wait();
  EXPECT_EQ(3, tsm()->count());

  // Navigate the current (third) tab to a different URL, so we can test
  // back/forward later.
  WindowedNotificationObserver load4(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open4(GURL(chrome::kChromeUIVersionURL), content::Referrer(),
                      WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open4);
  load4.Wait();

  // Navigate the third tab again, such that we have three navigation entries.
  WindowedNotificationObserver load5(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open5(GURL("chrome://dns"), content::Referrer(),
                      WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open5);
  load5.Wait();
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
  tsm()->ActivateTabAt(1, true);

  // Advance time so everything is urgent discardable again.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  EXPECT_EQ(1, tsm()->active_index());
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(1)));
  tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT);
  EXPECT_TRUE(IsTabDiscarded(GetWebContentsAt(2)));

  // Force creation of the FindBarController.
  browser()->GetFindBarController();

  // Select the first tab.  It should reload.
  WindowedNotificationObserver reload1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      base::Bind(&ObserveNavEntryCommitted,
                 GURL(chrome::kChromeUIChromeURLsURL)));
  chrome::SelectNumberedTab(browser(), 0);
  reload1.Wait();
  // Make sure the FindBarController gets the right WebContents.
  EXPECT_EQ(browser()->GetFindBarController()->web_contents(),
            tsm()->GetActiveWebContents());
  EXPECT_EQ(0, tsm()->active_index());
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(1)));
  EXPECT_TRUE(IsTabDiscarded(GetWebContentsAt(2)));

  // Select the third tab. It should reload.
  WindowedNotificationObserver reload2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      base::Bind(&ObserveNavEntryCommitted, GURL("chrome://dns")));
  chrome::SelectNumberedTab(browser(), 2);
  reload2.Wait();
  EXPECT_EQ(2, tsm()->active_index());
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(1)));
  EXPECT_FALSE(IsTabDiscarded(GetWebContentsAt(2)));

  // Navigate the third tab back twice.  We used to crash here due to
  // crbug.com/121373.
  EXPECT_TRUE(chrome::CanGoBack(browser()));
  EXPECT_FALSE(chrome::CanGoForward(browser()));
  WindowedNotificationObserver back1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      base::Bind(&ObserveNavEntryCommitted, GURL(chrome::kChromeUIVersionURL)));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  back1.Wait();
  EXPECT_TRUE(chrome::CanGoBack(browser()));
  EXPECT_TRUE(chrome::CanGoForward(browser()));
  WindowedNotificationObserver back2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      base::Bind(&ObserveNavEntryCommitted, GURL(chrome::kChromeUITermsURL)));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  back2.Wait();
  EXPECT_FALSE(chrome::CanGoBack(browser()));
  EXPECT_TRUE(chrome::CanGoForward(browser()));
}

// On Linux, memory pressure listener is not implemented yet.
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

// Test that the MemoryPressureListener event is properly triggering a tab
// discard upon |MEMORY_PRESSURE_LEVEL_CRITICAL| event.
IN_PROC_BROWSER_TEST_F(TabManagerTest, OomPressureListener) {
  // Get two tabs open.
  content::WindowedNotificationObserver load1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open1(GURL(chrome::kChromeUIAboutURL), content::Referrer(),
                      WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open1);
  load1.Wait();

  content::WindowedNotificationObserver load2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open2(GURL(chrome::kChromeUICreditsURL), content::Referrer(),
                      WindowOpenDisposition::NEW_FOREGROUND_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open2);
  load2.Wait();

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

#endif

IN_PROC_BROWSER_TEST_F(TabManagerTest, InvalidOrEmptyURL) {
  // Open two tabs. Wait for the foreground one to load but do not wait for the
  // background one.
  content::WindowedNotificationObserver load1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open1(GURL(chrome::kChromeUIAboutURL), content::Referrer(),
                      WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open1);
  load1.Wait();

  content::WindowedNotificationObserver load2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open2(GURL(chrome::kChromeUICreditsURL), content::Referrer(),
                      WindowOpenDisposition::NEW_BACKGROUND_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open2);

  ASSERT_EQ(2, tsm()->count());

  // This shouldn't be able to discard a tab as the background tab has not yet
  // started loading (its URL is not committed).
  EXPECT_FALSE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::PROACTIVE));

  // Wait for the background tab to load which then allows it to be discarded.
  load2.Wait();
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::PROACTIVE));
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
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // No discarding should be possible as the only background tab is displaying a
  // PDF page, hence protected.
  EXPECT_FALSE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::PROACTIVE));
}

#if !defined(OS_CHROMEOS)
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
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
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
  tsm->ActivateTabAt(1, true);
  EXPECT_EQ(1, tsm->active_index());

  // Advance the clock for less than the protection time.
  test_clock_.Advance(kBackgroundUrgentProtectionTime / 2);

  // Should not be able to urgent discard the tab.
  ASSERT_FALSE(tab_manager->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));

  // But should be able to proactive discard the tab.
  EXPECT_TRUE(
      tab_manager->DiscardTabImpl(LifecycleUnitDiscardReason::PROACTIVE));

  // This is necessary otherwise the test crashes in
  // WebContentsData::WebContentsDestroyed.
  tsm->CloseAllTabs();
}
#endif  // !defined(OS_CHROMEOS)

// Makes sure that tabs using media devices are protected.
IN_PROC_BROWSER_TEST_F(TabManagerTest, ProtectVideoTabs) {
  // Open 2 tabs, the second one being in the background.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIAboutURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  auto* tab = GetWebContentsAt(1);

  // Simulate that a video stream is now being captured.
  content::MediaStreamDevices video_devices(1);
  video_devices[0] =
      content::MediaStreamDevice(content::MEDIA_DEVICE_VIDEO_CAPTURE,
                                 "fake_media_device", "fake_media_device");
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaCaptureDevicesDispatcher::GetInstance();
  dispatcher->SetTestVideoCaptureDevices(video_devices);
  std::unique_ptr<content::MediaStreamUI> video_stream_ui =
      dispatcher->GetMediaStreamCaptureIndicator()->RegisterMediaStream(
          tab, video_devices);
  video_stream_ui->OnStarted(base::Closure());

  // Should not be able to discard a tab.
  ASSERT_FALSE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::PROACTIVE));

  // Remove the video stream.
  video_stream_ui.reset();

  // Should be able to discard the background tab now.
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::PROACTIVE));
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
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // No discarding should be possible as the only background tab is currently
  // using DevTools.
  EXPECT_FALSE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::PROACTIVE));

  // Close the DevTools window and repeat the test, this time use a non-docked
  // window.
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtool);
  devtool = DevToolsWindowTesting::OpenDevToolsWindowSync(
      GetWebContentsAt(0), false /* is_docked */);
  EXPECT_TRUE(devtool);
  EXPECT_FALSE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::PROACTIVE));

  // TODO(sebmarchand): Also ensure that the tab can't be frozen.

  // Close the DevTools window, ensure that the tab can be discarded.
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtool);
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::PROACTIVE));
}

IN_PROC_BROWSER_TEST_F(TabManagerTest, CanPurgeBackgroundedRenderer) {
  // Open 2 tabs, the second one being in the background.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIAboutURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  auto* tab = GetWebContentsAt(1);
  // Simulate that a video stream is now being captured.
  content::MediaStreamDevices video_devices(1);
  video_devices[0] =
      content::MediaStreamDevice(content::MEDIA_DEVICE_VIDEO_CAPTURE,
                                 "fake_media_device", "fake_media_device");
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaCaptureDevicesDispatcher::GetInstance();
  dispatcher->SetTestVideoCaptureDevices(video_devices);
  std::unique_ptr<content::MediaStreamUI> video_stream_ui =
      dispatcher->GetMediaStreamCaptureIndicator()->RegisterMediaStream(
          tab, video_devices);
  video_stream_ui->OnStarted(base::Closure());

  // Should not be able to suspend a tab which plays a video.
  int render_process_id = tab->GetMainFrame()->GetProcess()->GetID();
  ASSERT_FALSE(tab_manager()->CanPurgeBackgroundedRenderer(render_process_id));

  // Remove the video stream.
  video_stream_ui.reset();

  // Should be able to suspend the background tab now.
  EXPECT_TRUE(tab_manager()->CanPurgeBackgroundedRenderer(render_process_id));
}

IN_PROC_BROWSER_TEST_F(TabManagerTest, AutoDiscardable) {
  using content::WindowedNotificationObserver;

  // Get two tabs open.
  WindowedNotificationObserver load1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open1(GURL(chrome::kChromeUIAboutURL), content::Referrer(),
                      WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open1);
  load1.Wait();

  WindowedNotificationObserver load2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open2(GURL(chrome::kChromeUICreditsURL), content::Referrer(),
                      WindowOpenDisposition::NEW_FOREGROUND_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open2);
  load2.Wait();
  ASSERT_EQ(2, tsm()->count());

  // Set the auto-discardable state of the first tab to false.
  TabLifecycleUnitExternal::FromWebContents(GetWebContentsAt(0))
      ->SetAutoDiscardable(false);

  // Shouldn't discard the tab, since auto-discardable is deactivated.
  EXPECT_FALSE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::PROACTIVE));

  // Reset auto-discardable state to true.
  TabLifecycleUnitExternal::FromWebContents(GetWebContentsAt(0))
      ->SetAutoDiscardable(true);

  // Now it should be able to discard the tab.
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::PROACTIVE));
  EXPECT_TRUE(IsTabDiscarded(GetWebContentsAt(0)));
}

IN_PROC_BROWSER_TEST_F(TabManagerTest, PurgeBackgroundRenderer) {
  // Get three tabs open.
  content::WindowedNotificationObserver load1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open1(GURL(chrome::kChromeUIAboutURL), content::Referrer(),
                      WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open1);
  load1.Wait();

  content::WindowedNotificationObserver load2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open2(GURL(chrome::kChromeUICreditsURL), content::Referrer(),
                      WindowOpenDisposition::NEW_FOREGROUND_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open2);
  load2.Wait();

  content::WindowedNotificationObserver load3(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open3(GURL(chrome::kChromeUITermsURL), content::Referrer(),
                      WindowOpenDisposition::NEW_FOREGROUND_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open3);
  load3.Wait();

  TabManager::WebContentsData* tab1_contents_data =
      tab_manager()->GetWebContentsData(GetWebContentsAt(0));
  TabManager::WebContentsData* tab2_contents_data =
      tab_manager()->GetWebContentsData(GetWebContentsAt(1));
  TabManager::WebContentsData* tab3_contents_data =
      tab_manager()->GetWebContentsData(GetWebContentsAt(2));

  // The time-to-purge initialized at ActiveTabChanged should be in the
  // right default range.
  EXPECT_GE(tab1_contents_data->time_to_purge(),
            base::TimeDelta::FromMinutes(1));
  EXPECT_LE(tab1_contents_data->time_to_purge(),
            base::TimeDelta::FromMinutes(4));
  EXPECT_GE(tab2_contents_data->time_to_purge(),
            base::TimeDelta::FromMinutes(1));
  EXPECT_LE(tab2_contents_data->time_to_purge(),
            base::TimeDelta::FromMinutes(4));

  EXPECT_GE(tab3_contents_data->time_to_purge(),
            base::TimeDelta::FromMinutes(30));
  EXPECT_LE(tab3_contents_data->time_to_purge(),
            base::TimeDelta::FromMinutes(60));

  // To make it easy to test, configure time-to-purge here.
  base::TimeDelta time_to_purge1 = base::TimeDelta::FromMinutes(30);
  base::TimeDelta time_to_purge2 = base::TimeDelta::FromMinutes(40);
  tab1_contents_data->set_time_to_purge(time_to_purge1);
  tab2_contents_data->set_time_to_purge(time_to_purge2);
  tab3_contents_data->set_time_to_purge(time_to_purge1);

  // No tabs are not purged yet.
  ASSERT_FALSE(tab1_contents_data->is_purged());
  ASSERT_FALSE(tab2_contents_data->is_purged());
  ASSERT_FALSE(tab3_contents_data->is_purged());

  // Advance the clock for time_to_purge1.
  test_clock_.Advance(time_to_purge1);
  tab_manager()->PurgeBackgroundedTabsIfNeeded();

  ASSERT_FALSE(tab1_contents_data->is_purged());
  ASSERT_FALSE(tab2_contents_data->is_purged());
  ASSERT_FALSE(tab3_contents_data->is_purged());

  // Advance the clock for 1 minutes.
  test_clock_.Advance(base::TimeDelta::FromMinutes(1));
  tab_manager()->PurgeBackgroundedTabsIfNeeded();

  // Since tab1 is kept inactive and background for more than
  // time_to_purge1, tab1 should be purged.
  ASSERT_TRUE(tab1_contents_data->is_purged());
  ASSERT_FALSE(tab2_contents_data->is_purged());
  ASSERT_FALSE(tab3_contents_data->is_purged());

  // Advance the clock.
  test_clock_.Advance(time_to_purge2 - time_to_purge1);
  tab_manager()->PurgeBackgroundedTabsIfNeeded();

  // Since tab2 is kept inactive and background for more than
  // time_to_purge2, tab1 should be purged.
  // Since tab3 is active, tab3 should not be purged.
  ASSERT_TRUE(tab1_contents_data->is_purged());
  ASSERT_TRUE(tab2_contents_data->is_purged());
  ASSERT_FALSE(tab3_contents_data->is_purged());

  tsm()->CloseAllTabs();
}

IN_PROC_BROWSER_TEST_F(TabManagerTestWithTwoTabs,
                       ProactiveFastShutdownSingleTabProcess) {
  // The Tab Manager should be able to fast-kill a process for the discarded tab
  // on all platforms, as each tab will be running in a separate process by
  // itself regardless of the discard reason.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::NotificationService::AllSources());
  base::HistogramTester tester;
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::PROACTIVE));
  SimulateFreezeSignal(GetWebContentsAt(1));

  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldFastShutdown", true, 1);
  observer.Wait();
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

IN_PROC_BROWSER_TEST_F(TabManagerTest, ProactiveFastShutdownSharedTabProcess) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Set max renderers to 1 before opening tabs to force running out of
  // processes and for both these tabs to share a renderer.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);
  OpenTwoTabs(embedded_test_server()->GetURL("a.com", "/title1.html"),
              embedded_test_server()->GetURL("a.com", "/title2.html"));
  EXPECT_EQ(tsm()->GetWebContentsAt(0)->GetMainFrame()->GetProcess(),
            tsm()->GetWebContentsAt(1)->GetMainFrame()->GetProcess());

  // The Tab Manager will not be able to fast-kill either of the tabs since they
  // share the same process regardless of the discard reason. No unsafe attempts
  // will be made.
  base::HistogramTester tester;
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::PROACTIVE));
  SimulateFreezeSignal(GetWebContentsAt(1));

  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldFastShutdown", false, 1);
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
#ifdef OS_CHROMEOS
  // The unsafe killing attempt will fail for the same reason.
  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldUnsafeFastShutdown", false, 1);
#endif  // OS_CHROMEOS
  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldFastShutdown", false, 1);
}

IN_PROC_BROWSER_TEST_F(TabManagerTest,
                       DISABLED_ProactiveFastShutdownWithUnloadHandler) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Disable the protection of recent tabs.
  OpenTwoTabs(GURL(chrome::kChromeUIAboutURL),
              GURL(embedded_test_server()->GetURL("/unload.html")));

  // Advance time so everything is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  base::HistogramTester tester;
  // The Tab Manager will not be able to safely fast-kill either of the tabs as
  // one of them is current, and the other has an unload handler. No unsafe
  // attempts will be made.
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::PROACTIVE));
  SimulateFreezeSignal(GetWebContentsAt(1));

  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldFastShutdown", false, 1);
}

IN_PROC_BROWSER_TEST_F(TabManagerTest, UrgentFastShutdownWithUnloadHandler) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Disable the protection of recent tabs.
  OpenTwoTabs(GURL(chrome::kChromeUIAboutURL),
              GURL(embedded_test_server()->GetURL("/unload.html")));

  // Advance time so everything is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  // The Tab Manager will not be able to safely fast-kill either of the tabs as
  // one of them is current, and the other has an unload handler. An unsafe
  // attempt will be made on some platforms.
  base::HistogramTester tester;
#ifdef OS_CHROMEOS
  // The unsafe attempt for ChromeOS should succeed as ChromeOS ignores unload
  // handlers when in critical condition.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::NotificationService::AllSources());
#endif  // OS_CHROMEOS
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));
#ifdef OS_CHROMEOS
  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldUnsafeFastShutdown", true, 1);
  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldFastShutdown", true, 1);
  observer.Wait();
#else
  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldFastShutdown", false, 1);
#endif  // OS_CHROMEOS
}

// https://crbug.com/874915, flaky on all platform
IN_PROC_BROWSER_TEST_F(TabManagerTest,
                       DISABLED_ProactiveFastShutdownWithBeforeunloadHandler) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Disable the protection of recent tabs.
  OpenTwoTabs(GURL(chrome::kChromeUIAboutURL),
              GURL(embedded_test_server()->GetURL("/beforeunload.html")));

  // Advance time so everything is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  // The Tab Manager will not be able to safely fast-kill either of the tabs as
  // one of them is current, and the other has a beforeunload handler. No unsafe
  // attempts will be made.
  base::HistogramTester tester;
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::PROACTIVE));
  SimulateFreezeSignal(GetWebContentsAt(1));

  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldFastShutdown", false, 1);
}

IN_PROC_BROWSER_TEST_F(TabManagerTest,
                       UrgentFastShutdownWithBeforeunloadHandler) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Disable the protection of recent tabs.
  OpenTwoTabs(GURL(chrome::kChromeUIAboutURL),
              GURL(embedded_test_server()->GetURL("/beforeunload.html")));

  // Advance time so everything is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  // The Tab Manager will not be able to safely fast-kill either of the tabs as
  // one of them is current, and the other has a beforeunload handler. An unsafe
  // attempt will be made on some platforms.
  base::HistogramTester tester;
  EXPECT_TRUE(
      tab_manager()->DiscardTabImpl(LifecycleUnitDiscardReason::URGENT));
#ifdef OS_CHROMEOS
  // The unsafe killing attempt will fail as ChromeOS does not ignore
  // beforeunload handlers.
  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldUnsafeFastShutdown", false, 1);
#endif  // OS_CHROMEOS
  tester.ExpectUniqueSample(
      "TabManager.Discarding.DiscardedTabCouldFastShutdown", false, 1);
}

// Verifies the following state transitions for a tab:
// - Initial state: ACTIVE
// - Freeze(): ACTIVE->PENDING_FREEZE
// - Freeze happens in renderer: PENDING_FREEZE->FROZEN
// - Tab is made visible: FROZEN->ACTIVE
IN_PROC_BROWSER_TEST_F(TabManagerTest, TabFreezeAndMakeVisible) {
  TestTransitionFromActiveToFrozen();

  // Make the tab visible. It should transition to the ACTIVE state.
  GetWebContentsAt(1)->WasShown();
  {
    ExpectStateTransitionObserver expect_state_transition(
        GetLifecycleUnitAt(1), LifecycleUnitState::ACTIVE);
    expect_state_transition.Wait();
  }
}

// Verifies the following state transitions for a tab:
// - Initial state: ACTIVE
// - Freeze(): ACTIVE->PENDING_FREEZE
// - Freeze happens in renderer: PENDING_FREEZE->FROZEN
// - Unfreeze(): FROZEN->ACTIVE
IN_PROC_BROWSER_TEST_F(TabManagerTest, TabFreezeAndUnfreeze) {
  TestTransitionFromActiveToFrozen();

  // Unfreeze the tab. It should immediately transition to the PENDING_FREEZE
  // state. Then, it shuold transition to the ACTIVE state once the "onresume"
  // callback has run.
  EXPECT_TRUE(GetLifecycleUnitAt(1)->Unfreeze());
  EXPECT_EQ(LifecycleUnitState::PENDING_UNFREEZE,
            GetLifecycleUnitAt(1)->GetState());
  {
    ExpectStateTransitionObserver expect_state_transition(
        GetLifecycleUnitAt(1), LifecycleUnitState::ACTIVE);
    expect_state_transition.Wait();
  }
}

// Verifies the following state transitions for a tab:
// - Initial state: ACTIVE
// - Freeze(): ACTIVE->PENDING_FREEZE
// - Unfreeze(): PENDING_FREEZE->FROZEN
IN_PROC_BROWSER_TEST_F(TabManagerTest, TabPendingFreezeAndUnfreeze) {
  TestTransitionFromActiveToPendingFreeze();

  EXPECT_EQ(LifecycleUnitState::PENDING_FREEZE,
            GetLifecycleUnitAt(1)->GetState());

  {
    ExpectStateTransitionObserver expect_state_transition(
        GetLifecycleUnitAt(1), LifecycleUnitState::FROZEN);
    expect_state_transition.Wait();
  }
}

// Verifies the following state transitions for a tab:
// - Initial state: ACTIVE
// - Freeze(): ACTIVE->PENDING_FREEZE
// - Discard(kProactive): PENDING_FREEZE->PENDING_DISCARD
// - Freeze happens in renderer: PENDING_DISCARD->DISCARDED
IN_PROC_BROWSER_TEST_F(TabManagerTest,
                       TabFreezeAndProactiveDiscardBeforeFreezeCompletes) {
  TestTransitionFromActiveToPendingFreeze();

  // Proactively discard the background tab.
  EXPECT_TRUE(
      GetLifecycleUnitAt(1)->Discard(LifecycleUnitDiscardReason::PROACTIVE));
  EXPECT_EQ(LifecycleUnitState::PENDING_DISCARD,
            GetLifecycleUnitAt(1)->GetState());

  // The tab should transition to DISCARDED after the renderer freezes the page.
  ExpectStateTransitionObserver expect_state_transition(
      GetLifecycleUnitAt(1), LifecycleUnitState::DISCARDED);
  expect_state_transition.Wait();
}

// Verifies the following state transitions for a tab:
// - Initial state: ACTIVE
// - Freeze(): ACTIVE->PENDING_FREEZE
// - Discard(kUrgent): PENDING_FREEZE->DISCARDED
IN_PROC_BROWSER_TEST_F(TabManagerTestWithTwoTabs,
                       TabFreezeAndUrgentDiscardBeforeFreezeCompletes) {
  // Advance time so everything is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  // Freeze the background tab.
  EXPECT_EQ(LifecycleUnitState::ACTIVE, GetLifecycleUnitAt(1)->GetState());
  EXPECT_TRUE(GetLifecycleUnitAt(1)->Freeze());
  EXPECT_EQ(LifecycleUnitState::PENDING_FREEZE,
            GetLifecycleUnitAt(1)->GetState());

  // Urgently discard the background tab.
  EXPECT_TRUE(
      GetLifecycleUnitAt(1)->Discard(LifecycleUnitDiscardReason::URGENT));
  EXPECT_EQ(LifecycleUnitState::DISCARDED, GetLifecycleUnitAt(1)->GetState());
}

// Verifies the following state transitions for a tab:
// - Initial state: ACTIVE
// - Freeze(): ACTIVE->PENDING_FREEZE
// - Freeze happens in renderer: PENDING_FREEZE->FROZEN
// - Discard(kUrgent): FROZEN->DISCARDED
IN_PROC_BROWSER_TEST_F(TabManagerTest, TabFreezeAndUrgentDiscard) {
  // Advance time so everything is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  TestTransitionFromActiveToFrozen();

  // Urgently discard the background tab.
  EXPECT_TRUE(
      GetLifecycleUnitAt(1)->Discard(LifecycleUnitDiscardReason::URGENT));
  EXPECT_EQ(LifecycleUnitState::DISCARDED, GetLifecycleUnitAt(1)->GetState());
}

// Verifies the following state transitions for a tab:
// - Initial state: ACTIVE
// - Discard(kProactive): ACTIVE->PENDING_DISCARD
// - Focus: PENDING_DISCARD->PENDING_FREEZE
// - (optional) Freeze happens in renderer: PENDING_FREEZE->FROZEN
// - Renderer is notified of new visibility: PENDING_FREEZE->ACTIVE or
//   FROZEN->ACTIVE.
IN_PROC_BROWSER_TEST_F(TabManagerTestWithTwoTabs,
                       TabProactiveDiscardAndFocusBeforeFreezeCompletes) {
  // Proactively discard the background tab.
  EXPECT_EQ(LifecycleUnitState::ACTIVE, GetLifecycleUnitAt(1)->GetState());
  EXPECT_TRUE(
      GetLifecycleUnitAt(1)->Discard(LifecycleUnitDiscardReason::PROACTIVE));
  EXPECT_EQ(LifecycleUnitState::PENDING_DISCARD,
            GetLifecycleUnitAt(1)->GetState());

  // Focus and make visible the background tab. It should transition to
  // PENDING_FREEZE, to indicate that there is a freeze on its way to the
  // renderer but that no discard should happen if the renderer freezes the page
  // before being notified that it became visible.
  tsm()->ActivateTabAt(1, true);
  GetWebContentsAt(1)->WasShown();
  EXPECT_EQ(LifecycleUnitState::PENDING_FREEZE,
            GetLifecycleUnitAt(1)->GetState());

  // The tab should eventually transition to ACTIVE. If the renderer freezes the
  // page before being notified that the tab became visible, there could be an
  // intermediate transition to FROZEN.
  ExpectStateTransitionObserver expect_state_transition(
      GetLifecycleUnitAt(1), LifecycleUnitState::ACTIVE);
  expect_state_transition.AllowState(LifecycleUnitState::FROZEN);
  expect_state_transition.Wait();
}

// Verifies the following state transitions for a tab:
// - Initial state: ACTIVE
// - Discard(kProactive): ACTIVE->PENDING_DISCARD
// - Freeze happens in renderer: PENDING_DISCARD->DISCARDED
// - Focus: DISCARDED->ACTIVE
IN_PROC_BROWSER_TEST_F(TabManagerTestWithTwoTabs,
                       TabProactiveDiscardAndFocusToReload) {
  // Proactively discard the background tab.
  EXPECT_EQ(LifecycleUnitState::ACTIVE, GetLifecycleUnitAt(1)->GetState());
  EXPECT_TRUE(
      GetLifecycleUnitAt(1)->Discard(LifecycleUnitDiscardReason::PROACTIVE));
  EXPECT_EQ(LifecycleUnitState::PENDING_DISCARD,
            GetLifecycleUnitAt(1)->GetState());

  // After the freeze happens in the renderer, the tab is discarded.
  {
    ExpectStateTransitionObserver expect_state_transition(
        GetLifecycleUnitAt(1), LifecycleUnitState::DISCARDED);
    expect_state_transition.Wait();
  }

  // When the tab is focused and made visible, it transitions to ACTIVE.
  tsm()->ActivateTabAt(1, true);
  GetWebContentsAt(1)->WasShown();
  EXPECT_EQ(LifecycleUnitState::ACTIVE, GetLifecycleUnitAt(1)->GetState());
}

// Verifies the following state transitions for a tab:
// - Initial state: ACTIVE
// - Discard(kProactive): ACTIVE->PENDING_DISCARD
// - Freeze(): Disallowed
// - Freeze happens in renderer: PENDING_DISCARD->DISCARDED
IN_PROC_BROWSER_TEST_F(TabManagerTestWithTwoTabs,
                       TabFreezeDisallowedWhenProactivelyDiscarding) {
  // Proactively discard the background tab.
  EXPECT_EQ(LifecycleUnitState::ACTIVE, GetLifecycleUnitAt(1)->GetState());
  EXPECT_TRUE(
      GetLifecycleUnitAt(1)->Discard(LifecycleUnitDiscardReason::PROACTIVE));
  EXPECT_EQ(LifecycleUnitState::PENDING_DISCARD,
            GetLifecycleUnitAt(1)->GetState());

  // Freezing the tab should be disallowed.
  DecisionDetails decision_details;
  EXPECT_FALSE(GetLifecycleUnitAt(1)->CanFreeze(&decision_details));
  EXPECT_FALSE(GetLifecycleUnitAt(1)->Freeze());
  EXPECT_EQ(LifecycleUnitState::PENDING_DISCARD,
            GetLifecycleUnitAt(1)->GetState());

  // The tab should eventually transition to DISCARDED.
  ExpectStateTransitionObserver expect_state_transition(
      GetLifecycleUnitAt(1), LifecycleUnitState::DISCARDED);
  expect_state_transition.Wait();
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
  TabLifecycleUnitExternal::FromWebContents(web_contents)->DiscardTab();
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
  EXPECT_FALSE(content::SiteInstance::IsSameWebSite(
      browser()->profile(), main_frame->GetLastCommittedURL(),
      child_frame->GetLastCommittedURL()));
  if (content::AreAllSitesIsolatedForTesting()) {
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
  TabLifecycleUnitExternal::FromWebContents(contents)->DiscardTab();

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
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  }

  EXPECT_EQ(num_tabs, browser->tab_strip_model()->count());
}

// Creates a browser with |num_tabs| tabs.
Browser* CreateBrowserWithTabs(int num_tabs) {
  Browser* current_browser = BrowserList::GetInstance()->GetLastActive();
  ui_test_utils::BrowserAddedObserver browser_added_observer;
  chrome::NewWindow(current_browser);
  Browser* new_browser = browser_added_observer.WaitForSingleNewBrowser();
  EXPECT_EQ(new_browser, BrowserList::GetInstance()->GetLastActive());
  EnsureTabsInBrowser(new_browser, num_tabs);
  return new_browser;
}

}  // namespace

// Flaky on Linux.  Times out on Windows debug builds. http://crbug.com/772839.
#if defined(OS_LINUX) || (defined(OS_WIN) && !defined(NDEBUG))
#define MAYBE_DiscardTabsWithMinimizedAndOccludedWindows \
  DISABLED_DiscardTabsWithMinimizedAndOccludedWindows
#else
#define MAYBE_DiscardTabsWithMinimizedAndOccludedWindows \
  DiscardTabsWithMinimizedAndOccludedWindows
#endif
IN_PROC_BROWSER_TEST_F(TabManagerTest,
                       MAYBE_DiscardTabsWithMinimizedAndOccludedWindows) {
  // Covered by |browser2|.
  Browser* browser1 = browser();
  EnsureTabsInBrowser(browser1, 2);
  browser1->window()->SetBounds(gfx::Rect(10, 10, 10, 10));
  // Covers |browser1|.
  Browser* browser2 = CreateBrowserWithTabs(2);
  EXPECT_NE(browser1, browser2);
  browser2->window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  // Active browser.
  Browser* browser3 = CreateBrowserWithTabs(2);
  EXPECT_NE(browser1, browser3);
  EXPECT_NE(browser2, browser3);
  browser3->window()->SetBounds(gfx::Rect(110, 0, 100, 100));
  // Minimized browser.
  Browser* browser4 = CreateBrowserWithTabs(2);
  browser4->window()->Minimize();
  EXPECT_NE(browser1, browser4);
  EXPECT_NE(browser2, browser4);
  EXPECT_NE(browser3, browser4);

  // Advance time so everything is urgent discardable.
  test_clock_.Advance(kBackgroundUrgentProtectionTime);

  for (int i = 0; i < 8; ++i)
    tab_manager()->DiscardTab(LifecycleUnitDiscardReason::PROACTIVE);

  base::RunLoop().RunUntilIdle();

// On ChromeOS, active tabs are discarded if their window is non-visible. On
// other platforms, they are never discarded.
#if defined(OS_CHROMEOS)
  EXPECT_TRUE(IsTabDiscarded(browser1->tab_strip_model()->GetWebContentsAt(0)));
  EXPECT_FALSE(
      IsTabDiscarded(browser2->tab_strip_model()->GetWebContentsAt(0)));
  EXPECT_FALSE(
      IsTabDiscarded(browser3->tab_strip_model()->GetWebContentsAt(0)));
  EXPECT_TRUE(IsTabDiscarded(browser4->tab_strip_model()->GetWebContentsAt(0)));
#else
  EXPECT_FALSE(
      IsTabDiscarded(browser1->tab_strip_model()->GetWebContentsAt(0)));
  EXPECT_FALSE(
      IsTabDiscarded(browser2->tab_strip_model()->GetWebContentsAt(0)));
  EXPECT_FALSE(
      IsTabDiscarded(browser3->tab_strip_model()->GetWebContentsAt(0)));
  EXPECT_FALSE(
      IsTabDiscarded(browser4->tab_strip_model()->GetWebContentsAt(0)));
#endif

  // Non-active tabs can be discarded on all platforms.
  EXPECT_TRUE(IsTabDiscarded(browser1->tab_strip_model()->GetWebContentsAt(1)));
  EXPECT_TRUE(IsTabDiscarded(browser2->tab_strip_model()->GetWebContentsAt(1)));
  EXPECT_TRUE(IsTabDiscarded(browser3->tab_strip_model()->GetWebContentsAt(1)));
  EXPECT_TRUE(IsTabDiscarded(browser4->tab_strip_model()->GetWebContentsAt(1)));
}

IN_PROC_BROWSER_TEST_F(TabManagerTest, UnfreezeTabOnNavigationEvent) {
  TestTransitionFromActiveToPendingFreeze();

  browser()->tab_strip_model()->GetWebContentsAt(1)->GetController().Reload(
      content::ReloadType::NORMAL, false);

  ExpectStateTransitionObserver expect_state_transition(
      GetLifecycleUnitAt(1), LifecycleUnitState::ACTIVE);
  expect_state_transition.AllowState(LifecycleUnitState::PENDING_UNFREEZE);
  expect_state_transition.AllowState(LifecycleUnitState::FROZEN);
  expect_state_transition.Wait();
}

}  // namespace resource_coordinator

#endif  // OS_WIN || OS_MAXOSX || OS_LINUX || defined(OS_CHROMEOS)
