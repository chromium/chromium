// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/background_tab_navigation_throttle.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_unittest_utils.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_webcontents_observer.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/tab_manager_resource_coordinator_signal_observer.h"
#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"
#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/sessions/tab_loader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::MockNavigationHandle;
using content::NavigationThrottle;
using content::WebContents;
using content::WebContentsTester;

namespace resource_coordinator {
namespace {

using LoadingState = TabLoadTracker::LoadingState;

constexpr char kTestUrl[] = "http://www.example.com";

// Default parameters for testing proactive LifecycleUnit discarding.
constexpr int kLowLoadedTabCount = 5;
constexpr int kModerateLoadedTabCount = 10;
constexpr int kHighLoadedTabCount = 20;
constexpr base::TimeDelta kLowOccludedTimeout =
    base::TimeDelta::FromMinutes(100);
constexpr base::TimeDelta kModerateOccludedTimeout =
    base::TimeDelta::FromMinutes(10);
constexpr base::TimeDelta kHighOccludedTimeout =
    base::TimeDelta::FromMinutes(1);
constexpr base::TimeDelta kFreezeTimeout = base::TimeDelta::FromMinutes(10);
constexpr base::TimeDelta kUnfreezeTimeout = base::TimeDelta::FromMinutes(10);
constexpr base::TimeDelta kRefreezeTimeout = base::TimeDelta::FromSeconds(5);

class NonResumingBackgroundTabNavigationThrottle
    : public BackgroundTabNavigationThrottle {
 public:
  explicit NonResumingBackgroundTabNavigationThrottle(
      content::NavigationHandle* handle)
      : BackgroundTabNavigationThrottle(handle) {}

  void ResumeNavigation() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NonResumingBackgroundTabNavigationThrottle);
};

enum TestIndicies {
  kAutoDiscardable,
  kPinned,
  kApp,
  kPlayingAudio,
  kFormEntry,
  kRecent,
  kOld,
  kReallyOld,
  kOldButPinned,
  kInternalPage,
};

// Helper class to simulate being offline. NetworkChangeNotifier is a
// singleton, making this instance is actually globally accessible. Users of
// this class should first create a net::NetworkChangeNotifier::DisableForTest
// object to allow the creation of this new NetworkChangeNotifier.
class FakeOfflineNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  ConnectionType GetCurrentConnectionType() const override {
    return NetworkChangeNotifier::CONNECTION_NONE;
  }
};

}  // namespace

class TabManagerTest : public testing::ChromeTestHarnessWithLocalDB {
 public:
  TabManagerTest()
      : scoped_context_(
            std::make_unique<base::TestMockTimeTaskRunner::ScopedContext>(
                task_runner_)),
        scoped_set_tick_clock_for_testing_(task_runner_->GetMockTickClock()) {
    base::MessageLoopCurrent::Get()->SetTaskRunner(task_runner_);

    // Start with a non-zero time.
    task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(42));
  }

  std::unique_ptr<WebContents> CreateWebContents() {
    std::unique_ptr<WebContents> web_contents = CreateTestWebContents();
    ResourceCoordinatorTabHelper::CreateForWebContents(web_contents.get());
    // Commit an URL to allow discarding.
    content::WebContentsTester::For(web_contents.get())
        ->NavigateAndCommit(GURL("https://www.example.com"));

    base::RepeatingClosure run_loop_cb = base::BindRepeating(
        &base::TestMockTimeTaskRunner::RunUntilIdle, task_runner_);

    testing::WaitForLocalDBEntryToBeInitialized(web_contents.get(),
                                                run_loop_cb);
    testing::ExpireLocalDBObservationWindows(web_contents.get());
    return web_contents;
  }

  void SetUp() override {
    ChromeTestHarnessWithLocalDB::SetUp();
    tab_manager_ = g_browser_process->GetTabManager();
  }

  void ResetState() {
    // Simulate the DidFinishNavigation notification coming from the
    // NavigationHandles.
    if (nav_handle1_)
      tab_manager_->OnDidFinishNavigation(nav_handle1_.get());
    if (nav_handle2_)
      tab_manager_->OnDidFinishNavigation(nav_handle2_.get());
    if (nav_handle3_)
      tab_manager_->OnDidFinishNavigation(nav_handle3_.get());

    // NavigationHandles and NavigationThrottles must be deleted before the
    // associated WebContents.
    throttle1_.reset();
    throttle2_.reset();
    throttle3_.reset();
    nav_handle1_.reset();
    nav_handle2_.reset();
    nav_handle3_.reset();

    // WebContents must be deleted before
    // ChromeTestHarnessWithLocalDB::TearDown() deletes the
    // RenderProcessHost.
    contents1_.reset();
    contents2_.reset();
    contents3_.reset();
  }

  void TearDown() override {
    ResetState();

    task_runner_->RunUntilIdle();
    scoped_context_.reset();
    ChromeTestHarnessWithLocalDB::TearDown();
  }

  void PrepareTabs(const char* url1 = kTestUrl,
                   const char* url2 = kTestUrl,
                   const char* url3 = kTestUrl) {
    contents1_ = CreateTestWebContents();
    performance_manager::PerformanceManagerTabHelper::CreateForWebContents(
        contents1_.get());
    ResourceCoordinatorTabHelper::CreateForWebContents(contents1_.get());
    nav_handle1_ = CreateTabAndNavigation(url1, contents1_.get());
    contents2_ = CreateTestWebContents();
    performance_manager::PerformanceManagerTabHelper::CreateForWebContents(
        contents2_.get());
    ResourceCoordinatorTabHelper::CreateForWebContents(contents2_.get());
    nav_handle2_ = CreateTabAndNavigation(url2, contents2_.get());
    contents3_ = CreateTestWebContents();
    performance_manager::PerformanceManagerTabHelper::CreateForWebContents(
        contents3_.get());
    ResourceCoordinatorTabHelper::CreateForWebContents(contents3_.get());
    nav_handle3_ = CreateTabAndNavigation(url3, contents3_.get());

    contents1_->WasHidden();
    contents2_->WasHidden();
    contents3_->WasHidden();

    throttle1_ = std::make_unique<NonResumingBackgroundTabNavigationThrottle>(
        nav_handle1_.get());
    throttle2_ = std::make_unique<NonResumingBackgroundTabNavigationThrottle>(
        nav_handle2_.get());
    throttle3_ = std::make_unique<NonResumingBackgroundTabNavigationThrottle>(
        nav_handle3_.get());
  }

  // Simulate creating 3 tabs and their navigations.
  void MaybeThrottleNavigations(TabManager* tab_manager,
                                size_t loading_slots = 1,
                                const char* url1 = kTestUrl,
                                const char* url2 = kTestUrl,
                                const char* url3 = kTestUrl) {
    PrepareTabs(url1, url2, url3);

    NavigationThrottle::ThrottleCheckResult result1 =
        tab_manager->MaybeThrottleNavigation(throttle1_.get());
    NavigationThrottle::ThrottleCheckResult result2 =
        tab_manager->MaybeThrottleNavigation(throttle2_.get());
    NavigationThrottle::ThrottleCheckResult result3 =
        tab_manager->MaybeThrottleNavigation(throttle3_.get());

    CheckThrottleResults(result1, result2, result3, loading_slots);
  }

  bool IsTabDiscarded(content::WebContents* content) {
    return TabLifecycleUnitExternal::FromWebContents(content)->IsDiscarded();
  }

  TabLifecycleUnitSource::TabLifecycleUnit* GetTabLifecycleUnit(
      content::WebContents* content) {
    return GetTabLifecycleUnitSource()->GetTabLifecycleUnit(content);
  }

  bool IsTabFrozen(content::WebContents* content) {
    const LifecycleUnitState state = GetTabLifecycleUnit(content)->GetState();
    return state == LifecycleUnitState::PENDING_FREEZE ||
           state == LifecycleUnitState::FROZEN;
  }

  void SimulateFreezeCompletion(content::WebContents* content) {
    GetTabLifecycleUnit(content)->UpdateLifecycleState(
        performance_manager::mojom::LifecycleState::kFrozen);
  }

  void SimulateUnfreezeCompletion(content::WebContents* content) {
    GetTabLifecycleUnit(content)->UpdateLifecycleState(
        performance_manager::mojom::LifecycleState::kRunning);
  }

  virtual void CheckThrottleResults(
      NavigationThrottle::ThrottleCheckResult result1,
      NavigationThrottle::ThrottleCheckResult result2,
      NavigationThrottle::ThrottleCheckResult result3,
      size_t loading_slots) {
    // First tab starts navigation right away because there is no tab loading.
    EXPECT_EQ(content::NavigationThrottle::PROCEED, result1);
    switch (loading_slots) {
      case 1:
        EXPECT_EQ(content::NavigationThrottle::DEFER, result2);
        EXPECT_EQ(content::NavigationThrottle::DEFER, result3);
        break;
      case 2:
        EXPECT_EQ(content::NavigationThrottle::PROCEED, result2);
        EXPECT_EQ(content::NavigationThrottle::DEFER, result3);
        break;
      case 3:
        EXPECT_EQ(content::NavigationThrottle::PROCEED, result2);
        EXPECT_EQ(content::NavigationThrottle::PROCEED, result3);
        break;
      default:
        NOTREACHED();
    }
  }

 protected:
  std::unique_ptr<MockNavigationHandle> CreateTabAndNavigation(
      const char* url,
      content::WebContents* web_contents) {
    TabUIHelper::CreateForWebContents(web_contents);
    return std::make_unique<MockNavigationHandle>(GURL(url),
                                                  web_contents->GetMainFrame());
  }

  TabManager* tab_manager_ = nullptr;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_ =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  std::unique_ptr<base::TestMockTimeTaskRunner::ScopedContext> scoped_context_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;
  std::unique_ptr<BackgroundTabNavigationThrottle> throttle1_;
  std::unique_ptr<BackgroundTabNavigationThrottle> throttle2_;
  std::unique_ptr<BackgroundTabNavigationThrottle> throttle3_;
  std::unique_ptr<MockNavigationHandle> nav_handle1_;
  std::unique_ptr<MockNavigationHandle> nav_handle2_;
  std::unique_ptr<MockNavigationHandle> nav_handle3_;
  std::unique_ptr<WebContents> contents1_;
  std::unique_ptr<WebContents> contents2_;
  std::unique_ptr<WebContents> contents3_;
};

