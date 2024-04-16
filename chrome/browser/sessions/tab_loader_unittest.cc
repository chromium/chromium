// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/tab_loader.h"

#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/sessions/session_restore_test_utils.h"
#include "chrome/browser/sessions/tab_loader_tester.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

using resource_coordinator::TabLoadTracker;
using resource_coordinator::ResourceCoordinatorTabHelper;
using LoadingState = TabLoadTracker::LoadingState;

class TabLoaderTest : public BrowserWithTestWindowTest {
 protected:
  using Super = BrowserWithTestWindowTest;
  using RestoredTab = SessionRestoreDelegate::RestoredTab;

  TabLoaderTest() : max_simultaneous_loads_(1) {}

  TabLoaderTest(const TabLoaderTest&) = delete;
  TabLoaderTest& operator=(const TabLoaderTest&) = delete;

  void OnTabLoaderCreated(TabLoader* tab_loader) {
    tab_loader_.SetTabLoader(tab_loader);
    tab_loader_.SetTickClockForTesting(&clock_);
    if (max_simultaneous_loads_ != 0)
      tab_loader_.SetMaxSimultaneousLoadsForTesting(max_simultaneous_loads_);
  }

  // testing::Test:
  void SetUp() override {
    Super::SetUp();
    construction_callback_ = base::BindRepeating(
        &TabLoaderTest::OnTabLoaderCreated, base::Unretained(this));
    TabLoaderTester::SetConstructionCallbackForTesting(&construction_callback_);
    test_policy_ =
        std::make_unique<testing::ScopedAlwaysLoadSessionRestoreTestPolicy>();
  }

  void TearDown() override {
    if (TabLoaderTester::shared_tab_loader() != nullptr) {
      // Expect the TabLoader to detach after all tabs have loaded.
      SimulateLoadedAll();
      EXPECT_TRUE(TabLoaderTester::shared_tab_loader() == nullptr);
    }

    TabLoaderTester::SetConstructionCallbackForTesting(nullptr);
    task_environment()->RunUntilIdle();
    test_policy_.reset();
    Super::TearDown();
  }

  void SimulateLoadTimeout() {
    // Unfortunately there's no mock time in BrowserTaskEnvironment.
    // Fast-forward things and simulate firing the timer.
    // TODO(crbug.com/40602467): TaskEnvironment::TimeSource::MOCK_TIME now
    // supports this.
    EXPECT_TRUE(tab_loader_.force_load_timer().IsRunning());
    clock_.SetNowTicks(tab_loader_.force_load_time());
    tab_loader_.force_load_timer().Stop();
    tab_loader_.ForceLoadTimerFired();
    SimulatePrimaryPageChangedIfNecessary();
  }

  void SimulateStartedToLoad(size_t tab_index) {
    auto* contents = restored_tabs_[tab_index].contents();
    auto* tracker = TabLoadTracker::Get();
    tracker->TransitionStateForTesting(contents, LoadingState::LOADING);
    SimulatePrimaryPageChangedIfNecessary();
  }

  void SimulateLoaded(size_t tab_index) {
    // Transition to a LOADED state. This has to pass through the LOADING state
    // in order to satisfy the internal logic of SessionRestoreStatsCollector.
    auto* contents = restored_tabs_[tab_index].contents();
    auto* tracker = TabLoadTracker::Get();
    if (tracker->GetLoadingState(contents) != LoadingState::LOADING)
      tracker->TransitionStateForTesting(contents, LoadingState::LOADING);
    tracker->TransitionStateForTesting(contents, LoadingState::LOADED);
    SimulatePrimaryPageChangedIfNecessary();
  }

  void SimulateLoadedAll() {
    for (size_t i = 0; i < restored_tabs_.size(); ++i)
      SimulateLoaded(i);
  }

  content::WebContents* CreateRestoredWebContents(bool is_active) {
    std::unique_ptr<content::WebContents> test_contents =
        content::WebContentsTester::CreateTestWebContents(
            profile(), content::SiteInstance::Create(profile()));
    auto* raw_contents = test_contents.get();
    std::vector<std::unique_ptr<content::NavigationEntry>> entries;
    entries.push_back(content::NavigationEntry::Create());
    test_contents->GetController().Restore(0, content::RestoreType::kRestored,
                                           &entries);
    // TabLoadTracker needs the resource_coordinator WebContentsData to be
    // initialized.
    ResourceCoordinatorTabHelper::CreateForWebContents(raw_contents);
    restored_tabs_.push_back(
        RestoredTab(raw_contents, is_active /* is_active */, false /* is_app */,
                    false /* is_pinned */, std::nullopt /* group */));

    // Add the contents to the tab strip model, which becomes the owner.
    auto* tab_strip_model = browser()->tab_strip_model();
    tab_strip_model->AppendWebContents(std::move(test_contents), is_active);

    if (is_active) {
      // If the tab is active start "loading" it right away for consistency with
      // session restore code.
      raw_contents->GetController().LoadIfNecessary();
    }

    return raw_contents;
  }

