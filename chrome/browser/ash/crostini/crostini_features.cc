// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_features.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/experiences/guest_os/virtual_machines/virtual_machines_util.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"

namespace {

bool IsUnaffiliatedCrostiniAllowedByPolicy() {
  bool unaffiliated_crostini_allowed;
  if (ash::CrosSettings::Get()->GetBoolean(
          ash::kDeviceUnaffiliatedCrostiniAllowed,
          &unaffiliated_crostini_allowed)) {
    return unaffiliated_crostini_allowed;
  }
  // If device policy is not set, allow Crostini.
  return true;
}

}  // namespace

namespace crostini {

static CrostiniFeatures* g_crostini_features = nullptr;

CrostiniFeatures* CrostiniFeatures::Get() {
  if (!g_crostini_features) {
    g_crostini_features = new CrostiniFeatures();
  }
  return g_crostini_features;
}

void CrostiniFeatures::SetForTesting(CrostiniFeatures* features) {
  g_crostini_features = features;
}

CrostiniFeatures::CrostiniFeatures() = default;

CrostiniFeatures::~CrostiniFeatures() = default;

bool CrostiniFeatures::CouldBeAllowed(Profile* profile,
                                      std::string* reason) const {
  if (!base::FeatureList::IsEnabled(features::kCrostini)) {
    VLOG(1) << "Crostini is not enabled in feature list.";
    // Prior to M105, the /dev/kvm check used the same reason string.
    *reason = "Crostini is not supported on this device";
    return false;
  }

  if (!crostini::CrostiniManager::IsDevKvmPresent()) {
    // Hardware is physically incapable, no matter what the user wants.
    VLOG(1) << "Cannot run crostini because /dev/kvm is not present.";
    *reason = "Virtualization is not supported on this device";
    return false;
  }

  if (!crostini::CrostiniManager::IsVmLaunchAllowed()) {
    VLOG(1) << "Concierge does not allow VM to be launched.";
    *reason = "Virtualization is not supported on this device";
    return false;
  }

  if (!ash::ProfileHelper::IsPrimaryProfile(profile)) {
    VLOG(1) << "Crostini UI is not allowed on non-primary profiles.";
    *reason = "Crostini is only allowed in primary user sessions";
    return false;
  }

  if (!profile || profile->IsChild() || profile->IsOffTheRecord() ||
      ash::ProfileHelper::IsEphemeralUserProfile(profile)) {
    VLOG(1) << "Profile is not allowed to run crostini.";
    *reason = "This user session is not allowed to run crostini";
    return false;
  }

  return true;
}

bool CrostiniFeatures::CouldBeAllowed(Profile* profile) const {
  std::string reason;
  return CouldBeAllowed(profile, &reason);
}

bool CrostiniFeatures::IsAllowedNow(Profile* profile,
                                    std::string* reason) const {
  if (!CouldBeAllowed(profile, reason)) {
    return false;
  }

  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!IsUnaffiliatedCrostiniAllowedByPolicy() && !user->IsAffiliated()) {
    VLOG(1) << "Policy blocks unaffiliated user from running Crostini.";
    *reason = "Crostini for unaffiliated users is disabled by policy";
    return false;
  }

  const PrefService::Preference* crostini_allowed_by_policy =
      profile->GetPrefs()->FindPreference(
          crostini::prefs::kUserCrostiniAllowedByPolicy);
  if (!crostini_allowed_by_policy->GetValue()->GetBool()) {
    VLOG(1) << "kUserCrostiniAllowedByPolicy preference is false.";
    *reason = "Crostini is disabled by policy";
    return false;
  }
  if (!crostini_allowed_by_policy->IsManaged() && user->IsAffiliated()) {
    VLOG(1) << "Affiliated user is not allowed to run Crostini by default.";
    *reason = "Affiliated user is not allowed to run Crostini by default.";
    return false;
  }

  if (!virtual_machines::AreVirtualMachinesAllowedByPolicy()) {
    VLOG(1)
        << "Crostini cannot run as virtual machines are not allowed by policy.";
    *reason = "Virtual Machines are disabled by policy";
    return false;
  }

  return true;
}

bool CrostiniFeatures::IsAllowedNow(Profile* profile) const {
  std::string reason;
  return IsAllowedNow(profile, &reason);
}

bool CrostiniFeatures::IsEnabled(Profile* profile) const {
  return g_crostini_features->IsAllowedNow(profile) &&
         profile->GetPrefs()->GetBoolean(crostini::prefs::kCrostiniEnabled);
}

bool CrostiniFeatures::IsExportImportUIAllowed(Profile* profile) const {
  return g_crostini_features->IsAllowedNow(profile) &&
         profile->GetPrefs()->GetBoolean(
             crostini::prefs::kUserCrostiniExportImportUIAllowedByPolicy);
}

bool CrostiniFeatures::IsRootAccessAllowed(Profile* profile) const {
  if (base::FeatureList::IsEnabled(features::kCrostiniAdvancedAccessControls)) {
    return profile->GetPrefs()->GetBoolean(
        crostini::prefs::kUserCrostiniRootAccessAllowedByPolicy);
  }
  return true;
}

bool CrostiniFeatures::IsContainerUpgradeUIAllowed(Profile* profile) const {
  return g_crostini_features->IsAllowedNow(profile);
}

void CrostiniFeatures::CanChangeAdbSideloading(
    Profile* profile,
    CanChangeAdbSideloadingCallback callback) const {
  // First rule out a child account as it is a special case - a child can be an
  // owner, but ADB sideloading is currently not supported for this case
  if (profile->IsChild()) {
    DVLOG(1) << "adb sideloading is currently unsupported for child accounts";
    std::move(callback).Run(false);
    return;
  }

  // ADB sideloading is not supported for managed devices or profiles.
  auto* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (connector->IsDeviceEnterpriseManaged() ||
      profile->GetProfilePolicyConnector()->IsManaged()) {
    std::move(callback).Run(false);
    return;
  }

  // Here we are sure that the user is not enterprise-managed and we therefore
  // only check whether the user is the owner
  if (!ash::ProfileHelper::IsOwnerProfile(profile)) {
    DVLOG(1) << "Only the owner can change adb sideloading status";
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

bool CrostiniFeatures::IsPortForwardingAllowed(Profile* profile) const {
  if (!profile->GetPrefs()->GetBoolean(
          crostini::prefs::kCrostiniPortForwardingAllowedByPolicy)) {
    VLOG(1) << "kCrostiniPortForwardingAllowedByPolicy preference is false.";
    return false;
  }
  // If kCrostiniPortForwardingAllowedByPolicy is not false, then we know that
  // the user is either unmanaged, the policy is not set or the policy is set
  // as true. In either of those 3 cases, port forwarding is allowed.
  return true;
}

bool CrostiniFeatures::IsBaguette(Profile* profile) const {
  bool is_baguette = false;
  const base::Value::List& container_list =
      profile->GetPrefs()->GetList(guest_os::prefs::kGuestOsContainers);
  for (const auto& container : container_list) {
    guest_os::GuestId id(container);
    if (id.vm_type == guest_os::VmType::BAGUETTE) {
      is_baguette = true;
      break;
    }
  }

  return is_baguette;
}

bool CrostiniFeatures::IsMultiContainerAllowed(Profile* profile) const {
  return g_crostini_features->IsAllowedNow(profile) &&
         base::FeatureList::IsEnabled(ash::features::kCrostiniMultiContainer);
}

}  // namespace crostini
