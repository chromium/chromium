// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_METRICS_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_METRICS_H_

#include "base/time/time.h"

namespace borealis {

extern const char kBorealisInstallNumAttemptsHistogram[];
extern const char kBorealisInstallResultHistogram[];
extern const char kBorealisInstallOverallTimeHistogram[];
extern const char kBorealisInstallRetriesHistogram[];
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
enum class BorealisInstallResult {
  kSuccess = 0,
  kCancelled = 1,
  kBorealisNotAllowed = 2,
  kBorealisInstallInProgress = 3,
  kDlcInternalError = 4,
  kDlcUnsupportedError = 5,
  kDlcBusyError = 6,
  kDlcNeedRebootError = 7,
  kDlcNeedSpaceError = 8,
  kDlcUnknownError = 9,
  kOffline = 10,
  kDlcNeedUpdateError = 11,
  kStartupFailed = 12,
  kMainAppNotPresent = 13,
  kMaxValue = kMainAppNotPresent,
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
  kMountFailed = 2,
  kDiskImageFailed = 3,
  kStartVmFailed = 4,
  kAwaitBorealisStartupFailed = 5,
  kSyncDiskFailed = 6,
  kRequestWaylandFailed = 7,
  kDisallowed = 8,
  kMaxValue = kDisallowed,
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
void RecordBorealisInstallResultHistogram(BorealisInstallResult install_result);
void RecordBorealisInstallOverallTimeHistogram(base::TimeDelta install_time);
void RecordBorealisInstallRetries(int retry_count);
void RecordBorealisUninstallNumAttemptsHistogram();
void RecordBorealisUninstallResultHistogram(
    BorealisUninstallResult uninstall_result);
void RecordBorealisStartupNumAttemptsHistogram();
void RecordBorealisStartupResultHistogram(BorealisStartupResult startup_result);
void RecordBorealisStartupOverallTimeHistogram(base::TimeDelta startup_time);
void RecordBorealisShutdownNumAttemptsHistogram();
void RecordBorealisShutdownResultHistogram(
    BorealisShutdownResult shutdown_result);

}  // namespace borealis

std::ostream& operator<<(std::ostream& stream,
                         borealis::BorealisStartupResult result);

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_METRICS_H_
