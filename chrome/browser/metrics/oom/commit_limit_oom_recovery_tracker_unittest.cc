// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/oom/commit_limit_oom_recovery_tracker.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

class CommitLimitOOMRecoveryTrackerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  CommitLimitOOMRecoveryTrackerTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    EXPECT_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(user_data_host_));
    EXPECT_CALL(mock_tab_, GetContents())
        .WillRepeatedly(testing::Return(web_contents()));
    tracker_ = std::make_unique<CommitLimitOOMRecoveryTracker>(mock_tab_);
    ASSERT_NE(tracker_, nullptr);
  }

  void TearDown() override {
    tracker_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  CommitLimitOOMRecoveryTracker* tracker() { return tracker_.get(); }

  void SimulateCommitOOMCrash() {
    auto* rph = static_cast<content::MockRenderProcessHost*>(
        web_contents()->GetPrimaryMainFrame()->GetProcess());
    rph->SimulateRenderProcessExit(
        base::TERMINATION_STATUS_PROCESS_WAS_KILLED,
        CHROME_RESULT_CODE_TERMINATED_BY_OTHER_PROCESS_ON_COMMIT_FAILURE);
  }

  void SimulateNormalCrash() {
    auto* rph = static_cast<content::MockRenderProcessHost*>(
        web_contents()->GetPrimaryMainFrame()->GetProcess());
    rph->SimulateRenderProcessExit(base::TERMINATION_STATUS_PROCESS_CRASHED, 0);
  }

  void SimulateNormalTermination() {
    auto* rph = static_cast<content::MockRenderProcessHost*>(
        web_contents()->GetPrimaryMainFrame()->GetProcess());
    rph->SimulateRenderProcessExit(base::TERMINATION_STATUS_NORMAL_TERMINATION,
                                   0);
  }

  CommitLimitOOMRecoveryTracker::TrackingState GetTrackerState() {
    return tracker_->state_;
  }

  void SetTrackerState(CommitLimitOOMRecoveryTracker::TrackingState state) {
    tracker_->state_ = state;
  }

  void SimulateVisibilityChanged(content::Visibility visibility) {
    tracker_->OnVisibilityChanged(visibility);
  }

  content::WebContents* GetObservedWebContents() {
    return tracker_->web_contents();
  }

 protected:
  ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface mock_tab_;
  std::unique_ptr<CommitLimitOOMRecoveryTracker> tracker_;
};

TEST_F(CommitLimitOOMRecoveryTrackerTest, HiddenDiscardAndReloadSuccess) {
  const GURL kUrl("https://example.com");
  NavigateAndCommit(kUrl);

  web_contents()->WasHidden();
  EXPECT_EQ(GetTrackerState(),
            CommitLimitOOMRecoveryTracker::TrackingState::kIdle);

  base::HistogramTester histograms;
  SimulateCommitOOMCrash();

  EXPECT_EQ(
      GetTrackerState(),
      CommitLimitOOMRecoveryTracker::TrackingState::kAwaitingReactivation);
  histograms.ExpectTotalCount("Stability.CommitLimitTerminatedTab.ReloadResult",
                              0);
  histograms.ExpectUniqueSample(
      "Stability.CommitLimitTerminatedTab.Visibility",
      CommitLimitOOMRecoveryTracker::TerminationVisibility::kHidden, 1);

  web_contents()->SetWasDiscarded(true);
  web_contents()->WasShown();

  EXPECT_EQ(GetTrackerState(),
            CommitLimitOOMRecoveryTracker::TrackingState::kReloadInFlight);

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      kUrl, web_contents());
  simulator->Start();
  simulator->Commit();

  EXPECT_EQ(GetTrackerState(),
            CommitLimitOOMRecoveryTracker::TrackingState::kIdle);
  histograms.ExpectUniqueSample(
      "Stability.CommitLimitTerminatedTab.ReloadResult",
      CommitLimitOOMRecoveryTracker::ReloadResult::kSuccess, 1);
}

TEST_F(CommitLimitOOMRecoveryTrackerTest, HiddenNoDiscardOnReactivation) {
  const GURL kUrl("https://example.com");
  NavigateAndCommit(kUrl);

  web_contents()->WasHidden();

  base::HistogramTester histograms;
  SimulateCommitOOMCrash();

  EXPECT_EQ(
      GetTrackerState(),
      CommitLimitOOMRecoveryTracker::TrackingState::kAwaitingReactivation);

  web_contents()->WasShown();

  EXPECT_EQ(GetTrackerState(),
            CommitLimitOOMRecoveryTracker::TrackingState::kIdle);
  histograms.ExpectUniqueSample(
      "Stability.CommitLimitTerminatedTab.ReloadResult",
      CommitLimitOOMRecoveryTracker::ReloadResult::kFailedNoDiscard, 1);
}

