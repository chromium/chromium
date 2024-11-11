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
constexpr char kMigrationWriteAccessErrorSuffix[] = "WriteAccessError";
constexpr char kMigrationUploadErrorSuffix[] = "UploadError";
constexpr char kMigrationDialogActionSuffix[] = "DialogAction";
constexpr char kMigrationDialogShownSuffix[] = "DialogShown";

// Constants for cloud providers used in histogram names.
constexpr char kGoogleDriveProvider[] = "GoogleDrive";
constexpr char kOneDriveProvider[] = "OneDrive";

// Constants for upload triggers used in histogram names.
constexpr char kDownloadTrigger[] = "Download";
constexpr char kScreenCaptureTrigger[] = "ScreenCapture";
constexpr char kMigrationTrigger[] = "Migration";

// Converts `provider` to a string representation used to form a metric name.
std::string GetUMACloudProvider(CloudProvider provider) {
  switch (provider) {
    case CloudProvider::kNotSpecified:
      NOTREACHED();
    case CloudProvider::kGoogleDrive:
      return kGoogleDriveProvider;
    case CloudProvider::kOneDrive:
      return kOneDriveProvider;
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
    std::optional<CloudProvider> provider = std::nullopt) {
  std::vector<std::string> parts = {kSkyVaultUMAPrefix};
  if (trigger.has_value()) {
    parts.push_back(GetUMAAction(trigger.value()));
    parts.push_back(".");
  }
  if (provider.has_value()) {
    parts.push_back(GetUMACloudProvider(provider.value()));
    parts.push_back(".");
  }
  parts.push_back(suffix);
  return base::StrCat(parts);
}

}  // namespace

void SkyVaultDeleteErrorHistogram(UploadTrigger trigger,
                                  CloudProvider provider,
                                  bool value) {
  base::UmaHistogramBoolean(
      GetHistogramName(kDeleteErrorSuffix, trigger, provider), value);
}

void SkyVaultOneDriveSignInErrorHistogram(UploadTrigger trigger, bool value) {
  base::UmaHistogramBoolean(GetHistogramName(kOneDriveSignInErrorSuffix,
                                             trigger, CloudProvider::kOneDrive),
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

void SkyVaultMigrationEnabledHistogram(CloudProvider provider, bool value) {
  base::UmaHistogramBoolean(
      GetHistogramName(kMigrationEnabledSuffix, UploadTrigger::kMigration,
                       provider),
      value);
}

void SkyVaultMigrationMisconfiguredHistogram(CloudProvider provider,
                                             bool value) {
  base::UmaHistogramBoolean(
      GetHistogramName(kMigrationMisconfiguredSuffix, UploadTrigger::kMigration,
                       provider),
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

void SkyVaultMigrationStoppedHistogram(CloudProvider provider, bool value) {
  base::UmaHistogramBoolean(
      GetHistogramName(kMigrationStoppedSuffix, UploadTrigger::kMigration,
                       provider),
      value);
}

void SkyVaultMigrationWrongStateHistogram(CloudProvider provider,
                                          StateErrorContext context,
                                          State state) {
  base::UmaHistogramEnumeration(
      GetHistogramName(kMigrationStateErrorContextSuffix,
                       UploadTrigger::kMigration, provider),
      context);
  base::UmaHistogramEnumeration(
      GetHistogramName(kMigrationWrongStateSuffix, UploadTrigger::kMigration,
                       provider),
      state);
}

void SkyVaultMigrationFailedHistogram(CloudProvider provider, bool value) {
  base::UmaHistogramBoolean(
      GetHistogramName(kMigrationFailedSuffix, UploadTrigger::kMigration,
                       provider),
      value);
}

void SkyVaultMigrationWriteAccessErrorHistogram(bool value) {
  base::UmaHistogramBoolean(GetHistogramName(kMigrationWriteAccessErrorSuffix,
                                             UploadTrigger::kMigration),
                            value);
}

void SkyVaultMigrationUploadErrorHistogram(CloudProvider provider,
                                           MigrationUploadError error) {
  base::UmaHistogramEnumeration(
      GetHistogramName(kMigrationUploadErrorSuffix, UploadTrigger::kMigration,
                       provider),
      error);
}

void SkyVaultMigrationDialogActionHistogram(CloudProvider provider,
                                            DialogAction action) {
  base::UmaHistogramEnumeration(
      GetHistogramName(kMigrationDialogActionSuffix, UploadTrigger::kMigration,
                       provider),
      action);
}

void SkyVaultMigrationDialogShownHistogram(CloudProvider provider, bool value) {
  base::UmaHistogramBoolean(
      GetHistogramName(kMigrationDialogShownSuffix, UploadTrigger::kMigration,
                       provider),
      value);
}

}  // namespace policy::local_user_files
