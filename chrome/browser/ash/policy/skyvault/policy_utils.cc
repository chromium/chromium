// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/policy_utils.h"

#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/skyvault/file_location_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace policy::local_user_files {

namespace {

FileSaveDestination GetDestinationForPref(Profile* profile,
                                          const std::string& pref_name) {
  DCHECK(profile);
  auto* pref = profile->GetPrefs()->FindPreference(pref_name);
  if (!pref || !pref->GetValue() || !pref->IsManaged()) {
    return FileSaveDestination::kNotSpecified;
  }
  const auto value_str = pref->GetValue()->GetString();
  if (!IsValidLocationString(value_str)) {
    return FileSaveDestination::kNotSpecified;
  }
  if (value_str.find(kGoogleDrivePolicyVariableName) != std::string::npos) {
    return FileSaveDestination::kGoogleDrive;
  }
  if (value_str.find(kOneDrivePolicyVariableName) != std::string::npos) {
    return FileSaveDestination::kOneDrive;
  }

  return FileSaveDestination::kDownloads;
}

}  // namespace

constexpr char kGoogleDrivePolicyVariableName[] = "${google_drive}";
constexpr char kOneDrivePolicyVariableName[] = "${microsoft_onedrive}";

constexpr char kMigrationDestinationGoogleDrive[] = "google_drive";
constexpr char kMigrationDestinationOneDrive[] = "microsoft_onedrive";
constexpr char kMigrationDestinationDelete[] = "delete";

bool LocalUserFilesAllowed() {
  // If the flag is disabled, ignore the policy value and allow local storage.
  if (!base::FeatureList::IsEnabled(features::kSkyVault)) {
    return true;
  }
  // In tests, `g_browser_process` is null.
  if (!g_browser_process || !g_browser_process->local_state()) {
    CHECK_IS_TEST();
    return true;
  }
  return g_browser_process->local_state()->GetBoolean(
      prefs::kLocalUserFilesAllowed);
}

MigrationDestination GetMigrationDestination() {
  if (!base::FeatureList::IsEnabled(features::kSkyVault) ||
      !base::FeatureList::IsEnabled(features::kSkyVaultV2)) {
    return MigrationDestination::kNotSpecified;
  }

  const std::string destination = g_browser_process->local_state()->GetString(
      prefs::kLocalUserFilesMigrationDestination);

  if (destination == kMigrationDestinationGoogleDrive) {
    return MigrationDestination::kGoogleDrive;
  }
  if (destination == kMigrationDestinationOneDrive) {
    return MigrationDestination::kOneDrive;
  }
  if (base::FeatureList::IsEnabled(features::kSkyVaultV3) &&
      destination == kMigrationDestinationDelete) {
    return MigrationDestination::kDelete;
  }
  return MigrationDestination::kNotSpecified;
}

bool IsCloudDestination(MigrationDestination destination) {
  return destination == MigrationDestination::kGoogleDrive ||
         destination == MigrationDestination::kOneDrive;
}

FileSaveDestination GetDownloadsDestination(Profile* profile) {
  return GetDestinationForPref(profile, prefs::kDownloadDefaultDirectory);
}

FileSaveDestination GetScreenCaptureDestination(Profile* profile) {
  return GetDestinationForPref(profile, ash::prefs::kCaptureModePolicySavePath);
}

bool DownloadToTemp(Profile* profile) {
  return base::FeatureList::IsEnabled(features::kSkyVault) &&
         GetDownloadsDestination(profile) == FileSaveDestination::kOneDrive;
}

base::FilePath GetMyFilesPath(Profile* profile) {
  return profile->GetPath().Append("MyFiles");
}

std::optional<base::Time> GetMigrationStartTime(Profile* profile) {
  if (!base::FeatureList::IsEnabled(features::kSkyVaultV3)) {
    return std::nullopt;
  }
  PrefService* pref_service = profile->GetPrefs();
  base::Time scheduled_start_time =
      pref_service->GetTime(prefs::kSkyVaultMigrationScheduledStartTime);
  if (scheduled_start_time.is_null()) {
    LOG(ERROR) << "Migration/deletion start time cannot be determined.";
    return std::nullopt;
  }
  return scheduled_start_time;
}

}  // namespace policy::local_user_files
