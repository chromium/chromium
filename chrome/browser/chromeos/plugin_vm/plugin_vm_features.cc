// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_features.h"

#include "base/feature_list.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
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
bool PluginVmFeatures::IsAllowed(const Profile* profile, std::string* reason) {
  // Check that PluginVm feature is enabled.
  if (!base::FeatureList::IsEnabled(features::kPluginVm)) {
    *reason = "Parallels product is not supported on this device";
    return false;
  }

  // Check that the profile is eligible.
  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    VLOG(1) << "Parallels is not allowed on non-primary profiles.";
    *reason = "Parallels is only allowed in primary user sessions";
    return false;
  }

  if (!profile || profile->IsChild() || profile->IsOffTheRecord() ||
      chromeos::ProfileHelper::IsEphemeralUserProfile(profile) ||
      !chromeos::ProfileHelper::IsRegularProfile(profile)) {
    VLOG(1) << "Profile is not allowed to run Parallels.";
    *reason = "This user session is not allowed to run Parallels";
    return false;
  }

  // Bypass other checks when a fake policy is set, or running linux-chromeos.
  if (FakeLicenseKeyIsSet() || !base::SysInfo::IsRunningOnChromeOS())
    return true;

  // Check that the device is enterprise enrolled.
  if (!chromeos::InstallAttributes::Get()->IsEnterpriseManaged()) {
    VLOG(1) << "Parallels is only allowed on enterprise-managed devices.";
    *reason = "This is not an enterprise-managed device";
    return false;
  }

  // Check that the user is affiliated.
  const user_manager::User* const user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (user == nullptr || !user->IsAffiliated()) {
    VLOG(1) << "Parallels is only allowed for affiliated users.";
    *reason = "This user is not affiliated with the organization";
    return false;
  }

  // Check that PluginVm is allowed to run by policy.
  bool plugin_vm_allowed_for_device;
  if (!chromeos::CrosSettings::Get()->GetBoolean(
          chromeos::kPluginVmAllowed, &plugin_vm_allowed_for_device)) {
    VLOG(1) << "Unable to determine Parallels device-level policy.";
    *reason = "Unable to determine if device-level policy allows running VMs";
    return false;
  }

  if (!plugin_vm_allowed_for_device) {
    VLOG(1) << "Parallels is disabled by device-level policy.";
    *reason = "VMs are disallowed by policy on this device";
    return false;
  }

  bool plugin_vm_allowed_for_user =
      profile->GetPrefs()->GetBoolean(plugin_vm::prefs::kPluginVmAllowed);
  if (!plugin_vm_allowed_for_user) {
    VLOG(1) << "Parallels is disabled by user-level policy.";
    *reason = "VMs are disallowed by policy";
    return false;
  }

  if (GetPluginVmLicenseKey().empty() &&
      GetPluginVmUserIdForProfile(profile).empty()) {
    VLOG(1) << "Parallels require a license be set up in policy.";
    *reason = "License for the product is not set up in policy";
    return false;
  }

  return true;
}

bool PluginVmFeatures::IsAllowed(const Profile* profile) {
  std::string reason;
  return IsAllowed(profile, &reason);
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
