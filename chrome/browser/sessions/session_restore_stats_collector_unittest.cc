// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore_stats_collector.h"

#include <stddef.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
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
        report_stats_collector_death_call_count_(0u) {}

  MockStatsReportingDelegate(const MockStatsReportingDelegate&) = delete;
  MockStatsReportingDelegate& operator=(const MockStatsReportingDelegate&) =
      delete;

  ~MockStatsReportingDelegate() override = default;

  void ReportTabLoaderStats(const TabLoaderStats& stats) override {
    report_tab_loader_stats_call_count_++;
    tab_loader_stats_ = stats;
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
      int foreground_tab_first_paint_ms,
      SessionRestoreStatsCollector::SessionRestorePaintFinishReasonUma
          finish_reason) {
    EXPECT_EQ(tab_count, tab_loader_stats_.tab_count);
    EXPECT_EQ(base::Milliseconds(foreground_tab_first_paint_ms),
              tab_loader_stats_.foreground_tab_first_paint);
    EXPECT_EQ(tab_loader_stats_.tab_first_paint_reason, finish_reason);
  }

  void EnsureNoUnexpectedCalls() {
    EXPECT_EQ(0u, report_tab_loader_stats_call_count_);
    EXPECT_EQ(0u, report_stats_collector_death_call_count_);

    report_tab_loader_stats_call_count_ = 0u;
    report_stats_collector_death_call_count_ = 0u;
    tab_loader_stats_ = TabLoaderStats();
  }

  void ExpectEnded() {
    EXPECT_EQ(1u, report_tab_loader_stats_call_count_);
    EXPECT_EQ(1u, report_stats_collector_death_call_count_);
  }

 private:
  size_t report_tab_loader_stats_call_count_;
  size_t report_stats_collector_death_call_count_;
  TabLoaderStats tab_loader_stats_;
};

// A pass-through stats reporting delegate. This is used to decouple the
// lifetime of the mock reporting delegate from the SessionRestoreStatsCollector
// under test. The SessionRestoreStatsCollector has ownership of this delegate,
// which will notify the mock delegate upon its death.
class PassthroughStatsReportingDelegate : public StatsReportingDelegate {
 public:
  PassthroughStatsReportingDelegate() : reporting_delegate_(nullptr) {}

  PassthroughStatsReportingDelegate(const PassthroughStatsReportingDelegate&) =
      delete;
  PassthroughStatsReportingDelegate& operator=(
      const PassthroughStatsReportingDelegate&) = delete;

  ~PassthroughStatsReportingDelegate() override {
    reporting_delegate_->ReportStatsCollectorDeath();
  }

  void set_reporting_delegate(MockStatsReportingDelegate* reporting_delegate) {
    reporting_delegate_ = reporting_delegate;
  }

  void ReportTabLoaderStats(const TabLoaderStats& tab_loader_stats) override {
    reporting_delegate_->ReportTabLoaderStats(tab_loader_stats);
  }

 private:
  raw_ptr<MockStatsReportingDelegate> reporting_delegate_;
};

}  // namespace

class SessionRestoreStatsCollectorTest : public testing::Test {
 public:
  using RestoredTab = SessionRestoreDelegate::RestoredTab;

  SessionRestoreStatsCollectorTest()
      : task_environment_{base::test::TaskEnvironment::TimeSource::MOCK_TIME} {}

  SessionRestoreStatsCollectorTest(const SessionRestoreStatsCollectorTest&) =
      delete;
  SessionRestoreStatsCollectorTest& operator=(
      const SessionRestoreStatsCollectorTest&) = delete;

  void SetUp() override {
    test_web_contents_factory_ =
        std::make_unique<content::TestWebContentsFactory>();

    // Ownership of the reporting delegate is passed to the
    // SessionRestoreStatsCollector, but a raw pointer is kept to it so it can
    // be queried by the test.
    passthrough_reporting_delegate_ = new PassthroughStatsReportingDelegate();

    // Force creation of the stats collector.
    stats_collector_ = SessionRestoreStatsCollector::GetOrCreateInstance(
        base::TimeTicks::Now(), std::unique_ptr<StatsReportingDelegate>(
                                    passthrough_reporting_delegate_));
  }

  void TearDown() override {
    passthrough_reporting_delegate_ = nullptr;

    // Clean up any tabs that were generated by the unittest.
    restored_tabs_.clear();
    test_web_contents_factory_.reset();
  }

