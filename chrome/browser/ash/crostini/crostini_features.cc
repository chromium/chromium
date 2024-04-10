// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_features.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/guest_os/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/settings/cros_settings.h"
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

bool IsArcManagedAdbSideloadingAllowedByUserPolicy(
    crostini::CrostiniArcAdbSideloadingUserAllowanceMode user_policy) {
  if (user_policy ==
      crostini::CrostiniArcAdbSideloadingUserAllowanceMode::kAllow) {
    return true;
  }

  DVLOG(1) << "adb sideloading is not allowed by the user policy";
  return false;
}

using CanChangeAdbSideloadingCallback =
    crostini::CrostiniFeatures::CanChangeAdbSideloadingCallback;

// Reads the value of DeviceCrostiniArcAdbSideloadingAllowed. If the value is
// temporarily untrusted the |callback| will be invoked later when trusted
// values are available.
void CanChangeAdbSideloadingOnManagedDevice(
    CanChangeAdbSideloadingCallback callback,
    bool is_profile_enterprise_managed,
    bool is_affiliated_user,
    crostini::CrostiniArcAdbSideloadingUserAllowanceMode user_policy) {
  // Split |callback| in 2 OnceCallbacks. This is necessary to cater to the
  // somewhat awkward PrepareTrustedValues interface, which for some return
  // values invokes the callback passed to it, and for others requires the code
  // here to do so.
  auto split_callback = base::SplitOnceCallback(std::move(callback));

  auto* const cros_settings = ash::CrosSettings::Get();
  auto status = cros_settings->PrepareTrustedValues(base::BindOnce(
      &CanChangeAdbSideloadingOnManagedDevice, std::move(split_callback.first),
      is_profile_enterprise_managed, is_affiliated_user, user_policy));

  if (status != ash::CrosSettingsProvider::TRUSTED) {
    return;
  }

  // Get the updated policy.
  int crostini_arc_abd_sideloading_device_allowance_mode = -1;
  if (!cros_settings->GetInteger(
          ash::kDeviceCrostiniArcAdbSideloadingAllowed,
          &crostini_arc_abd_sideloading_device_allowance_mode)) {
    // If the device policy is not set, adb sideloading is not allowed
    DVLOG(1) << "adb sideloading device policy is not set, therefore "
                "sideloading is not allowed";
    std::move(split_callback.second).Run(false);
    return;
  }

  using Mode =
      enterprise_management::DeviceCrostiniArcAdbSideloadingAllowedProto;

  bool is_adb_sideloading_allowed_by_device_policy;
  switch (crostini_arc_abd_sideloading_device_allowance_mode) {
    case Mode::DISALLOW:
    case Mode::DISALLOW_WITH_POWERWASH:
      is_adb_sideloading_allowed_by_device_policy = false;
      break;
    case Mode::ALLOW_FOR_AFFILIATED_USERS:
      is_adb_sideloading_allowed_by_device_policy = true;
      break;
    default:
      is_adb_sideloading_allowed_by_device_policy = false;
      break;
  }

  if (is_adb_sideloading_allowed_by_device_policy) {
    if (is_profile_enterprise_managed) {
      if (!is_affiliated_user) {
        DVLOG(1) << "adb sideloading not allowed because user is not "
                    "affiliated with the device";
        std::move(split_callback.second).Run(false);
        return;
      }
      std::move(split_callback.second)
          .Run(IsArcManagedAdbSideloadingAllowedByUserPolicy(user_policy));
      return;
    }

    DVLOG(1) << "adb sideloading is unsupported for this managed device";
    std::move(split_callback.second).Run(false);
    return;
  }

  DVLOG(1) << "adb sideloading is not allowed by the device policy";
  std::move(split_callback.second).Run(false);
}

