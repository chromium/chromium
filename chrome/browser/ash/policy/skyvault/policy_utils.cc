// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/policy_utils.h"

#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
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

bool LocalUserFilesAllowed() {
  // If the flag is disabled, ignore the policy value and allow local storage.
  if (!base::FeatureList::IsEnabled(features::kSkyVault)) {
    return true;
  }
  return g_browser_process->local_state()->GetBoolean(
      prefs::kLocalUserFilesAllowed);
}

FileSaveDestination GetDownloadsDestination(Profile* profile) {
  return GetDestinationForPref(profile, prefs::kDownloadDefaultDirectory);
}

FileSaveDestination GetScreenCaptureDestination(Profile* profile) {
  return GetDestinationForPref(profile, ash::prefs::kCaptureModePolicySavePath);
}

}  // namespace policy::local_user_files