class TabManagerWithExperimentDisabledTest : public TabManagerTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(
        features::kStaggeredBackgroundTabOpeningExperiment);
    TabManagerTest::SetUp();
  }

  void CheckThrottleResults(NavigationThrottle::ThrottleCheckResult result1,
                            NavigationThrottle::ThrottleCheckResult result2,
                            NavigationThrottle::ThrottleCheckResult result3,
                            size_t loading_slots) override {
    EXPECT_EQ(content::NavigationThrottle::PROCEED, result1);
    EXPECT_EQ(content::NavigationThrottle::PROCEED, result2);
    EXPECT_EQ(content::NavigationThrottle::PROCEED, result3);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class TabManagerWithProactiveDiscardExperimentEnabledTest
    : public TabManagerTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kProactiveTabFreezeAndDiscard);

    // Pretend that Chrome is in use.
    metrics::DesktopSessionDurationTracker::Initialize();
    MarkChromeInUse(true);

    TabManagerTest::SetUp();

    // Use test constants for proactive discarding parameters.
    tab_manager_->proactive_freeze_discard_params_ =
        GetTestProactiveDiscardParams();
  }

  void TearDown() override {
    TabManagerTest::TearDown();
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
  }

  void MarkChromeInUse(bool in_use) {
    auto* tracker = metrics::DesktopSessionDurationTracker::Get();
    if (in_use) {
      tracker->OnVisibilityChanged(true, base::TimeDelta());
      tracker->OnUserEvent();
    } else {
      tracker->OnVisibilityChanged(false, base::TimeDelta());
    }
  }

  ProactiveTabFreezeAndDiscardParams GetTestProactiveDiscardParams() {
    // Return a ProactiveTabFreezeAndDiscardParams struct with default test
    // parameters.
    ProactiveTabFreezeAndDiscardParams params = {};
    params.should_proactively_discard = true;
    params.should_periodically_unfreeze = true;
    params.low_occluded_timeout = kLowOccludedTimeout;
    params.moderate_occluded_timeout = kModerateOccludedTimeout;
    params.high_occluded_timeout = kHighOccludedTimeout;
    params.low_loaded_tab_count = kLowLoadedTabCount;
    params.moderate_loaded_tab_count = kModerateLoadedTabCount;
    params.high_loaded_tab_count = kHighLoadedTabCount;
    params.freeze_timeout = kFreezeTimeout;
    params.unfreeze_timeout = kUnfreezeTimeout;
    params.refreeze_timeout = kRefreezeTimeout;
    return params;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(georgesak): Add tests for protection to tabs with form input and
// playing audio;

TEST_F(TabManagerTest, IsInternalPage) {
  EXPECT_TRUE(TabManager::IsInternalPage(GURL(chrome::kChromeUIDownloadsURL)));
  EXPECT_TRUE(TabManager::IsInternalPage(GURL(chrome::kChromeUIHistoryURL)));
  EXPECT_TRUE(TabManager::IsInternalPage(GURL(chrome::kChromeUINewTabURL)));
  EXPECT_TRUE(TabManager::IsInternalPage(GURL(chrome::kChromeUISettingsURL)));

// Debugging URLs are not included.
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(TabManager::IsInternalPage(GURL(chrome::kChromeUIDiscardsURL)));
#endif
  EXPECT_FALSE(
      TabManager::IsInternalPage(GURL(chrome::kChromeUINetInternalsURL)));

  // Prefix matches are included.
  GURL::Replacements replace_fake_path;
  replace_fake_path.SetPathStr("fakeSetting");
  EXPECT_TRUE(TabManager::IsInternalPage(
      GURL(chrome::kChromeUISettingsURL).ReplaceComponents(replace_fake_path)));
}

// Data race on Linux. http://crbug.com/787842
// Flaky on Mac and Windows: https://crbug.com/995682
#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
#define MAYBE_DiscardTabWithNonVisibleTabs DISABLED_DiscardTabWithNonVisibleTabs
#else
#define MAYBE_DiscardTabWithNonVisibleTabs DiscardTabWithNonVisibleTabs
#endif

// Verify that:
// - On ChromeOS, DiscardTab can discard every non-visible tab, but cannot
//   discard a visible tab.
// - On other platforms, DiscardTab can discard every tab that is not active in
//   its tab strip.
TEST_F(TabManagerTest, MAYBE_DiscardTabWithNonVisibleTabs) {
  // Create 2 tab strips. Simulate the second tab strip being hidden by hiding
  // its active tab.
  auto window1 = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params1(profile(), true);
  params1.type = Browser::TYPE_NORMAL;
  params1.window = window1.get();
  auto browser1 = std::make_unique<Browser>(params1);
  TabStripModel* tab_strip1 = browser1->tab_strip_model();
  tab_strip1->AppendWebContents(CreateWebContents(), true);
  tab_strip1->AppendWebContents(CreateWebContents(), false);
  tab_strip1->GetWebContentsAt(0)->WasShown();
  tab_strip1->GetWebContentsAt(1)->WasHidden();

  auto window2 = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params2(profile(), true);
  params2.type = Browser::TYPE_NORMAL;
  params2.window = window2.get();
  auto browser2 = std::make_unique<Browser>(params1);
  TabStripModel* tab_strip2 = browser2->tab_strip_model();
  tab_strip2->AppendWebContents(CreateWebContents(), true);
  tab_strip2->AppendWebContents(CreateWebContents(), false);
  tab_strip2->GetWebContentsAt(0)->WasHidden();
  tab_strip2->GetWebContentsAt(1)->WasHidden();

  // Advance time enough that the tabs are urgent discardable.
  task_runner_->AdvanceMockTickClock(kBackgroundUrgentProtectionTime);

  for (int i = 0; i < 4; ++i)
    tab_manager_->DiscardTab(LifecycleUnitDiscardReason::URGENT);

  // Active tab in a visible window should not be discarded.
  EXPECT_FALSE(IsTabDiscarded(tab_strip1->GetWebContentsAt(0)));

  // Non-active tabs should be discarded.
  EXPECT_TRUE(IsTabDiscarded(tab_strip1->GetWebContentsAt(1)));
  EXPECT_TRUE(IsTabDiscarded(tab_strip2->GetWebContentsAt(1)));

#if defined(OS_CHROMEOS)
  // On ChromeOS, a non-visible tab should be discarded even if it's active in
  // its tab strip.
  EXPECT_TRUE(IsTabDiscarded(tab_strip2->GetWebContentsAt(0)));
#else
  // On other platforms, an active tab is never discarded, even if it's not
  // visible.
  EXPECT_FALSE(IsTabDiscarded(tab_strip2->GetWebContentsAt(0)));
#endif  // defined(OS_CHROMEOS)

  // Tabs with a committed URL must be closed explicitly to avoid DCHECK errors.
  tab_strip1->CloseAllTabs();
  tab_strip2->CloseAllTabs();
}

TEST_F(TabManagerTest, MaybeThrottleNavigation) {
  tab_manager_->ResetMemoryPressureListenerForTest();
  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());

  // Tab 1 is loading. The other 2 tabs's navigations are delayed.
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));

  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));
}

