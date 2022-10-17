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

extern const char kCrostiniEnabled[];
extern const char kCrostiniSharedUsbDevices[];
extern const char kCrostiniMicAllowed[];

extern const char kCrostiniCreateOptionsSharePathsKey[];
extern const char kCrostiniCreateOptionsContainerUsernameKey[];
extern const char kCrostiniCreateOptionsDiskSizeBytesKey[];
extern const char kCrostiniCreateOptionsImageServerUrlKey[];
extern const char kCrostiniCreateOptionsImageAliasKey[];
extern const char kCrostiniCreateOptionsAnsiblePlaybookKey[];
extern const char kCrostiniCreateOptionsUsedKey[];

extern const char kUserCrostiniAllowedByPolicy[];
extern const char kUserCrostiniExportImportUIAllowedByPolicy[];
extern const char kVmManagementCliAllowedByPolicy[];
extern const char kUserCrostiniRootAccessAllowedByPolicy[];
extern const char kCrostiniAnsiblePlaybookFilePath[];
extern const char kCrostiniDefaultContainerConfigured[];
extern const char kCrostiniArcAdbSideloadingUserPref[];
extern const char kCrostiniPortForwardingAllowedByPolicy[];
extern const char kTerminalSshAllowedByPolicy[];

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
