// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"

#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"

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

}  // namespace chromeos
