// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/test_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/tab_contents/form_interaction_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace resource_coordinator {

namespace {

using LoadingState = TabLoadTracker::LoadingState;
using PageNode = performance_manager::PageNode;
using PageLiveStateDecorator = performance_manager::PageLiveStateDecorator;
using PerformanceManager = performance_manager::PerformanceManager;
using performance_manager::policies::CannotDiscardReason;

class MockLifecycleUnitObserver : public LifecycleUnitObserver {
 public:
  MockLifecycleUnitObserver() = default;

  MockLifecycleUnitObserver(const MockLifecycleUnitObserver&) = delete;
  MockLifecycleUnitObserver& operator=(const MockLifecycleUnitObserver&) =
      delete;

  MOCK_METHOD(void,
              OnLifecycleUnitStateChanged,
              (LifecycleUnit*, LifecycleUnitState),
              (override));
};

class MockPageLiveStateObserver
    : public performance_manager::PageLiveStateObserver {
 public:
  MOCK_METHOD(void,
              OnIsAutoDiscardableChanged,
              (const PageNode* page_node),
              (override));
};

}  // namespace

class TabLifecycleUnitTest : public ChromeRenderViewHostTestHarness {
 protected:
  using TabLifecycleUnit = TabLifecycleUnitSource::TabLifecycleUnit;

  // This is an internal class so that it is also friends with
  // TabLifecycleUnitTest.
  class ScopedEnterpriseOptOut;

  TabLifecycleUnitTest()
      : scoped_set_clocks_for_testing_(&test_clock_, &test_tick_clock_) {
    test_clock_.SetNow(base::Time::NowFromSystemTime());
    // Advance the clock so that it doesn't yield null time ticks.
    test_tick_clock_.Advance(base::Seconds(1));
  }

  TabLifecycleUnitTest(const TabLifecycleUnitTest&) = delete;
  TabLifecycleUnitTest& operator=(const TabLifecycleUnitTest&) = delete;

  void SetUp() override {
    TestingBrowserProcess::GetGlobal()->CreateGlobalFeaturesForTesting();
    ChromeRenderViewHostTestHarness::SetUp();
    pm_helper_.SetUp();

    PerformanceManager::GetGraph()->PassToGraph(
        std::make_unique<PageLiveStateDecorator>());

    // FormInteractionTabHelper asserts that its observer exists whenever
    // PerformanceManager is initialized.
    PerformanceManager::GetGraph()->PassToGraph(
        FormInteractionTabHelper::CreateGraphObserver());

    metrics::DesktopSessionDurationTracker::Initialize();

    // Force TabManager/TabLifecycleUnitSource creation.
    g_browser_process->GetTabManager();

    std::unique_ptr<content::WebContents> test_web_contents =
        CreateTestWebContents();
    web_contents_ = test_web_contents.get();
    auto* tester = content::WebContentsTester::For(web_contents_);
    tester->SetLastActiveTimeTicks(NowTicks());
    tester->SetLastActiveTime(Now());
    ResourceCoordinatorTabHelper::CreateForWebContents(web_contents_);
    // Commit an URL to allow discarding.
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        GURL("https://www.example.com"), web_contents_);
    navigation->SetKeepLoading(true);
    navigation->Commit();

    tab_strip_model_ =
        std::make_unique<TabStripModel>(&tab_strip_model_delegate_, profile());
    tab_strip_model_->AppendWebContents(std::move(test_web_contents), false);
    web_contents_->WasHidden();

    std::unique_ptr<content::WebContents> second_web_contents =
        CreateTestWebContents();
    content::WebContents* raw_second_web_contents = second_web_contents.get();
    tab_strip_model_->AppendWebContents(std::move(second_web_contents),
                                        /*foreground=*/true);
    raw_second_web_contents->WasHidden();

