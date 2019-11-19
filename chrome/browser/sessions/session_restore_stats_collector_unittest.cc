// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore_stats_collector.h"

#include <stddef.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using TabLoaderStats = SessionRestoreStatsCollector::TabLoaderStats;
using StatsReportingDelegate =
    SessionRestoreStatsCollector::StatsReportingDelegate;

// A mock StatsReportingDelegate. This is used by the unittests to validate the
// reporting and lifetime behaviour of the SessionRestoreStatsCollector under
// test.
class MockStatsReportingDelegate : public StatsReportingDelegate {
 public:
  MockStatsReportingDelegate()
      : report_tab_loader_stats_call_count_(0u),
        report_tab_deferred_call_count_(0u),
        report_deferred_tab_loaded_call_count_(0u),
        report_stats_collector_death_call_count_(0u),
        report_tab_time_since_active_call_count_(0u),
        report_tab_site_engagement_score_call_count_(0u) {}

  ~MockStatsReportingDelegate() override { EnsureNoUnexpectedCalls(); }

  void ReportTabLoaderStats(const TabLoaderStats& stats) override {
    report_tab_loader_stats_call_count_++;
    tab_loader_stats_ = stats;
  }

  void ReportTabDeferred() override { report_tab_deferred_call_count_++; }

  void ReportDeferredTabLoaded() override {
    report_deferred_tab_loaded_call_count_++;
  }

  void ReportTabTimeSinceActive(base::TimeDelta elapsed) override {
    report_tab_time_since_active_call_count_++;
  }

  void ReportTabSiteEngagementScore(double engagement) override {
    report_tab_site_engagement_score_call_count_++;
  }

  // This is not part of the StatsReportingDelegate, but an added function that
  // is invoked by the PassthroughStatsReportingDelegate when it dies. This
  // allows the tests to be notified the moment the underlying stats collector
  // terminates itself.
  void ReportStatsCollectorDeath() {
    report_stats_collector_death_call_count_++;
  }

  void ExpectReportTabLoaderStatsCalled(
      size_t tab_count,
      size_t tabs_deferred,
      size_t tabs_load_started,
      size_t tabs_loaded,
      int foreground_tab_first_loaded_ms,
      int foreground_tab_first_paint_ms,
      int non_deferred_tabs_loaded_ms,
      SessionRestoreStatsCollector::SessionRestorePaintFinishReasonUma
          finish_reason) {
    EXPECT_LT(0u, report_tab_loader_stats_call_count_);
    report_tab_loader_stats_call_count_--;

    EXPECT_EQ(tab_count, tab_loader_stats_.tab_count);
    EXPECT_EQ(tabs_deferred, tab_loader_stats_.tabs_deferred);
    EXPECT_EQ(tabs_load_started, tab_loader_stats_.tabs_load_started);
    EXPECT_EQ(tabs_loaded, tab_loader_stats_.tabs_loaded);
    EXPECT_EQ(base::TimeDelta::FromMilliseconds(foreground_tab_first_loaded_ms),
              tab_loader_stats_.foreground_tab_first_loaded);
    EXPECT_EQ(base::TimeDelta::FromMilliseconds(foreground_tab_first_paint_ms),
              tab_loader_stats_.foreground_tab_first_paint);
    EXPECT_EQ(base::TimeDelta::FromMilliseconds(non_deferred_tabs_loaded_ms),
              tab_loader_stats_.non_deferred_tabs_loaded);
    EXPECT_EQ(tab_loader_stats_.tab_first_paint_reason, finish_reason);
  }

  void ExpectReportTabDeferredCalled() {
    EXPECT_LT(0u, report_tab_deferred_call_count_);
    report_tab_deferred_call_count_--;
  }

  void ExpectReportDeferredTabLoadedCalled() {
    EXPECT_LT(0u, report_deferred_tab_loaded_call_count_);
    report_deferred_tab_loaded_call_count_--;
  }

  void ExpectReportStatsCollectorDeathCalled() {
    EXPECT_LT(0u, report_stats_collector_death_call_count_);
    report_stats_collector_death_call_count_--;
  }

