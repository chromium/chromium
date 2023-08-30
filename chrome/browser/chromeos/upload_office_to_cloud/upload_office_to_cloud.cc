// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"

#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {

constexpr char kCloudUploadPolicyAllowed[] = "allowed";
constexpr char kCloudUploadPolicyDisallowed[] = "disallowed";
constexpr char kCloudUploadPolicyAutomated[] = "automated";

}  // namespace

namespace chromeos {

bool IsEligibleAndEnabledUploadOfficeToCloud(Profile* profile) {
  if (!chromeos::features::IsUploadOfficeToCloudEnabled()) {
    return false;
  }
  if (!profile) {
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

bool IsMicrosoftOfficeCloudUploadDisabledByPolicy(Profile* profile) {
  return chromeos::features::IsUploadOfficeToCloudForEnterpriseEnabled() &&
         profile->GetPrefs()->GetString(prefs::kMicrosoftOfficeCloudUpload) ==
             kCloudUploadPolicyDisallowed;
}

bool IsMicrosoftOfficeCloudUploadAutomatedByPolicy(Profile* profile) {
  return chromeos::features::IsUploadOfficeToCloudForEnterpriseEnabled() &&
         profile->GetPrefs()->GetString(prefs::kMicrosoftOfficeCloudUpload) ==
             kCloudUploadPolicyAutomated;
}

bool IsGoogleWorkspaceCloudUploadDisabledByPolicy(Profile* profile) {
  return chromeos::features::IsUploadOfficeToCloudForEnterpriseEnabled() &&
         profile->GetPrefs()->GetString(prefs::kGoogleWorkspaceCloudUpload) ==
             kCloudUploadPolicyDisallowed;
}

bool IsGoogleWorkspaceCloudUploadAutomatedByPolicy(Profile* profile) {
  return chromeos::features::IsUploadOfficeToCloudForEnterpriseEnabled() &&
         profile->GetPrefs()->GetString(prefs::kGoogleWorkspaceCloudUpload) ==
             kCloudUploadPolicyAutomated;
}

}  // namespace cloud_upload

}  // namespace chromeos
