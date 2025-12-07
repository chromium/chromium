// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_metrics_session_manager.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

namespace {
constexpr base::TimeDelta kStartTimeout = base::Seconds(30);
constexpr base::TimeDelta kInactivityTimeout = base::Minutes(45);
constexpr base::TimeDelta kHiddenTimeout = base::Minutes(30);
constexpr base::TimeDelta kDebounceTimeout = base::Seconds(5);
}  // namespace

class GlicMetricsSessionManagerTest : public testing::Test {
 public:
  GlicMetricsSessionManagerTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicMetricsSession,
        {{features::kGlicMetricsSessionStartTimeout.name,
          base::NumberToString(kStartTimeout.InSeconds()) + "s"},
         {features::kGlicMetricsSessionInactivityTimeout.name,
          base::NumberToString(kInactivityTimeout.InSeconds()) + "s"},
         {features::kGlicMetricsSessionHiddenTimeout.name,
          base::NumberToString(kHiddenTimeout.InSeconds()) + "s"},
         {features::kGlicMetricsSessionRestartDebounceTimer.name,
          base::NumberToString(kDebounceTimeout.InSeconds()) + "s"}});
  }

  void SetUp() override { metrics_ = std::make_unique<GlicInstanceMetrics>(); }

 protected:
  void StartSession() {
    metrics_->OnVisibilityChanged(true);
    metrics_->OnActivationChanged(true);
    task_environment_.FastForwardBy(kStartTimeout);
    EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.Start"),
              1);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  std::unique_ptr<GlicInstanceMetrics> metrics_;
};

TEST_F(GlicMetricsSessionManagerTest, StartsSessionAfterTimeout) {
  metrics_->OnVisibilityChanged(true);
  metrics_->OnActivationChanged(true);

  // Not started yet
  task_environment_.FastForwardBy(kStartTimeout - base::Seconds(1));
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.Start"),
            0);

  // Now it should start
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.Start"),
            1);
}

TEST_F(GlicMetricsSessionManagerTest, HidingCancelsStart) {
  metrics_->OnVisibilityChanged(true);
  task_environment_.FastForwardBy(kStartTimeout / 2);
  metrics_->OnVisibilityChanged(false);

  // Should not start even after original timeout would have passed
  task_environment_.FastForwardBy(kStartTimeout);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.Start"),
            0);
}

TEST_F(GlicMetricsSessionManagerTest, InactivityCancelsStart) {
  metrics_->OnVisibilityChanged(true);
  metrics_->OnActivationChanged(true);
  task_environment_.FastForwardBy(kStartTimeout / 2);
  metrics_->OnActivationChanged(false);

  // Should not start even after original timeout would have passed
  task_environment_.FastForwardBy(kStartTimeout);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.Start"),
            0);
}

TEST_F(GlicMetricsSessionManagerTest, EndsSessionOnInactivity) {
  StartSession();

  metrics_->OnActivationChanged(false);
  task_environment_.FastForwardBy(kInactivityTimeout);

  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.End"), 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Session.EndReason",
      GlicMultiInstanceSessionEndReason::kInactivity, 1);
}

TEST_F(GlicMetricsSessionManagerTest, EndsSessionOnHidden) {
  StartSession();

  metrics_->OnVisibilityChanged(false);
  task_environment_.FastForwardBy(kHiddenTimeout);

  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.End"), 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Session.EndReason",
      GlicMultiInstanceSessionEndReason::kHidden, 1);
}

TEST_F(GlicMetricsSessionManagerTest, InactivityEndsSessionWhileHidden) {
  StartSession();

  // Become inactive. Inactivity timer starts (45m).
  metrics_->OnActivationChanged(false);
  // Wait 20m. Inactivity has 45 - 20 = 25m left.
  task_environment_.FastForwardBy(base::Minutes(20));

  // Become hidden. Hidden timer starts (30m).
  metrics_->OnVisibilityChanged(false);

  // Wait 25m. Inactivity timer should fire.
  task_environment_.FastForwardBy(base::Minutes(25));

  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.End"), 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Session.EndReason",
      GlicMultiInstanceSessionEndReason::kInactivity, 1);
}

