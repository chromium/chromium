// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"

#include <string_view>

#include "base/containers/contains.h"
#include "chrome/browser/chromeos/enterprise/cloud_storage/policy_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/odfs_config_private.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

using extensions::api::odfs_config_private::Mount;

namespace chromeos {

namespace {

bool IsPrefValueSetToAllowed(std::string_view pref_value) {
  return pref_value == cloud_upload::kCloudUploadPolicyAllowed ||
         pref_value == cloud_upload::kCloudUploadPolicyAutomated;
}

bool IsPrefValueSetToAutomated(std::string_view pref_value) {
  return pref_value == cloud_upload::kCloudUploadPolicyAutomated;
}

}  // namespace

bool IsEligibleAndEnabledUploadOfficeToCloud(const Profile* profile) {
  if (!chromeos::features::IsUploadOfficeToCloudEnabled()) {
    return false;
  }
  if (!profile) {
    return false;
  }
  // Drive and OneDrive are disabled in Guest mode.
  if (profile->IsGuestSession()) {
    return false;
  }
  // If `kUploadOfficeToCloudForEnterprise` flag is enabled, we loosen the
  // condition below to allow managed accounts.
  if (chromeos::features::IsUploadOfficeToCloudForEnterpriseEnabled()) {
    return !profile->IsChild();
  }
  // Managed users, e.g. enterprise account, child account, are not eligible.
  if (profile->GetProfilePolicyConnector()->IsManaged()) {
    return false;
  }
  return true;
}

namespace cloud_upload {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kMicrosoftOfficeCloudUpload,
                               kCloudUploadPolicyAllowed);
  registry->RegisterStringPref(prefs::kGoogleWorkspaceCloudUpload,
                               kCloudUploadPolicyAllowed);
}

bool IsMicrosoftOfficeOneDriveIntegrationAllowed(const Profile* profile) {
  if (!IsEligibleAndEnabledUploadOfficeToCloud(profile)) {
    return false;
  }

  if (profile->GetProfilePolicyConnector()->IsManaged()) {
    return chromeos::features::
               IsMicrosoftOneDriveIntegrationForEnterpriseEnabled() &&
           base::Contains(
               std::vector<Mount>{Mount::kAllowed, Mount::kAutomated},
               chromeos::cloud_storage::GetMicrosoftOneDriveMount(profile));
  }
  return true;
}

bool IsMicrosoftOfficeCloudUploadAllowed(Profile* profile) {
  if (!chromeos::features::IsUploadOfficeToCloudForEnterpriseEnabled()) {
    return IsEligibleAndEnabledUploadOfficeToCloud(profile);
  }
  return IsEligibleAndEnabledUploadOfficeToCloud(profile) &&
         IsMicrosoftOfficeOneDriveIntegrationAllowed(profile) &&
         IsPrefValueSetToAllowed(profile->GetPrefs()->GetString(
             prefs::kMicrosoftOfficeCloudUpload));
}

bool IsMicrosoftOfficeCloudUploadAutomated(Profile* profile) {
  if (!chromeos::features::IsUploadOfficeToCloudForEnterpriseEnabled()) {
    return false;
  }
  return IsEligibleAndEnabledUploadOfficeToCloud(profile) &&
         IsMicrosoftOfficeOneDriveIntegrationAllowed(profile) &&
         IsPrefValueSetToAutomated(profile->GetPrefs()->GetString(
             prefs::kMicrosoftOfficeCloudUpload));
}

bool IsGoogleWorkspaceCloudUploadAllowed(Profile* profile) {
  if (!chromeos::features::IsUploadOfficeToCloudForEnterpriseEnabled()) {
    return IsEligibleAndEnabledUploadOfficeToCloud(profile);
  }
  return IsEligibleAndEnabledUploadOfficeToCloud(profile) &&
         IsPrefValueSetToAllowed(profile->GetPrefs()->GetString(
             prefs::kGoogleWorkspaceCloudUpload));
}

bool IsGoogleWorkspaceCloudUploadAutomated(Profile* profile) {
  if (!chromeos::features::IsUploadOfficeToCloudForEnterpriseEnabled()) {
    return false;
  }
  return IsEligibleAndEnabledUploadOfficeToCloud(profile) &&
         IsPrefValueSetToAutomated(profile->GetPrefs()->GetString(
             prefs::kGoogleWorkspaceCloudUpload));
}

}  // namespace cloud_upload

}  // namespace chromeos
