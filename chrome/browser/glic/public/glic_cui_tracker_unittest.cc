// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_cui_tracker.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_fre_impression_cui_tracker.h"
#include "chrome/browser/glic/public/glic_response_completion_cui_tracker.h"
#include "chrome/browser/glic/public/glic_submit_query_cui_tracker.h"
#include "chrome/browser/glic/public/glic_window_invocation_tracker.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class TestGlicCuiTracker : public GlicCuiTracker {
 public:
  TestGlicCuiTracker() : GlicCuiTracker("Glic.TestCui") {}
  ~TestGlicCuiTracker() override = default;
};

}  // namespace

class GlicCuiTrackerTest : public testing::Test {
 protected:
  base::HistogramTester histogram_tester_;
};

TEST_F(GlicCuiTrackerTest, SuccessRecordsOutcomeAndLatency) {
  {
    TestGlicCuiTracker tracker;
    tracker.Resolve(GlicCuiOutcome::kSuccess);
  }

  histogram_tester_.ExpectUniqueSample("Glic.TestCui.Outcome",
                                       GlicCuiOutcome::kSuccess, 1);
  histogram_tester_.ExpectTotalCount("Glic.TestCui.Latency", 1);
  histogram_tester_.ExpectTotalCount("Glic.TestCui.Latency.Success", 1);
}

TEST_F(GlicCuiTrackerTest, DestroyBeforeReadyRecordsUnknownCancel) {
  {
    TestGlicCuiTracker tracker;
  }

  histogram_tester_.ExpectUniqueSample("Glic.TestCui.Outcome",
                                       GlicCuiOutcome::kUnknownCancel, 1);
  histogram_tester_.ExpectTotalCount("Glic.TestCui.Latency", 0);
  histogram_tester_.ExpectTotalCount("Glic.TestCui.Latency.UnknownCancel", 0);
}

TEST_F(GlicCuiTrackerTest, CancelRecordsReason) {
  {
    TestGlicCuiTracker tracker;
    tracker.Resolve(GlicCuiOutcome::kAbandoned);
  }

  histogram_tester_.ExpectUniqueSample("Glic.TestCui.Outcome",
                                       GlicCuiOutcome::kAbandoned, 1);
  histogram_tester_.ExpectTotalCount("Glic.TestCui.Latency", 1);
  histogram_tester_.ExpectTotalCount("Glic.TestCui.Latency.Abandoned", 1);
}

TEST_F(GlicCuiTrackerTest, OnlyFirstResolutionIsRecorded) {
  {
    TestGlicCuiTracker tracker;
    tracker.Resolve(GlicCuiOutcome::kFailed);
    tracker.Resolve(GlicCuiOutcome::kSuccess);  // Should be ignored
  }

  histogram_tester_.ExpectUniqueSample("Glic.TestCui.Outcome",
                                       GlicCuiOutcome::kFailed, 1);
  histogram_tester_.ExpectTotalCount("Glic.TestCui.Latency", 1);
  histogram_tester_.ExpectTotalCount("Glic.TestCui.Latency.Failed", 1);
  histogram_tester_.ExpectTotalCount("Glic.TestCui.Latency.Success", 0);
}

TEST_F(GlicCuiTrackerTest, WindowInvocationTrackerUsesCorrectPrefix) {
  {
    GlicWindowInvocationTracker tracker;
    tracker.Resolve(GlicCuiOutcome::kSuccess);
  }

  histogram_tester_.ExpectUniqueSample(
      "Glic.CUI.WindowEntryPointInvocation.Outcome", GlicCuiOutcome::kSuccess,
      1);
  histogram_tester_.ExpectTotalCount(
      "Glic.CUI.WindowEntryPointInvocation.Latency", 1);
}

TEST_F(GlicCuiTrackerTest, SubmitQueryTrackerUsesCorrectPrefix) {
  {
    GlicSubmitQueryCuiTracker tracker;
    tracker.Resolve(GlicCuiOutcome::kSuccess);
  }

  histogram_tester_.ExpectUniqueSample("Glic.CUI.SubmitQuery.Outcome",
                                       GlicCuiOutcome::kSuccess, 1);
  histogram_tester_.ExpectTotalCount("Glic.CUI.SubmitQuery.Latency", 1);
}

TEST_F(GlicCuiTrackerTest, SubmitQueryTrackerResolvesOnEvents) {
  {
    GlicSubmitQueryCuiTracker tracker;
    tracker.OnEvent(GlicInstanceEvent::kResponseStarted);
  }

  histogram_tester_.ExpectUniqueSample("Glic.CUI.SubmitQuery.Outcome",
                                       GlicCuiOutcome::kSuccess, 1);
}

TEST_F(GlicCuiTrackerTest, FreImpressionTrackerResolvesOnOptInImpression) {
  {
    GlicFreImpressionCuiTracker tracker;
    EXPECT_FALSE(tracker.OnEvent(GlicInstanceEvent::kClientReady));
    EXPECT_TRUE(tracker.OnEvent(GlicInstanceEvent::kOptInImpression));
  }

  histogram_tester_.ExpectUniqueSample("Glic.CUI.FreImpression.Outcome",
                                       GlicCuiOutcome::kSuccess, 1);
  histogram_tester_.ExpectTotalCount("Glic.CUI.FreImpression.Latency", 1);
  histogram_tester_.ExpectTotalCount("Glic.CUI.FreImpression.Latency.Success",
                                     1);
}

