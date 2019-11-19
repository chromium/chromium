// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PREF_NAMES_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PREF_NAMES_H_

class PrefRegistrySimple;

namespace crostini {
namespace prefs {

extern const char kCrostiniEnabled[];
extern const char kCrostiniMimeTypes[];
extern const char kCrostiniRegistry[];
extern const char kCrostiniSharedUsbDevices[];
extern const char kCrostiniContainers[];
extern const char kCrostiniTerminalSettings[];
extern const char kVmKey[];
extern const char kContainerKey[];

extern const char kUserCrostiniAllowedByPolicy[];
extern const char kUserCrostiniExportImportUIAllowedByPolicy[];
extern const char kVmManagementCliAllowedByPolicy[];
extern const char kUserCrostiniRootAccessAllowedByPolicy[];
extern const char kCrostiniAnsiblePlaybookFilePath[];

extern const char kReportCrostiniUsageEnabled[];
extern const char kCrostiniLastLaunchTerminaComponentVersion[];
extern const char kCrostiniLastLaunchTerminaKernelVersion[];
extern const char kCrostiniLastLaunchTimeWindowStart[];
extern const char kCrostiniLastDiskSize[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PREF_NAMES_H_
