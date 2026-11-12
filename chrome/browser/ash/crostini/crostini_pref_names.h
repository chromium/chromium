// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_PREF_NAMES_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_PREF_NAMES_H_

class PrefRegistrySimple;

namespace crostini {

// Enum that specifies allowance modes for the adb sideloading user policy
enum class CrostiniArcAdbSideloadingUserAllowanceMode {
  kDisallow = 0,
  kAllow = 1,
};

namespace prefs {

// A boolean preference representing whether a user has opted in to use
// Crostini (Called "Linux Apps" in UI).
inline constexpr char kCrostiniEnabled[] = "crostini.enabled";
extern const char kCrostiniSharedUsbDevices[];
extern const char kCrostiniMicAllowed[];

extern const char kCrostiniCreateOptionsSharePathsKey[];
extern const char kCrostiniCreateOptionsContainerUsernameKey[];
extern const char kCrostiniCreateOptionsDiskSizeBytesKey[];
extern const char kCrostiniCreateOptionsImageServerUrlKey[];
extern const char kCrostiniCreateOptionsImageAliasKey[];
extern const char kCrostiniCreateOptionsUsedKey[];

extern const char kUserCrostiniAllowedByPolicy[];
extern const char kUserCrostiniExportImportUIAllowedByPolicy[];
extern const char kVmManagementCliAllowedByPolicy[];
extern const char kUserCrostiniRootAccessAllowedByPolicy[];
extern const char kCrostiniAnsiblePlaybookFilePath[];
extern const char kCrostiniDefaultContainerConfigured[];
extern const char kCrostiniArcAdbSideloadingUserPref[];
extern const char kCrostiniPortForwardingAllowedByPolicy[];
// A boolean preference representing a user level enterprise policy to allow
// SSH in Terminal System App.
inline constexpr char kTerminalSshAllowedByPolicy[] =
    "crostini.terminal_ssh_allowed_by_policy";

extern const char kReportCrostiniUsageEnabled[];
extern const char kCrostiniLastLaunchTerminaComponentVersion[];
extern const char kCrostiniLastLaunchTerminaKernelVersion[];
extern const char kCrostiniLastLaunchTimeWindowStart[];
extern const char kCrostiniLastDiskSize[];
extern const char kCrostiniPortForwarding[];

extern const char kEngagementPrefsPrefix[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_PREF_NAMES_H_