TEST_F(GlicMetricsSessionManagerTest, BriefInactivityDoesNotEndSession) {
  StartSession();

  // Become inactive for a while, but less than the timeout.
  metrics_->OnActivationChanged(false);
  task_environment_.FastForwardBy(kInactivityTimeout / 2);

  // Become active again. This should cancel the inactivity timer.
  metrics_->OnActivationChanged(true);

  // Advance time past when the original inactivity timer would have fired.
  task_environment_.FastForwardBy(kInactivityTimeout);

  // Session should STILL be active.
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.End"), 0);
}

TEST_F(GlicMetricsSessionManagerTest, BriefHiddenDoesNotEndSession) {
  StartSession();

  // Become hidden for a while, but less than the timeout.
  metrics_->OnVisibilityChanged(false);
  task_environment_.FastForwardBy(kHiddenTimeout / 2);

  // Become visible again. This should cancel the hidden timer.
  metrics_->OnVisibilityChanged(true);

  // Advance time past when the original hidden timer would have fired.
  task_environment_.FastForwardBy(kHiddenTimeout);

  // Session should STILL be active.
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.End"), 0);
}

TEST_F(GlicMetricsSessionManagerTest,
       BriefActivityDoesNotResetInactivityTimer) {
  StartSession();

  // Become inactive
  metrics_->OnActivationChanged(false);

  // Wait just before session would end.
  task_environment_.FastForwardBy(kInactivityTimeout - base::Minutes(1));

  // Become active again for a short flicker.
  metrics_->OnActivationChanged(true);
  task_environment_.FastForwardBy(kDebounceTimeout / 2);
  metrics_->OnActivationChanged(false);

  // Session should STILL be active
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.End"), 0);

  // ... but since the reactivation was only for a short period the original
  // session end timer should be uninterrupted.
  task_environment_.FastForwardBy(base::Minutes(1) - kDebounceTimeout / 2);

  // Session should end now.
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.End"), 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Session.EndReason",
      GlicMultiInstanceSessionEndReason::kInactivity, 1);
}

TEST_F(GlicMetricsSessionManagerTest, BriefVisibilityDoesNotResetHiddenTimer) {
  StartSession();

  // Become hidden
  metrics_->OnVisibilityChanged(false);

  // Wait just before session would end.
  task_environment_.FastForwardBy(kHiddenTimeout - base::Minutes(1));

  // Become visible again for a short flicker.
  metrics_->OnVisibilityChanged(true);
  task_environment_.FastForwardBy(kDebounceTimeout / 2);
  metrics_->OnVisibilityChanged(false);

  // Session should STILL be active
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.End"), 0);

  // ... but since the reactivation was only for a short period the original
  // session end timer should be uninterrupted.
  task_environment_.FastForwardBy(base::Minutes(1) - kDebounceTimeout / 2);

  // Session should end now.
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.End"), 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Session.EndReason",
      GlicMultiInstanceSessionEndReason::kHidden, 1);
}

TEST_F(GlicMetricsSessionManagerTest, OwnerDestructionEndsSession) {
  StartSession();

  metrics_.reset();

  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.End"), 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Session.EndReason",
      GlicMultiInstanceSessionEndReason::kOwnerDestroyed, 1);
}

TEST_F(GlicMetricsSessionManagerTest, StartsInactiveIfOnlyVisible) {
  metrics_->OnVisibilityChanged(true);
  // Activation never happens

  task_environment_.FastForwardBy(kStartTimeout);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.Start"),
            1);

  // Should immediately start inactivity timer since it started inactive
  task_environment_.FastForwardBy(kInactivityTimeout);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.End"), 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Session.EndReason",
      GlicMultiInstanceSessionEndReason::kInactivity, 1);
}

TEST_F(GlicMetricsSessionManagerTest, CapturesEventsWhilePending) {
  metrics_->OnVisibilityChanged(true);
  // Session is pending.
  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText);

  task_environment_.FastForwardBy(kStartTimeout);
  // Session started.

  metrics_->OnInstanceDestroyed();
  // Session ended.

  histogram_tester_.ExpectBucketCount("Glic.Instance.Session.InputModesUsed",
                                      InputModesUsed::kOnlyText, 1);
}

