// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/histogram_helper.h"

#include <string>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"

namespace policy::local_user_files {
namespace {
// Converts `provider` to a string representation used to form a metric name.
std::string GetUMACloudProvider(CloudProvider provider) {
  switch (provider) {
    case CloudProvider::kNotSpecified:
      NOTREACHED_NORETURN();
    case CloudProvider::kGoogleDrive:
      return "GoogleDrive";
    case CloudProvider::kOneDrive:
      return "OneDrive";
  }
}

// Converts `trigger` to a string representation used to form a metric name.
std::string GetUMAAction(UploadTrigger trigger) {
  std::string action;
  switch (trigger) {
    case UploadTrigger::kDownload:
      return "Download";
    case UploadTrigger::kScreenCapture:
      return "ScreenCapture";
    case UploadTrigger::kMigration:
      return "Migration";
  }
}
}  // namespace

void SkyVaultDeleteErrorHistogram(UploadTrigger trigger,
                                  CloudProvider provider,
                                  bool value) {
  base::UmaHistogramBoolean(
      base::StrCat({"Enterprise.SkyVault.", GetUMAAction(trigger), ".",
                    GetUMACloudProvider(provider), ".DeleteError"}),
      value);
}

void SkyVaultOneDriveSignInErrorHistogram(UploadTrigger trigger, bool value) {
  base::UmaHistogramBoolean(
      base::StrCat({"Enterprise.SkyVault.", GetUMAAction(trigger),
                    ".OneDrive.SignInError"}),
      value);
}

void SkyVaultLocalStorageEnabledHistogram(bool value) {
  base::UmaHistogramBoolean("Enterprise.SkyVault.LocalStorage.Enabled", value);
}

void SkyVaultMigrationEnabledHistogram(CloudProvider provider, bool value) {
  base::UmaHistogramBoolean(
      base::StrCat({"Enterprise.SkyVault.Migration.",
                    GetUMACloudProvider(provider), ".Enabled"}),
      value);
}

void SkyVaultMigrationMisconfiguredHistogram(CloudProvider provider,
                                             bool value) {
  base::UmaHistogramBoolean(
      base::StrCat({"Enterprise.SkyVault.Migration.",
                    GetUMACloudProvider(provider), ".Misconfigured"}),
      value);
}

void SkyVaultMigrationResetHistogram(bool value) {
  base::UmaHistogramBoolean("Enterprise.SkyVault.Migration.Reset", value);
}

void SkyVaultMigrationStoppedHistogram(CloudProvider provider, bool value) {
  base::UmaHistogramBoolean(
      base::StrCat({"Enterprise.SkyVault.Migration.",
                    GetUMACloudProvider(provider), ".Stopped"}),
      value);
}

void SkyVaultMigrationWrongStateHistogram(CloudProvider provider,
                                          StateErrorContext context,
                                          State state) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Enterprise.SkyVault.Migration.",
                    GetUMACloudProvider(provider), ".StateErrorContext"}),
      context);
  base::UmaHistogramEnumeration(
      base::StrCat({"Enterprise.SkyVault.Migration.",
                    GetUMACloudProvider(provider), ".WrongState"}),
      state);
}

void SkyVaultMigrationFailedHistogram(CloudProvider provider, bool value) {
  base::UmaHistogramBoolean(
      base::StrCat({"Enterprise.SkyVault.Migration.",
                    GetUMACloudProvider(provider), ".Failed"}),
      value);
}

void SkyVaultMigrationWriteAccessErrorHistogram(bool value) {
  base::UmaHistogramBoolean("Enterprise.SkyVault.Migration.WriteAccessError",
                            value);
}

}  // namespace policy::local_user_files