  void CreateMultipleRestoredWebContents(size_t num_active,
                                         size_t num_inactive) {
    // At least one active tab must be created.
    DCHECK_LT(0u, num_active);
    for (size_t i = 0; i < num_active; ++i)
      CreateRestoredWebContents(true);
    for (size_t i = 0; i < num_inactive; ++i)
      CreateRestoredWebContents(false);
  }

  // Since it couldn't get PrimaryPageChanged() by loading, it simulates
  // PrimaryPageChanged() to update the status.
  void SimulatePrimaryPageChanged(content::WebContents* web_contents) {
    auto* helper = ResourceCoordinatorTabHelper::FromWebContents(web_contents);
    helper->PrimaryPageChanged(web_contents->GetPrimaryPage());
  }

  // If the tab initiates loading, TransitionState is updated by
  // PrimaryPageChanged() in a normal browser flow. Since this is a unit test,
  // we simulate SimulatePrimaryPageChanged() for the loading initiated tabs.
  void SimulatePrimaryPageChangedIfNecessary() {
    if (!TabLoaderTester::shared_tab_loader())
      return;

    // Copy because the set can change while calling
    // SimulatePrimaryPageChanged() and the iteration is invalidated.
    base::flat_set<raw_ptr<content::WebContents, CtnExperimental>>
        load_initiated = tab_loader_.tabs_load_initiated();
    for (content::WebContents* web_contents : load_initiated) {
      SimulatePrimaryPageChanged(web_contents);
    }
  }

  void StartTabLoader() {
    // Call PrimaryPageChanged() that would be caused by LoadIfNecessary() from
    // CreateRestoredWebContents().
    for (auto& tab : restored_tabs_) {
      if (tab.is_active())
        SimulatePrimaryPageChanged(tab.contents());
    }

    TabLoader::RestoreTabs(restored_tabs_, clock_.NowTicks());
    EXPECT_TRUE(tab_loader_.IsSharedTabLoader());
    EXPECT_FALSE(tab_loader_.IsLoadingEnabled());
    tab_loader_.WaitForTabLoadingEnabled();
  }

  // The number of loading slots to use. This needs to be set before the
  // TabLoader is created in order to be picked up by it.
  size_t max_simultaneous_loads_;

  // Set of restored tabs that is populated by calls to
  // CreateRestoredWebContents.
  std::vector<RestoredTab> restored_tabs_;

  // Automatically attaches to the tab loader that is created by the test.
  TabLoaderTester tab_loader_;

  // The tick clock that is injected into the tab loader.
  base::SimpleTestTickClock clock_;

  // The post-construction testing seam that is invoked by TabLoader.
  base::RepeatingCallback<void(TabLoader*)> construction_callback_;

  std::unique_ptr<testing::ScopedAlwaysLoadSessionRestoreTestPolicy>
      test_policy_;
};