TEST_F(CommitLimitOOMRecoveryTrackerTest,
       ReactivationReloadFailsWithCommitOOM) {
  const GURL kUrl("https://example.com");
  NavigateAndCommit(kUrl);

  web_contents()->WasHidden();

  base::HistogramTester histograms;
  SimulateCommitOOMCrash();

  web_contents()->SetWasDiscarded(true);
  web_contents()->WasShown();

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      kUrl, web_contents());
  simulator->Start();

  // Renderer dies again with commit OOM during reload.
  SimulateCommitOOMCrash();

  EXPECT_EQ(GetTrackerState(),
            CommitLimitOOMRecoveryTracker::TrackingState::kIdle);
  histograms.ExpectUniqueSample(
      "Stability.CommitLimitTerminatedTab.ReloadResult",
      CommitLimitOOMRecoveryTracker::ReloadResult::kFailedOOM, 1);
}

TEST_F(CommitLimitOOMRecoveryTrackerTest, NavigationFailsThenSucceeds) {
  const GURL kUrl("https://example.com");
  NavigateAndCommit(kUrl);

  web_contents()->WasHidden();

  base::HistogramTester histograms;
  SimulateCommitOOMCrash();

  web_contents()->SetWasDiscarded(true);
  web_contents()->WasShown();

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      kUrl, web_contents());
  simulator->Start();
  simulator->Fail(net::ERR_ABORTED);

  // We should still be tracking because the navigation failed but the renderer
  // didn't crash.
  EXPECT_EQ(GetTrackerState(),
            CommitLimitOOMRecoveryTracker::TrackingState::kReloadInFlight);
  histograms.ExpectTotalCount("Stability.CommitLimitTerminatedTab.ReloadResult",
                              0);

  // Retry succeeds.
  auto simulator2 = content::NavigationSimulator::CreateBrowserInitiated(
      kUrl, web_contents());
  simulator2->Start();
  simulator2->Commit();

  EXPECT_EQ(GetTrackerState(),
            CommitLimitOOMRecoveryTracker::TrackingState::kIdle);
  histograms.ExpectUniqueSample(
      "Stability.CommitLimitTerminatedTab.ReloadResult",
      CommitLimitOOMRecoveryTracker::ReloadResult::kSuccess, 1);
}

TEST_F(CommitLimitOOMRecoveryTrackerTest, RecoveryViaRedirect) {
  const GURL kUrl("https://example.com");
  const GURL kRedirectUrl("https://example.com/redirected");
  NavigateAndCommit(kUrl);

  web_contents()->WasHidden();

  base::HistogramTester histograms;
  SimulateCommitOOMCrash();

  web_contents()->SetWasDiscarded(true);
  web_contents()->WasShown();

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      kUrl, web_contents());
  simulator->Start();
  simulator->Redirect(kRedirectUrl);
  simulator->Commit();

  EXPECT_EQ(GetTrackerState(),
            CommitLimitOOMRecoveryTracker::TrackingState::kIdle);
  histograms.ExpectUniqueSample(
      "Stability.CommitLimitTerminatedTab.ReloadResult",
      CommitLimitOOMRecoveryTracker::ReloadResult::kSuccess, 1);
}

TEST_F(CommitLimitOOMRecoveryTrackerTest, ForegroundCommitOOMKill) {
  const GURL kUrl("https://example.com");
  NavigateAndCommit(kUrl);

  web_contents()->WasShown();

  base::HistogramTester histograms;
  SimulateCommitOOMCrash();

  EXPECT_EQ(GetTrackerState(),
            CommitLimitOOMRecoveryTracker::TrackingState::kIdle);
  histograms.ExpectUniqueSample(
      "Stability.CommitLimitTerminatedTab.Visibility",
      CommitLimitOOMRecoveryTracker::TerminationVisibility::kVisible, 1);
  histograms.ExpectTotalCount("Stability.CommitLimitTerminatedTab.ReloadResult",
                              0);
}

TEST_F(CommitLimitOOMRecoveryTrackerTest, OccludedCommitOOMKill) {
  const GURL kUrl("https://example.com");
  NavigateAndCommit(kUrl);

  web_contents()->WasOccluded();

  base::HistogramTester histograms;
  SimulateCommitOOMCrash();

  EXPECT_EQ(GetTrackerState(),
            CommitLimitOOMRecoveryTracker::TrackingState::kIdle);
  histograms.ExpectUniqueSample(
      "Stability.CommitLimitTerminatedTab.Visibility",
      CommitLimitOOMRecoveryTracker::TerminationVisibility::kOccluded, 1);
  histograms.ExpectTotalCount("Stability.CommitLimitTerminatedTab.ReloadResult",
                              0);
}

TEST_F(CommitLimitOOMRecoveryTrackerTest, CensoredHideDuringReload) {
  const GURL kUrl("https://example.com");
  NavigateAndCommit(kUrl);

  web_contents()->WasHidden();

  base::HistogramTester histograms;
  SimulateCommitOOMCrash();

  web_contents()->SetWasDiscarded(true);
  web_contents()->WasShown();

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      kUrl, web_contents());
  simulator->Start();

  // Hide during reload.
  web_contents()->WasHidden();

  EXPECT_EQ(GetTrackerState(),
            CommitLimitOOMRecoveryTracker::TrackingState::kIdle);
  histograms.ExpectTotalCount("Stability.CommitLimitTerminatedTab.ReloadResult",
                              0);
}

