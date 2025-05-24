// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_METRICS_H_
#define ASH_SCANNER_SCANNER_METRICS_H_

#include <string_view>

#include "ash/ash_export.h"
#include "base/time/time.h"

namespace ash {

inline constexpr std::string_view
    kScannerFeatureTimerExecutePopulatedNewCalendarEventAction =
        "Ash.ScannerFeature.Timer.ExecutePopulatedNewCalendarEventAction";

inline constexpr std::string_view
    kScannerFeatureTimerExecutePopulatedNewContactAction =
        "Ash.ScannerFeature.Timer.ExecutePopulatedNewContactAction";

inline constexpr std::string_view
    kScannerFeatureTimerExecutePopulatedNewGoogleSheetAction =
        "Ash.ScannerFeature.Timer.ExecutePopulatedNewGoogleSheetAction";

inline constexpr std::string_view
    kScannerFeatureTimerExecutePopulatedNewGoogleDocAction =
        "Ash.ScannerFeature.Timer.ExecutePopulatedNewGoogleDocAction";

inline constexpr std::string_view
    kScannerFeatureTimerExecutePopulatedNewCopyToClipboardAction =
        "Ash.ScannerFeature.Timer.ExecutePopulatedNewCopyToClipboardAction";

inline constexpr std::string_view kScannerFeatureTimerFetchActionsForImage =
    "Ash.ScannerFeature.Timer.FetchActionsForImage";

inline constexpr std::string_view
    kScannerFeatureTimerPopulateNewCalendarEventAction =
        "Ash.ScannerFeature.Timer.PopulateNewCalendarEventAction";

inline constexpr std::string_view kScannerFeatureTimerPopulateNewContactAction =
    "Ash.ScannerFeature.Timer.PopulateNewContactAction";

inline constexpr std::string_view
    kScannerFeatureTimerPopulateNewGoogleSheetAction =
        "Ash.ScannerFeature.Timer.PopulateNewGoogleSheetAction";

inline constexpr std::string_view
    kScannerFeatureTimerPopulateNewGoogleDocAction =
        "Ash.ScannerFeature.Timer.PopulateNewGoogleDocAction";

inline constexpr std::string_view
    kScannerFeatureTimerPopulateNewCopyToClipboardAction =
        "Ash.ScannerFeature.Timer.PopulateNewCopyToClipboardAction";

// Enum for histogram. Stores what state the user is in.
// LINT.IfChange(ScannerFeatureUserState)
enum class ScannerFeatureUserState {
  kConsentDisclaimerAccepted,
  kConsentDisclaimerRejected,
  kSunfishScreenEnteredViaShortcut,
  kDeprecatedSunfishScreenInitialScreenCaptureSentToScannerServer,
  kScreenCaptureModeScannerButtonShown,
  kDeprecatedScreenCaptureModeInitialScreenCaptureSentToScannerServer,
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

  kCanShowUiReturnedFalse = 27,
  kCanShowUiReturnedTrueWithoutConsent = 28,
  kCanShowUiReturnedTrueWithConsent = 29,

  kCanShowUiReturnedFalseDueToNoShellInstance = 30,
  kCanShowUiReturnedFalseDueToNoControllerOnShell = 31,
  kCanShowUiReturnedFalseDueToEnterprisePolicy = 32,
  kCanShowUiReturnedFalseDueToNoProfileScopedDelegate = 33,
  kCanShowUiReturnedFalseDueToSettingsToggle = 34,
  kCanShowUiReturnedFalseDueToFeatureFlag = 35,
  kCanShowUiReturnedFalseDueToFeatureManagement = 36,
  kCanShowUiReturnedFalseDueToSecretKey = 37,
  kCanShowUiReturnedFalseDueToAccountCapabilities = 38,
  kCanShowUiReturnedFalseDueToCountry = 39,
  kCanShowUiReturnedFalseDueToKioskMode = 40,

  kLauncherShownWithoutSunfishSessionButton = 41,
  kLauncherShownWithSunfishSessionButton = 42,

  kSunfishSessionImageCapturedAndActionsNotFetched = 43,
  kSunfishSessionImageCapturedAndActionsFetchStarted = 44,
  kSmartActionsButtonImageCapturedAndActionsNotFetched = 45,
  kSmartActionsButtonImageCapturedAndActionsFetchStarted = 46,

  kSmartActionsButtonNotShownDueToFeatureChecks = 47,
  kSmartActionsButtonNotShownDueToTextDetectionCancelled = 48,
  kSmartActionsButtonNotShownDueToNoTextDetected = 49,
  kSmartActionsButtonNotShownDueToCanShowUiFalse = 50,
  kSmartActionsButtonNotShownDueToOffline = 51,

  kSunfishSessionStartedFromDebugShortcut = 52,
  kSunfishSessionStartedFromLauncherButton = 53,
  kSunfishSessionStartedFromHomeButtonLongPress = 54,

  kFeedbackFormOpened = 55,
  kFeedbackSent = 56,

  // These enum values should semantically be placed in a group above:
  // Should be placed after `NoControllerOnShell` and before `EnterprisePolicy`.
  kCanShowUiReturnedFalseDueToPinnedMode = 57,
  kSunfishSessionStartedFromKeyboardShortcut = 58,

  kMaxValue = kSunfishSessionStartedFromKeyboardShortcut,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ash/enums.xml:ScannerFeatureUserState)

ASH_EXPORT void RecordScannerFeatureUserState(ScannerFeatureUserState state);

ASH_EXPORT void RecordOnDeviceOcrTimerCompleted(
    base::TimeTicks ocr_attempt_start_time);

ASH_EXPORT void RecordSunfishSessionButtonVisibilityOnLauncherShown(
    bool is_visible);

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_METRICS_H_
