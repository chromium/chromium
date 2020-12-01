// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/kaleidoscope/kaleidoscope_metrics_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/media/kaleidoscope/mojom/kaleidoscope.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

using FirstRunProgress = KaleidoscopeMetricsRecorder::FirstRunProgress;

class KaleidoscopeMetricsRecorderTest : public testing::Test {
 public:
  void ExpectFirstRunProgressCount(FirstRunProgress progress,
                                   int expected_count) {
    histogram_tester_.ExpectBucketCount("Media.Kaleidoscope.FirstRunProgress",
                                        progress, expected_count);
  }

  void ExpectFirstRunProgressTotalCount(int expected_count) {
    histogram_tester_.ExpectTotalCount("Media.Kaleidoscope.FirstRunProgress",
                                       expected_count);
  }

  KaleidoscopeMetricsRecorder& recorder() { return recorder_; }

 private:
  KaleidoscopeMetricsRecorder recorder_;
  base::HistogramTester histogram_tester_;
};

TEST_F(KaleidoscopeMetricsRecorderTest, OnCompletedRecordsCompleted) {
  ExpectFirstRunProgressCount(FirstRunProgress::kCompleted, 0);
  recorder().OnFirstRunExperienceStepChanged(
      media::mojom::KaleidoscopeFirstRunExperienceStep::kCompleted);
  ExpectFirstRunProgressCount(FirstRunProgress::kCompleted, 1);
  ExpectFirstRunProgressTotalCount(1);
}

TEST_F(KaleidoscopeMetricsRecorderTest, OnExitWithNoStepsRecordsNothing) {
  ExpectFirstRunProgressTotalCount(0);
  recorder().OnExitPage();
  ExpectFirstRunProgressTotalCount(0);
}

TEST_F(KaleidoscopeMetricsRecorderTest, OnExitRecordsCurrentStep) {
  ExpectFirstRunProgressTotalCount(0);
  recorder().OnFirstRunExperienceStepChanged(
      media::mojom::KaleidoscopeFirstRunExperienceStep::kProviderSelection);
  ExpectFirstRunProgressTotalCount(0);
  recorder().OnFirstRunExperienceStepChanged(
      media::mojom::KaleidoscopeFirstRunExperienceStep::kWelcome);
  ExpectFirstRunProgressTotalCount(0);
  recorder().OnFirstRunExperienceStepChanged(
      media::mojom::KaleidoscopeFirstRunExperienceStep::kMediaFeedsConsent);
  ExpectFirstRunProgressTotalCount(0);
  recorder().OnExitPage();
  ExpectFirstRunProgressCount(FirstRunProgress::kMediaFeedsConsent, 1);
  ExpectFirstRunProgressTotalCount(1);
}

TEST_F(KaleidoscopeMetricsRecorderTest,
       OnExitAfterCompletedDoesNotRecordAnythingElse) {
  // Go through the steps and then complete.
  ExpectFirstRunProgressTotalCount(0);
  recorder().OnFirstRunExperienceStepChanged(
      media::mojom::KaleidoscopeFirstRunExperienceStep::kProviderSelection);
  ExpectFirstRunProgressTotalCount(0);
  recorder().OnFirstRunExperienceStepChanged(
      media::mojom::KaleidoscopeFirstRunExperienceStep::kMediaFeedsConsent);
  ExpectFirstRunProgressTotalCount(0);
  recorder().OnFirstRunExperienceStepChanged(
      media::mojom::KaleidoscopeFirstRunExperienceStep::kCompleted);

  // Completed should be recorded.
  ExpectFirstRunProgressCount(FirstRunProgress::kCompleted, 1);
  ExpectFirstRunProgressTotalCount(1);

  // OnExitPage should not record any new data.
  recorder().OnExitPage();
  ExpectFirstRunProgressTotalCount(1);
}