void CanChangeManagedAdbSideloading(
    bool is_device_enterprise_managed,
    bool is_profile_enterprise_managed,
    bool is_owner_profile,
    bool is_affiliated_user,
    crostini::CrostiniArcAdbSideloadingUserAllowanceMode user_policy,
    CanChangeAdbSideloadingCallback callback) {
  DCHECK(is_device_enterprise_managed || is_profile_enterprise_managed);

  if (!base::FeatureList::IsEnabled(
          ash::features::kArcManagedAdbSideloadingSupport)) {
    DVLOG(1) << "adb sideloading is disabled by a feature flag";
    std::move(callback).Run(false);
    return;
  }

  if (is_device_enterprise_managed) {
    CanChangeAdbSideloadingOnManagedDevice(std::move(callback),
                                           is_profile_enterprise_managed,
                                           is_affiliated_user, user_policy);
    return;
  }

  if (is_owner_profile) {
    // We know here that the profile is enterprise-managed so no need to check
    std::move(callback).Run(
        IsArcManagedAdbSideloadingAllowedByUserPolicy(user_policy));
    return;
  }

  DVLOG(1) << "Only the owner can change adb sideloading status";
  std::move(callback).Run(false);
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

bool CrostiniFeatures::CouldBeAllowed(Profile* profile, std::string* reason) {
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
      ash::ProfileHelper::IsEphemeralUserProfile(profile) ||
      ash::ProfileHelper::IsLockScreenAppProfile(profile)) {
    VLOG(1) << "Profile is not allowed to run crostini.";
    *reason = "This user session is not allowed to run crostini";
    return false;
  }

  return true;
}

bool CrostiniFeatures::CouldBeAllowed(Profile* profile) {
  std::string reason;
  return CouldBeAllowed(profile, &reason);
}

bool CrostiniFeatures::IsAllowedNow(Profile* profile, std::string* reason) {
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

bool CrostiniFeatures::IsAllowedNow(Profile* profile) {
  std::string reason;
  return IsAllowedNow(profile, &reason);
}

bool CrostiniFeatures::IsEnabled(Profile* profile) {
  return g_crostini_features->IsAllowedNow(profile) &&
         profile->GetPrefs()->GetBoolean(crostini::prefs::kCrostiniEnabled);
}

bool CrostiniFeatures::IsExportImportUIAllowed(Profile* profile) {
  return g_crostini_features->IsAllowedNow(profile) &&
         profile->GetPrefs()->GetBoolean(
             crostini::prefs::kUserCrostiniExportImportUIAllowedByPolicy);
}

bool CrostiniFeatures::IsRootAccessAllowed(Profile* profile) {
  if (base::FeatureList::IsEnabled(features::kCrostiniAdvancedAccessControls)) {
    return profile->GetPrefs()->GetBoolean(
        crostini::prefs::kUserCrostiniRootAccessAllowedByPolicy);
  }
  return true;
}

bool CrostiniFeatures::IsContainerUpgradeUIAllowed(Profile* profile) {
  return g_crostini_features->IsAllowedNow(profile);
}

void CrostiniFeatures::CanChangeAdbSideloading(
    Profile* profile,
    CanChangeAdbSideloadingCallback callback) {
  // First rule out a child account as it is a special case - a child can be an
  // owner, but ADB sideloading is currently not supported for this case
  if (profile->IsChild()) {
    DVLOG(1) << "adb sideloading is currently unsupported for child accounts";
    std::move(callback).Run(false);
    return;
  }

  // Check the managed device and/or user case
  auto* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  bool is_device_enterprise_managed = connector->IsDeviceEnterpriseManaged();
  bool is_profile_enterprise_managed =
      profile->GetProfilePolicyConnector()->IsManaged();
  bool is_owner_profile = ash::ProfileHelper::IsOwnerProfile(profile);
  if (is_device_enterprise_managed || is_profile_enterprise_managed) {
    auto user_policy =
        static_cast<crostini::CrostiniArcAdbSideloadingUserAllowanceMode>(
            profile->GetPrefs()->GetInteger(
                crostini::prefs::kCrostiniArcAdbSideloadingUserPref));

    const auto* user = ash::ProfileHelper::Get()->GetUserByProfile(profile);
    bool is_affiliated_user = user && user->IsAffiliated();

    CanChangeManagedAdbSideloading(
        is_device_enterprise_managed, is_profile_enterprise_managed,
        is_owner_profile, is_affiliated_user, user_policy, std::move(callback));
    return;
  }

  // Here we are sure that the user is not enterprise-managed and we therefore
  // only check whether the user is the owner
  if (!is_owner_profile) {
    DVLOG(1) << "Only the owner can change adb sideloading status";
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

bool CrostiniFeatures::IsPortForwardingAllowed(Profile* profile) {
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

bool CrostiniFeatures::IsMultiContainerAllowed(Profile* profile) {
  return g_crostini_features->IsAllowedNow(profile) &&
         base::FeatureList::IsEnabled(ash::features::kCrostiniMultiContainer);
}

}  // namespace crostini