  void ExpectReportTabTimeSinceActiveCalled(size_t count) {
    EXPECT_LE(count, report_tab_time_since_active_call_count_);
    report_tab_time_since_active_call_count_ -= count;
  }

  void ExpectReportTabSiteEngagementScoreCalled(size_t count) {
    EXPECT_LE(count, report_tab_site_engagement_score_call_count_);
    report_tab_site_engagement_score_call_count_ -= count;
  }

  void EnsureNoUnexpectedCalls() {
    EXPECT_EQ(0u, report_tab_loader_stats_call_count_);
    EXPECT_EQ(0u, report_tab_deferred_call_count_);
    EXPECT_EQ(0u, report_deferred_tab_loaded_call_count_);
    EXPECT_EQ(0u, report_stats_collector_death_call_count_);
    EXPECT_EQ(0u, report_tab_time_since_active_call_count_);
    EXPECT_EQ(0u, report_tab_site_engagement_score_call_count_);

    report_tab_loader_stats_call_count_ = 0u;
    report_tab_deferred_call_count_ = 0u;
    report_deferred_tab_loaded_call_count_ = 0u;
    report_stats_collector_death_call_count_ = 0u;
    report_tab_time_since_active_call_count_ = 0u;
    tab_loader_stats_ = TabLoaderStats();
  }

 private:
  size_t report_tab_loader_stats_call_count_;
  size_t report_tab_deferred_call_count_;
  size_t report_deferred_tab_loaded_call_count_;
  size_t report_stats_collector_death_call_count_;
  size_t report_tab_time_since_active_call_count_;
  size_t report_tab_site_engagement_score_call_count_;
  TabLoaderStats tab_loader_stats_;

  DISALLOW_COPY_AND_ASSIGN(MockStatsReportingDelegate);
};

// A pass-through stats reporting delegate. This is used to decouple the
// lifetime of the mock reporting delegate from the SessionRestoreStatsCollector
// under test. The SessionRestoreStatsCollector has ownership of this delegate,
// which will notify the mock delegate upon its death.
class PassthroughStatsReportingDelegate : public StatsReportingDelegate {
 public:
  PassthroughStatsReportingDelegate() : reporting_delegate_(nullptr) {}
  ~PassthroughStatsReportingDelegate() override {
    reporting_delegate_->ReportStatsCollectorDeath();
  }

  void set_reporting_delegate(MockStatsReportingDelegate* reporting_delegate) {
    reporting_delegate_ = reporting_delegate;
  }

  void ReportTabLoaderStats(const TabLoaderStats& tab_loader_stats) override {
    reporting_delegate_->ReportTabLoaderStats(tab_loader_stats);
  }

  void ReportTabDeferred() override {
    reporting_delegate_->ReportTabDeferred();
  }

  void ReportDeferredTabLoaded() override {
    reporting_delegate_->ReportDeferredTabLoaded();
  }

  void ReportTabTimeSinceActive(base::TimeDelta elapsed) override {
    reporting_delegate_->ReportTabTimeSinceActive(elapsed);
  }

  void ReportTabSiteEngagementScore(double engagement) override {
    reporting_delegate_->ReportTabSiteEngagementScore(engagement);
  }

 private:
  MockStatsReportingDelegate* reporting_delegate_;

  DISALLOW_COPY_AND_ASSIGN(PassthroughStatsReportingDelegate);
};

}  // namespace

class TestSessionRestoreStatsCollector : public SessionRestoreStatsCollector {
 public:
  using SessionRestoreStatsCollector::Observe;
  using SessionRestoreStatsCollector::RenderWidgetHostVisibilityChanged;

  TestSessionRestoreStatsCollector(
      std::unique_ptr<const base::TickClock> tick_clock,
      std::unique_ptr<StatsReportingDelegate> reporting_delegate)
      : SessionRestoreStatsCollector(tick_clock->NowTicks(),
                                     std::move(reporting_delegate)) {
    set_tick_clock(std::move(tick_clock));
  }

 private:
  friend class base::RefCounted<TestSessionRestoreStatsCollector>;

  ~TestSessionRestoreStatsCollector() override {}

  base::SimpleTestTickClock* test_tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(TestSessionRestoreStatsCollector);
};

