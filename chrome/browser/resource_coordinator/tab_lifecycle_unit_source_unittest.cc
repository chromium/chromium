// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source_observer.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/test_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/graph_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

constexpr base::TimeDelta kShortDelay = base::Seconds(1);

using PreDiscardResourceUsage = performance_manager::user_tuning::
    UserPerformanceTuningManager::PreDiscardResourceUsage;

class MockLifecycleUnitSourceObserver : public LifecycleUnitSourceObserver {
 public:
  MockLifecycleUnitSourceObserver() = default;

  MockLifecycleUnitSourceObserver(const MockLifecycleUnitSourceObserver&) =
      delete;
  MockLifecycleUnitSourceObserver& operator=(
      const MockLifecycleUnitSourceObserver&) = delete;

  MOCK_METHOD1(OnLifecycleUnitCreated, void(LifecycleUnit*));
};

class MockTabLifecycleObserver : public TabLifecycleObserver {
 public:
  MockTabLifecycleObserver() = default;

  MockTabLifecycleObserver(const MockTabLifecycleObserver&) = delete;
  MockTabLifecycleObserver& operator=(const MockTabLifecycleObserver&) = delete;

  MOCK_METHOD3(OnDiscardedStateChange,
               void(content::WebContents* contents,
                    LifecycleUnitDiscardReason reason,
                    bool is_discarded));
  MOCK_METHOD2(OnAutoDiscardableStateChange,
               void(content::WebContents* contents, bool is_auto_discardable));
};

class MockLifecycleUnitObserver : public LifecycleUnitObserver {
 public:
  MockLifecycleUnitObserver() = default;

  MockLifecycleUnitObserver(const MockLifecycleUnitObserver&) = delete;
  MockLifecycleUnitObserver& operator=(const MockLifecycleUnitObserver&) =
      delete;

  MOCK_METHOD3(OnLifecycleUnitStateChanged,
               void(LifecycleUnit* lifecycle_unit,
                    LifecycleUnitState,
                    LifecycleUnitStateChangeReason));
  MOCK_METHOD2(OnLifecycleUnitVisibilityChanged,
               void(LifecycleUnit* lifecycle_unit,
                    content::Visibility visibility));
  MOCK_METHOD1(OnLifecycleUnitDestroyed, void(LifecycleUnit* lifecycle_unit));
};

bool IsFocused(LifecycleUnit* lifecycle_unit) {
  return lifecycle_unit->GetLastFocusedTimeTicks() == base::TimeTicks::Max();
}

class TabLifecycleUnitSourceTest : public ChromeRenderViewHostTestHarness {
 public:
  TabLifecycleUnitSourceTest(const TabLifecycleUnitSourceTest&) = delete;
  TabLifecycleUnitSourceTest& operator=(const TabLifecycleUnitSourceTest&) =
      delete;