TEST_F(GlicMetricsSessionManagerTest,
       ReactivationStartsNewSessionWithoutVisibilityChange) {
  StartSession();

  // Become inactive and let session time out.
  metrics_->OnActivationChanged(false);
  task_environment_.FastForwardBy(kInactivityTimeout);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.End"), 1);

  // Instance is still visible. Reactivate it.
  metrics_->OnActivationChanged(true);

  // A new session should start after the start timeout.
  task_environment_.FastForwardBy(kStartTimeout);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.Start"),
            2);
}

TEST_F(GlicMetricsSessionManagerTest, RecordsTimeBetweenSessions) {
  StartSession();

  // End first session.
  metrics_->OnActivationChanged(false);
  task_environment_.FastForwardBy(kInactivityTimeout);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.End"), 1);

  // Wait some time between sessions.
  base::TimeDelta time_between = base::Hours(2);
  task_environment_.FastForwardBy(time_between);

  // Start second session.
  metrics_->OnActivationChanged(true);
  task_environment_.FastForwardBy(kStartTimeout);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.Start"),
            2);

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeBetweenSessions.24H", time_between, 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeBetweenSessions.7D", time_between, 1);
}

TEST_F(GlicMetricsSessionManagerTest, UserInputStartsSessionImmediately) {
  metrics_->OnVisibilityChanged(true);
  // Session is pending.
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.Start"),
            0);

  // User input should immediately start the session.
  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.Start"),
            1);

  // Fast forward past the original timeout to ensure no double-start.
  task_environment_.FastForwardBy(kStartTimeout);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.Start"),
            1);
}

TEST_F(GlicMetricsSessionManagerTest,
       NewPendingSessionNotAffectedByCancelledOne) {
  // 1. Start pending session 1
  metrics_->OnVisibilityChanged(true);
  task_environment_.FastForwardBy(kStartTimeout / 2);

  // 2. Cancel it
  metrics_->OnVisibilityChanged(false);

  // 3. Start pending session 2 immediately
  metrics_->OnVisibilityChanged(true);

  // 4. Wait for the REMAINDER of the first timeout.
  // If the first timer wasn't cancelled, it might fire now (incorrectly
  // starting session 2 early).
  task_environment_.FastForwardBy(kStartTimeout / 2 + base::Seconds(1));

  // Session 2 should NOT have started yet (it needs full kStartTimeout).
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.Start"),
            0);

  // 5. Wait for the rest of session 2's timeout.
  task_environment_.FastForwardBy(kStartTimeout / 2);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.Start"),
            1);
}

TEST_F(GlicMetricsSessionManagerTest, Session_HadEvent_RecordedOncePerSession) {
  StartSession();
  metrics_->OnCreateTab();
  metrics_->OnCreateTab();

  metrics_.reset();  // End session

  histogram_tester_.ExpectBucketCount("Glic.Instance.Session.HadEvent",
                                      GlicInstanceEvent::kCreateTab, 1);
}

TEST_F(GlicMetricsSessionManagerTest,
       Session_CreateTabCount_RecordedCorrectly) {
  StartSession();
  metrics_->OnCreateTab();
  metrics_->OnCreateTab();
  metrics_->OnCreateTab();

  metrics_.reset();  // End session

  histogram_tester_.ExpectUniqueSample("Glic.Instance.Session.CreateTabCount",
                                       3, 1);
}

TEST_F(GlicMetricsSessionManagerTest, Session_ToggleCount_RecordedCorrectly) {
  StartSession();
  metrics_->OnToggle(mojom::InvocationSource::kOsHotkey,
                     ShowOptions::ForFloating(gfx::Rect()), true);
  metrics_->OnToggle(mojom::InvocationSource::kOsButton,
                     ShowOptions::ForFloating(gfx::Rect()), true);

  metrics_.reset();  // End session

  histogram_tester_.ExpectUniqueSample("Glic.Instance.Session.ToggleCount", 2,
                                       1);
}