class SessionRestoreStatsCollectorTest : public testing::Test {
 public:
  using RestoredTab = SessionRestoreDelegate::RestoredTab;

  SessionRestoreStatsCollectorTest() = default;

  void SetUp() override {
    test_web_contents_factory_.reset(new content::TestWebContentsFactory);

    // Ownership of the reporting delegate is passed to the
    // SessionRestoreStatsCollector, but a raw pointer is kept to it so it can
    // be queried by the test.
    passthrough_reporting_delegate_ = new PassthroughStatsReportingDelegate();

    // Ownership of this clock is passed to the SessionRestoreStatsCollector.
    // A raw pointer is kept to it so that it can be modified from the outside.
    // The unittest must take care to access the clock only while the
    // SessionRestoreStatsCollector under test is still alive.
    test_tick_clock_ = new base::SimpleTestTickClock();

    // Create a stats collector, keep a raw pointer to it, and detach from it.
    // The stats collector will stay alive as long as it has not yet completed
    // its job, and will clean itself up when done.
    scoped_refptr<TestSessionRestoreStatsCollector> stats_collector =
        new TestSessionRestoreStatsCollector(
            std::unique_ptr<const base::TickClock>(test_tick_clock_),
            std::unique_ptr<StatsReportingDelegate>(
                passthrough_reporting_delegate_));
    stats_collector_ = stats_collector.get();
    stats_collector = nullptr;
  }

  void TearDown() override {
    passthrough_reporting_delegate_ = nullptr;
    test_tick_clock_ = nullptr;
    stats_collector_ = nullptr;

    // Clean up any tabs that were generated by the unittest.
    restored_tabs_.clear();
    test_web_contents_factory_.reset();
  }

  // Advances the test clock by 1ms.
  void Tick() {
    test_tick_clock_->Advance(base::TimeDelta::FromMilliseconds(1));
  }

  void Show(size_t tab_index) {
    restored_tabs_[tab_index].contents()->WasShown();
    GenerateRenderWidgetVisiblityChanged(tab_index, /*visible=*/true);
  }

  void Hide(size_t tab_index) {
    restored_tabs_[tab_index].contents()->WasHidden();
    GenerateRenderWidgetVisiblityChanged(tab_index, /*visible=*/false);
  }

  // Creates a restored tab backed by dummy WebContents/NavigationController/
  // RenderWidgetHost/RenderWidgetHostView.
  void CreateRestoredTab(bool is_active) {
    content::WebContents* contents =
        test_web_contents_factory_->CreateWebContents(&testing_profile_);
    std::vector<std::unique_ptr<content::NavigationEntry>> entries;
    entries.push_back(content::NavigationEntry::Create());
    contents->GetController().Restore(
        0, content::RestoreType::LAST_SESSION_EXITED_CLEANLY, &entries);
    // Create a last active time in the past.
    content::WebContentsTester::For(contents)->SetLastActiveTime(
        base::TimeTicks::Now() - base::TimeDelta::FromMinutes(1));
    restored_tabs_.push_back(
        RestoredTab(contents, is_active, false, false, base::nullopt));
    if (is_active)
      Show(restored_tabs_.size() - 1);
  }

  // Helper function for various notification generation.
  void GenerateControllerNotification(size_t tab_index, int type) {
    content::WebContents* contents = restored_tabs_[tab_index].contents();
    content::NavigationController* controller = &contents->GetController();
    stats_collector_->Observe(
        type, content::Source<content::NavigationController>(controller),
        content::NotificationService::NoDetails());
  }

  // Generates a load start notification for the given tab.
  void GenerateLoadStart(size_t tab_index) {
    GenerateControllerNotification(tab_index, content::NOTIFICATION_LOAD_START);
  }

  // Generates a load stop notification for the given tab.
  void GenerateLoadStop(size_t tab_index) {
    GenerateControllerNotification(tab_index, content::NOTIFICATION_LOAD_STOP);
  }

  // Generates a web contents destroyed notification for the given tab.
  void GenerateWebContentsDestroyed(size_t tab_index) {
    content::WebContents* contents = restored_tabs_[tab_index].contents();
    stats_collector_->Observe(content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                              content::Source<content::WebContents>(contents),
                              content::NotificationService::NoDetails());
  }