TEST_F(TabLoaderTest, AllLoadingSlotsUsed) {
  // Create 2 active tabs and 4 inactive tabs.
  CreateMultipleRestoredWebContents(2, 4);

  // Use 4 loading slots. The active tabs will only use 2 which means 2 of the
  // inactive tabs should immediately be scheduled to load as well.
  max_simultaneous_loads_ = 4;

  StartTabLoader();

  // The loader should be enabled, with 2 tabs loading and 4 tabs left to go.
  // The initial load should exclusively allow active tabs time to load, and
  // fill up the rest of the loading slots.
  EXPECT_TRUE(tab_loader_.IsLoadingEnabled());
  EXPECT_EQ(4u, tab_loader_.tabs_to_load().size());
  EXPECT_EQ(2u, tab_loader_.scheduled_to_load_count());
  EXPECT_EQ(2u, TabLoadTracker::Get()->GetLoadingTabCount());

  // Trying to load another tab should do nothing as no tab has yet finished
  // loading.
  tab_loader_.MaybeLoadSomeTabsForTesting();
  EXPECT_EQ(4u, tab_loader_.tabs_to_load().size());
  EXPECT_EQ(2u, tab_loader_.scheduled_to_load_count());
  EXPECT_EQ(2u, TabLoadTracker::Get()->GetLoadingTabCount());
  EXPECT_TRUE(tab_loader_.IsSharedTabLoader());

  // Mark an active tab as having finished loading. This marks the end of the
  // exclusive loading period and all slots should be full now.
  SimulateLoaded(0);
  EXPECT_EQ(1u, tab_loader_.tabs_to_load().size());
  EXPECT_EQ(5u, tab_loader_.scheduled_to_load_count());
  EXPECT_EQ(4u, TabLoadTracker::Get()->GetLoadingTabCount());
  EXPECT_TRUE(tab_loader_.IsSharedTabLoader());

  // Trying to load more tabs should still do nothing.
  tab_loader_.MaybeLoadSomeTabsForTesting();
  EXPECT_EQ(1u, tab_loader_.tabs_to_load().size());
  EXPECT_EQ(5u, tab_loader_.scheduled_to_load_count());
  EXPECT_EQ(4u, TabLoadTracker::Get()->GetLoadingTabCount());
  EXPECT_TRUE(tab_loader_.IsSharedTabLoader());
}

TEST_F(TabLoaderTest, ForceLoadTimer) {
  // Create 1 active tab and 1 inactive tab with 1 loading slot.
  CreateMultipleRestoredWebContents(1, 1);
  max_simultaneous_loads_ = 1;

  StartTabLoader();

  // The loader should be enabled, with 1 tab loading and 1 tab left to go.
  EXPECT_TRUE(tab_loader_.IsLoadingEnabled());
  EXPECT_EQ(1u, tab_loader_.tabs_to_load().size());
  EXPECT_EQ(1u, tab_loader_.scheduled_to_load_count());
  EXPECT_EQ(1u, TabLoadTracker::Get()->GetLoadingTabCount());
  EXPECT_TRUE(tab_loader_.IsSharedTabLoader());

  SimulateLoadTimeout();
  EXPECT_FALSE(tab_loader_.HasTimedOutLoads());

  // Expect all tabs to be loading. Note that this also validates that
  // force-loads can exceed the number of loadingslots.
  EXPECT_TRUE(tab_loader_.IsLoadingEnabled());
  EXPECT_TRUE(tab_loader_.tabs_to_load().empty());
  EXPECT_EQ(2u, tab_loader_.scheduled_to_load_count());
  EXPECT_EQ(2u, TabLoadTracker::Get()->GetLoadingTabCount());
  EXPECT_TRUE(tab_loader_.IsSharedTabLoader());
}

TEST_F(TabLoaderTest, LoadsAreStaggered) {
  // Create 1 active tab and 1 inactive tab with 1 loading slot.
  CreateMultipleRestoredWebContents(1, 1);
  max_simultaneous_loads_ = 1;

  StartTabLoader();

  // The loader should be enabled, with 1 tab loading and 1 tab left to go.
  EXPECT_TRUE(tab_loader_.IsLoadingEnabled());
  EXPECT_EQ(1u, tab_loader_.tabs_to_load().size());
  EXPECT_EQ(1u, tab_loader_.scheduled_to_load_count());
  EXPECT_EQ(1u, TabLoadTracker::Get()->GetLoadingTabCount());
  EXPECT_TRUE(tab_loader_.IsSharedTabLoader());

  // Simulate the first tab finishing loading.
  SimulateLoaded(0);

  // Expect all tabs to be loaded/loading.
  EXPECT_TRUE(tab_loader_.IsLoadingEnabled());
  EXPECT_TRUE(tab_loader_.tabs_to_load().empty());
  EXPECT_EQ(2u, tab_loader_.scheduled_to_load_count());
  EXPECT_EQ(1u, TabLoadTracker::Get()->GetLoadedTabCount());
  EXPECT_EQ(1u, TabLoadTracker::Get()->GetLoadingTabCount());
  EXPECT_TRUE(tab_loader_.IsSharedTabLoader());
}

