// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_features.h"

#include "base/feature_list.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"

namespace {}  // namespace

namespace plugin_vm {

static PluginVmFeatures* g_plugin_vm_features = nullptr;

PluginVmFeatures* PluginVmFeatures::Get() {
  if (!g_plugin_vm_features) {
    g_plugin_vm_features = new PluginVmFeatures();
  }
  return g_plugin_vm_features;
}

void PluginVmFeatures::SetForTesting(PluginVmFeatures* features) {
  g_plugin_vm_features = features;
}

PluginVmFeatures::PluginVmFeatures() = default;

PluginVmFeatures::~PluginVmFeatures() = default;

// For PluginVm to be allowed:
// * Profile should be eligible.
// * PluginVm feature should be enabled.
// * Device should be enterprise enrolled:
//   * User should be affiliated.
//   * PluginVmAllowed device policy should be set to true.
//   * UserPluginVmAllowed user policy should be set to true.
// * At least one of the following should be set:
//   * PluginVmLicenseKey policy.
//   * PluginVmUserId policy.
bool PluginVmFeatures::IsAllowed(const Profile* profile) {
  // Check that the profile is eligible.
  if (!profile || profile->IsChild() || profile->IsLegacySupervised() ||
      profile->IsOffTheRecord() ||
      chromeos::ProfileHelper::IsEphemeralUserProfile(profile) ||
      chromeos::ProfileHelper::IsLockScreenAppProfile(profile) ||
      !chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    return false;
  }

  // Check that PluginVm feature is enabled.
  if (!base::FeatureList::IsEnabled(features::kPluginVm))
    return false;

  // Bypass other checks when a fake policy is set
  if (FakeLicenseKeyIsSet())
    return true;

  // Check that the device is enterprise enrolled.
  if (!chromeos::InstallAttributes::Get()->IsEnterpriseManaged())
    return false;

  // Check that the user is affiliated.
  const user_manager::User* const user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (user == nullptr || !user->IsAffiliated())
    return false;

  // Check that PluginVm is allowed to run by policy.
  bool plugin_vm_allowed_for_device;
  if (!chromeos::CrosSettings::Get()->GetBoolean(
          chromeos::kPluginVmAllowed, &plugin_vm_allowed_for_device)) {
    return false;
  }
  bool plugin_vm_allowed_for_user =
      profile->GetPrefs()->GetBoolean(plugin_vm::prefs::kPluginVmAllowed);
  if (!plugin_vm_allowed_for_device || !plugin_vm_allowed_for_user)
    return false;

  if (GetPluginVmLicenseKey().empty() &&
      GetPluginVmUserIdForProfile(profile).empty())
    return false;

  return true;
}

bool PluginVmFeatures::IsConfigured(const Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      plugin_vm::prefs::kPluginVmImageExists);
}

bool PluginVmFeatures::IsEnabled(const Profile* profile) {
  return PluginVmFeatures::Get()->IsAllowed(profile) &&
         PluginVmFeatures::Get()->IsConfigured(profile);
}

}  // namespace plugin_vm