TEST_F(TabManagerTest, OnDidFinishNavigation) {
  tab_manager_->ResetMemoryPressureListenerForTest();
  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  tab_manager_->GetWebContentsData(contents2_.get())
      ->DidFinishNavigation(nav_handle2_.get());
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
}

TEST_F(TabManagerTest, OnTabIsLoaded) {
  tab_manager_->ResetMemoryPressureListenerForTest();
  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(ResourceCoordinatorTabHelper::IsLoaded(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));

  // Simulate tab 1 has finished loading.
  TabLoadTracker::Get()->TransitionStateForTesting(contents1_.get(),
                                                   LoadingState::LOADED);

  // After tab 1 has finished loading, TabManager starts loading the next tab.
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(ResourceCoordinatorTabHelper::IsLoaded(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
}

TEST_F(TabManagerTest, OnWebContentsDestroyed) {
  tab_manager_->ResetMemoryPressureListenerForTest();
  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());

  // Tab 2 is destroyed when its navigation is still delayed. Its states are
  // cleaned up.
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  tab_manager_->OnWebContentsDestroyed(contents2_.get());
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));

  // Tab 1 is destroyed when it is still loading. Its states are cleaned up and
  // Tabmanager starts to load the next tab (tab 3).
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));
  tab_manager_->GetWebContentsData(contents1_.get())->WebContentsDestroyed();
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));
}

TEST_F(TabManagerTest, OnDelayedTabSelected) {
  tab_manager_->ResetMemoryPressureListenerForTest();
  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate selecting tab 3, which should start loading immediately.
  tab_manager_->OnActiveTabChanged(contents1_.get(), contents3_.get());

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate tab 1 has finished loading. TabManager will NOT load the next tab
  // (tab 2) because tab 3 is still loading.
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  TabLoadTracker::Get()->TransitionStateForTesting(contents1_.get(),
                                                   LoadingState::LOADED);
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));

  // Simulate tab 3 has finished loading. TabManager starts loading the next tab
  // (tab 2).
  TabLoadTracker::Get()->TransitionStateForTesting(contents3_.get(),
                                                   LoadingState::LOADED);
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
}

TEST_F(TabManagerTest, TimeoutWhenLoadingBackgroundTabs) {
  tab_manager_->ResetMemoryPressureListenerForTest();

  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate timeout when loading the 1st tab. TabManager should start loading
  // the 2nd tab.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(10));

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate timeout again. TabManager should start loading the 3rd tab.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(10));

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));
}

TEST_F(TabManagerTest, BackgroundTabLoadingMode) {
  tab_manager_->ResetMemoryPressureListenerForTest();

  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kStaggered,
            tab_manager_->background_tab_loading_mode_);

  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // TabManager pauses loading pending background tabs.
  tab_manager_->PauseBackgroundTabOpeningIfNeeded();
  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kPaused,
            tab_manager_->background_tab_loading_mode_);

  // Simulate timeout when loading the 1st tab.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(10));

  // Tab 2 and 3 are still pending because of the paused loading mode.
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate tab 1 has finished loading.
  TabLoadTracker::Get()->TransitionStateForTesting(contents1_.get(),
                                                   LoadingState::LOADED);

  // Tab 2 and 3 are still pending because of the paused loading mode.
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // TabManager resumes loading pending background tabs.
  tab_manager_->ResumeBackgroundTabOpeningIfNeeded();
  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kStaggered,
            tab_manager_->background_tab_loading_mode_);

  // Tab 2 should start loading right away.
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));

  // Simulate tab 2 has finished loading.
  TabLoadTracker::Get()->TransitionStateForTesting(contents2_.get(),
                                                   LoadingState::LOADED);

  // Tab 3 should start loading now in staggered loading mode.
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));
}

