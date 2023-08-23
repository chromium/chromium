// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"

#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_registry_simple.h"

namespace {
constexpr char kCloudUploadPolicyAllowed[] = "allowed";
}  // namespace

namespace chromeos {

bool IsEligibleAndEnabledUploadOfficeToCloud(Profile* profile) {
  if (!chromeos::features::IsUploadOfficeToCloudEnabled()) {
    return false;
  }
  if (!profile) {
    return false;
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

}  // namespace cloud_upload

}  // namespace chromeos
