// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_features.h"
#include <memory>
#include <string>

#include "ash/components/settings/cros_settings_names.h"
#include "base/callback.h"
#include "base/cpu.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/guest_os/infra/cached_callback.h"
#include "chrome/browser/ash/guest_os/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/prefs/pref_service.h"
#include "third_party/re2/src/re2/re2.h"

using AllowStatus = borealis::BorealisFeatures::AllowStatus;

namespace borealis {

namespace {

// The special borealis variants distinguish internal developer-only boards
// used by the borealis team for testing. They are not publicly available.
constexpr char kOverrideHardwareChecksBoardSuffix[] = "-borealis";

constexpr const char* kAllowedModelNames[] = {
    "delbin", "voxel", "volta", "lindar", "elemi", "volet", "drobit"};

constexpr int64_t kGibi = 1024 * 1024 * 1024;
constexpr int64_t kMinimumMemoryBytes = 7 * kGibi;

// Matches i5 and i7 of the 11th generation and up.
constexpr char kMinimumCpuRegex[] = "[1-9][1-9].. Gen.*i[57]-";

AllowStatus GetAsyncAllowStatus() {
  // First, avoid excluding -borealis variants, which are dev-only boards that
  // get a free pass.
  if (base::EndsWith(base::SysInfo::GetLsbReleaseBoard(),
                     kOverrideHardwareChecksBoardSuffix)) {
    // Note: the comment on GetLsbReleaseBoard() (rightly) points out that we're
    // not supposed to use LsbReleaseBoard directly, but rather set a flag in
    // the overlay. I am not doing that as the following check is only a
    // temporary hack necessary while we release borealis, but will be removed
    // shortly afterwards. This check can fail in either direction and we won't
    // be too upset.
    return AllowStatus::kAllowed;
  }

  // Second, exclude variants of the boards that we don't expect to work on.
  std::string model_name;
  if (!chromeos::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          chromeos::system::kCustomizationIdKey, &model_name)) {
    return AllowStatus::kHardwareChecksFailed;
  }
  bool found = false;
  for (const char* allowed_model : kAllowedModelNames) {
    if (model_name == allowed_model) {
      found = true;
      break;
    }
  }
  if (!found)
    return AllowStatus::kUnsupportedModel;

  // Third, check system requirements.
  if (base::SysInfo::AmountOfPhysicalMemory() < kMinimumMemoryBytes) {
    return AllowStatus::kHardwareChecksFailed;
  } else if (!RE2::PartialMatch(
                 base::CPU::GetInstanceNoAllocation().cpu_brand(),
                 kMinimumCpuRegex)) {
    return AllowStatus::kHardwareChecksFailed;
  }

  return AllowStatus::kAllowed;
}

}  // namespace

class AsyncAllowChecker : public guest_os::CachedCallback<AllowStatus, bool> {
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

    chromeos::system::StatisticsProvider::GetInstance()
        ->ScheduleOnMachineStatisticsLoaded(base::BindOnce(
            [](RealCallback callback) {
              base::ThreadPool::PostTaskAndReplyWithResult(
                  FROM_HERE, base::MayBlock(),
                  base::BindOnce(&GetAsyncAllowStatus),
                  base::BindOnce(
                      [](RealCallback callback, AllowStatus status) {
                        // "Success" here means we successfully determined the
                        // status, which we can't really fail to do because any
                        // failure to determine something is treated as a
                        // disallowed status.
                        std::move(callback).Run(Success(status));
                      },
                      std::move(callback)));
            },
            std::move(callback)));
  }
};

BorealisFeatures::BorealisFeatures(Profile* profile)
    : profile_(profile), async_checker_(std::make_unique<AsyncAllowChecker>()) {
  // Issue a request for the async status immediately upon creation, in case
  // it's needed.
  async_checker_->Get(base::DoNothing());
}

BorealisFeatures::~BorealisFeatures() = default;

void BorealisFeatures::IsAllowed(
    base::OnceCallback<void(AllowStatus)> callback) {
  AllowStatus partial_status = MightBeAllowed();
  if (partial_status != AllowStatus::kAllowed) {
    std::move(callback).Run(partial_status);
    return;
  }
  async_checker_->Get(base::BindOnce(
      [](base::OnceCallback<void(AllowStatus)> callback,
         AsyncAllowChecker::Result result) {
        if (result) {
          std::move(callback).Run(*result.Value());
          return;
        }
        std::move(callback).Run(AllowStatus::kFailedToDetermine);
      },
      std::move(callback)));
}

AllowStatus BorealisFeatures::MightBeAllowed() {
  if (!base::FeatureList::IsEnabled(features::kBorealis))
    return AllowStatus::kFeatureDisabled;

  if (!virtual_machines::AreVirtualMachinesAllowedByPolicy())
    return AllowStatus::kVmPolicyBlocked;

  if (!profile_ || !profile_->IsRegularProfile())
    return AllowStatus::kBlockedOnIrregularProfile;

  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile_))
    return AllowStatus::kBlockedOnNonPrimaryProfile;

  if (profile_->IsChild())
    return AllowStatus::kBlockedOnChildAccount;

  const PrefService::Preference* user_allowed_pref =
      profile_->GetPrefs()->FindPreference(prefs::kBorealisAllowedForUser);
  if (!user_allowed_pref || !user_allowed_pref->GetValue()->GetBool())
    return AllowStatus::kUserPrefBlocked;

  // For managed users the preference must be explicitly set true. So we block
  // in the case where the user is managed and the pref isn't.
  if (!user_allowed_pref->IsManaged() &&
      profile_->GetProfilePolicyConnector()->IsManaged()) {
    return AllowStatus::kUserPrefBlocked;
  }
  return AllowStatus::kAllowed;
}

bool BorealisFeatures::IsEnabled() {
  if (MightBeAllowed() != AllowStatus::kAllowed)
    return false;
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
    case AllowStatus::kUnsupportedModel:
      return os << "Borealis is not supported on this model hardware";
    case AllowStatus::kHardwareChecksFailed:
      return os << "Insufficient CPU/Memory to run Borealis";
  }
}