 protected:
  TabLifecycleUnitSourceTest()
      : ChromeRenderViewHostTestHarness(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {
    task_runner_ = task_environment()->GetMainThreadTaskRunner();
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Force TabManager/TabLifecycleUnitSource creation.
    g_browser_process->GetTabManager();

    source_ = GetTabLifecycleUnitSource();
    source_->AddObserver(&source_observer_);
    source_->AddTabLifecycleObserver(&tab_observer_);

    tab_strip_model_ =
        std::make_unique<TabStripModel>(&tab_strip_model_delegate_, profile());
    tab_strip_model_->AddObserver(source_);
  }

  void TearDown() override {
    tab_strip_model_->CloseAllTabs();
    tab_strip_model_.reset();

    task_environment()->RunUntilIdle();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // If |focus_tab_strip| is true, focuses the tab strip. Then, appends 2 tabs
  // to the tab strip and returns the associated LifecycleUnits via
  // |first_lifecycle_unit| and |second_lifecycle_unit|. The first tab is
  // background and the second tab is active.
  void CreateTwoTabs(bool focus_tab_strip,
                     LifecycleUnit** first_lifecycle_unit,
                     LifecycleUnit** second_lifecycle_unit) {
    if (focus_tab_strip)
      source_->SetFocusedTabStripModelForTesting(tab_strip_model_.get());

    // Add a foreground tab to the tab strip.
    task_environment()->FastForwardBy(kShortDelay);
    auto time_before_first_tab = NowTicks();
    EXPECT_CALL(source_observer_, OnLifecycleUnitCreated(::testing::_))
        .WillOnce(::testing::Invoke([&](LifecycleUnit* lifecycle_unit) {
          *first_lifecycle_unit = lifecycle_unit;

          if (focus_tab_strip) {
            EXPECT_TRUE(IsFocused(*first_lifecycle_unit));
          } else {
            EXPECT_EQ(time_before_first_tab,
                      (*first_lifecycle_unit)->GetLastFocusedTimeTicks());
          }
        }));
    std::unique_ptr<content::WebContents> first_web_contents =
        CreateAndNavigateWebContents();
    content::WebContents* raw_first_web_contents = first_web_contents.get();
    tab_strip_model_->AppendWebContents(std::move(first_web_contents), true);
    ::testing::Mock::VerifyAndClear(&source_observer_);
    EXPECT_TRUE(source_->GetTabLifecycleUnitExternal(raw_first_web_contents));
    base::RepeatingClosure run_loop_cb = base::BindRepeating(
        &base::test::SingleThreadTaskEnvironment::RunUntilIdle,
        base::Unretained(task_environment()));

    // Add another foreground tab to the focused tab strip.
    task_environment()->FastForwardBy(kShortDelay);
    auto time_before_second_tab = NowTicks();
    EXPECT_CALL(source_observer_, OnLifecycleUnitCreated(::testing::_))
        .WillOnce(::testing::Invoke([&](LifecycleUnit* lifecycle_unit) {
          *second_lifecycle_unit = lifecycle_unit;

          if (focus_tab_strip) {
            EXPECT_EQ(time_before_second_tab,
                      (*first_lifecycle_unit)->GetLastFocusedTimeTicks());
            EXPECT_TRUE(IsFocused(*second_lifecycle_unit));
          } else {
            EXPECT_EQ(time_before_first_tab,
                      (*first_lifecycle_unit)->GetLastFocusedTimeTicks());
            EXPECT_EQ(time_before_second_tab,
                      (*second_lifecycle_unit)->GetLastFocusedTimeTicks());
          }
        }));
    std::unique_ptr<content::WebContents> second_web_contents =
        CreateAndNavigateWebContents();
    content::WebContents* raw_second_web_contents = second_web_contents.get();
    tab_strip_model_->AppendWebContents(std::move(second_web_contents), true);
    ::testing::Mock::VerifyAndClear(&source_observer_);
    EXPECT_TRUE(source_->GetTabLifecycleUnitExternal(raw_second_web_contents));

    // TabStripModel doesn't update the visibility of its WebContents by itself.
    raw_first_web_contents->WasHidden();
  }

  void TestAppendTabsToTabStrip(bool focus_tab_strip) {
    LifecycleUnit* first_lifecycle_unit = nullptr;
    LifecycleUnit* second_lifecycle_unit = nullptr;
    CreateTwoTabs(focus_tab_strip, &first_lifecycle_unit,
                  &second_lifecycle_unit);

    const base::TimeTicks first_tab_last_focused_time =
        first_lifecycle_unit->GetLastFocusedTimeTicks();
    const base::TimeTicks second_tab_last_focused_time =
        second_lifecycle_unit->GetLastFocusedTimeTicks();

    // Add a background tab to the focused tab strip.
    task_environment()->FastForwardBy(kShortDelay);
    LifecycleUnit* third_lifecycle_unit = nullptr;
    EXPECT_CALL(source_observer_, OnLifecycleUnitCreated(::testing::_))
        .WillOnce(::testing::Invoke([&](LifecycleUnit* lifecycle_unit) {
          third_lifecycle_unit = lifecycle_unit;

          if (focus_tab_strip) {
            EXPECT_EQ(first_tab_last_focused_time,
                      first_lifecycle_unit->GetLastFocusedTimeTicks());
            EXPECT_TRUE(IsFocused(second_lifecycle_unit));
          } else {
            EXPECT_EQ(first_tab_last_focused_time,
                      first_lifecycle_unit->GetLastFocusedTimeTicks());
            EXPECT_EQ(second_tab_last_focused_time,
                      second_lifecycle_unit->GetLastFocusedTimeTicks());
          }
          EXPECT_EQ(NowTicks(),
                    third_lifecycle_unit->GetLastFocusedTimeTicks());
        }));
    std::unique_ptr<content::WebContents> third_web_contents =
        CreateAndNavigateWebContents();
    content::WebContents* raw_third_web_contents = third_web_contents.get();
    tab_strip_model_->AppendWebContents(std::move(third_web_contents), false);
    ::testing::Mock::VerifyAndClear(&source_observer_);
    EXPECT_TRUE(source_->GetTabLifecycleUnitExternal(raw_third_web_contents));

    // Expect notifications when tabs are closed.
    CloseTabsAndExpectNotifications(
        tab_strip_model_.get(),
        {first_lifecycle_unit, second_lifecycle_unit, third_lifecycle_unit});
  }

  void CloseTabsAndExpectNotifications(
      TabStripModel* tab_strip_model,
      std::vector<LifecycleUnit*> lifecycle_units) {
    std::vector<
        std::unique_ptr<::testing::StrictMock<MockLifecycleUnitObserver>>>
        observers;
    for (LifecycleUnit* lifecycle_unit : lifecycle_units) {
      observers.emplace_back(
          std::make_unique<::testing::StrictMock<MockLifecycleUnitObserver>>());
      lifecycle_unit->AddObserver(observers.back().get());
      EXPECT_CALL(*observers.back().get(),
                  OnLifecycleUnitDestroyed(lifecycle_unit));
    }
    tab_strip_model->CloseAllTabs();
  }

  void DiscardAndAttachTabHelpers(LifecycleUnit* lifecycle_unit) {}

  void DetachWebContentsTest(LifecycleUnitDiscardReason reason) {
    LifecycleUnit* first_lifecycle_unit = nullptr;
    LifecycleUnit* second_lifecycle_unit = nullptr;
    CreateTwoTabs(true /* focus_tab_strip */, &first_lifecycle_unit,
                  &second_lifecycle_unit);

    // Advance time so tabs are urgent discardable.
    task_environment()->AdvanceClock(kBackgroundUrgentProtectionTime);

    // Detach the non-active tab. Verify that it can no longer be discarded.
    ExpectCanDiscardTrueAllReasons(first_lifecycle_unit);
    std::unique_ptr<tabs::TabModel> detached_tab =
        tab_strip_model_->DetachTabAtForInsertion(0);
    ExpectCanDiscardFalseTrivialAllReasons(first_lifecycle_unit);

    // Create a second tab strip.
    TestTabStripModelDelegate other_tab_strip_model_delegate;
    TabStripModel other_tab_strip_model(&other_tab_strip_model_delegate,
                                        profile());
    other_tab_strip_model.AddObserver(source_);

    // Make sure that the second tab strip has a foreground tab.
    EXPECT_CALL(source_observer_, OnLifecycleUnitCreated(::testing::_));
    other_tab_strip_model.AppendWebContents(CreateTestWebContents(),
                                            /*foreground=*/true);

    // Insert the tab into the second tab strip without focusing it. Verify that
    // it can be discarded.
    other_tab_strip_model.AppendTab(std::move(detached_tab), false);
    ExpectCanDiscardTrueAllReasons(first_lifecycle_unit);

    EXPECT_EQ(LifecycleUnitState::ACTIVE, first_lifecycle_unit->GetState());
    EXPECT_CALL(tab_observer_,
                OnDiscardedStateChange(::testing::_, reason, true));
    first_lifecycle_unit->Discard(reason);

    ::testing::Mock::VerifyAndClear(&tab_observer_);

    // Expect a notification when the tab is closed.
    CloseTabsAndExpectNotifications(&other_tab_strip_model,
                                    {first_lifecycle_unit});
  }

  void DiscardTest(LifecycleUnitDiscardReason reason) {
    const base::TimeTicks kDummyLastActiveTime =
        base::TimeTicks() + kShortDelay;

    LifecycleUnit* background_lifecycle_unit = nullptr;
    LifecycleUnit* foreground_lifecycle_unit = nullptr;
    CreateTwoTabs(true /* focus_tab_strip */, &background_lifecycle_unit,
                  &foreground_lifecycle_unit);
    content::WebContents* initial_web_contents =
        tab_strip_model_->GetWebContentsAt(0);
    content::WebContentsTester::For(initial_web_contents)
        ->SetLastActiveTimeTicks(kDummyLastActiveTime);

    // Advance time so tabs are urgent discardable.
    task_environment()->AdvanceClock(kBackgroundUrgentProtectionTime);

    // Discard the tab.
    EXPECT_EQ(LifecycleUnitState::ACTIVE,
              background_lifecycle_unit->GetState());
    EXPECT_CALL(tab_observer_,
                OnDiscardedStateChange(::testing::_, reason, true));
    background_lifecycle_unit->Discard(reason);
    ::testing::Mock::VerifyAndClear(&tab_observer_);

    EXPECT_NE(initial_web_contents, tab_strip_model_->GetWebContentsAt(0));
    EXPECT_FALSE(tab_strip_model_->GetWebContentsAt(0)
                     ->GetController()
                     .GetPendingEntry());
    EXPECT_EQ(kDummyLastActiveTime,
              tab_strip_model_->GetWebContentsAt(0)->GetLastActiveTimeTicks());

    source_->SetFocusedTabStripModelForTesting(nullptr);
  }

  void DiscardAndActivateTest(LifecycleUnitDiscardReason reason) {
    LifecycleUnit* background_lifecycle_unit = nullptr;
    LifecycleUnit* foreground_lifecycle_unit = nullptr;
    CreateTwoTabs(true /* focus_tab_strip */, &background_lifecycle_unit,
                  &foreground_lifecycle_unit);
    content::WebContents* initial_web_contents =
        tab_strip_model_->GetWebContentsAt(0);

    // Advance time so tabs are urgent discardable.
    task_environment()->AdvanceClock(kBackgroundUrgentProtectionTime);

    // Discard the tab.
    EXPECT_EQ(LifecycleUnitState::ACTIVE,
              background_lifecycle_unit->GetState());
    EXPECT_CALL(tab_observer_,
                OnDiscardedStateChange(::testing::_, reason, true));
    background_lifecycle_unit->Discard(reason);
    ::testing::Mock::VerifyAndClear(&tab_observer_);

    EXPECT_NE(initial_web_contents, tab_strip_model_->GetWebContentsAt(0));
    EXPECT_FALSE(tab_strip_model_->GetWebContentsAt(0)
                     ->GetController()
                     .GetPendingEntry());

    // Focus the tab. Expect the state to be ACTIVE.
    EXPECT_CALL(tab_observer_,
                OnDiscardedStateChange(::testing::_, reason, false));
    tab_strip_model_->ActivateTabAt(
        0, TabStripUserGestureDetails(
               TabStripUserGestureDetails::GestureType::kOther));
    ::testing::Mock::VerifyAndClear(&tab_observer_);
    EXPECT_EQ(LifecycleUnitState::ACTIVE,
              background_lifecycle_unit->GetState());
    EXPECT_TRUE(tab_strip_model_->GetWebContentsAt(0)
                    ->GetController()
                    .GetPendingEntry());
  }

  void DiscardAndExplicitlyReloadTest(LifecycleUnitDiscardReason reason) {
    LifecycleUnit* background_lifecycle_unit = nullptr;
    LifecycleUnit* foreground_lifecycle_unit = nullptr;
    CreateTwoTabs(true /* focus_tab_strip */, &background_lifecycle_unit,
                  &foreground_lifecycle_unit);
    content::WebContents* initial_web_contents =
        tab_strip_model_->GetWebContentsAt(0);

    // Advance time so tabs are urgent discardable.
    task_environment()->AdvanceClock(kBackgroundUrgentProtectionTime);

    // Discard the tab.
    EXPECT_EQ(LifecycleUnitState::ACTIVE,
              background_lifecycle_unit->GetState());
    EXPECT_CALL(tab_observer_,
                OnDiscardedStateChange(::testing::_, reason, true));
    background_lifecycle_unit->Discard(reason);
    ::testing::Mock::VerifyAndClear(&tab_observer_);

    EXPECT_NE(initial_web_contents, tab_strip_model_->GetWebContentsAt(0));
    EXPECT_FALSE(tab_strip_model_->GetWebContentsAt(0)
                     ->GetController()
                     .GetPendingEntry());

    // Explicitly reload the tab. Expect the state to be ACTIVE.
    EXPECT_CALL(tab_observer_,
                OnDiscardedStateChange(::testing::_, reason, false));
    tab_strip_model_->GetWebContentsAt(0)->GetController().Reload(
        content::ReloadType::NORMAL, false);
    ::testing::Mock::VerifyAndClear(&tab_observer_);
    EXPECT_EQ(LifecycleUnitState::ACTIVE,
              background_lifecycle_unit->GetState());
    EXPECT_TRUE(tab_strip_model_->GetWebContentsAt(0)
                    ->GetController()
                    .GetPendingEntry());
  }

  raw_ptr<TabLifecycleUnitSource> source_ = nullptr;
  ::testing::StrictMock<MockLifecycleUnitSourceObserver> source_observer_;
  ::testing::StrictMock<MockTabLifecycleObserver> tab_observer_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::unique_ptr<content::WebContents> CreateAndNavigateWebContents() {
    std::unique_ptr<content::WebContents> web_contents =
        CreateTestWebContents();
    // Attach the RC tab helper. In production code the browser
    // WebContentsDelegate takes care of this.
    ResourceCoordinatorTabHelper::CreateForWebContents(web_contents.get());
    // Commit an URL to allow discarding.
    content::WebContentsTester::For(web_contents.get())
        ->NavigateAndCommit(GURL("https://www.example.com"));
    return web_contents;
  }

 private:
  TestTabStripModelDelegate tab_strip_model_delegate_;
  tabs::PreventTabFeatureInitialization prevent_;
};

}  // namespace

TEST_F(TabLifecycleUnitSourceTest, AppendTabsToFocusedTabStrip) {
  TestAppendTabsToTabStrip(true /* focus_tab_strip */);
}

TEST_F(TabLifecycleUnitSourceTest, AppendTabsToNonFocusedTabStrip) {
  TestAppendTabsToTabStrip(false /* focus_tab_strip */);
}

TEST_F(TabLifecycleUnitSourceTest, SwitchTabInFocusedTabStrip) {
  LifecycleUnit* first_lifecycle_unit = nullptr;
  LifecycleUnit* second_lifecycle_unit = nullptr;
  CreateTwoTabs(true /* focus_tab_strip */, &first_lifecycle_unit,
                &second_lifecycle_unit);

  // Activate the first tab.
  task_environment()->FastForwardBy(kShortDelay);
  auto time_before_activate = NowTicks();
  tab_strip_model_->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_TRUE(IsFocused(first_lifecycle_unit));
  EXPECT_EQ(time_before_activate,
            second_lifecycle_unit->GetLastFocusedTimeTicks());

  // Expect notifications when tabs are closed.
  CloseTabsAndExpectNotifications(
      tab_strip_model_.get(), {first_lifecycle_unit, second_lifecycle_unit});
}

TEST_F(TabLifecycleUnitSourceTest, CloseTabInFocusedTabStrip) {
  LifecycleUnit* first_lifecycle_unit = nullptr;
  LifecycleUnit* second_lifecycle_unit = nullptr;
  CreateTwoTabs(true /* focus_tab_strip */, &first_lifecycle_unit,
                &second_lifecycle_unit);

  // Close the second tab. The first tab should be focused.
  task_environment()->FastForwardBy(kShortDelay);
  ::testing::StrictMock<MockLifecycleUnitObserver> second_observer;
  second_lifecycle_unit->AddObserver(&second_observer);
  EXPECT_CALL(second_observer, OnLifecycleUnitDestroyed(second_lifecycle_unit));
  tab_strip_model_->CloseWebContentsAt(1, 0);
  ::testing::Mock::VerifyAndClear(&source_observer_);
  EXPECT_TRUE(IsFocused(first_lifecycle_unit));

  // Expect notifications when tabs are closed.
  CloseTabsAndExpectNotifications(tab_strip_model_.get(),
                                  {first_lifecycle_unit});
}

TEST_F(TabLifecycleUnitSourceTest, DiscardWebContents) {
  LifecycleUnit* first_lifecycle_unit = nullptr;
  LifecycleUnit* second_lifecycle_unit = nullptr;
  CreateTwoTabs(true /* focus_tab_strip */, &first_lifecycle_unit,
                &second_lifecycle_unit);

  // Replace the WebContents in the active tab with a second WebContents. Expect
  // GetTabLifecycleUnitExternal() to return the TabLifecycleUnitExternal when
  // called with the second WebContents as argument.
  content::WebContents* original_web_contents =
      tab_strip_model_->GetWebContentsAt(1);
  TabLifecycleUnitExternal* tab_lifecycle_unit_external =
      source_->GetTabLifecycleUnitExternal(original_web_contents);
  std::unique_ptr<content::WebContents> new_web_contents =
      CreateTestWebContents();
  content::WebContents* raw_new_web_contents = new_web_contents.get();
  std::unique_ptr<content::WebContents> original_web_contents_deleter =
      tab_strip_model_->DiscardWebContentsAt(1, std::move(new_web_contents));
  EXPECT_EQ(original_web_contents, original_web_contents_deleter.get());
  EXPECT_FALSE(source_->GetTabLifecycleUnitExternal(original_web_contents));
  EXPECT_EQ(tab_lifecycle_unit_external,
            source_->GetTabLifecycleUnitExternal(raw_new_web_contents));

  original_web_contents_deleter.reset();

  // Expect notifications when tabs are closed.
  CloseTabsAndExpectNotifications(
      tab_strip_model_.get(), {first_lifecycle_unit, second_lifecycle_unit});
}

// Tracks discard notifications for a tab's WebContents.
class TabDiscardNotificationsObserver : public content::MockWebContentsObserver,
                                        public TabStripModelObserver {
 public:
  TabDiscardNotificationsObserver(TabStripModel* tab_strip_model,
                                  content::WebContents* original_web_contents)
      : content::MockWebContentsObserver(original_web_contents),
        tab_strip_model_(tab_strip_model) {
    tab_strip_model_->AddObserver(this);
  }
  ~TabDiscardNotificationsObserver() override {
    tab_strip_model_->RemoveObserver(this);
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    CHECK_EQ(change.type(), TabStripModelChange::kReplaced);
    CHECK_EQ(change.GetReplace()->old_contents, web_contents());
    Observe(change.GetReplace()->new_contents);
  }

 private:
  raw_ptr<TabStripModel> tab_strip_model_;
};

TEST_F(TabLifecycleUnitSourceTest, PropagatesWebContentsDiscardNotifications) {
  LifecycleUnit* first_lifecycle_unit = nullptr;
  LifecycleUnit* second_lifecycle_unit = nullptr;
  CreateTwoTabs(true /* focus_tab_strip */, &first_lifecycle_unit,
                &second_lifecycle_unit);

  content::WebContents* original_web_contents =
      tab_strip_model_->GetWebContentsAt(1);
  ::testing::NiceMock<TabDiscardNotificationsObserver>
      tab_discard_notifications_observer(tab_strip_model_.get(),
                                         original_web_contents);
  EXPECT_CALL(tab_discard_notifications_observer,
              AboutToBeDiscarded(::testing::_));
  EXPECT_CALL(tab_discard_notifications_observer, WasDiscarded());
  EXPECT_CALL(tab_observer_,
              OnDiscardedStateChange(
                  ::testing::_, LifecycleUnitDiscardReason::PROACTIVE, true));
  EXPECT_TRUE(second_lifecycle_unit->Discard(
      LifecycleUnitDiscardReason::PROACTIVE, 100));
}

TEST_F(TabLifecycleUnitSourceTest, UpdateMemorySavingsOnMultipleDiscards) {
  LifecycleUnit* first_lifecycle_unit = nullptr;
  LifecycleUnit* second_lifecycle_unit = nullptr;
  CreateTwoTabs(/*focus_tab_strip=*/true, &first_lifecycle_unit,
                &second_lifecycle_unit);

  // Discard the tab.
  EXPECT_CALL(tab_observer_,
              OnDiscardedStateChange(
                  ::testing::_, LifecycleUnitDiscardReason::PROACTIVE, true));
  EXPECT_TRUE(second_lifecycle_unit->Discard(
      LifecycleUnitDiscardReason::PROACTIVE, 100));
  ::testing::Mock::VerifyAndClear(&tab_observer_);

  const auto* pre_discard_resource_usage =
      PreDiscardResourceUsage::FromWebContents(
          tab_strip_model_->GetWebContentsAt(1));
  EXPECT_NE(pre_discard_resource_usage, nullptr);
  EXPECT_EQ(pre_discard_resource_usage->memory_footprint_estimate_kb(), 100u);

  // Navigate the tab so that it is no longer discarded.
  EXPECT_CALL(tab_observer_,
              OnDiscardedStateChange(
                  ::testing::_, LifecycleUnitDiscardReason::PROACTIVE, false));
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://www.example.com"), tab_strip_model_->GetWebContentsAt(1));
  navigation->SetKeepLoading(true);
  navigation->Commit();
  ::testing::Mock::VerifyAndClear(&tab_observer_);

  // Discarding the tab with a different memory usage should update the
  // PreDiscardResourceUsage tab helper.
  EXPECT_CALL(tab_observer_,
              OnDiscardedStateChange(
                  ::testing::_, LifecycleUnitDiscardReason::PROACTIVE, true));
  EXPECT_TRUE(second_lifecycle_unit->Discard(
      LifecycleUnitDiscardReason::PROACTIVE, 500));
  pre_discard_resource_usage = PreDiscardResourceUsage::FromWebContents(
      tab_strip_model_->GetWebContentsAt(1));
  EXPECT_NE(pre_discard_resource_usage, nullptr);
  EXPECT_EQ(pre_discard_resource_usage->memory_footprint_estimate_kb(), 500u);
  ::testing::Mock::VerifyAndClear(&tab_observer_);

  // Expect notifications when tabs are closed.
  CloseTabsAndExpectNotifications(
      tab_strip_model_.get(), {first_lifecycle_unit, second_lifecycle_unit});
}

TEST_F(TabLifecycleUnitSourceTest, DetachWebContents_Urgent) {
  DetachWebContentsTest(LifecycleUnitDiscardReason::URGENT);
}

TEST_F(TabLifecycleUnitSourceTest, DetachWebContents_External) {
  DetachWebContentsTest(LifecycleUnitDiscardReason::EXTERNAL);
}

// Regression test for https://crbug.com/818454. Previously, TabLifecycleUnits
// were destroyed from TabStripModelObserver::TabClosingAt(). If a tab was
// detached (TabStripModel::DetachWebContentsAt) and its WebContents destroyed,
// the TabLifecycleUnit was never destroyed. This was solved by giving ownership
// of a tab lifecycleunit to a WebContentsUserData.
TEST_F(TabLifecycleUnitSourceTest, DetachAndDeleteWebContents) {
  LifecycleUnit* first_lifecycle_unit = nullptr;
  LifecycleUnit* second_lifecycle_unit = nullptr;
  CreateTwoTabs(true /* focus_tab_strip */, &first_lifecycle_unit,
                &second_lifecycle_unit);

  ::testing::StrictMock<MockLifecycleUnitObserver> observer;
  first_lifecycle_unit->AddObserver(&observer);

  // Detach and destroy the non-active tab. Verify that the LifecycleUnit is
  // destroyed.
  std::unique_ptr<tabs::TabModel> detached_tab =
      tab_strip_model_->DetachTabAtForInsertion(0);
  EXPECT_CALL(observer, OnLifecycleUnitDestroyed(first_lifecycle_unit));
  detached_tab.reset();
  ::testing::Mock::VerifyAndClear(&observer);
}

// Tab discarding is tested here rather than in TabLifecycleUnitTest because
// collaboration from the TabLifecycleUnitSource is required to replace the
// WebContents in the TabLifecycleUnit.

TEST_F(TabLifecycleUnitSourceTest, Discard_Urgent) {
  DiscardTest(LifecycleUnitDiscardReason::URGENT);
}

TEST_F(TabLifecycleUnitSourceTest, Discard_External) {
  DiscardTest(LifecycleUnitDiscardReason::EXTERNAL);
}

TEST_F(TabLifecycleUnitSourceTest, DiscardAndActivate_Urgent) {
  DiscardAndActivateTest(LifecycleUnitDiscardReason::URGENT);
}

TEST_F(TabLifecycleUnitSourceTest, DiscardAndActivate_External) {
  DiscardAndActivateTest(LifecycleUnitDiscardReason::EXTERNAL);
}

TEST_F(TabLifecycleUnitSourceTest, DiscardAndExplicitlyReload_Urgent) {
  DiscardAndExplicitlyReloadTest(LifecycleUnitDiscardReason::URGENT);
}

TEST_F(TabLifecycleUnitSourceTest, DiscardAndExplicitlyReload_External) {
  DiscardAndExplicitlyReloadTest(LifecycleUnitDiscardReason::EXTERNAL);
}

}  // namespace resource_coordinator