  // Generates a paint notification for the given tab.
  void GenerateRenderWidgetHostDidUpdateBackingStore(size_t tab_index) {
    content::WebContents* contents = restored_tabs_[tab_index].contents();
    content::RenderWidgetHost* host =
        contents->GetRenderWidgetHostView()->GetRenderWidgetHost();
    stats_collector_->Observe(
        content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_VISUAL_PROPERTIES,
        content::Source<content::RenderWidgetHost>(host),
        content::NotificationService::NoDetails());
  }

  void GenerateRenderWidgetVisiblityChanged(size_t tab_index, bool visible) {
    content::WebContents* contents = restored_tabs_[tab_index].contents();
    content::RenderWidgetHost* host =
        contents->GetRenderWidgetHostView()->GetRenderWidgetHost();
    stats_collector_->RenderWidgetHostVisibilityChanged(host, visible);
  }

  // Defers a tab.
  void DeferTab(size_t tab_index) {
    content::WebContents* contents = restored_tabs_[tab_index].contents();
    content::NavigationController* controller = &contents->GetController();
    stats_collector_->DeferTab(controller);
  }

  content::BrowserTaskEnvironment task_environment_;
  // |task_environment_| needs to still be alive when
  // |testing_profile_| is destroyed.
  TestingProfile testing_profile_;

  // Inputs to the stats collector. Reset prior to each test.
  base::SimpleTestTickClock* test_tick_clock_;
  std::vector<RestoredTab> restored_tabs_;

  // A new web contents factory is generated per test. This automatically cleans
  // up any tabs created by previous tests.
  std::unique_ptr<content::TestWebContentsFactory> test_web_contents_factory_;

  // These are recreated for each test. The reporting delegate allows the test
  // to observe the behaviour of the SessionRestoreStatsCollector under test.
  PassthroughStatsReportingDelegate* passthrough_reporting_delegate_;
  TestSessionRestoreStatsCollector* stats_collector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionRestoreStatsCollectorTest);
};

TEST_F(SessionRestoreStatsCollectorTest, SingleTabPaintBeforeLoad) {
  MockStatsReportingDelegate mock_reporting_delegate;
  passthrough_reporting_delegate_->set_reporting_delegate(
      &mock_reporting_delegate);

  CreateRestoredTab(true);
  stats_collector_->TrackTabs(restored_tabs_);
  mock_reporting_delegate.ExpectReportTabTimeSinceActiveCalled(1);
  mock_reporting_delegate.ExpectReportTabSiteEngagementScoreCalled(1);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  Tick();  // 1ms.
  GenerateRenderWidgetHostDidUpdateBackingStore(0);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  Tick();  // 2ms.
  GenerateLoadStop(0);
  mock_reporting_delegate.ExpectReportTabLoaderStatsCalled(
      1, 0, 1, 1, 2, 1, 2,
      SessionRestoreStatsCollector::PAINT_FINISHED_UMA_DONE);
  mock_reporting_delegate.ExpectReportStatsCollectorDeathCalled();
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
}

TEST_F(SessionRestoreStatsCollectorTest, SingleTabPaintAfterLoad) {
  MockStatsReportingDelegate mock_reporting_delegate;
  passthrough_reporting_delegate_->set_reporting_delegate(
      &mock_reporting_delegate);

  CreateRestoredTab(true);
  stats_collector_->TrackTabs(restored_tabs_);
  mock_reporting_delegate.ExpectReportTabTimeSinceActiveCalled(1);
  mock_reporting_delegate.ExpectReportTabSiteEngagementScoreCalled(1);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  Tick();  // 1ms.
  GenerateLoadStop(0);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  Tick();  // 2ms.
  GenerateRenderWidgetHostDidUpdateBackingStore(0);
  mock_reporting_delegate.ExpectReportTabLoaderStatsCalled(
      1, 0, 1, 1, 1, 2, 1,
      SessionRestoreStatsCollector::PAINT_FINISHED_UMA_DONE);
  mock_reporting_delegate.ExpectReportStatsCollectorDeathCalled();
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
}

