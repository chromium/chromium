// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_HISTOGRAM_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_HISTOGRAM_HELPER_H_

#include <string>

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"

namespace policy {

namespace dlp {

// Constants with UMA histogram name suffixes.
constexpr char kCaptureModeInitBlockedUMA[] = "CaptureModeInitBlocked";
constexpr char kClipboardReadBlockedUMA[] = "ClipboardReadBlocked";
constexpr char kDataTransferControllerStartedUMA[] =
    "DataTransferControllerStarted";
constexpr char kDlpPolicyPresentUMA[] = "DlpPolicyPresent";
constexpr char kDragDropBlockedUMA[] = "DragDropBlocked";
constexpr char kFilesDaemonStartedUMA[] = "FilesDaemonStarted";
constexpr char kPrintingBlockedUMA[] = "PrintingBlocked";
constexpr char kPrivacyScreenEnforcedUMA[] = "PrivacyScreenEnforced";
constexpr char kScreenShareBlockedUMA[] = "ScreenShareBlocked";
constexpr char kScreenSharePausedOrResumedUMA[] = "ScreenSharePausedOrResumed";
constexpr char kScreenshotBlockedUMA[] = "ScreenshotBlocked";
constexpr char kVideoCaptureInterruptedUMA[] = "VideoCaptureInterrupted";
constexpr char kVideoCaptureBlockedUMA[] = "VideoCaptureBlocked";

}  // namespace dlp

std::string GetDlpHistogramPrefix();

void DlpBooleanHistogram(const std::string& suffix, bool value);

void DlpRestrictionConfiguredHistogram(DlpRulesManager::Restriction value);

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_HISTOGRAM_HELPER_H_