TEST_F(TabManagerTest, BackgroundTabLoadingSlots) {
  TabManager tab_manager1(TabLoadTracker::Get());
  MaybeThrottleNavigations(&tab_manager1, 1);
  EXPECT_FALSE(tab_manager1.IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_TRUE(tab_manager1.IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager1.IsNavigationDelayedForTest(nav_handle3_.get()));
  ResetState();

  TabManager tab_manager2(TabLoadTracker::Get());
  tab_manager2.SetLoadingSlotsForTest(2);
  MaybeThrottleNavigations(&tab_manager2, 2);
  EXPECT_FALSE(tab_manager2.IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_FALSE(tab_manager2.IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager2.IsNavigationDelayedForTest(nav_handle3_.get()));
  ResetState();

  TabManager tab_manager3(TabLoadTracker::Get());
  tab_manager3.SetLoadingSlotsForTest(3);
  MaybeThrottleNavigations(&tab_manager3, 3);
  EXPECT_FALSE(tab_manager3.IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_FALSE(tab_manager3.IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_FALSE(tab_manager3.IsNavigationDelayedForTest(nav_handle3_.get()));
}

TEST_F(TabManagerTest, BackgroundTabsLoadingOrdering) {
  tab_manager_->ResetMemoryPressureListenerForTest();

  MaybeThrottleNavigations(
      tab_manager_, 1, kTestUrl,
      chrome::kChromeUISettingsURL,  // Using internal page URL for tab 2.
      kTestUrl);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate tab 1 has finished loading. Tab 3 should be loaded before tab 2,
  // because tab 2 is internal page.
  TabLoadTracker::Get()->TransitionStateForTesting(contents1_.get(),
                                                   LoadingState::LOADED);

  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));
}

TEST_F(TabManagerTest, PauseAndResumeBackgroundTabOpening) {
  tab_manager_->ResetMemoryPressureListenerForTest();
  PrepareTabs();

  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kStaggered,
            tab_manager_->background_tab_loading_mode_);
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // Start background tab opening session.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            tab_manager_->MaybeThrottleNavigation(throttle1_.get()));
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // TabManager pauses loading pending background tabs.
  tab_manager_->PauseBackgroundTabOpeningIfNeeded();
  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kPaused,
            tab_manager_->background_tab_loading_mode_);
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // Simulate tab 1 has finished loading, which was scheduled to load before
  // pausing.
  TabLoadTracker::Get()->TransitionStateForTesting(contents1_.get(),
                                                   LoadingState::LOADED);

  // TabManager cannot enter BackgroundTabOpening session when it is in paused
  // mode.
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            tab_manager_->MaybeThrottleNavigation(throttle2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // TabManager resumes loading pending background tabs.
  tab_manager_->ResumeBackgroundTabOpeningIfNeeded();
  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kStaggered,
            tab_manager_->background_tab_loading_mode_);
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // Tab 2 should start loading right away.
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
}

TEST_F(TabManagerTest, IsInBackgroundTabOpeningSession) {
  tab_manager_->ResetMemoryPressureListenerForTest();
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  TabLoadTracker::Get()->TransitionStateForTesting(contents1_.get(),
                                                   LoadingState::LOADED);
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  TabLoadTracker::Get()->TransitionStateForTesting(contents2_.get(),
                                                   LoadingState::LOADED);
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // It is still in background tab opening session even if tab3 is brought to
  // foreground. The session only ends after tab1, tab2 and tab3 have all
  // finished loading.
  contents3_->WasShown();
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  TabLoadTracker::Get()->TransitionStateForTesting(contents3_.get(),
                                                   LoadingState::LOADED);
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());
}

TEST_F(TabManagerWithExperimentDisabledTest, IsInBackgroundTabOpeningSession) {
  EXPECT_FALSE(base::FeatureList::IsEnabled(
      features::kStaggeredBackgroundTabOpeningExperiment));

  tab_manager_->ResetMemoryPressureListenerForTest();
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());
  tab_manager_->GetWebContentsData(contents2_.get())
      ->DidStartNavigation(nav_handle1_.get());
  tab_manager_->GetWebContentsData(contents3_.get())
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  TabLoadTracker::Get()->TransitionStateForTesting(contents1_.get(),
                                                   LoadingState::LOADED);
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  TabLoadTracker::Get()->TransitionStateForTesting(contents2_.get(),
                                                   LoadingState::LOADED);
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // It is still in background tab opening session even if tab3 is brought to
  // foreground. The session only ends after tab1, tab2 and tab3 have all
  // finished loading.
  contents3_->WasShown();
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  TabLoadTracker::Get()->TransitionStateForTesting(contents3_.get(),
                                                   LoadingState::LOADED);
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());
}

TEST_F(TabManagerTest, SessionRestoreBeforeBackgroundTabOpeningSession) {
  tab_manager_->ResetMemoryPressureListenerForTest();
  PrepareTabs();

  // Start session restore.
  tab_manager_->OnSessionRestoreStartedLoadingTabs();
  EXPECT_TRUE(tab_manager_->IsSessionRestoreLoadingTabs());
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  tab_manager_->GetWebContentsData(contents1_.get())
      ->SetIsInSessionRestore(true);
  tab_manager_->GetWebContentsData(contents2_.get())
      ->SetIsInSessionRestore(false);
  tab_manager_->GetWebContentsData(contents3_.get())
      ->SetIsInSessionRestore(false);

  // Do not enter BackgroundTabOpening session if the background tab is in
  // session restore.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            tab_manager_->MaybeThrottleNavigation(throttle1_.get()));
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // Enter BackgroundTabOpening session when there are background tabs not in
  // session restore, though the first background tab can still proceed.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            tab_manager_->MaybeThrottleNavigation(throttle2_.get()));
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  EXPECT_EQ(content::NavigationThrottle::DEFER,
            tab_manager_->MaybeThrottleNavigation(throttle3_.get()));
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // Stop session restore.
  tab_manager_->OnSessionRestoreFinishedLoadingTabs();
  EXPECT_FALSE(tab_manager_->IsSessionRestoreLoadingTabs());
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());
}

