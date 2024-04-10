// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_features.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/borealis/borealis_hardware_checker.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/guest_os/infra/cached_callback.h"
#include "chrome/browser/ash/guest_os/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"

using AllowStatus = borealis::BorealisFeatures::AllowStatus;

namespace borealis {

class AsyncHardwareChecker
    : public guest_os::CachedCallback<AllowStatus, bool> {
 public:
  AsyncHardwareChecker() = default;

 private:
  void Build(RealCallback callback) override {
    // Testing hardware capabilities in unit tests is kindof pointless. The
    // following check bypasses any attempt to do async checks unless we're
    // running on a real CrOS device.
    //
    // Also do this first so we don't have to mock out statistics providers and
    // other things in tests.
    if (!base::SysInfo::IsRunningOnChromeOS()) {
      std::move(callback).Run(Success(AllowStatus::kAllowed));
      return;
    }

    // If the user has opted in to running on unsupported hardware then we don't
    // need to actually check it.
    if (base::FeatureList::IsEnabled(
            ash::features::kBorealisEnableUnsupportedHardware)) {
      std::move(callback).Run(Success(AllowStatus::kAllowed));
      return;
    }

    HasSufficientHardware(base::BindOnce(
        [](RealCallback callback, bool has_sufficient_hardware) {
          // "Success" here means we successfully determined the
          // status, which we can't really fail to do because any
          // failure to determine something is treated as a
          // disallowed status.
          std::move(callback).Run(Success(
              has_sufficient_hardware ? AllowStatus::kAllowed
                                      : AllowStatus::kInsufficientHardware));
        },
        std::move(callback)));
  }
};

BorealisFeatures::BorealisFeatures(Profile* profile)
    : profile_(profile),
      async_checker_(std::make_unique<AsyncHardwareChecker>()) {
  // Issue a request for the status immediately upon creation, in case
  // it's needed later.
  IsAllowed(base::DoNothing());
}

BorealisFeatures::~BorealisFeatures() = default;

void BorealisFeatures::IsAllowed(
    base::OnceCallback<void(AllowStatus)> callback) {
  AllowStatus partial_status = PreHardwareChecks();
  if (partial_status != AllowStatus::kAllowed) {
    std::move(callback).Run(partial_status);
    return;
  }
  async_checker_->Get(base::BindOnce(&BorealisFeatures::OnHardwareChecked,
                                     weak_factory_.GetWeakPtr(),
                                     std::move(callback)));
}

AllowStatus BorealisFeatures::PreHardwareChecks() {
  // Only put failures here if the user has no means of changing them.  I.e.
  // failures here should be as set-in-stone as hardware.
  if (!base::FeatureList::IsEnabled(features::kBorealis)) {
    return AllowStatus::kFeatureDisabled;
  }

  return AllowStatus::kAllowed;
}

AllowStatus BorealisFeatures::PostHardwareChecks() {
  // Failures here should be avoidable (in some sense) without users going and
  // replacing their hardware.
  if (!virtual_machines::AreVirtualMachinesAllowedByPolicy()) {
    return AllowStatus::kVmPolicyBlocked;
  }

  if (!profile_ || !profile_->IsRegularProfile()) {
    return AllowStatus::kBlockedOnIrregularProfile;
  }

  if (!ash::ProfileHelper::IsPrimaryProfile(profile_)) {
    return AllowStatus::kBlockedOnNonPrimaryProfile;
  }

  if (profile_->IsChild()) {
    return AllowStatus::kBlockedOnChildAccount;
  }

  const PrefService::Preference* user_allowed_pref =
      profile_->GetPrefs()->FindPreference(prefs::kBorealisAllowedForUser);
  if (!user_allowed_pref || !user_allowed_pref->GetValue()->GetBool()) {
    return AllowStatus::kUserPrefBlocked;
  }

  // For managed users the preference must be explicitly set true. So we block
  // in the case where the user is managed and the pref isn't.
  //
  // TODO(b/213398438): We migrated to using `default_for_enterprise_users` in
  // crrev.com/c/4121754, which means we should remove the below code since an
  // enterprise user will always have the policy set to its default (false).
  if (!user_allowed_pref->IsManaged() &&
      profile_->GetProfilePolicyConnector()->IsManaged()) {
    return AllowStatus::kUserPrefBlocked;
  }

  if (!base::FeatureList::IsEnabled(ash::features::kBorealisPermitted)) {
    return AllowStatus::kBlockedByFlag;
  }

  return AllowStatus::kAllowed;
}

void BorealisFeatures::OnHardwareChecked(
    base::OnceCallback<void(AllowStatus)> callback,
    AsyncHardwareChecker::Result hardware_status) {
  if (!hardware_status.has_value()) {
    std::move(callback).Run(AllowStatus::kFailedToDetermine);
    return;
  }
  if (*hardware_status.value() != AllowStatus::kAllowed) {
    std::move(callback).Run(*hardware_status.value());
    return;
  }
  std::move(callback).Run(PostHardwareChecks());
}

bool BorealisFeatures::IsEnabled() {
  return profile_->GetPrefs()->GetBoolean(prefs::kBorealisInstalledOnDevice);
}

}  // namespace borealis

std::ostream& operator<<(std::ostream& os, const AllowStatus& reason) {
  switch (reason) {
    case AllowStatus::kAllowed:
      return os << "Borealis is allowed";
    case AllowStatus::kFeatureDisabled:
      return os << "Borealis has not been released on this device";
    case AllowStatus::kFailedToDetermine:
      return os << "Could not verify that Borealis was allowed. Please Retry "
                   "in a bit";
    case AllowStatus::kBlockedOnIrregularProfile:
      return os << "Borealis is only available on normal login sessions";
    case AllowStatus::kBlockedOnNonPrimaryProfile:
      return os << "Borealis is only available on the primary profile";
    case AllowStatus::kBlockedOnChildAccount:
      return os << "Borealis is not available on child accounts";
    case AllowStatus::kVmPolicyBlocked:
      return os << "Your admin has blocked borealis (virtual machines are "
                   "disabled)";
    case AllowStatus::kUserPrefBlocked:
      return os << "Your admin has blocked borealis (for your account)";
    case AllowStatus::kBlockedByFlag:
      return os << "Borealis is still being worked on. You must set the "
                   "#borealis-enabled feature flag.";
    case AllowStatus::kInsufficientHardware:
      return os << "Borealis is not supported on this hardware";
  }
}
