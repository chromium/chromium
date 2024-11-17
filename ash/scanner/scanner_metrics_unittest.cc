// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_metrics.h"

#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "scanner_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using enum ScannerFeatureUserState;

class ScannerMetricsTest
    : public AshTestBase,
      public testing::WithParamInterface<ScannerFeatureUserState> {};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ScannerMetricsTest,
    testing::ValuesIn<ScannerFeatureUserState>({
        kConsentDisclaimerAccepted,
        kConsentDisclaimerRejected,
        kSunfishScreenEnteredViaShortcut,
        kSunfishScreenInitialScreenCaptureSentToScannerServer,
        kScreenCaptureModeScannerButtonShown,
        kScreenCaptureModeInitialScreenCaptureSentToScannerServer,
        kNoActionsDetected,
        kNewCalendarEventActionDetected,
        kNewCalendarEventActionFinishedSuccessfully,
        kNewCalendarEventActionPopulationFailed,
        kNewContactActionDetected,
        kNewContactActionFinishedSuccessfully,
        kNewContactActionPopulationFailed,
        kNewGoogleSheetActionDetected,
        kNewGoogleSheetActionFinishedSuccessfully,
        kNewGoogleSheetActionPopulationFailed,
        kNewGoogleDocActionDetected,
        kNewGoogleDocActionFinishedSuccessfully,
        kNewGoogleDocActionPopulationFailed,
        kCopyToClipboardActionDetected,
        kCopyToClipboardActionFinishedSuccessfully,
        kCopyToClipboardActionPopulationFailed,
        kNewCalendarEventPopulatedActionExecutionFailed,
        kNewContactPopulatedActionExecutionFailed,
        kNewGoogleSheetPopulatedActionExecutionFailed,
        kNewGoogleDocPopulatedActionExecutionFailed,
        kCopyToClipboardPopulatedActionExecutionFailed,
    }));

TEST_P(ScannerMetricsTest, Record) {
  base::HistogramTester histogram_tester;

  RecordScannerFeatureUserState(GetParam());

  histogram_tester.ExpectBucketCount("Ash.ScannerFeature.UserState", GetParam(),
                                     1);
}

}  // namespace

}  // namespace ash
