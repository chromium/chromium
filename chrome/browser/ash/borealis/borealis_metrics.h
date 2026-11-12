// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_METRICS_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_METRICS_H_

#include "base/time/time.h"
#include "chrome/browser/ash/borealis/borealis_types.mojom-forward.h"

namespace borealis {

extern const char kBorealisInstallNumAttemptsHistogram[];
extern const char kBorealisInstallResultHistogram[];
extern const char kBorealisInstallOverallTimeHistogram[];
extern const char kBorealisStabilityHistogram[];
extern const char kBorealisUninstallNumAttemptsHistogram[];
extern const char kBorealisUninstallResultHistogram[];

enum class BorealisLaunchSource {
  kUnknown = 0,
  kInstallUrl = 1,
  kUnifiedAppInstaller = 2,
  kSteamInstallerApp = 3,
  kInsertCoin = 4,
  kAppUninstaller = 5,
  kAppUrlHandler = 6,
  kErrorDialogRetryButton = 7,
  kPostInstallLaunch = 8,
  kMaxValue = kPostInstallLaunch,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BorealisUninstallResult {
  kSuccess = 0,
  kAlreadyInProgress = 1,
  kShutdownFailed = 2,
  kRemoveDiskFailed = 3,
  kRemoveDlcFailed = 4,
  kMaxValue = kRemoveDlcFailed,
};

void RecordBorealisInstallNumAttemptsHistogram();
void RecordBorealisInstallResultHistogram(
    borealis::mojom::InstallResult install_result);
void RecordBorealisInstallOverallTimeHistogram(base::TimeDelta install_time);
void RecordBorealisUninstallNumAttemptsHistogram();
void RecordBorealisUninstallResultHistogram(
    BorealisUninstallResult uninstall_result);

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_METRICS_H_