TEST_F(TabManagerTest, SessionRestoreAfterBackgroundTabOpeningSession) {
  tab_manager_->ResetMemoryPressureListenerForTest();
  PrepareTabs();

  EXPECT_FALSE(tab_manager_->IsSessionRestoreLoadingTabs());
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // Start background tab opening session.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            tab_manager_->MaybeThrottleNavigation(throttle1_.get()));
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  EXPECT_EQ(content::NavigationThrottle::DEFER,
            tab_manager_->MaybeThrottleNavigation(throttle2_.get()));
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // Now session restore starts after background tab opening session starts.
  tab_manager_->OnSessionRestoreStartedLoadingTabs();
  EXPECT_TRUE(tab_manager_->IsSessionRestoreLoadingTabs());
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // The following background tabs are still delayed if they are not in session
  // restore.
  tab_manager_->GetWebContentsData(contents3_.get())
      ->SetIsInSessionRestore(false);
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            tab_manager_->MaybeThrottleNavigation(throttle3_.get()));

  // The background tab opening session ends after existing tracked tabs have
  // finished loading.
  TabLoadTracker::Get()->TransitionStateForTesting(contents1_.get(),
                                                   LoadingState::LOADED);
  TabLoadTracker::Get()->TransitionStateForTesting(contents2_.get(),
                                                   LoadingState::LOADED);
  TabLoadTracker::Get()->TransitionStateForTesting(contents3_.get(),
                                                   LoadingState::LOADED);
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());
}

TEST_F(TabManagerTest, IsTabRestoredInForeground) {
  std::unique_ptr<WebContents> contents = CreateWebContents();
  contents->WasShown();
  tab_manager_->OnWillRestoreTab(contents.get());
  EXPECT_TRUE(tab_manager_->IsTabRestoredInForeground(contents.get()));

  contents = CreateWebContents();
  contents->WasHidden();
  tab_manager_->OnWillRestoreTab(contents.get());
  EXPECT_FALSE(tab_manager_->IsTabRestoredInForeground(contents.get()));
}

TEST_F(TabManagerTest, TrackingNumberOfLoadedLifecycleUnits) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  // TabManager should start out with 0 loaded LifecycleUnits.
  EXPECT_EQ(tab_manager_->num_loaded_lifecycle_units_, 0);

  // Number of loaded LifecycleUnits should go up by 1 for each new WebContents.
  for (int i = 1; i <= 5; i++) {
    tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
    EXPECT_EQ(tab_manager_->num_loaded_lifecycle_units_, i);
  }

  // Closing loaded tabs should reduce |num_loaded_lifecycle_units_| back to the
  // original amount.
  tab_strip->CloseAllTabs();
  EXPECT_EQ(tab_manager_->num_loaded_lifecycle_units_, 0);

  // Number of loaded LifecycleUnits should go up by 1 for each new WebContents.
  for (int i = 1; i <= 5; i++) {
    tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
    EXPECT_EQ(tab_manager_->num_loaded_lifecycle_units_, i);
  }

  // Number of loaded LifecycleUnits should go down by 1 for each discarded
  // WebContents.
  for (int i = 0; i < 5; i++) {
    TabLifecycleUnitExternal::FromWebContents(tab_strip->GetWebContentsAt(i))
        ->DiscardTab();
    EXPECT_EQ(tab_manager_->num_loaded_lifecycle_units_, 4 - i);
  }

  // All tabs were discarded, so there should be no loaded LifecycleUnits.
  EXPECT_EQ(tab_manager_->num_loaded_lifecycle_units_, 0);

  tab_strip->CloseAllTabs();

  // Closing discarded tabs shouldn't affect |num_loaded_lifecycle_units_|.
  EXPECT_EQ(tab_manager_->num_loaded_lifecycle_units_, 0);
}

TEST_F(TabManagerTest, GetSortedLifecycleUnits) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  const int num_of_tabs_to_test = 20;
  for (int i = 0; i < num_of_tabs_to_test; ++i) {
    task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(10));
    tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
  }

  LifecycleUnitVector lifecycle_units = tab_manager_->GetSortedLifecycleUnits();
  EXPECT_EQ(lifecycle_units.size(), static_cast<size_t>(num_of_tabs_to_test));

  // Check that the lifecycle_units are sorted with ascending importance.
  for (int i = 0; i < num_of_tabs_to_test - 1; ++i) {
    EXPECT_TRUE(lifecycle_units[i]->GetSortKey() <
                lifecycle_units[i + 1]->GetSortKey());
  }

  tab_strip->CloseAllTabs();
}

TEST_F(TabManagerWithProactiveDiscardExperimentEnabledTest,
       GetTimeInBackgroundBeforeProactiveDiscardTest) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  // Move through every tab count in the low state and verify
  // GetTimeInBackgroundBeforeProactiveDiscard returns the low threshold's
  // timeout.
  while (tab_manager_->num_loaded_lifecycle_units_ < kLowLoadedTabCount) {
    EXPECT_EQ(tab_manager_->GetTimeInBackgroundBeforeProactiveDiscard(),
              kLowOccludedTimeout);
    tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
  }

  // Move through every tab count in the moderate state and verify
  // GetTimeInBackgroundBeforeProactiveDiscard returns the moderate threshold's
  // timeout.
  while (tab_manager_->num_loaded_lifecycle_units_ < kModerateLoadedTabCount) {
    EXPECT_EQ(tab_manager_->GetTimeInBackgroundBeforeProactiveDiscard(),
              kModerateOccludedTimeout);
    tab_strip->AppendWebContents(CreateWebContents(), false);
  }

  // Move through every tab count in the high state and verify
  // GetTimeInBackgroundBeforeProactiveDiscard returns the high threshold's
  // timeout.
  while (tab_manager_->num_loaded_lifecycle_units_ < kHighLoadedTabCount) {
    EXPECT_EQ(tab_manager_->GetTimeInBackgroundBeforeProactiveDiscard(),
              kHighOccludedTimeout);
    tab_strip->AppendWebContents(CreateWebContents(), false);
  }

  // Add one tab to move from high state to excessive.
  tab_strip->AppendWebContents(CreateWebContents(), false);
  EXPECT_EQ(tab_manager_->GetTimeInBackgroundBeforeProactiveDiscard(),
            base::TimeDelta());

  tab_strip->CloseAllTabs();

  EXPECT_EQ(tab_manager_->GetTimeInBackgroundBeforeProactiveDiscard(),
            kLowOccludedTimeout);
}

TEST_F(TabManagerWithProactiveDiscardExperimentEnabledTest,
       ProactiveDiscardTestLow) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
  tab_strip->GetWebContentsAt(0)->WasShown();
  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
  tab_strip->GetWebContentsAt(1)->WasShown();

  tab_strip->GetWebContentsAt(0)->WasHidden();

  // Fast forward to just before the low threshold timeout.
  base::TimeDelta less_than_low_timeout =
      kLowOccludedTimeout - base::TimeDelta::FromSeconds(1);
  task_runner_->FastForwardBy(less_than_low_timeout);

  // Verify that 1 second before the Low threshold timeout, nothing has been
  // discarded.
  EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(1)));

  // Fast forward time until past the low threshold timeout
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Verify that once the Low threshold timeout has passed, the hidden tab was
  // discarded.
  EXPECT_TRUE(IsTabDiscarded(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(1)));

  tab_strip->CloseAllTabs();
}

