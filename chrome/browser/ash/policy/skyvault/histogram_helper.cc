// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/histogram_helper.h"

#include <string>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/policy/skyvault/local_files_migration_constants.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"

namespace policy::local_user_files {
namespace {

constexpr char kSkyVaultUMAPrefix[] = "Enterprise.SkyVault.";

// Suffixes for upload flow histograms.
constexpr char kDeleteErrorSuffix[] = "DeleteError";
constexpr char kOneDriveSignInErrorSuffix[] = "SignInError";

// Suffixes for local storage histograms.
constexpr char kLocalStorageEnabledSuffix[] = "LocalStorage.Enabled";
constexpr char kLocalStorageMisconfiguredSuffix[] =
    "LocalStorage.Misconfigured";

// Suffixes for migration flow histograms.
constexpr char kMigrationEnabledSuffix[] = "Enabled";
constexpr char kMigrationMisconfiguredSuffix[] = "Misconfigured";
constexpr char kMigrationResetSuffix[] = "Reset";
constexpr char kMigrationRetrySuffix[] = "Retry";
constexpr char kMigrationStoppedSuffix[] = "Stopped";
constexpr char kMigrationStateErrorContextSuffix[] = "StateErrorContext";
constexpr char kMigrationWrongStateSuffix[] = "WrongState";
constexpr char kMigrationFailedSuffix[] = "Failed";
constexpr char kMigrationSuccessDurationSuffix[] = "SuccessDuration";
constexpr char kMigrationFailureDurationSuffix[] = "FailureDuration";
constexpr char kMigrationWriteAccessErrorSuffix[] = "WriteAccessError";
constexpr char kMigrationUploadErrorSuffix[] = "UploadError";
constexpr char kMigrationWaitForConnectionSuffix[] = "WaitForConnection";
constexpr char kMigrationReconnectionDurationSuffix[] = "ReconnectionDuration";
constexpr char kMigrationCleanupErrorSuffix[] = "CleanupError";
constexpr char kMigrationDialogActionSuffix[] = "DialogAction";
constexpr char kMigrationDialogShownSuffix[] = "DialogShown";
constexpr char kScheduledTimeInPastInformUser[] =
    "ScheduledTimeInPast.InformUser";
constexpr char kScheduledTimeInPastScheduleMigration[] =
    "ScheduledTimeInPast.ScheduleMigration";

// Constants for cloud providers used in histogram names.
constexpr char kGoogleDriveProvider[] = "GoogleDrive";
constexpr char kOneDriveProvider[] = "OneDrive";
constexpr char kDelete[] = "Delete";

// Constants for upload triggers used in histogram names.
constexpr char kDownloadTrigger[] = "Download";
constexpr char kScreenCaptureTrigger[] = "ScreenCapture";
constexpr char kMigrationTrigger[] = "Migration";

// Min, max, and bucket count for migration duration histograms.
constexpr base::TimeDelta kMigrationDurationMin = base::Milliseconds(1);
constexpr base::TimeDelta kMigrationDurationMax = base::Hours(36);
// Number of buckets calculated to have a bucket size of 5 minutes:
// (kMax in h * 60 min/h) / 5 min/bucket = 36 * 60 / 5 = 432 buckets
constexpr int kMigrationDurationBuckets = 432;

// Min, max, and bucket count for reconnectivity waiting time histograms.
constexpr base::TimeDelta kReconnectionDurationMin = base::Milliseconds(1);
constexpr base::TimeDelta kReconnectionDurationMax = base::Hours(4);
// Number of buckets calculated to have a bucket size of 1 minute:
// (kMax in h * 60 min/h) / 1 min/bucket = 4 * 60 = 240 buckets
constexpr int kReconnectionDurationBuckets = 240;

// Converts `destination` to a string representation used to form a metric name.
std::string GetUMAMigrationDestination(MigrationDestination destination) {
  switch (destination) {
    case MigrationDestination::kNotSpecified:
      NOTREACHED();
    case MigrationDestination::kGoogleDrive:
      return kGoogleDriveProvider;
    case MigrationDestination::kOneDrive:
      return kOneDriveProvider;
    case MigrationDestination::kDelete:
      return kDelete;
  }
}

// Converts `trigger` to a string representation used to form a metric name.
std::string GetUMAAction(UploadTrigger trigger) {
  std::string action;
  switch (trigger) {
    case UploadTrigger::kDownload:
      return kDownloadTrigger;
    case UploadTrigger::kScreenCapture:
      return kScreenCaptureTrigger;
    case UploadTrigger::kMigration:
      return kMigrationTrigger;
  }
}

std::string GetHistogramName(
    const std::string& suffix,
    std::optional<UploadTrigger> trigger = std::nullopt,
    std::optional<MigrationDestination> destination = std::nullopt) {
  std::vector<std::string> parts = {kSkyVaultUMAPrefix};
  if (trigger.has_value()) {
    parts.push_back(GetUMAAction(trigger.value()));
    parts.push_back(".");
  }
  if (destination.has_value()) {
    parts.push_back(GetUMAMigrationDestination(destination.value()));
    parts.push_back(".");
  }
  parts.push_back(suffix);
  return base::StrCat(parts);
}

}  // namespace

void SkyVaultDeleteErrorHistogram(UploadTrigger trigger,
                                  MigrationDestination destination,
                                  bool value) {
  base::UmaHistogramBoolean(
      GetHistogramName(kDeleteErrorSuffix, trigger, destination), value);
}

void SkyVaultOneDriveSignInErrorHistogram(UploadTrigger trigger, bool value) {
  base::UmaHistogramBoolean(
      GetHistogramName(kOneDriveSignInErrorSuffix, trigger,
                       MigrationDestination::kOneDrive),
      value);
}

void SkyVaultLocalStorageEnabledHistogram(bool value) {
  base::UmaHistogramBoolean(GetHistogramName(kLocalStorageEnabledSuffix),
                            value);
}

void SkyVaultLocalStorageMisconfiguredHistogram(bool value) {
  base::UmaHistogramBoolean(GetHistogramName(kLocalStorageMisconfiguredSuffix),
                            value);
}

void SkyVaultMigrationEnabledHistogram(MigrationDestination destination,
                                       bool value) {
  base::UmaHistogramBoolean(
      GetHistogramName(kMigrationEnabledSuffix, UploadTrigger::kMigration,
                       destination),
      value);
}

void SkyVaultMigrationMisconfiguredHistogram(MigrationDestination destination,
                                             bool value) {
  base::UmaHistogramBoolean(
      GetHistogramName(kMigrationMisconfiguredSuffix, UploadTrigger::kMigration,
                       destination),
      value);
}

void SkyVaultMigrationResetHistogram(bool value) {
  base::UmaHistogramBoolean(
      GetHistogramName(kMigrationResetSuffix, UploadTrigger::kMigration),
      value);
}

void SkyVaultMigrationRetryHistogram(int count) {
  base::UmaHistogramCustomCounts(
      GetHistogramName(kMigrationRetrySuffix, UploadTrigger::kMigration), count,
      1, kMaxRetryCount, kMaxRetryCount);
}

void SkyVaultDeletionRetryHistogram(int count) {
  base::UmaHistogramCustomCounts(
      GetHistogramName(kMigrationRetrySuffix, UploadTrigger::kMigration,
                       MigrationDestination::kDelete),
      count, 1, kMaxRetryCount, kMaxRetryCount);
}

void SkyVaultMigrationStoppedHistogram(MigrationDestination destination,
                                       bool value) {
  base::UmaHistogramBoolean(
      GetHistogramName(kMigrationStoppedSuffix, UploadTrigger::kMigration,
                       destination),
      value);
}

void SkyVaultMigrationWrongStateHistogram(MigrationDestination destination,
                                          StateErrorContext context,
                                          State state) {
  base::UmaHistogramEnumeration(
      GetHistogramName(kMigrationStateErrorContextSuffix,
                       UploadTrigger::kMigration, destination),
      context);
  base::UmaHistogramEnumeration(
      GetHistogramName(kMigrationWrongStateSuffix, UploadTrigger::kMigration,
                       destination),
      state);
}

void SkyVaultDeletionDoneHistogram(bool success) {
  base::UmaHistogramBoolean(
      GetHistogramName(kMigrationFailedSuffix, UploadTrigger::kMigration,
                       MigrationDestination::kDelete),
      !success);
}

void SkyVaultMigrationDoneHistograms(MigrationDestination destination,
                                     bool success,
                                     base::TimeDelta duration) {
  base::UmaHistogramBoolean(
      GetHistogramName(kMigrationFailedSuffix, UploadTrigger::kMigration,
                       destination),
      !success);

  const std::string suffix = success ? kMigrationSuccessDurationSuffix
                                     : kMigrationFailureDurationSuffix;
  base::UmaHistogramCustomTimes(
      GetHistogramName(suffix, UploadTrigger::kMigration, destination),
      duration, kMigrationDurationMin, kMigrationDurationMax,
      kMigrationDurationBuckets);
}

void SkyVaultMigrationWriteAccessErrorHistogram(bool value) {
  base::UmaHistogramBoolean(GetHistogramName(kMigrationWriteAccessErrorSuffix,
                                             UploadTrigger::kMigration),
                            value);
}

void SkyVaultMigrationUploadErrorHistogram(MigrationDestination destination,
                                           MigrationUploadError error) {
  base::UmaHistogramEnumeration(
      GetHistogramName(kMigrationUploadErrorSuffix, UploadTrigger::kMigration,
                       destination),
      error);
}

void SkyVaultMigrationWaitForConnectionHistogram(
    MigrationDestination destination,
    bool waiting_for_connection) {
  base::UmaHistogramBoolean(
      GetHistogramName(kMigrationWaitForConnectionSuffix,
                       UploadTrigger::kMigration, destination),
      waiting_for_connection);
}

void SkyVaultMigrationReconnectionDurationHistogram(
    MigrationDestination destination,
    base::TimeDelta duration) {
  base::UmaHistogramCustomTimes(
      GetHistogramName(kMigrationReconnectionDurationSuffix,
                       UploadTrigger::kMigration, destination),
      duration, kReconnectionDurationMin, kReconnectionDurationMax,
      kReconnectionDurationBuckets);
}

void SkyVaultMigrationCleanupErrorHistogram(MigrationDestination destination,
                                            bool value) {
  base::UmaHistogramBoolean(
      GetHistogramName(kMigrationCleanupErrorSuffix, UploadTrigger::kMigration,
                       destination),
      value);
}

void SkyVaultMigrationScheduledTimeInPastInformUser(
    MigrationDestination destination,
    bool value) {
  base::UmaHistogramBoolean(
      GetHistogramName(kScheduledTimeInPastInformUser,
                       UploadTrigger::kMigration, destination),
      value);
}

void SkyVaultMigrationScheduledTimeInPastScheduleMigration(
    MigrationDestination destination,
    bool value) {
  base::UmaHistogramBoolean(
      GetHistogramName(kScheduledTimeInPastScheduleMigration,
                       UploadTrigger::kMigration, destination),
      value);
}

void SkyVaultMigrationDialogActionHistogram(MigrationDestination destination,
                                            DialogAction action) {
  base::UmaHistogramEnumeration(
      GetHistogramName(kMigrationDialogActionSuffix, UploadTrigger::kMigration,
                       destination),
      action);
}

void SkyVaultMigrationDialogShownHistogram(MigrationDestination destination,
                                           bool value) {
  base::UmaHistogramBoolean(
      GetHistogramName(kMigrationDialogShownSuffix, UploadTrigger::kMigration,
                       destination),
      value);
}

}  // namespace policy::local_user_files
