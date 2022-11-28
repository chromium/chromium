// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_HISTOGRAM_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_HISTOGRAM_HELPER_H_

#include <string>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"

namespace policy {

namespace dlp {

// Constants with UMA histogram name suffixes.
constexpr char kCaptureModeInitBlockedUMA[] = "CaptureModeInitBlocked";
constexpr char kCaptureModeInitWarnedUMA[] = "CaptureModeInitWarned";
constexpr char kClipboardReadBlockedUMA[] = "ClipboardReadBlocked";
constexpr char kDataTransferReportingTimeDiffUMA[] =
    "DataTransferReportingTimeDiff";
constexpr char kDataTransferControllerStartedUMA[] =
    "DataTransferControllerStarted";
constexpr char kDlpPolicyPresentUMA[] = "DlpPolicyPresent";
constexpr char kDragDropBlockedUMA[] = "DragDropBlocked";
constexpr char kFilesDaemonStartedUMA[] = "FilesDaemonStarted";
constexpr char kFileActionBlockedUMA[] = "FileActionBlocked";
constexpr char kFileActionWarnedUMA[] = "FileActionWarned";
constexpr char kFileActionWarnProceededUMA[] = "FileActionWarnProceeded";
constexpr char kPrintingBlockedUMA[] = "PrintingBlocked";
constexpr char kPrintingWarnedUMA[] = "PrintingWarned";
constexpr char kPrintingWarnProceededUMA[] = "PrintingWarnProceeded";
constexpr char kPrintingWarnSilentProceededUMA[] =
    "PrintingWarnSilentProceeded";
constexpr char kPrivacyScreenEnforcedUMA[] = "PrivacyScreenEnforced";
constexpr char kScreenShareBlockedUMA[] = "ScreenShareBlocked";
constexpr char kScreenShareWarnedUMA[] = "ScreenShareWarned";
constexpr char kScreenShareWarnProceededUMA[] = "ScreenShareWarnProceeded";
constexpr char kScreenShareWarnSilentProceededUMA[] =
    "ScreenShareWarnSilentProceeded";
constexpr char kScreenSharePausedOrResumedUMA[] = "ScreenSharePausedOrResumed";
constexpr char kScreenshotBlockedUMA[] = "ScreenshotBlocked";
constexpr char kScreenshotWarnedUMA[] = "ScreenshotWarned";
constexpr char kScreenshotWarnProceededUMA[] = "ScreenshotWarnProceeded";
constexpr char kScreenshotWarnSilentProceededUMA[] =
    "ScreenshotWarnSilentProceeded";
constexpr char kVideoCaptureInterruptedUMA[] = "VideoCaptureInterrupted";
constexpr char kReportedBlockLevelRestriction[] =
    "ReportedBlockLevelRestriction";
constexpr char kReportedReportLevelRestriction[] =
    "ReportedReportLevelRestriction";
constexpr char kReportedWarnLevelRestriction[] = "ReportedWarnLevelRestriction";
constexpr char kReportedWarnProceedLevelRestriction[] =
    "ReportedWarnProceedLevelRestriction";
constexpr char kReportedEventStatus[] = "ReportedEventStatus";
constexpr char kConfidentialContentsCount[] = "ConfidentialContentsCount";
constexpr char kActiveFileEventsCount[] = "ActiveFileEventsCount";
constexpr char kErrorsReportQueueNotReady[] = "Errors.ReportQueueNotReady";
constexpr char kErrorsFilesPolicySetup[] = "Errors.FilesPolicySetup";

}  // namespace dlp

std::string GetDlpHistogramPrefix();

void DlpBooleanHistogram(const std::string& suffix, bool value);

void DlpCountHistogram(const std::string& suffix, int sample, int max);

void DlpRestrictionConfiguredHistogram(DlpRulesManager::Restriction value);

template <typename T>
void DlpHistogramEnumeration(const std::string& suffix, T sample) {
  base::UmaHistogramEnumeration(GetDlpHistogramPrefix() + suffix, sample);
}

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_HISTOGRAM_HELPER_H_
