// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_METRICS_H_
#define ASH_SCANNER_SCANNER_METRICS_H_

#include <string_view>

#include "ash/ash_export.h"

namespace ash {

inline constexpr std::string_view kScannerFeatureTimerFetchActionsForImage =
    "Ash.ScannerFeature.Timer.FetchActionsForImage";

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
  kMaxValue = kCopyToClipboardPopulatedActionExecutionFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ash/enums.xml:ScannerFeatureUserState)

ASH_EXPORT void RecordScannerFeatureUserState(ScannerFeatureUserState state);

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_METRICS_H_