TEST_F(TabLoaderTest, OnMemoryPressure) {
  // Multiple contents are necessary to make sure that the tab loader
  // doesn't immediately kick off loading of all tabs and detach.
  CreateMultipleRestoredWebContents(1, 2);

  max_simultaneous_loads_ = 1;
  StartTabLoader();
  EXPECT_EQ(1u, tab_loader_.scheduled_to_load_count());

  // Simulate memory pressure and expect the tab loader to disable loading.
  EXPECT_TRUE(tab_loader_.IsLoadingEnabled());
  tab_loader_.OnMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_FALSE(tab_loader_.IsLoadingEnabled());

  // Finish loading the tab and expect the tab loader to disconnect.
  SimulateLoaded(0);
  EXPECT_TRUE(TabLoaderTester::shared_tab_loader() == nullptr);
}

TEST_F(TabLoaderTest, TimeoutCanExceedLoadingSlots) {
  CreateMultipleRestoredWebContents(1, 4);

  // Create the tab loader with 2 loading slots. This should initially start
  // loading 1 tab, due to exclusive initial loading of active tabs.
  max_simultaneous_loads_ = 2;
  StartTabLoader();
  EXPECT_EQ(4u, tab_loader_.tabs_to_load().size());
  EXPECT_EQ(1u, tab_loader_.scheduled_to_load_count());

  // Simulate a timeout and expect there to be 2 loading tabs and 3 left to
  // load.
  SimulateLoadTimeout();
  EXPECT_FALSE(tab_loader_.HasTimedOutLoads());
  EXPECT_EQ(3u, tab_loader_.tabs_to_load().size());
  EXPECT_EQ(2u, tab_loader_.scheduled_to_load_count());
  EXPECT_EQ(2u, tab_loader_.force_load_delay_multiplier());
  EXPECT_TRUE(tab_loader_.IsSharedTabLoader());

  // Do it again and expect 3 tabs to be loading.
  SimulateLoadTimeout();
  EXPECT_FALSE(tab_loader_.HasTimedOutLoads());
  EXPECT_EQ(2u, tab_loader_.tabs_to_load().size());
  EXPECT_EQ(3u, tab_loader_.scheduled_to_load_count());
  EXPECT_EQ(4u, tab_loader_.force_load_delay_multiplier());
  EXPECT_TRUE(tab_loader_.IsSharedTabLoader());

  // Do it again and expect 4 tabs to be loading.
  SimulateLoadTimeout();
  EXPECT_FALSE(tab_loader_.HasTimedOutLoads());
  EXPECT_EQ(1u, tab_loader_.tabs_to_load().size());
  EXPECT_EQ(4u, tab_loader_.scheduled_to_load_count());
  EXPECT_EQ(8u, tab_loader_.force_load_delay_multiplier());
  EXPECT_TRUE(tab_loader_.IsSharedTabLoader());

  // Simulate the first tab finishing loading and don't expect more tabs to
  // start loading.
  SimulateLoaded(0);
  EXPECT_EQ(1u, tab_loader_.tabs_to_load().size());
  EXPECT_EQ(4u, tab_loader_.scheduled_to_load_count());
  EXPECT_TRUE(tab_loader_.IsSharedTabLoader());

  // Simulate the second tab finishing loading and don't expect more tabs to
  // start loading.
  SimulateLoaded(1);
  EXPECT_EQ(1u, tab_loader_.tabs_to_load().size());
  EXPECT_EQ(4u, tab_loader_.scheduled_to_load_count());
  EXPECT_TRUE(tab_loader_.IsSharedTabLoader());

  // Simulate the third tab finishing loading and this time expect the last tab
  // load to be initiated. There are no tabs left so the TabLoader should also
  // have initiated a self-destroy.
  SimulateLoaded(2);
  EXPECT_TRUE(tab_loader_.tabs_to_load().empty());
  EXPECT_EQ(5u, tab_loader_.scheduled_to_load_count());
  EXPECT_TRUE(tab_loader_.IsSharedTabLoader());
}

