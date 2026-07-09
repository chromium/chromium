// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_cui_tracker.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_window_invocation_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class TestGlicCuiTracker : public GlicCuiTracker {
 public:
  TestGlicCuiTracker() = default;
  ~TestGlicCuiTracker() override {
    if (!IsResolved()) {
      Resolve(GlicCuiOutcome::kUnknownCancel);
    }
  }

 protected:
  const char* GetMetricName() const override { return "Glic.TestCui"; }
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

}  // namespace glic