TEST_F(GlicMetricsSessionManagerTest, Events_NotRecordedBeforeSessionStarts) {
  metrics_->OnVisibilityChanged(true);
  // Session pending, not started.
  metrics_->OnToggle(mojom::InvocationSource::kOsHotkey,
                     ShowOptions::ForFloating(gfx::Rect()), true);
  metrics_->OnCreateTab();

  // End session before it starts (e.g. by hiding)
  metrics_->OnVisibilityChanged(false);
  task_environment_.FastForwardBy(
      kHiddenTimeout);  // Ensure it ends if it was pending

  // Verify it didn't start.
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Session.Start"),
            0);

  histogram_tester_.ExpectTotalCount("Glic.Instance.Session.ToggleCount", 0);
  histogram_tester_.ExpectTotalCount("Glic.Instance.Session.CreateTabCount", 0);
  histogram_tester_.ExpectTotalCount("Glic.Instance.Session.HadEvent", 0);
}

TEST_F(GlicMetricsSessionManagerTest,
       MultipleTabsPinned_NoResponse_RecordsFalse) {
  StartSession();
  metrics_->session_manager().SetPinnedTabCount(3);

  metrics_.reset();  // End session

  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Session.MultipleTabsPinnedInAnyTurn", false, 1);
  histogram_tester_.ExpectUniqueSample("Glic.Instance.Session.MaxPinnedTabs", 3,
                                       1);
}

TEST_F(GlicMetricsSessionManagerTest,
       SingleTabPinned_WithResponse_RecordsFalse) {
  StartSession();
  metrics_->session_manager().SetPinnedTabCount(1);
  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText);

  metrics_.reset();  // End session

  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Session.MultipleTabsPinnedInAnyTurn", false, 1);
  histogram_tester_.ExpectUniqueSample("Glic.Instance.Session.MaxPinnedTabs", 1,
                                       1);
}

TEST_F(GlicMetricsSessionManagerTest,
       MultipleResponses_OneWithMultipleTabs_RecordsTrue) {
  StartSession();
  metrics_->session_manager().SetPinnedTabCount(1);
  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_->session_manager().SetPinnedTabCount(3);
  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_->session_manager().SetPinnedTabCount(1);
  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText);

  metrics_.reset();  // End session

  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Session.MultipleTabsPinnedInAnyTurn", true, 1);
  histogram_tester_.ExpectUniqueSample("Glic.Instance.Session.MaxPinnedTabs", 3,
                                       1);
}

TEST_F(GlicMetricsSessionManagerTest, RecordsSessionDurations) {
  StartSession();
  // Session has been active and visible for kStartTimeout (30s).

  // Stay in this state for another 10 minutes.
  task_environment_.FastForwardBy(base::Minutes(10));
  // Total active/visible so far: 10m 30s.

  // Become inactive (but still visible) for 5 minutes.
  metrics_->OnActivationChanged(false);
  task_environment_.FastForwardBy(base::Minutes(5));

  // Become active again for 15 minutes.
  metrics_->OnActivationChanged(true);
  task_environment_.FastForwardBy(base::Minutes(15));

  // Become hidden (and thus inactive) for 2 minutes.
  metrics_->OnActivationChanged(false);
  metrics_->OnVisibilityChanged(false);
  task_environment_.FastForwardBy(base::Minutes(2));

  // End the session.
  metrics_.reset();

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.Session.Duration",
      kStartTimeout + base::Minutes(10 + 5 + 15 + 2), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.Session.TotalActiveDuration",
      kStartTimeout + base::Minutes(10 + 15), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.Session.TotalInactiveDuration", base::Minutes(5 + 2), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.Session.TotalVisibleDuration",
      kStartTimeout + base::Minutes(10 + 5 + 15), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.Session.TotalHiddenDuration", base::Minutes(2), 1);

  // Uninterrupted Active Durations
  histogram_tester_.ExpectTimeBucketCount(
      "Glic.Instance.Session.UninterruptedActiveDuration",
      kStartTimeout + base::Minutes(10), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "Glic.Instance.Session.UninterruptedActiveDuration", base::Minutes(15),
      1);

  // Uninterrupted Visible Duration
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.Session.UninterruptedVisibleDuration",
      kStartTimeout + base::Minutes(10 + 5 + 15), 1);
}

}  // namespace glic