    performance_manager::Graph* graph = PerformanceManager::GetGraph();
    graph->PassToGraph(
        std::make_unique<
            performance_manager::policies::DiscardEligibilityPolicy>());
  }

  void TearDown() override {
    while (!tab_strip_model_->empty())
      tab_strip_model_->DetachAndDeleteWebContentsAt(0);
    tab_strip_model_.reset();
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
    pm_helper_.TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Create a new test WebContents and append it to the tab strip to allow
  // testing discarding operations on it. The returned WebContents is in the
  // hidden state.
  content::WebContents* AddNewHiddenWebContentsToTabStrip() {
    std::unique_ptr<content::WebContents> test_web_contents =
        CreateTestWebContents();
    content::WebContents* web_contents = test_web_contents.get();
    ResourceCoordinatorTabHelper::CreateForWebContents(web_contents);
    tab_strip_model_->AppendWebContents(std::move(test_web_contents), false);
    web_contents->WasHidden();
    return web_contents;
  }

  // Create a new test WebContents, as in AddNewHiddenWebContentsToTabStrip().
  // If the TabLifecycleUnitSource is observing the tab strip, returns the
  // TabLifecycleUnit that it created for the WebContents, otherwise returns
  // nullptr. (By default tests don't observe the tab strip so that they can
  // manually create TabLifecycleUnits.)
  TabLifecycleUnit* AddNewHiddenLifecycleUnitToTabStrip() {
    content::WebContents* contents = AddNewHiddenWebContentsToTabStrip();
    return GetTabLifecycleUnitSource()->GetTabLifecycleUnit(contents);
  }

  raw_ptr<content::WebContents, DanglingUntriaged>
      web_contents_;  // Owned by tab_strip_model_.
  std::unique_ptr<TabStripModel> tab_strip_model_;
  base::SimpleTestClock test_clock_;
  base::SimpleTestTickClock test_tick_clock_;

 private:
  // So that the main thread looks like the UI thread as expected.
  TestTabStripModelDelegate tab_strip_model_delegate_;
  ScopedSetClocksForTesting scoped_set_clocks_for_testing_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  performance_manager::PerformanceManagerTestHarnessHelper pm_helper_;
};

class TabLifecycleUnitTest::ScopedEnterpriseOptOut {
 public:
  ScopedEnterpriseOptOut() {
    GetTabLifecycleUnitSource()->SetTabLifecyclesEnterprisePolicy(false);
  }

  ~ScopedEnterpriseOptOut() {
    GetTabLifecycleUnitSource()->SetTabLifecyclesEnterprisePolicy(true);
  }
};

TEST_F(TabLifecycleUnitTest, AsTabLifecycleUnitExternal) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(),
                                      web_contents_, tab_strip_model_.get());
  EXPECT_TRUE(tab_lifecycle_unit.AsTabLifecycleUnitExternal());
}

TEST_F(TabLifecycleUnitTest, CanDiscardByDefault) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(),
                                      web_contents_, tab_strip_model_.get());
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, AutoDiscardable) {
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents_);
  ASSERT_TRUE(page_node);
  auto* page_live_state_data =
      PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node.get());

  ::testing::StrictMock<MockPageLiveStateObserver> page_observer;
  base::ScopedObservation<PageLiveStateDecorator::Data,
                          MockPageLiveStateObserver>
      page_observation(&page_observer);
  page_observation.Observe(page_live_state_data);

  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(),
                                      web_contents_, tab_strip_model_.get());

  EXPECT_TRUE(tab_lifecycle_unit.IsAutoDiscardable());
  EXPECT_TRUE(page_live_state_data->IsAutoDiscardable());
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  EXPECT_CALL(page_observer, OnIsAutoDiscardableChanged(page_node.get()));
  tab_lifecycle_unit.SetAutoDiscardable(false);
  ::testing::Mock::VerifyAndClear(&page_observer);
  EXPECT_FALSE(tab_lifecycle_unit.IsAutoDiscardable());
  EXPECT_FALSE(page_live_state_data->IsAutoDiscardable());
  ExpectCanDiscardFalse(&tab_lifecycle_unit,
                        CannotDiscardReason::kExtensionProtected,
                        LifecycleUnitDiscardReason::URGENT);
  // Auto discardable shouldn't change external discard behavior.
  ExpectCanDiscardTrue(&tab_lifecycle_unit,
                       LifecycleUnitDiscardReason::EXTERNAL);

  EXPECT_CALL(page_observer, OnIsAutoDiscardableChanged(page_node.get()));
  tab_lifecycle_unit.SetAutoDiscardable(true);
  ::testing::Mock::VerifyAndClear(&page_observer);
  EXPECT_TRUE(tab_lifecycle_unit.IsAutoDiscardable());
  EXPECT_TRUE(page_live_state_data->IsAutoDiscardable());
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, AutoDiscardablePersistsThroughDiscard) {
  // Start observing the TabStripModel so that TabLifecycleUnitSource will be
  // informed when the tab is discarded. TabLifecycleUnitSource expects to fully
  // manage TabLifecycleUnits after this, so create a new WebContents that will
  // get a TabLifecycleUnit attached.
  tab_strip_model_->AddObserver(GetTabLifecycleUnitSource());
  TabLifecycleUnit* tab_lifecycle_unit = AddNewHiddenLifecycleUnitToTabStrip();

  tab_lifecycle_unit->SetAutoDiscardable(false);

  // Manual discard by an extension is allowed when AutoDiscardable is false.
  EXPECT_TRUE(
      tab_lifecycle_unit->DiscardTab(LifecycleUnitDiscardReason::EXTERNAL, 0));
  EXPECT_FALSE(tab_lifecycle_unit->IsAutoDiscardable());
  EXPECT_FALSE(PageLiveStateDecorator::IsAutoDiscardable(
      tab_lifecycle_unit->GetWebContents()));

  EXPECT_TRUE(tab_lifecycle_unit->Load());
  EXPECT_FALSE(tab_lifecycle_unit->IsAutoDiscardable());
  EXPECT_FALSE(PageLiveStateDecorator::IsAutoDiscardable(
      tab_lifecycle_unit->GetWebContents()));

  tab_strip_model_->RemoveObserver(GetTabLifecycleUnitSource());
}