TEST_F(TabManagerWithProactiveDiscardExperimentEnabledTest,
       ProactiveDiscardTestModerate) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  // Create enough tabs to enter the moderate state.
  for (int tabs = 0; tabs < kLowLoadedTabCount; tabs++) {
    tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
    tab_strip->GetWebContentsAt(tabs)->WasShown();
  }

  tab_strip->GetWebContentsAt(0)->WasHidden();

  // Fast forward to just before the moderate threshold timeout.
  base::TimeDelta less_than_moderate_timeout =
      kModerateOccludedTimeout - base::TimeDelta::FromSeconds(1);
  task_runner_->FastForwardBy(less_than_moderate_timeout);

  for (int tab = 0; tab < kLowLoadedTabCount; tab++)
    EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(tab)));

  // Fast forward time until past the moderate threshold timeout.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));

  // The hidden tab (at index 0) should be the only discarded tab.
  EXPECT_TRUE(IsTabDiscarded(tab_strip->GetWebContentsAt(0)));
  for (int tab = 1; tab < kLowLoadedTabCount; tab++)
    EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(tab)));

  tab_strip->CloseAllTabs();
}

TEST_F(TabManagerWithProactiveDiscardExperimentEnabledTest,
       ProactiveDiscardTestHigh) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  // Create enough tabs to enter the high state.
  for (int tabs = 0; tabs < kModerateLoadedTabCount; tabs++) {
    tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
    tab_strip->GetWebContentsAt(tabs)->WasShown();
  }

  tab_strip->GetWebContentsAt(0)->WasHidden();

  // Fast forward to just before the high threshold timeout.
  base::TimeDelta less_than_high_timeout =
      kHighOccludedTimeout - base::TimeDelta::FromSeconds(1);
  task_runner_->FastForwardBy(less_than_high_timeout);

  for (int tab = 0; tab < kModerateLoadedTabCount; tab++)
    EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(tab)));

  // Fast forward time until past the high threshold timeout.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));

  // The hidden tab (at index 0) should be the only discarded tab.
  EXPECT_TRUE(IsTabDiscarded(tab_strip->GetWebContentsAt(0)));
  for (int tab = 1; tab < kModerateLoadedTabCount; tab++)
    EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(tab)));

  tab_strip->CloseAllTabs();
}

TEST_F(TabManagerWithProactiveDiscardExperimentEnabledTest,
       ProactiveDiscardTestExcessive) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  // Create enough tabs to enter the excessive state.
  for (int tabs = 0; tabs < kHighLoadedTabCount; tabs++) {
    tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
    tab_strip->GetWebContentsAt(tabs)->WasShown();
  }

  // Hide a tab and run tasks to let state transitions happen.
  tab_strip->GetWebContentsAt(0)->WasHidden();
  task_runner_->RunUntilIdle();

  // The hidden tab (at index 0) should be the only discarded tab.
  EXPECT_TRUE(IsTabDiscarded(tab_strip->GetWebContentsAt(0)));
  for (int tab = 1; tab < kHighLoadedTabCount; tab++)
    EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(tab)));

  tab_strip->CloseAllTabs();
}

TEST_F(TabManagerWithProactiveDiscardExperimentEnabledTest,
       ProactiveDiscardTestChangingStates) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  // Create the minumum number of tabs to enter the high state.
  for (int tabs = 0; tabs < kModerateLoadedTabCount; tabs++) {
    tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
    tab_strip->GetWebContentsAt(tabs)->WasShown();
  }

  // Run tasks to let state transitions happen.
  task_runner_->RunUntilIdle();

  // Nothing should be discarded initially.
  for (int tab = 0; tab < kModerateLoadedTabCount; tab++)
    EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(tab)));

  // Hide the first two tabs, waiting once second between to enforce that the
  // first tab is discarded first.
  tab_strip->GetWebContentsAt(0)->WasHidden();
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  tab_strip->GetWebContentsAt(1)->WasHidden();

  // Fast forward the moderate state timeout.
  task_runner_->FastForwardBy(kHighOccludedTimeout);

  // Verify that the first tab is discarded. TabManager is now in the moderate
  // state as a result of the discard.
  EXPECT_TRUE(IsTabDiscarded(tab_strip->GetWebContentsAt(0)));

  // Verify the rest of the tabs are not discarded.
  for (int tab = 1; tab < kModerateLoadedTabCount; tab++)
    EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(tab)));

  // Fast forward the difference between the moderate and the high threshold
  // timeouts so the second tab was hidden the moderate threshold amount of
  // time. This should cause the second tab to be discarded.
  task_runner_->FastForwardBy(kModerateOccludedTimeout - kHighOccludedTimeout);

  // Verify that the first 2 tabs are now discarded.
  for (int tab = 0; tab < 2; tab++)
    EXPECT_TRUE(IsTabDiscarded(tab_strip->GetWebContentsAt(tab)));

  // Verify the rest of the tabs are not discarded.
  for (int tab = 2; tab < kModerateLoadedTabCount; tab++)
    EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(tab)));

  // Hide the next 4 tabs. Once these are discarded, TabManager will be in the
  // low state.
  tab_strip->GetWebContentsAt(2)->WasHidden();
  tab_strip->GetWebContentsAt(3)->WasHidden();
  tab_strip->GetWebContentsAt(4)->WasHidden();
  tab_strip->GetWebContentsAt(5)->WasHidden();

  // Fast forward by the moderate threshold timeout.
  task_runner_->FastForwardBy(kModerateOccludedTimeout);

  // Verify that the first 6 tabs are now discarded. Now in the low state.
  for (int tab = 0; tab < 6; tab++)
    EXPECT_TRUE(IsTabDiscarded(tab_strip->GetWebContentsAt(tab)));

  // Verify the rest of the tabs are not discarded.
  for (int tab = 6; tab < kModerateLoadedTabCount; tab++)
    EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(tab)));

  // Hide the seventh tab.
  tab_strip->GetWebContentsAt(6)->WasHidden();

  // Fast forward the moderate threshold. Currently in the low state, so nothing
  // should happen.
  task_runner_->FastForwardBy(kModerateOccludedTimeout);

  // Verify that the first 6 tabs are now discarded.
  for (int tab = 0; tab < 6; tab++)
    EXPECT_TRUE(IsTabDiscarded(tab_strip->GetWebContentsAt(tab)));

  // Verify the rest of the tabs are not discarded.
  for (int tab = 6; tab < kModerateLoadedTabCount; tab++)
    EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(tab)));

  // Fast forward the difference between the low and the moderate threshold
  // timeouts so the seventh tab was hidden the moderate threshold amount of
  // time. This should cause the seventh tab to be discarded.
  task_runner_->FastForwardBy(kLowOccludedTimeout - kModerateOccludedTimeout);

  // Verify that the first 7 tabs are now discarded.
  for (int tab = 0; tab < 7; tab++)
    EXPECT_TRUE(IsTabDiscarded(tab_strip->GetWebContentsAt(tab)));

  // Verify the rest of the tabs are not discarded.
  for (int tab = 7; tab < kModerateLoadedTabCount; tab++)
    EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(tab)));

  tab_strip->CloseAllTabs();
}