TEST_F(SessionRestoreStatsCollectorTest, MultipleTabsLoadSerially) {
  MockStatsReportingDelegate mock_reporting_delegate;
  passthrough_reporting_delegate_->set_reporting_delegate(
      &mock_reporting_delegate);

  CreateRestoredTab(true);
  CreateRestoredTab(false);
  CreateRestoredTab(false);
  stats_collector_->TrackTabs(restored_tabs_);
  mock_reporting_delegate.ExpectReportTabTimeSinceActiveCalled(3);
  mock_reporting_delegate.ExpectReportTabSiteEngagementScoreCalled(3);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Foreground tab paints then finishes loading.
  Tick();  // 1ms.
  GenerateRenderWidgetHostDidUpdateBackingStore(0);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
  Tick();  // 2ms.
  GenerateLoadStop(0);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // First background tab starts loading, paints, then finishes loading.
  Tick();  // 3ms.
  GenerateLoadStart(1);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
  Tick();  // 4ms.
  GenerateRenderWidgetHostDidUpdateBackingStore(1);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
  Tick();  // 5ms.
  GenerateLoadStop(1);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Second background tab starts loading, finishes loading, but never paints.
  Tick();  // 6ms.
  GenerateLoadStart(2);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  Tick();  // 7ms.
  GenerateLoadStop(2);
  mock_reporting_delegate.ExpectReportTabLoaderStatsCalled(
      3, 0, 3, 3, 2, 1, 7,
      SessionRestoreStatsCollector::PAINT_FINISHED_UMA_DONE);
  mock_reporting_delegate.ExpectReportStatsCollectorDeathCalled();
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
}

TEST_F(SessionRestoreStatsCollectorTest, MultipleTabsLoadSimultaneously) {
  MockStatsReportingDelegate mock_reporting_delegate;
  passthrough_reporting_delegate_->set_reporting_delegate(
      &mock_reporting_delegate);

  CreateRestoredTab(true);
  CreateRestoredTab(false);
  CreateRestoredTab(false);
  stats_collector_->TrackTabs(restored_tabs_);
  mock_reporting_delegate.ExpectReportTabTimeSinceActiveCalled(3);
  mock_reporting_delegate.ExpectReportTabSiteEngagementScoreCalled(3);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Foreground tab paints then finishes loading.
  Tick();  // 1ms.
  GenerateRenderWidgetHostDidUpdateBackingStore(0);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
  Tick();  // 2ms.
  GenerateLoadStop(0);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Both background tabs start loading at the same time. The first one paints
  // before finishing loading, the second one paints after finishing loading
  // (the stats collector never sees the paint event).
  Tick();  // 3ms.
  GenerateLoadStart(1);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
  GenerateLoadStart(2);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
  Tick();  // 4ms.
  GenerateRenderWidgetHostDidUpdateBackingStore(1);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
  Tick();  // 5ms.
  GenerateLoadStop(1);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
  Tick();  // 6ms.
  GenerateLoadStop(2);
  mock_reporting_delegate.ExpectReportTabLoaderStatsCalled(
      3, 0, 3, 3, 2, 1, 6,
      SessionRestoreStatsCollector::PAINT_FINISHED_UMA_DONE);
  mock_reporting_delegate.ExpectReportStatsCollectorDeathCalled();
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
}

