// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_features.h"

#include "ash/components/settings/cros_settings_names.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/settings/cros_settings.h"
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

  bool allowed_for_device;
  if (ash::CrosSettings::Get()->GetBoolean(ash::kBorealisAllowedForDevice,
                                           &allowed_for_device)) {
    if (!allowed_for_device)
      return AllowStatus::kDevicePolicyBlocked;
  }

  if (!profile->GetPrefs()->GetBoolean(prefs::kBorealisAllowedForUser))
    return AllowStatus::kUserPrefBlocked;

  return AllowStatus::kAllowed;
}

BorealisFeatures::BorealisFeatures(Profile* profile) : profile_(profile) {}

namespace {

bool g_should_show_reason = true;

}  // namespace

bool BorealisFeatures::IsAllowed() {
  AllowStatus reason = GetAllowanceForProfile(profile_);
  if (reason != AllowStatus::kAllowed) {
    LOG_IF(ERROR, g_should_show_reason)
        << "Borealis is not allowed: " << reason;
    g_should_show_reason = false;
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
    case AllowStatus::kDevicePolicyBlocked:
      return os << "Device is enrolled and borealis is disabled by policy";
    case AllowStatus::kUserPrefBlocked:
      return os << "User profile preferences disallow borealis";
  }
}
