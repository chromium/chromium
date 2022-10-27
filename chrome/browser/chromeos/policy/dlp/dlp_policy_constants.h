// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_POLICY_CONSTANTS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_POLICY_CONSTANTS_H_

namespace policy {

// The following const strings are used to parse the
// DataLeakPreventionRules policy pref value.
namespace dlp {

constexpr char kClipboardRestriction[] = "CLIPBOARD";
constexpr char kScreenshotRestriction[] = "SCREENSHOT";
constexpr char kPrintingRestriction[] = "PRINTING";
constexpr char kPrivacyScreenRestriction[] = "PRIVACY_SCREEN";
constexpr char kScreenShareRestriction[] = "SCREEN_SHARE";
constexpr char kFilesRestriction[] = "FILES";

constexpr char kArc[] = "ARC";
constexpr char kCrostini[] = "CROSTINI";
constexpr char kPluginVm[] = "PLUGIN_VM";
constexpr char kDrive[] = "DRIVE";
constexpr char kUsb[] = "USB";

constexpr char kAllowLevel[] = "ALLOW";
constexpr char kBlockLevel[] = "BLOCK";
constexpr char kWarnLevel[] = "WARN";
constexpr char kReportLevel[] = "REPORT";

// Link to the Help Center article about Data Leak Prevention.
constexpr char kDlpLearnMoreUrl[] =
    "https://support.google.com/chrome/a/?p=chromeos_datacontrols";

}  // namespace dlp

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_POLICY_CONSTANTS_H_