TEST_F(SessionRestoreStatsCollectorTest, DeferredTabs) {
  MockStatsReportingDelegate mock_reporting_delegate;
  passthrough_reporting_delegate_->set_reporting_delegate(
      &mock_reporting_delegate);

  CreateRestoredTab(true);
  CreateRestoredTab(false);
  stats_collector_->TrackTabs(restored_tabs_);
  mock_reporting_delegate.ExpectReportTabTimeSinceActiveCalled(2);
  mock_reporting_delegate.ExpectReportTabSiteEngagementScoreCalled(2);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Foreground tab paints, then the background tab is deferred.
  Tick();  // 1ms.
  GenerateRenderWidgetHostDidUpdateBackingStore(0);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
  DeferTab(1);
  mock_reporting_delegate.ExpectReportTabDeferredCalled();
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Foreground tab finishes loading and stats get reported.
  Tick();  // 2ms.
  GenerateLoadStop(0);
  mock_reporting_delegate.ExpectReportTabLoaderStatsCalled(
      2, 1, 1, 1, 2, 1, 2,
      SessionRestoreStatsCollector::PAINT_FINISHED_UMA_DONE);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Background tab starts loading, paints and stops loading. This fires off a
  // deferred tab loaded notification.
  Tick();  // 3ms.
  GenerateLoadStart(1);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
  Tick();  // 4ms.
  GenerateRenderWidgetHostDidUpdateBackingStore(1);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
  Tick();  // 5ms.
  GenerateLoadStop(1);
  mock_reporting_delegate.ExpectReportDeferredTabLoadedCalled();
  mock_reporting_delegate.ExpectReportStatsCollectorDeathCalled();
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
}

TEST_F(SessionRestoreStatsCollectorTest, FocusSwitchNoForegroundPaintOrLoad) {
  MockStatsReportingDelegate mock_reporting_delegate;
  passthrough_reporting_delegate_->set_reporting_delegate(
      &mock_reporting_delegate);

  CreateRestoredTab(true);
  stats_collector_->TrackTabs(restored_tabs_);
  mock_reporting_delegate.ExpectReportTabTimeSinceActiveCalled(1);
  mock_reporting_delegate.ExpectReportTabSiteEngagementScoreCalled(1);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Create another tab and make it the foreground tab. This tab is not actually
  // being tracked by the SessionRestoreStatsCollector, but its paint events
  // will be observed.
  CreateRestoredTab(false);
  Hide(0);
  Show(1);

  // Load and paint the restored tab (now the background tab). Don't expect
  // any calls to the mock as a visible tab paint has not yet been observed.
  Tick();  // 1ms.
  GenerateRenderWidgetHostDidUpdateBackingStore(0);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
  Tick();  // 2ms.
  GenerateLoadStop(0);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Mark the new foreground tab as having painted. This should cause the
  // stats to be emitted, but with empty foreground paint and load values.
  Tick();  // 3ms.
  GenerateRenderWidgetHostDidUpdateBackingStore(1);
  mock_reporting_delegate.ExpectReportTabLoaderStatsCalled(
      1, 0, 1, 1, 0, 0, 2,
      SessionRestoreStatsCollector::
          PAINT_FINISHED_NON_RESTORED_TAB_PAINTED_FIRST);
  mock_reporting_delegate.ExpectReportStatsCollectorDeathCalled();
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
}

TEST_F(SessionRestoreStatsCollectorTest, FocusSwitchNoForegroundPaint) {
  MockStatsReportingDelegate mock_reporting_delegate;
  passthrough_reporting_delegate_->set_reporting_delegate(
      &mock_reporting_delegate);

  CreateRestoredTab(true);
  stats_collector_->TrackTabs(restored_tabs_);
  mock_reporting_delegate.ExpectReportTabTimeSinceActiveCalled(1);
  mock_reporting_delegate.ExpectReportTabSiteEngagementScoreCalled(1);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Load the foreground tab.
  Tick();  // 1ms.
  GenerateLoadStop(0);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Create another tab and make it the foreground tab. This tab is not actually
  // being tracked by the SessionRestoreStatsCollector, but its paint events
  // will still be observed.
  CreateRestoredTab(false);
  Hide(0);
  Show(1);

  // Load and paint the restored tab (now the background tab). Don't expect
  // any calls to the mock as a visible tab paint has not yet been observed.
  Tick();  // 2ms.
  GenerateRenderWidgetHostDidUpdateBackingStore(0);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Mark the new foreground tab as having painted. This should cause the
  // stats to be emitted, but with an empty foreground paint value.
  Tick();  // 3ms.
  GenerateRenderWidgetHostDidUpdateBackingStore(1);
  mock_reporting_delegate.ExpectReportTabLoaderStatsCalled(
      1, 0, 1, 1, 1, 0, 1,
      SessionRestoreStatsCollector::
          PAINT_FINISHED_NON_RESTORED_TAB_PAINTED_FIRST);
  mock_reporting_delegate.ExpectReportStatsCollectorDeathCalled();
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
}

