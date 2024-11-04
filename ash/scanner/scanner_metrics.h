// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_METRICS_H_
#define ASH_SCANNER_SCANNER_METRICS_H_

#include "ash/ash_export.h"

namespace ash {

// Enum for histogram. Stores what state the user is in.
// LINT.IfChange(ScannerFeatureUserState)
enum class ScannerFeatureUserState {
  kConsentDisclaimerAccepted,
  kConsentDisclaimerRejected,
  kSunfishScreenEnteredViaShortcut,
  kSunfishScreenInitialScreenCaptureSentToScannerServer,
  kScreenCaptureModeScannerButtonShown,
  kScreenCaptureModeInitialScreenCaptureSentToScannerServer,
  kNoActionsDetected,
  kNewCalendarEventActionDetected,
  kNewCalendarEventActionFinishedSuccessfully,
  kNewCalendarEventActionFailed,
  kNewContactActionDetected,
  kNewContactActionFinishedSuccessfully,
  kNewContactActionFailed,
  kNewGoogleSheetActionDetected,
  kNewGoogleSheetActionFinishedSuccessfully,
  kNewGoogleSheetActionFailed,
  kNewGoogleDocActionDetected,
  kNewGoogleDocActionFinishedSuccessfully,
  kNewGoogleDocActionFailed,
  kCopyToClipboardActionDetected,
  kCopyToClipboardActionFinishedSuccessfully,
  kCopyToClipboardActionFailed,
  kMaxValue = kCopyToClipboardActionFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ash/enums.xml:ScannerFeatureUserState)

ASH_EXPORT void RecordScannerFeatureUserState(ScannerFeatureUserState state);

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_METRICS_H_
