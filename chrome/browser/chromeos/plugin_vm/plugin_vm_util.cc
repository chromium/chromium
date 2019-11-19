// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/exo/shell_surface_util.h"
#include "components/prefs/pref_service.h"

namespace plugin_vm {

// For PluginVm to be allowed:
// * Profile should be eligible.
// * PluginVm feature should be enabled.
// If device is not enterprise enrolled:
//     * Device should be in a dev mode.
// If device is enterprise enrolled:
//     * User should be affiliated.
//     * All necessary policies should be set (PluginVmAllowed, PluginVmImage
//       and PluginVmLicenseKey).
//
// TODO(okalitova, aoldemeier): PluginVm should be disabled in case of
// non-managed devices once it is launched. Currently this conditions are used
// for making manual tests easier.
bool IsPluginVmAllowedForProfile(const Profile* profile) {
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

  // TODO(okalitova, aoldemeier): Remove once PluginVm is ready to be launched.
  // Check for alternative condition for manual testing, i.e. the device is in
  // developer mode and the device is not enterprise-enrolled.
  if (!chromeos::InstallAttributes::Get()->IsEnterpriseManaged()) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            chromeos::switches::kSystemDevMode)) {
      return true;
    }
    return false;
  }

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
  if (!plugin_vm_allowed_for_device)
    return false;

  // Check that a license key is set.
  std::string plugin_vm_license_key;
  if (!chromeos::CrosSettings::Get()->GetString(chromeos::kPluginVmLicenseKey,
                                                &plugin_vm_license_key)) {
    return false;
  }
  if (plugin_vm_license_key == std::string())
    return false;

  // Check that a VM image is set.
  if (!profile->GetPrefs()->HasPrefPath(plugin_vm::prefs::kPluginVmImage))
    return false;

  return true;
}

bool IsPluginVmConfigured(Profile* profile) {
  if (!profile->GetPrefs()->GetBoolean(
          plugin_vm::prefs::kPluginVmImageExists)) {
    return false;
  }
  return true;
}

bool IsPluginVmEnabled(Profile* profile) {
  return IsPluginVmAllowedForProfile(profile) && IsPluginVmConfigured(profile);
}

bool IsPluginVmRunning(Profile* profile) {
  return plugin_vm::PluginVmManager::GetForProfile(profile)->vm_state() ==
             vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING &&
         ChromeLauncherController::instance()->IsOpen(
             ash::ShelfID(kPluginVmAppId));
}

bool IsPluginVmWindow(const aura::Window* window) {
  const std::string* app_id = exo::GetShellApplicationId(window);
  if (!app_id)
    return false;
  return *app_id == "org.chromium.plugin_vm_ui";
}

std::string GetPluginVmLicenseKey() {
  std::string plugin_vm_license_key;
  if (!chromeos::CrosSettings::Get()->GetString(chromeos::kPluginVmLicenseKey,
                                                &plugin_vm_license_key)) {
    return std::string();
  }
  return plugin_vm_license_key;
}

}  // namespace plugin_vm