  // Advances the test clock by 1ms.
  void Tick() { task_environment_.FastForwardBy(base::Milliseconds(1)); }

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
    contents->GetController().Restore(0, content::RestoreType::kRestored,
                                      &entries);
    // Create a last active time in the past.
    content::WebContentsTester::For(contents)->SetLastActiveTime(
        base::Time::Now() - base::Minutes(1));
    restored_tabs_.push_back(
        RestoredTab(contents, is_active, false, false, std::nullopt));
    if (is_active)
      Show(restored_tabs_.size() - 1);
  }

  // Generates a render widget host destroyed notification for the given tab.
  void GenerateRenderWidgetHostDestroyed(size_t tab_index) {
    content::WebContents* contents = restored_tabs_[tab_index].contents();
    stats_collector_->RenderWidgetHostDestroyed(
        contents->GetRenderWidgetHostView()->GetRenderWidgetHost());
  }

  // Generates a paint notification for the given tab.
  void GenerateRenderWidgetHostDidUpdateBackingStore(size_t tab_index) {
    content::WebContents* contents = restored_tabs_[tab_index].contents();
    content::RenderWidgetHost* host =
        contents->GetRenderWidgetHostView()->GetRenderWidgetHost();
    stats_collector_->RenderWidgetHostDidUpdateVisualProperties(host);
  }

  void GenerateRenderWidgetVisiblityChanged(size_t tab_index, bool visible) {
    content::WebContents* contents = restored_tabs_[tab_index].contents();
    content::RenderWidgetHost* host =
        contents->GetRenderWidgetHostView()->GetRenderWidgetHost();
    stats_collector_->RenderWidgetHostVisibilityChanged(host, visible);
  }

  content::BrowserTaskEnvironment task_environment_;
  // |task_environment_| needs to still be alive when
  // |testing_profile_| is destroyed.
  TestingProfile testing_profile_;

  // Inputs to the stats collector. Reset prior to each test.
  std::vector<RestoredTab> restored_tabs_;

  // A new web contents factory is generated per test. This automatically cleans
  // up any tabs created by previous tests.
  std::unique_ptr<content::TestWebContentsFactory> test_web_contents_factory_;

  // These are recreated for each test. The reporting delegate allows the test
  // to observe the behaviour of the SessionRestoreStatsCollector under test.
  raw_ptr<PassthroughStatsReportingDelegate, DanglingUntriaged>
      passthrough_reporting_delegate_;
  raw_ptr<SessionRestoreStatsCollector, DanglingUntriaged> stats_collector_;
};

TEST_F(SessionRestoreStatsCollectorTest, MultipleTabsLoadSerially) {
  MockStatsReportingDelegate mock_reporting_delegate;
  passthrough_reporting_delegate_->set_reporting_delegate(
      &mock_reporting_delegate);

  CreateRestoredTab(true);
  CreateRestoredTab(false);
  CreateRestoredTab(false);
  stats_collector_->TrackTabs(restored_tabs_);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
  // Foreground tab paints then finishes loading.
  Tick();  // 1ms.
  GenerateRenderWidgetHostDidUpdateBackingStore(0);
  mock_reporting_delegate.ExpectReportTabLoaderStatsCalled(
      3, 1, SessionRestoreStatsCollector::PAINT_FINISHED_UMA_DONE);
  mock_reporting_delegate.ExpectEnded();
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
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
  Tick();  // 1ms.

  // Mark the tab as hidden/occluded.
  GenerateRenderWidgetVisiblityChanged(0, /*visible=*/false);
  Tick();  // 2ms.

  // Mark the tab as visible.
  GenerateRenderWidgetVisiblityChanged(0, /*visible=*/true);
  // Mark the tab as having painted. This should cause the
  // stats to be emitted, but with an empty foreground paint value because it
  // was not visible at one point during restore.
  GenerateRenderWidgetHostDidUpdateBackingStore(0);
  // Destroy the tab.
  GenerateRenderWidgetHostDestroyed(0);
  mock_reporting_delegate.ExpectReportTabLoaderStatsCalled(
      1, 0,
      SessionRestoreStatsCollector::
          PAINT_FINISHED_UMA_NO_COMPLETELY_VISIBLE_TABS);
  mock_reporting_delegate.ExpectEnded();
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
  mock_reporting_delegate.EnsureNoUnexpectedCalls();
  Tick();  // 1ms.

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

  // Mark the second tab as painted.
  GenerateRenderWidgetHostDidUpdateBackingStore(1);
  mock_reporting_delegate.ExpectReportTabLoaderStatsCalled(
      2, 5, SessionRestoreStatsCollector::PAINT_FINISHED_UMA_DONE);
  mock_reporting_delegate.ExpectEnded();
}

TEST_F(SessionRestoreStatsCollectorTest, LoadingTabDestroyedBeforePaint) {
  MockStatsReportingDelegate mock_reporting_delegate;
  passthrough_reporting_delegate_->set_reporting_delegate(
      &mock_reporting_delegate);

  CreateRestoredTab(true);
  stats_collector_->TrackTabs(restored_tabs_);
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Destroy the tab. Expect all timings to be zero.
  GenerateRenderWidgetHostDestroyed(0);
  mock_reporting_delegate.ExpectReportTabLoaderStatsCalled(
      1, 0, SessionRestoreStatsCollector::PAINT_FINISHED_UMA_NO_PAINT);
  mock_reporting_delegate.ExpectEnded();
}

TEST_F(SessionRestoreStatsCollectorTest, FocusSwitchNoForegroundPaintOrLoad) {
  MockStatsReportingDelegate mock_reporting_delegate;
  passthrough_reporting_delegate_->set_reporting_delegate(
      &mock_reporting_delegate);

  CreateRestoredTab(true);
  stats_collector_->TrackTabs(restored_tabs_);
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
  mock_reporting_delegate.EnsureNoUnexpectedCalls();

  // Mark the new foreground tab as having painted. This should cause the
  // stats to be emitted, but with empty foreground paint and load values.
  Tick();  // 3ms.
  GenerateRenderWidgetHostDidUpdateBackingStore(1);
  mock_reporting_delegate.ExpectReportTabLoaderStatsCalled(
      1, 0,
      SessionRestoreStatsCollector::
          PAINT_FINISHED_NON_RESTORED_TAB_PAINTED_FIRST);
  mock_reporting_delegate.ExpectEnded();
}
