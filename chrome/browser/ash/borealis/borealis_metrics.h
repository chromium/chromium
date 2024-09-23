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
extern const char kBorealisInstallSourceHistogram[];
extern const char kBorealisInstallOverallTimeHistogram[];
extern const char kBorealisShutdownNumAttemptsHistogram[];
extern const char kBorealisShutdownResultHistogram[];
extern const char kBorealisStabilityHistogram[];
extern const char kBorealisStartupNumAttemptsHistogram[];
extern const char kBorealisStartupResultHistogram[];
extern const char kBorealisStartupOverallTimeHistogram[];
extern const char kBorealisUninstallNumAttemptsHistogram[];
extern const char kBorealisUninstallResultHistogram[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BorealisStartupResult {
  kSuccess = 0,
  kCancelled = 1,
  // kMountFailed = 2, // No longer used, expanded into kDlc*.
  kDiskImageFailed = 3,
  kStartVmFailed = 4,
  kAwaitBorealisStartupFailed = 5,
  kSyncDiskFailed = 6,
  kRequestWaylandFailed = 7,
  kDisallowed = 8,
  kDlcCancelled = 9,
  kDlcOffline = 10,
  kDlcNeedUpdateError = 11,
  kDlcNeedRebootError = 12,
  kDlcNeedSpaceError = 13,
  kDlcBusyError = 14,
  kDlcInternalError = 15,
  kDlcUnsupportedError = 16,
  kDlcUnknownError = 17,
  kConciergeUnavailable = 18,
  kEmptyDiskResponse = 19,
  kStartVmEmptyResponse = 20,
  // Remember to add new entries to histograms/enums.xml.
  kMaxValue = kStartVmEmptyResponse,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BorealisGetDiskInfoResult {
  kSuccess = 0,
  kAlreadyInProgress = 1,
  kFailedGettingExpandableSpace = 2,
  kConciergeFailed = 3,
  kInvalidRequest = 4,
  kMaxValue = kInvalidRequest,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BorealisResizeDiskResult {
  kSuccess = 0,
  kAlreadyInProgress = 1,
  kFailedToGetDiskInfo = 2,
  kInvalidDiskType = 3,
  kNotEnoughExpandableSpace = 4,
  kWouldNotLeaveEnoughSpace = 5,
  kViolatesMinimumSize = 6,
  kConciergeFailed = 7,
  kFailedGettingUpdate = 8,
  kInvalidRequest = 9,
  kOverflowError = 10,
  kFailedToFulfillRequest = 11,
  kMaxValue = kFailedToFulfillRequest,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BorealisSyncDiskSizeResult {
  kDiskNotFixed = 0,
  kNoActionNeeded = 1,
  kNotEnoughSpaceToExpand = 2,
  kResizedPartially = 3,
  kResizedSuccessfully = 4,
  kAlreadyInProgress = 5,
  kFailedToGetDiskInfo = 6,
  kResizeFailed = 7,
  kDiskSizeSmallerThanMin = 8,
  kMaxValue = kDiskSizeSmallerThanMin,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BorealisShutdownResult {
  kSuccess = 0,
  kInProgress = 1,
  kFailed = 2,
  kMaxValue = kFailed,
};

void RecordBorealisInstallNumAttemptsHistogram();
void RecordBorealisInstallResultHistogram(
    borealis::mojom::InstallResult install_result);
void RecordBorealisInstallSourceHistogram(BorealisLaunchSource install_source);
void RecordBorealisInstallOverallTimeHistogram(base::TimeDelta install_time);
void RecordBorealisLaunchSourceHistogram(BorealisLaunchSource launch_source);
void RecordBorealisUninstallNumAttemptsHistogram();
void RecordBorealisUninstallResultHistogram(
    BorealisUninstallResult uninstall_result);
void RecordBorealisStartupNumAttemptsHistogram();
void RecordBorealisStartupResultHistogram(BorealisStartupResult startup_result);
void RecordBorealisStartupOverallTimeHistogram(base::TimeDelta startup_time);
void RecordBorealisStartupTimeToFirstWindowHistogram(
    base::TimeDelta startup_time);
void RecordBorealisShutdownNumAttemptsHistogram();
void RecordBorealisShutdownResultHistogram(
    BorealisShutdownResult shutdown_result);

}  // namespace borealis

std::ostream& operator<<(std::ostream& stream,
                         borealis::BorealisStartupResult result);

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_METRICS_H_