TEST_F(TabLifecycleUnitTest, CannotDiscardInvalidURL) {
  content::WebContents* web_contents = AddNewHiddenWebContentsToTabStrip();
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), web_contents,
                                      tab_strip_model_.get());
  // TODO(sebmarchand): Fix this test, this doesn't really test that it's not
  // possible to discard an invalid URL, TestWebContents::GetLastCommittedURL()
  // doesn't return the URL set with "SetLastCommittedURL" if this one is
  // invalid.
  content::WebContentsTester::For(web_contents)
      ->SetLastCommittedURL(GURL("Invalid :)"));
  ExpectCanDiscardFalseTrivialAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CannotDiscardEmptyURL) {
  content::WebContents* web_contents = AddNewHiddenWebContentsToTabStrip();
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(), web_contents,
                                      tab_strip_model_.get());

  ExpectCanDiscardFalseTrivialAllReasons(&tab_lifecycle_unit);
}

// Verify the initial GetWallTimeWhenHidden() of a visible LifecycleUnit.
TEST_F(TabLifecycleUnitTest, InitialLastActiveTimeForVisibleLifecycleUnit) {
  web_contents_->WasShown();
  TabLifecycleUnit lifecycle_unit(GetTabLifecycleUnitSource(), web_contents_,
                                  tab_strip_model_.get());
  EXPECT_EQ(base::TimeTicks::Max(),
            lifecycle_unit.GetWallTimeWhenHiddenForTesting());
}

// Verify the initial GetWallTimeWhenHidden() of a hidden LifecycleUnit.
TEST_F(TabLifecycleUnitTest, InitialLastActiveTimeForHiddenLifecycleUnit) {
  web_contents_->WasHidden();
  TabLifecycleUnit lifecycle_unit(GetTabLifecycleUnitSource(), web_contents_,
                                  tab_strip_model_.get());
  EXPECT_EQ(NowTicks(), lifecycle_unit.GetWallTimeWhenHiddenForTesting());
}

TEST_F(TabLifecycleUnitTest, LastActiveTimeUpdatedOnVisibilityChange) {
  TabLifecycleUnit tab_lifecycle_unit(GetTabLifecycleUnitSource(),
                                      web_contents_, tab_strip_model_.get());

  web_contents_->WasShown();
  EXPECT_EQ(base::TimeTicks::Max(),
            tab_lifecycle_unit.GetWallTimeWhenHiddenForTesting());

  test_tick_clock_.Advance(base::Minutes(1));
  web_contents_->WasHidden();
  base::TimeTicks wall_time_when_hidden = NowTicks();
  EXPECT_EQ(wall_time_when_hidden,
            tab_lifecycle_unit.GetWallTimeWhenHiddenForTesting());

  test_tick_clock_.Advance(base::Minutes(1));
  web_contents_->WasOccluded();
  // `wall_time_when_hidden` not updated because it was already HIDDEN.
  EXPECT_EQ(wall_time_when_hidden,
            tab_lifecycle_unit.GetWallTimeWhenHiddenForTesting());

  test_tick_clock_.Advance(base::Minutes(1));
  web_contents_->WasShown();
  EXPECT_EQ(base::TimeTicks::Max(),
            tab_lifecycle_unit.GetWallTimeWhenHiddenForTesting());

  test_tick_clock_.Advance(base::Minutes(1));
  web_contents_->WasOccluded();
  wall_time_when_hidden = NowTicks();
  EXPECT_EQ(wall_time_when_hidden,
            tab_lifecycle_unit.GetWallTimeWhenHiddenForTesting());
}

}  // namespace resource_coordinator