TEST_F(CommitLimitOOMRecoveryTrackerTest,
       CensoredHideDuringAwaitingReactivation) {
  const GURL kUrl("https://example.com");
  NavigateAndCommit(kUrl);

  web_contents()->WasHidden();

  base::HistogramTester histograms;
  SimulateCommitOOMCrash();

  // Tab is hidden again.
  SimulateVisibilityChanged(content::Visibility::HIDDEN);

  EXPECT_EQ(GetTrackerState(),
            CommitLimitOOMRecoveryTracker::TrackingState::kIdle);
  histograms.ExpectTotalCount("Stability.CommitLimitTerminatedTab.ReloadResult",
                              0);
}

TEST_F(CommitLimitOOMRecoveryTrackerTest, CensoredNavigateAway) {
  const GURL kUrl("https://example.com");
  const GURL kDifferentUrl("https://different.com");
  NavigateAndCommit(kUrl);

  web_contents()->WasHidden();

  base::HistogramTester histograms;
  SimulateCommitOOMCrash();

  web_contents()->SetWasDiscarded(true);
  web_contents()->WasShown();

  // Navigate to a different URL.
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      kDifferentUrl, web_contents());
  simulator->Start();

  EXPECT_EQ(GetTrackerState(),
            CommitLimitOOMRecoveryTracker::TrackingState::kIdle);
  histograms.ExpectTotalCount("Stability.CommitLimitTerminatedTab.ReloadResult",
                              0);
}

TEST_F(CommitLimitOOMRecoveryTrackerTest,
       CensoredNormalTerminationDuringTracking) {
  const GURL kUrl("https://example.com");
  NavigateAndCommit(kUrl);

  web_contents()->WasHidden();

  base::HistogramTester histograms;
  SimulateCommitOOMCrash();
  EXPECT_EQ(
      GetTrackerState(),
      CommitLimitOOMRecoveryTracker::TrackingState::kAwaitingReactivation);

  SimulateNormalTermination();

  EXPECT_EQ(GetTrackerState(),
            CommitLimitOOMRecoveryTracker::TrackingState::kIdle);
  histograms.ExpectTotalCount("Stability.CommitLimitTerminatedTab.ReloadResult",
                              0);
}

TEST_F(CommitLimitOOMRecoveryTrackerTest, NonCommitFailureTerminationNoSample) {
  const GURL kUrl("https://example.com");
  NavigateAndCommit(kUrl);

  web_contents()->WasHidden();

  base::HistogramTester histograms;
  SimulateNormalCrash();

  EXPECT_EQ(GetTrackerState(),
            CommitLimitOOMRecoveryTracker::TrackingState::kIdle);
  histograms.ExpectTotalCount("Stability.CommitLimitTerminatedTab.ReloadResult",
                              0);
}

TEST_F(CommitLimitOOMRecoveryTrackerTest, NavigateAwayToleratesFragment) {
  const GURL kUrl("https://example.com#foo");
  const GURL kReloadUrl("https://example.com#bar");
  NavigateAndCommit(kUrl);

  web_contents()->WasHidden();

  base::HistogramTester histograms;
  SimulateCommitOOMCrash();

  web_contents()->SetWasDiscarded(true);
  web_contents()->WasShown();

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      kReloadUrl, web_contents());
  simulator->Start();

  // We should still be in kReloadInFlight because fragment differences are
  // tolerated.
  EXPECT_EQ(GetTrackerState(),
            CommitLimitOOMRecoveryTracker::TrackingState::kReloadInFlight);

  simulator->Commit();
  histograms.ExpectUniqueSample(
      "Stability.CommitLimitTerminatedTab.ReloadResult",
      CommitLimitOOMRecoveryTracker::ReloadResult::kSuccess, 1);
}

TEST_F(CommitLimitOOMRecoveryTrackerTest, TabDiscardSwizzlesWebContents) {
  // Destroy the tracker created by SetUp.
  tracker_.reset();

  tabs::TabInterface::WillDiscardContentsCallback discard_callback;
  EXPECT_CALL(mock_tab_, RegisterWillDiscardContents(testing::_))
      .WillOnce([&](tabs::TabInterface::WillDiscardContentsCallback cb) {
        discard_callback = std::move(cb);
        return base::CallbackListSubscription();
      });

  tracker_ = std::make_unique<CommitLimitOOMRecoveryTracker>(mock_tab_);
  ASSERT_TRUE(discard_callback);
  EXPECT_EQ(GetObservedWebContents(), web_contents());

  // Create a new mock WebContents to represent the swizzled target.
  std::unique_ptr<content::WebContents> new_web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  // Trigger the discard.
  discard_callback.Run(&mock_tab_, web_contents(), new_web_contents.get());

  // The tracker should now be observing the new WebContents.
  EXPECT_EQ(GetObservedWebContents(), new_web_contents.get());
}