TEST_F(TabLoaderTest, DelegatePolicyIsApplied) {
  namespace rc = resource_coordinator;

  test_policy_.reset();

  // Don't directly configure the max simultaneous loads, but rather let it be
  // configured via the policy engine.
  max_simultaneous_loads_ = 0;

  // Create 5 tabs to restore, 1 foreground and 4 background.
  CreateMultipleRestoredWebContents(1, 4);

  // Create the tab loader. This should initially start loading 1 tab, due to
  // exclusive initial loading of active tabs.
  StartTabLoader();
  EXPECT_EQ(4u, tab_loader_.tabs_to_load().size());
  EXPECT_EQ(1u, tab_loader_.scheduled_to_load_count());

  // Configure the policy engine explicitly. Values of zero disable those
  // particular aspects of the policy engine.
  auto* policy = tab_loader_.GetPolicy();
  policy->MinSimultaneousTabLoadsForTesting() = 2;
  policy->MaxSimultaneousTabLoadsForTesting() = 2;
  policy->CoresPerSimultaneousTabLoadForTesting() = 0;
  policy->MinTabsToRestoreForTesting() = 1;
  policy->MaxTabsToRestoreForTesting() = 3;
  policy->MbFreeMemoryPerTabToRestoreForTesting() = 0;
  policy->MaxTimeSinceLastUseToRestoreForTesting() = base::TimeDelta();
  policy->MinSiteEngagementToRestoreForTesting() = 0;
  policy->CalculateSimultaneousTabLoadsForTesting();

  // Simulate the first tab as having loaded. Another 2 should start loading.
  SimulateLoaded(0);
  EXPECT_EQ(2u, tab_loader_.tabs_to_load().size());
  EXPECT_EQ(3u, tab_loader_.scheduled_to_load_count());
  EXPECT_TRUE(tab_loader_.IsSharedTabLoader());

  // Simulate another tab as having loaded. The last 2 tabs should be deferred
  // (still need reloads) and the tab loader should detach.
  SimulateLoaded(1);
  SimulateLoaded(2);
  EXPECT_TRUE(restored_tabs_[3].contents()->GetController().NeedsReload());
  EXPECT_TRUE(restored_tabs_[4].contents()->GetController().NeedsReload());
  EXPECT_TRUE(TabLoaderTester::shared_tab_loader() == nullptr);
}

TEST_F(TabLoaderTest, ObservesExternallyInitiatedLoads) {
  CreateMultipleRestoredWebContents(1, 2);

  // Create the tab loader with 1 loading slots. This should initially start
  // loading 1 tab, due to exclusive initial loading of active tabs.
  max_simultaneous_loads_ = 1;
  StartTabLoader();
  EXPECT_EQ(2u, tab_loader_.tabs_to_load().size());
  EXPECT_EQ(1u, tab_loader_.scheduled_to_load_count());

  // Manually initiate the load on one of the tabs, as would occur if a user
  // focused a tab. The tab should no longer be in the scheduled to load bucket.
  SimulateStartedToLoad(1);
  EXPECT_EQ(1u, tab_loader_.tabs_to_load().size());
  EXPECT_EQ(2u, tab_loader_.scheduled_to_load_count());
  EXPECT_TRUE(tab_loader_.IsSharedTabLoader());
}

TEST_F(TabLoaderTest, CloseAllTabs) {
  CreateMultipleRestoredWebContents(1, 2);

  // Create the tab loader with 1 loading slots. This should initially start
  // loading 1 tab, due to exclusive initial loading of active tabs.
  max_simultaneous_loads_ = 1;
  StartTabLoader();
  EXPECT_EQ(2u, tab_loader_.tabs_to_load().size());
  EXPECT_EQ(1u, tab_loader_.scheduled_to_load_count());

  // The loader should entirely disconnect when all tabs are closed.
  browser()->tab_strip_model()->CloseAllTabs();
  EXPECT_TRUE(TabLoaderTester::shared_tab_loader() == nullptr);
}

TEST_F(TabLoaderTest, RemoveFromTabStrip) {
  CreateMultipleRestoredWebContents(1, 1);

  // Create the tab loader with 1 loading slots. This should initially start
  // loading 1 tab, due to exclusive initial loading of active tabs.
  max_simultaneous_loads_ = 1;
  StartTabLoader();
  EXPECT_EQ(1u, tab_loader_.tabs_to_load().size());
  EXPECT_EQ(1u, tab_loader_.scheduled_to_load_count());

  // Remove the second tab from the tab strip model.
  browser()->tab_strip_model()->DetachAndDeleteWebContentsAt(1);

  // The tab being removed won't be noticed by the loader until some state
  // change it cares about occurs. Simulate the first tab finishing loading, at
  // which point the loader should realize the other tab is no longer attached
  // to a tab strip, and destroy itself because it has no work left to do.
  SimulateLoaded(0);
  EXPECT_TRUE(TabLoaderTester::shared_tab_loader() == nullptr);
}
