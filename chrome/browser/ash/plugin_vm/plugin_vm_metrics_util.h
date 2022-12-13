// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_METRICS_UTIL_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_METRICS_UTIL_H_

#include "base/metrics/histogram_functions.h"

namespace plugin_vm {

extern const char kPluginVmLaunchResultHistogram[];
extern const char kPluginVmSetupResultHistogram[];
extern const char kPluginVmDlcUseResultHistogram[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PluginVmLaunchResult {
  kSuccess = 0,
  kError = 1,
  kInvalidLicense = 2,
  kVmMissing = 3,
  kExpiredLicense = 4,
  kNetworkError = 5,
  kInsufficientDiskSpace = 6,
  kStoppedWaitingForVmTools = 7,
  kMaxValue = kStoppedWaitingForVmTools,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PluginVmSetupResult {
  kSuccess = 0,
  // kPluginVmIsNotAllowed = 1,
  // kErrorDownloadingPluginVmImage = 2,
  // kErrorImportingPluginVmImage = 3,
  kUserCancelledDownloadingPluginVmImage = 4,
  kUserCancelledImportingPluginVmImage = 5,
  // kErrorDownloadingPluginVmDlc = 6,
  kUserCancelledDownloadingPluginVmDlc = 7,
  kVmAlreadyExists = 8,
  kUserCancelledCheckingForExistingVm = 9,
  // kErrorInsufficientDiskSpace = 10,
  // kUserCancelledLowDiskSpace = 11,
  kUserCancelledCheckingDiskSpace = 12,
  // Failure reasons are broken down in PluginVm.SetupFailureReason.
  kError = 13,
  kUserCancelledWithoutStarting = 14,
  kUserCancelledValidatingLicense = 15,
  kUserCancelledStartingDispatcher = 16,

  kMaxValue = kUserCancelledStartingDispatcher,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PluginVmDlcUseResult {
  kDlcSuccess = 0,
  kInvalidDlcError = 1,
  kInternalDlcError = 2,
  kBusyDlcError = 3,
  kNeedRebootDlcError = 4,
  kNeedSpaceDlcError = 5,
  kNoImageFoundDlcError = 6,
  kMaxValue = kNoImageFoundDlcError,
};

void RecordPluginVmLaunchResultHistogram(PluginVmLaunchResult launch_result);
void RecordPluginVmSetupResultHistogram(PluginVmSetupResult setup_result);
void RecordPluginVmDlcUseResultHistogram(PluginVmDlcUseResult dlc_use_result);
void RecordPluginVmSetupTimeHistogram(base::TimeDelta setup_time);

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_METRICS_UTIL_H_
