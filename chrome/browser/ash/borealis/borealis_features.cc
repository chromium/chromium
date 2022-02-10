// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_features.h"

#include "ash/components/settings/cros_settings_names.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/guest_os/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/prefs/pref_service.h"

using AllowStatus = borealis::BorealisFeatures::AllowStatus;

namespace borealis {

// static
AllowStatus BorealisFeatures::GetAllowanceForProfile(Profile* profile) {
  if (!base::FeatureList::IsEnabled(features::kBorealis))
    return AllowStatus::kFeatureDisabled;

  if (!virtual_machines::AreVirtualMachinesAllowedByPolicy())
    return AllowStatus::kVmPolicyBlocked;

  if (!profile || !profile->IsRegularProfile())
    return AllowStatus::kBlockedOnIrregularProfile;

  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile))
    return AllowStatus::kBlockedOnNonPrimaryProfile;

  if (profile->IsChild())
    return AllowStatus::kBlockedOnChildAccount;

  const PrefService::Preference* user_allowed_pref =
      profile->GetPrefs()->FindPreference(prefs::kBorealisAllowedForUser);
  if (!user_allowed_pref || !user_allowed_pref->GetValue()->GetBool())
    return AllowStatus::kUserPrefBlocked;

  // For managed users the preference must be explicitly set true. So we block
  // in the case where the user is managed and the pref isn't.
  if (!user_allowed_pref->IsManaged() &&
      profile->GetProfilePolicyConnector()->IsManaged()) {
    return AllowStatus::kUserPrefBlocked;
  }

  return AllowStatus::kAllowed;
}

BorealisFeatures::BorealisFeatures(Profile* profile) : profile_(profile) {}

bool BorealisFeatures::IsAllowed() {
  AllowStatus reason = GetAllowanceForProfile(profile_);
  if (reason != AllowStatus::kAllowed) {
    VLOG(1) << "Borealis is not allowed: " << reason;
    return false;
  }
  return true;
}

bool BorealisFeatures::IsEnabled() {
  if (!IsAllowed())
    return false;
  return profile_->GetPrefs()->GetBoolean(prefs::kBorealisInstalledOnDevice);
}

}  // namespace borealis

std::ostream& operator<<(std::ostream& os, const AllowStatus& reason) {
  switch (reason) {
    case AllowStatus::kAllowed:
      return os << "Borealis is allowed";
    case AllowStatus::kFeatureDisabled:
      return os << "Borealis feature is unavailable";
    case AllowStatus::kBlockedOnIrregularProfile:
      return os << "Borealis is only available on regular sessions";
    case AllowStatus::kBlockedOnNonPrimaryProfile:
      return os << "Borealis is only available on the primary profile";
    case AllowStatus::kBlockedOnChildAccount:
      return os << "Borealis is not available on child accounts";
    case AllowStatus::kVmPolicyBlocked:
      return os << "Virtual machines policy disallows borealis";
    case AllowStatus::kUserPrefBlocked:
      return os << "User profile preferences disallow borealis";
  }
}