// Test that if a single restored foreground tab is occluded at any point
// before first paint, the time to first paint is not recorded, and the
// FinishReason is PAINT_FINISHED_UMA_NO_COMPLETELY_VISIBLE_TABS.
TEST_F(SessionRestoreStatsCollectorTest, ForegroundTabOccluded) {
  MockStatsReportingDelegate mock_reporting_delegate;
  passthrough_reporting_delegate_->set_reporting_delegate(
      &mock_reporting_delegate);

  CreateRestoredTab(/*is_active=*/true);
  stats_collector_->TrackTabs(restored_tabs_);
  mock_reporting_delegate.ExpectReportTabTimeSinceActiveCalled(1);
  mock_reporting_delegate.ExpectReportTabSiteEngagementScoreCalled(1);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Load the foreground tab.
  Tick();  // 1ms.
  GenerateLoadStop(0);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Mark the tab as hidden/occluded.
  GenerateRenderWidgetVisiblityChanged(0, /*visible=*/false);
  Tick();  // 2ms.

  // Mark the tab as visible.
  GenerateRenderWidgetVisiblityChanged(0, /*visible=*/true);
  // Mark the tab as having painted. This should cause the
  // stats to be emitted, but with an empty foreground paint value because it
  // was not visible at one point during restore.
  GenerateRenderWidgetHostDidUpdateBackingStore(0);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
  // Destroy the tab.
  GenerateWebContentsDestroyed(0);
  mock_reporting_delegate.ExpectReportTabLoaderStatsCalled(
      1, 0, 1, 1, 1, 0, 1,
      SessionRestoreStatsCollector::
          PAINT_FINISHED_UMA_NO_COMPLETELY_VISIBLE_TABS);
  mock_reporting_delegate.ExpectReportStatsCollectorDeathCalled();
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
}

// Test that if the first tab painted was hidden/occluded before first paint,
// the first paint time of the second tab to be painted is recorded.
TEST_F(SessionRestoreStatsCollectorTest, FirstOfTwoTabsOccluded) {
  MockStatsReportingDelegate mock_reporting_delegate;
  passthrough_reporting_delegate_->set_reporting_delegate(
      &mock_reporting_delegate);

  CreateRestoredTab(/*is_active=*/true);
  CreateRestoredTab(/*is_active=*/true);
  stats_collector_->TrackTabs(restored_tabs_);
  mock_reporting_delegate.ExpectReportTabTimeSinceActiveCalled(2);
  mock_reporting_delegate.ExpectReportTabSiteEngagementScoreCalled(2);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Load the first tab.
  Tick();  // 1ms.
  GenerateLoadStop(0);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Mark the tab as hidden/occluded.
  GenerateRenderWidgetVisiblityChanged(0, /*visible=*/false);
  Tick();  // 2ms.

  // Mark the tab as visible.
  GenerateRenderWidgetVisiblityChanged(0, /*visible=*/true);
  // Mark the tab as having painted. Because it was occluded,
  // no stat should be emitted.
  GenerateRenderWidgetHostDidUpdateBackingStore(0);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  Tick();  // 3ms.
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
  Tick();  // 4ms.
  Tick();  // 5ms.
  // Mark the second tab as loaded and painted.
  GenerateLoadStop(1);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
  GenerateRenderWidgetHostDidUpdateBackingStore(1);
  mock_reporting_delegate.ExpectReportTabLoaderStatsCalled(
      2, 0, 2, 2, 1, 5, 5,
      SessionRestoreStatsCollector::PAINT_FINISHED_UMA_DONE);
  mock_reporting_delegate.ExpectReportStatsCollectorDeathCalled();
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
}