TEST_F(TabManagerWithProactiveDiscardExperimentEnabledTest,
       ProactiveDiscardTestTabClosedPriorToDiscard) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
  tab_strip->GetWebContentsAt(0)->WasShown();
  tab_strip->GetWebContentsAt(0)->WasHidden();

  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));

  tab_strip->CloseWebContentsAt(
      0, TabStripModel::CloseTypes::CLOSE_CREATE_HISTORICAL_TAB |
             TabStripModel::CloseTypes::CLOSE_USER_GESTURE);

  // Success in this test is no crash, meaning that closing the tab caused the
  // timer to be stopped, rather than triggering after the low threshold timeout
  // on a closed LifecycleUnit.
  task_runner_->FastForwardBy(kLowOccludedTimeout);
}

TEST_F(TabManagerWithProactiveDiscardExperimentEnabledTest,
       FreezeBackgroundTabs) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  // Create 3 tabs.
  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
  tab_strip->GetWebContentsAt(0)->WasShown();
  TabLoadTracker::Get()->TransitionStateForTesting(
      tab_strip->GetWebContentsAt(0), TabLoadTracker::LoadingState::LOADED);

  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/false);
  tab_strip->GetWebContentsAt(1)->WasHidden();
  TabLoadTracker::Get()->TransitionStateForTesting(
      tab_strip->GetWebContentsAt(1), TabLoadTracker::LoadingState::LOADED);

  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/false);
  tab_strip->GetWebContentsAt(2)->WasHidden();
  TabLoadTracker::Get()->TransitionStateForTesting(
      tab_strip->GetWebContentsAt(2), TabLoadTracker::LoadingState::LOADED);

  // Run tasks to let state transitions happen.
  task_runner_->RunUntilIdle();

  // No tab should be frozen initially.
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(2)));

  // Make the 2nd tab visible and the 2nd tab hidden after half the freeze
  // timeout. No tab should be frozen.
  task_runner_->FastForwardBy(kFreezeTimeout / 2);
  tab_strip->GetWebContentsAt(0)->WasHidden();
  tab_strip->GetWebContentsAt(1)->WasShown();
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(2)));

  // After the full freeze timeout, the 3rd tab should be frozen.
  task_runner_->FastForwardBy(kFreezeTimeout / 2);
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(2)));
  EXPECT_FALSE(
      ResourceCoordinatorTabHelper::IsFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(
      ResourceCoordinatorTabHelper::IsFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_TRUE(
      ResourceCoordinatorTabHelper::IsFrozen(tab_strip->GetWebContentsAt(2)));

  // After half the freeze timeout, the 1st tab should be frozen.
  task_runner_->FastForwardBy(kFreezeTimeout / 2);
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(2)));
  EXPECT_TRUE(
      ResourceCoordinatorTabHelper::IsFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(
      ResourceCoordinatorTabHelper::IsFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_TRUE(
      ResourceCoordinatorTabHelper::IsFrozen(tab_strip->GetWebContentsAt(2)));

  tab_strip->CloseAllTabs();
}

TEST_F(TabManagerWithProactiveDiscardExperimentEnabledTest, FreezeOnceLoaded) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  // Create a hidden tab in the LOADING state.
  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  web_contents->WasHidden();
  TabLoadTracker::Get()->TransitionStateForTesting(
      web_contents, TabLoadTracker::LoadingState::LOADING);

  // After the freeze timeout, the tab should not be frozen because it is still
  // LOADING.
  task_runner_->FastForwardBy(kFreezeTimeout + base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(IsTabFrozen(web_contents));

  // Once loaded, the tab should be frozen.
  TabLoadTracker::Get()->TransitionStateForTesting(
      web_contents, TabLoadTracker::LoadingState::LOADED);
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(IsTabFrozen(web_contents));

  tab_strip->CloseAllTabs();
}

TEST_F(TabManagerWithProactiveDiscardExperimentEnabledTest,
       FreezeUnfreezeRefreeze) {
  constexpr base::TimeDelta kShortTimeout = base::TimeDelta::FromSeconds(1);

  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  // Create 3 tabs.
  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
  tab_strip->GetWebContentsAt(0)->WasShown();
  TabLoadTracker::Get()->TransitionStateForTesting(
      tab_strip->GetWebContentsAt(0), TabLoadTracker::LoadingState::LOADED);

  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/false);
  tab_strip->GetWebContentsAt(1)->WasHidden();
  TabLoadTracker::Get()->TransitionStateForTesting(
      tab_strip->GetWebContentsAt(1), TabLoadTracker::LoadingState::LOADED);

  task_runner_->FastForwardBy(kShortTimeout);
  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/false);
  tab_strip->GetWebContentsAt(2)->WasHidden();
  TabLoadTracker::Get()->TransitionStateForTesting(
      tab_strip->GetWebContentsAt(2), TabLoadTracker::LoadingState::LOADED);

  // No tab should be frozen initially.
  task_runner_->RunUntilIdle();
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(2)));

  // After the freeze timeout, the first background tab should be frozen.
  task_runner_->FastForwardBy(kFreezeTimeout - kShortTimeout);
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(2)));
  SimulateFreezeCompletion(tab_strip->GetWebContentsAt(1));

  // After the short delay, the second background tab should be frozen.
  task_runner_->FastForwardBy(kShortTimeout);
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(2)));
  SimulateFreezeCompletion(tab_strip->GetWebContentsAt(2));

  // After the unfreeze timeout, the first background tab should be unfrozen.
  task_runner_->FastForwardBy(kUnfreezeTimeout - kShortTimeout);
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(2)));
  SimulateUnfreezeCompletion(tab_strip->GetWebContentsAt(1));

  // The second background tab shouldn't be unfrozen before the first background
  // tab is refrozen.
  task_runner_->FastForwardBy(kShortTimeout * 2);
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(2)));

  // After the refreeze timeout, the first background tab should be re-frozen
  // and the second background tab should be unfrozen.
  task_runner_->FastForwardBy(kRefreezeTimeout - kShortTimeout * 2);
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(2)));
  SimulateFreezeCompletion(tab_strip->GetWebContentsAt(1));
  SimulateUnfreezeCompletion(tab_strip->GetWebContentsAt(2));

  // After the refreeze timeout, both background tabs should be frozen.
  task_runner_->FastForwardBy(kRefreezeTimeout);
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(2)));
  SimulateFreezeCompletion(tab_strip->GetWebContentsAt(2));

  // Once the freeze timeout has expired since the first background tab was
  // refrozen, it should be unfrozen again.
  task_runner_->FastForwardBy(kUnfreezeTimeout - kRefreezeTimeout);
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(2)));
  SimulateUnfreezeCompletion(tab_strip->GetWebContentsAt(1));

  // After the refreeze timeout has expired, the first background tab should be
  // frozen and the second background tab should be unfrozen.
  task_runner_->FastForwardBy(kRefreezeTimeout);
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(2)));
  SimulateFreezeCompletion(tab_strip->GetWebContentsAt(1));
  SimulateUnfreezeCompletion(tab_strip->GetWebContentsAt(2));

  // Finally, after the refreeze timeout has expired, both background tabs
  // should be frozen.
  task_runner_->FastForwardBy(kRefreezeTimeout);
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(2)));
  SimulateFreezeCompletion(tab_strip->GetWebContentsAt(2));

  tab_strip->CloseAllTabs();
}