TEST_F(GlicCuiTrackerTest,
       FreImpressionTrackerCancelsOnUserInputSubmittedForMessageFirstFre) {
  {
    GlicFreImpressionCuiTracker tracker;
    EXPECT_TRUE(tracker.OnEvent(GlicInstanceEvent::kUserInputSubmitted));
  }
  {
    GlicFreImpressionCuiTracker tracker;
    EXPECT_TRUE(tracker.OnEvent(GlicInstanceEvent::kInputFocused));
  }

  histogram_tester_.ExpectUniqueSample("Glic.CUI.FreImpression.Outcome",
                                       GlicCuiOutcome::kUnknownCancel, 2);
  histogram_tester_.ExpectTotalCount("Glic.CUI.FreImpression.Latency", 0);
}

TEST_F(GlicCuiTrackerTest, FreImpressionTrackerAbandonsOnCloseOrHide) {
  {
    GlicFreImpressionCuiTracker tracker;
    EXPECT_TRUE(tracker.OnEvent(GlicInstanceEvent::kClose));
  }

  histogram_tester_.ExpectUniqueSample("Glic.CUI.FreImpression.Outcome",
                                       GlicCuiOutcome::kAbandoned, 1);
  histogram_tester_.ExpectTotalCount("Glic.CUI.FreImpression.Latency.Abandoned",
                                     1);
}

TEST_F(GlicCuiTrackerTest, FreImpressionTrackerFailsOnWebUiError) {
  {
    GlicFreImpressionCuiTracker tracker;
    EXPECT_TRUE(tracker.OnEvent(GlicInstanceEvent::kWebUiStateError));
  }

  histogram_tester_.ExpectUniqueSample("Glic.CUI.FreImpression.Outcome",
                                       GlicCuiOutcome::kFailed, 1);
  histogram_tester_.ExpectTotalCount("Glic.CUI.FreImpression.Latency.Failed",
                                     1);
}

TEST_F(GlicCuiTrackerTest,
       ResponseCompletionTrackerResolvesOnResponseStoppedSuccess) {
  {
    GlicResponseCompletionCuiTracker tracker;
    EXPECT_FALSE(tracker.OnEvent(GlicInstanceEvent::kResponseStarted));
    EXPECT_TRUE(tracker.OnEvent(GlicInstanceEvent::kResponseStoppedSuccess));
  }

  histogram_tester_.ExpectUniqueSample("Glic.CUI.ResponseCompletion.Outcome",
                                       GlicCuiOutcome::kSuccess, 1);
  histogram_tester_.ExpectTotalCount("Glic.CUI.ResponseCompletion.Latency", 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.CUI.ResponseCompletion.Latency.Success", 1);
}

TEST_F(GlicCuiTrackerTest, ResponseCompletionTrackerAbandonsOnCloseOrHide) {
  {
    GlicResponseCompletionCuiTracker tracker;
    EXPECT_TRUE(tracker.OnEvent(GlicInstanceEvent::kClose));
  }

  histogram_tester_.ExpectUniqueSample("Glic.CUI.ResponseCompletion.Outcome",
                                       GlicCuiOutcome::kAbandoned, 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.CUI.ResponseCompletion.Latency.Abandoned", 1);
}

TEST_F(GlicCuiTrackerTest,
       ResponseCompletionTrackerAbandonsOnResponseStoppedByUser) {
  {
    GlicResponseCompletionCuiTracker tracker;
    EXPECT_TRUE(tracker.OnEvent(GlicInstanceEvent::kResponseStoppedByUser));
  }

  histogram_tester_.ExpectUniqueSample("Glic.CUI.ResponseCompletion.Outcome",
                                       GlicCuiOutcome::kAbandoned, 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.CUI.ResponseCompletion.Latency.Abandoned", 1);
}

TEST_F(GlicCuiTrackerTest, ResponseCompletionTrackerFailsOnWebUiError) {
  {
    GlicResponseCompletionCuiTracker tracker;
    EXPECT_TRUE(tracker.OnEvent(GlicInstanceEvent::kWebUiStateError));
  }

  histogram_tester_.ExpectUniqueSample("Glic.CUI.ResponseCompletion.Outcome",
                                       GlicCuiOutcome::kFailed, 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.CUI.ResponseCompletion.Latency.Failed", 1);
}

TEST_F(GlicCuiTrackerTest,
       ResponseCompletionTrackerFailsOnResponseStoppedOther) {
  {
    GlicResponseCompletionCuiTracker tracker;
    EXPECT_TRUE(tracker.OnEvent(GlicInstanceEvent::kResponseStoppedOther));
  }

  histogram_tester_.ExpectUniqueSample("Glic.CUI.ResponseCompletion.Outcome",
                                       GlicCuiOutcome::kFailed, 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.CUI.ResponseCompletion.Latency.Failed", 1);
}

}  // namespace glic