TEST_F(SessionRestoreStatsCollectorTest, LoadingTabDestroyedBeforePaint) {
  MockStatsReportingDelegate mock_reporting_delegate;
  passthrough_reporting_delegate_->set_reporting_delegate(
      &mock_reporting_delegate);

  CreateRestoredTab(true);
  stats_collector_->TrackTabs(restored_tabs_);
  mock_reporting_delegate.ExpectReportTabTimeSinceActiveCalled(1);
  mock_reporting_delegate.ExpectReportTabSiteEngagementScoreCalled(1);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Destroy the tab. Expect all timings to be zero.
  GenerateWebContentsDestroyed(0);
  mock_reporting_delegate.ExpectReportTabLoaderStatsCalled(
      1, 0, 1, 0, 0, 0, 0,
      SessionRestoreStatsCollector::PAINT_FINISHED_UMA_NO_PAINT);
  mock_reporting_delegate.ExpectReportStatsCollectorDeathCalled();
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
}

TEST_F(SessionRestoreStatsCollectorTest, LoadingTabDestroyedAfterPaint) {
  MockStatsReportingDelegate mock_reporting_delegate;
  passthrough_reporting_delegate_->set_reporting_delegate(
      &mock_reporting_delegate);

  CreateRestoredTab(true);
  stats_collector_->TrackTabs(restored_tabs_);
  mock_reporting_delegate.ExpectReportTabTimeSinceActiveCalled(1);
  mock_reporting_delegate.ExpectReportTabSiteEngagementScoreCalled(1);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  Tick();  // 1 ms.
  GenerateRenderWidgetHostDidUpdateBackingStore(0);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Destroy the tab. Expect both load timings to be zero.
  GenerateWebContentsDestroyed(0);
  mock_reporting_delegate.ExpectReportTabLoaderStatsCalled(
      1, 0, 1, 0, 0, 1, 0,
      SessionRestoreStatsCollector::PAINT_FINISHED_UMA_DONE);
  mock_reporting_delegate.ExpectReportStatsCollectorDeathCalled();
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
}

TEST_F(SessionRestoreStatsCollectorTest, BrowseAwayBeforePaint) {
  MockStatsReportingDelegate mock_reporting_delegate;
  passthrough_reporting_delegate_->set_reporting_delegate(
      &mock_reporting_delegate);

  CreateRestoredTab(true);
  stats_collector_->TrackTabs(restored_tabs_);
  mock_reporting_delegate.ExpectReportTabTimeSinceActiveCalled(1);
  mock_reporting_delegate.ExpectReportTabSiteEngagementScoreCalled(1);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Load the tab.
  Tick();  // 1 ms.
  GenerateLoadStop(0);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Reload the tab. Expect the paint timing to be zero.
  Tick();  // 2 ms.
  GenerateLoadStart(0);
  mock_reporting_delegate.ExpectReportTabLoaderStatsCalled(
      1, 0, 1, 1, 1, 0, 1,
      SessionRestoreStatsCollector::PAINT_FINISHED_UMA_NO_PAINT);
  mock_reporting_delegate.ExpectReportStatsCollectorDeathCalled();
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
}

TEST_F(SessionRestoreStatsCollectorTest, DiscardDeferredTabs) {
  MockStatsReportingDelegate mock_reporting_delegate;
  passthrough_reporting_delegate_->set_reporting_delegate(
      &mock_reporting_delegate);

  CreateRestoredTab(true);
  CreateRestoredTab(false);
  stats_collector_->TrackTabs(restored_tabs_);
  mock_reporting_delegate.ExpectReportTabTimeSinceActiveCalled(2);
  mock_reporting_delegate.ExpectReportTabSiteEngagementScoreCalled(2);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Defer the background tab.
  Tick();  // 1 ms.
  DeferTab(1);
  mock_reporting_delegate.ExpectReportTabDeferredCalled();
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Discard the foreground tab. The stats tab loader stats should be reported
  // with all zero timings.
  Tick();  // 2 ms.
  GenerateWebContentsDestroyed(0);
  mock_reporting_delegate.ExpectReportTabLoaderStatsCalled(
      2, 1, 1, 0, 0, 0, 0,
      SessionRestoreStatsCollector::PAINT_FINISHED_UMA_NO_PAINT);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Destroy the background tab. The collector should release itself.
  Tick();  // 3 ms.
  GenerateWebContentsDestroyed(1);
  mock_reporting_delegate.ExpectReportStatsCollectorDeathCalled();
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
}