TEST_F(TabManagerWithProactiveDiscardExperimentEnabledTest,
       NoUnfreezeWhenUnfreezingVariationParamDisabled) {
  tab_manager_->proactive_freeze_discard_params_.should_periodically_unfreeze =
      false;

  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  // Create 2 tabs.
  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
  tab_strip->GetWebContentsAt(0)->WasShown();
  TabLoadTracker::Get()->TransitionStateForTesting(
      tab_strip->GetWebContentsAt(0), TabLoadTracker::LoadingState::LOADED);

  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/false);
  tab_strip->GetWebContentsAt(1)->WasHidden();
  TabLoadTracker::Get()->TransitionStateForTesting(
      tab_strip->GetWebContentsAt(1), TabLoadTracker::LoadingState::LOADED);

  // No tab should be frozen initially.
  task_runner_->RunUntilIdle();
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));

  // After the freeze timeout, the background tab should be frozen.
  task_runner_->FastForwardBy(kFreezeTimeout);
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  SimulateFreezeCompletion(tab_strip->GetWebContentsAt(1));

  // After the unfreeze timeout, the background tab should still be frozen as
  // the unfreeze feature is disabled..
  task_runner_->FastForwardBy(kUnfreezeTimeout);
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));

  tab_strip->CloseAllTabs();
}

TEST_F(TabManagerWithProactiveDiscardExperimentEnabledTest,
       NoProactiveDiscardWhenDiscardingVariationParamDisabled) {
  tab_manager_->proactive_freeze_discard_params_.should_proactively_discard =
      false;

  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/false);
  tab_strip->GetWebContentsAt(1)->WasShown();
  tab_strip->GetWebContentsAt(1)->WasHidden();

  task_runner_->FastForwardBy(kLowOccludedTimeout);

  EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(1)));

  tab_strip->CloseAllTabs();
}

TEST_F(TabManagerWithProactiveDiscardExperimentEnabledTest,
       FreezingWhenDiscardingVariationParamDisabled) {
  tab_manager_->proactive_freeze_discard_params_.should_proactively_discard =
      false;

  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/false);
  TabLoadTracker::Get()->TransitionStateForTesting(
      tab_strip->GetWebContentsAt(1), TabLoadTracker::LoadingState::LOADED);
  tab_strip->GetWebContentsAt(1)->WasShown();
  tab_strip->GetWebContentsAt(1)->WasHidden();

  task_runner_->FastForwardBy(kFreezeTimeout);

  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));

  tab_strip->CloseAllTabs();
}

TEST_F(TabManagerWithProactiveDiscardExperimentEnabledTest,
       NoProactiveDiscardWhenOffline) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  // Simulate being offline.
  net::NetworkChangeNotifier::DisableForTest net_change_notifier_disabler_;
  FakeOfflineNetworkChangeNotifier fake_offline_state;

  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/false);
  tab_strip->GetWebContentsAt(1)->WasShown();
  tab_strip->GetWebContentsAt(1)->WasHidden();

  task_runner_->FastForwardBy(kLowOccludedTimeout);

  // The background tab shouldn't have been discarded while offline.
  EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(1)));

  tab_strip->CloseAllTabs();
}

TEST_F(TabManagerWithProactiveDiscardExperimentEnabledTest,
       NoProactiveDiscardWhenChromeNotInUse) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  // Create 2 tabs.
  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
  tab_strip->GetWebContentsAt(0)->WasShown();
  TabLoadTracker::Get()->TransitionStateForTesting(
      tab_strip->GetWebContentsAt(0), TabLoadTracker::LoadingState::LOADED);

  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/false);
  tab_strip->GetWebContentsAt(1)->WasHidden();
  TabLoadTracker::Get()->TransitionStateForTesting(
      tab_strip->GetWebContentsAt(1), TabLoadTracker::LoadingState::LOADED);

  // Run tasks to let state transitions happen.
  task_runner_->RunUntilIdle();

  // No tab should be frozen or discarded initially.
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(1)));

  // Fast-forward time when Chrome is not in use.
  MarkChromeInUse(false);
  task_runner_->FastForwardBy(kFreezeTimeout);

  // The background tab should be frozen normally.
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(1)));

  // Fast-forward time again when Chrome is not in use.
  task_runner_->FastForwardBy(base::TimeDelta::FromDays(1));

  // No discard should happen when Chrome is not in use.
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(1)));

  // Fast-forward time less than the discard timeout when Chrome is in use.
  constexpr base::TimeDelta kShortDelay = base::TimeDelta::FromSeconds(42);
  MarkChromeInUse(true);
  task_runner_->FastForwardBy(kShortDelay);

  // No discard should happen yet.
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_TRUE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(1)));

  // Fast-forward time enough for the discard timeout to expire.
  task_runner_->FastForwardBy(kLowOccludedTimeout - kShortDelay);

  // The background tab should be discarded.
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(0)));
  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));
  EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(0)));
  EXPECT_TRUE(IsTabDiscarded(tab_strip->GetWebContentsAt(1)));

  tab_strip->CloseAllTabs();
}

TEST_F(TabManagerTest, NoProactiveDiscardWhenFeatureDisabled) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/false);
  tab_strip->GetWebContentsAt(1)->WasShown();
  tab_strip->GetWebContentsAt(1)->WasHidden();

  task_runner_->FastForwardBy(kLowOccludedTimeout);

  EXPECT_FALSE(IsTabDiscarded(tab_strip->GetWebContentsAt(1)));

  tab_strip->CloseAllTabs();
}

TEST_F(TabManagerTest, NoFreezingWhenFeatureDisabled) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
  tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/false);
  TabLoadTracker::Get()->TransitionStateForTesting(
      tab_strip->GetWebContentsAt(1), TabLoadTracker::LoadingState::LOADED);
  tab_strip->GetWebContentsAt(1)->WasShown();
  tab_strip->GetWebContentsAt(1)->WasHidden();

  task_runner_->FastForwardBy(kFreezeTimeout);

  EXPECT_FALSE(IsTabFrozen(tab_strip->GetWebContentsAt(1)));

  tab_strip->CloseAllTabs();
}

}  // namespace resource_coordinator
