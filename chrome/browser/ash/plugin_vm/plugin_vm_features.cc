// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"

#include "base/feature_list.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/guest_os/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"

namespace plugin_vm {

namespace {

using ProfileSupported = PluginVmFeatures::ProfileSupported;
using PolicyConfigured = PluginVmFeatures::PolicyConfigured;

ProfileSupported CheckProfileSupported(const Profile* profile) {
  if (!profile) {
    VLOG(1) << "profile == nullptr";
    return ProfileSupported::kErrorNotSupported;
  }

  if (!ash::ProfileHelper::IsPrimaryProfile(profile)) {
    return ProfileSupported::kErrorNonPrimary;
  }

  if (profile->IsChild()) {
    return ProfileSupported::kErrorChildAccount;
  }

  if (profile->IsOffTheRecord()) {
    return ProfileSupported::kErrorOffTheRecord;
  }

  if (ash::ProfileHelper::IsEphemeralUserProfile(profile)) {
    return ProfileSupported::kErrorEphemeral;
  }

  if (!ash::ProfileHelper::IsUserProfile(profile)) {
    VLOG(1) << "non-regular profile is not supported";
    // If this happens, the profile is for something like the sign in screen or
    // lock screen. Return a generic error code because the user will not be
    // able to see the error code/message anyway.
    return ProfileSupported::kErrorNotSupported;
  }

  return ProfileSupported::kOk;
}

PolicyConfigured CheckPolicyConfigured(const Profile* profile) {
  // Bypass other checks when a fake policy is set, or running linux-chromeos.
  if (FakeLicenseKeyIsSet() || !base::SysInfo::IsRunningOnChromeOS()) {
    return PolicyConfigured::kOk;
  }

  if (profile == nullptr) {
    VLOG(1) << "Profile is null";
    return PolicyConfigured::kErrorUnableToCheckPolicy;
  }

  // Check that the device is enterprise enrolled.
  if (!ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    return PolicyConfigured::kErrorNotEnterpriseEnrolled;
  }

  // Check that the user is affiliated.
  const user_manager::User* const user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (user == nullptr || !user->IsAffiliated()) {
    return PolicyConfigured::kErrorUserNotAffiliated;
  }

  // Check that VirtualMachines are allowed by policy.
  if (!virtual_machines::AreVirtualMachinesAllowedByPolicy()) {
    return PolicyConfigured::kErrorVirtualMachinesNotAllowed;
  }

  // Check that PluginVm is allowed to run by policy.
  bool plugin_vm_allowed_for_device;
  if (!ash::CrosSettings::Get()->GetBoolean(ash::kPluginVmAllowed,
                                            &plugin_vm_allowed_for_device)) {
    return PolicyConfigured::kErrorUnableToCheckDevicePolicy;
  }

  if (!plugin_vm_allowed_for_device) {
    return PolicyConfigured::kErrorNotAllowedByDevicePolicy;
  }

  bool plugin_vm_allowed_for_user =
      profile->GetPrefs()->GetBoolean(plugin_vm::prefs::kPluginVmAllowed);
  if (!plugin_vm_allowed_for_user) {
    return PolicyConfigured::kErrorNotAllowedByUserPolicy;
  }

  if (GetPluginVmUserIdForProfile(profile).empty()) {
    return PolicyConfigured::kErrorLicenseNotSetUp;
  }

  return PolicyConfigured::kOk;
}

}  // namespace

bool PluginVmFeatures::IsAllowedDiagnostics::IsOk() const {
  return device_supported && profile_supported == ProfileSupported::kOk &&
         policy_configured == PolicyConfigured::kOk;
}

std::string PluginVmFeatures::IsAllowedDiagnostics::GetTopError() const {
  if (!device_supported) {
    return "Parallels Desktop is not supported on this device";
  }

  switch (profile_supported) {
    case ProfileSupported::kOk:
      break;
    case ProfileSupported::kErrorNonPrimary:
      return "Parallels Desktop is only allowed in primary user sessions";
    case ProfileSupported::kErrorChildAccount:
      return "Child accounts are not supported";
    case ProfileSupported::kErrorOffTheRecord:
      return "Guest profiles are not supported";
    case ProfileSupported::kErrorEphemeral:
      return "Ephemeral user profiles are not supported";
    case ProfileSupported::kErrorNotSupported:
      return "This user session is not allowed to run Parallels Desktop";
  }

  switch (policy_configured) {
    case PolicyConfigured::kOk:
      break;
    case PolicyConfigured::kErrorUnableToCheckPolicy:
      return "Unable to check policy";
    case PolicyConfigured::kErrorNotEnterpriseEnrolled:
      return "This is not an enterprise-managed device";
    case PolicyConfigured::kErrorUserNotAffiliated:
      return "This user is not affiliated with the organization";
    case PolicyConfigured::kErrorUnableToCheckDevicePolicy:
      return "Unable to determine if device-level policy allows running VMs";
    case PolicyConfigured::kErrorNotAllowedByDevicePolicy:
      return "VMs are disallowed by policy on this device";
    case PolicyConfigured::kErrorNotAllowedByUserPolicy:
      return "VMs are disallowed by policy";
    case PolicyConfigured::kErrorLicenseNotSetUp:
      return "License for the product is not set up in policy";
    case PolicyConfigured::kErrorVirtualMachinesNotAllowed:
      return "No Virtual Machines are allowed on this device";
  }

  return "";
}

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
// * PluginVm feature should be enabled.
// * Profile should be eligible.
// * Device should be enterprise enrolled:
//   * User should be affiliated.
//   * PluginVmAllowed device policy should be set to true.
//   * UserPluginVmAllowed user policy should be set to true.
// * PluginVmUserId policy is set.
PluginVmFeatures::IsAllowedDiagnostics
PluginVmFeatures::GetIsAllowedDiagnostics(const Profile* profile) {
  auto diagnostics = IsAllowedDiagnostics{
      /*device_supported=*/base::FeatureList::IsEnabled(features::kPluginVm),
      /*profile_supported=*/CheckProfileSupported(profile),
      /*policy_configured=*/CheckPolicyConfigured(profile),
  };
  if (!diagnostics.IsOk()) {
    VLOG(1) << diagnostics.GetTopError();
  }

  return diagnostics;
}

bool PluginVmFeatures::IsAllowed(const Profile* profile, std::string* reason) {
  auto diagnostics = GetIsAllowedDiagnostics(profile);
  if (!diagnostics.IsOk()) {
    if (reason) {
      *reason = diagnostics.GetTopError();
      DCHECK(!reason->empty());
    }
    return false;
  }

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
