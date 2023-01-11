// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_features.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "borealis_features_util.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/guest_os/infra/cached_callback.h"
#include "chrome/browser/ash/guest_os/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"

using AllowStatus = borealis::BorealisFeatures::AllowStatus;

namespace borealis {

namespace {

constexpr uint64_t kGibi = 1024ull * 1024 * 1024;

// Used to make it difficult to tell what someone's token is based on their
// prefs.
constexpr char kSaltForPrefStorage[] = "/!RoFN8,nDxiVgTI6CvU";

// Regex used for CPU checks on intel processors, this means "any 11th
// generation or greater i5/i7 processor".
constexpr char kBorealisCapableIntelCpuRegex[] = "[1-9][1-9].. Gen.*i[357]-";

// Checks the current hardware+token configuration to determine if the user
// should be able to run borealis.
//
// If you are supposed to know the correct token, then you will be able to
// find it ~if you go to the place we all know and love~.
class FullChecker : public TokenHardwareChecker {
 public:
  explicit FullChecker(Data data) : TokenHardwareChecker(std::move(data)) {}

  AllowStatus Check() const {
    // Tokens provide more fine-grained control over whether borealis can be run
    // on a specific device. The different kinds of token are:
    //  * "Super" token: Allows borealis on any device.
    //  * "Test" token: Allows borealis on any device with sufficient hardware
    //    (where *-borealis boards are always considered sufficient).
    //  * /board token: Similar to the super token, but only works for a subset
    //  of boards.
    //
    // All tokens will only function if borealis is already available on that
    // board based on its use flags.

    // The "super" token.
    if (TokenHashMatches("i9n6HT3+3Bo:C1p^_qk!\\",
                         "X1391g+2yiuBQrceA3gRGrT7+DQcaYGR/GkmFscyOfQ=")) {
      LOG(WARNING) << "Super-token provided, bypassing hardware checks.";
      return AllowStatus::kAllowed;
    }

    // The "test" token.
    if (TokenHashMatches("MpOI9+d58she4,97rI",
                         "Eec1m+UrIkLUu3L6mV+5zTYZId6HJ+vz+50MseJJaGw=")) {
      LOG(WARNING) << "Test-token provided, bypassing hardware checks.";
      return AllowStatus::kAllowed;
    }

    // The board-specific tokens.
    if (BoardIn({"hatch-borealis", "puff-borealis", "zork-borealis",
                 "volteer-borealis", "aurora-borealis"})) {
      if (TokenHashMatches("MXlY+SFZ!2,P_k^02]hK",
                           "FbxB2mxNa/uqskX4X+NqHhAE6ebHeWC0u+Y+UlGEB/4=")) {
        LOG(WARNING) << "Dogfooder token provided, bypassing hardware checks.";
        return AllowStatus::kAllowed;
      }
      return AllowStatus::kIncorrectToken;
    } else if (IsBoard("volteer")) {
      if (TokenHashMatches("w/8GMLXyB.EOkFaP/-AA",
                           "waiTIRjxZCFjFIRkuUVlnAbiDOMBSzyp3iSJl5x3YwA=")) {
        LOG(WARNING) << "Vendor token provided, bypassing hardware checks.";
        return AllowStatus::kAllowed;
      }
      // Volteer is released, so it is allowed as long as the device has an 11th
      // gen i5-i7 with 8G memory and is the correct model.
      if (!ModelIn({"delbin", "voxel", "volta", "lindar", "elemi", "volet",
                    "drobit", "lillipup", "delbing", "eldrid", "chronicler"})) {
        return AllowStatus::kUnsupportedModel;
      }
      return ReleasedBoardChecks(kBorealisCapableIntelCpuRegex);
    } else if (BoardIn({"brya", "adlrvp", "brask"})) {
      if (TokenHashMatches("tPl24iMxXNR,w$h6,g",
                           "LWULWUcemqmo6Xvdu2LalOYOyo/V4/CkljTmAneXF+U=")) {
        LOG(WARNING) << "Vendor token provided, bypassing hardware checks.";
        return AllowStatus::kAllowed;
      }
      return ReleasedBoardChecks(kBorealisCapableIntelCpuRegex);
    } else if (BoardIn({"guybrush", "majolica"})) {
      if (TokenHashMatches("^_GkTVWDP.FQo5KclS",
                           "ftqv2wT3qeJKajioXqd+VrEW34CciMsigH3MGfMiMsU=")) {
        LOG(WARNING) << "Vendor token provided, bypassing hardware checks.";
        return AllowStatus::kAllowed;
      }
      return ReleasedBoardChecks("Ryzen [357]");
    } else if (IsBoard("draco")) {
      return AllowStatus::kAllowed;
    }
    return AllowStatus::kIncorrectToken;
  }

  // Similar to the above, but also constructs the checker.
  static AllowStatus BuildAndCheck(Data data) {
    return FullChecker(std::move(data)).Check();
  }

 private:
  // Returns the allow status for a standard released board.
  AllowStatus ReleasedBoardChecks(const std::string& cpu_regex) const {
    if (!HasMemory(7 * kGibi)) {
      return AllowStatus::kHardwareChecksFailed;
    }
    return CpuRegexMatches(cpu_regex) ? AllowStatus::kAllowed
                                      : AllowStatus::kHardwareChecksFailed;
  }
};

}  // namespace

class AsyncAllowChecker : public guest_os::CachedCallback<AllowStatus, bool> {
 public:
  explicit AsyncAllowChecker(Profile* profile) : profile_(profile) {}

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

    // Bail out if the prefs service is not up and running, this just means we
    // retry later.
    if (!profile_ || !profile_->GetPrefs()) {
      std::move(callback).Run(Failure(Reject()));
      return;
    }

    TokenHardwareChecker::GetData(
        profile_->GetPrefs()->GetString(prefs::kBorealisVmTokenHash),
        base::BindOnce(
            [](RealCallback callback, TokenHardwareChecker::Data data) {
              base::ThreadPool::PostTaskAndReplyWithResult(
                  FROM_HERE, base::MayBlock(),
                  base::BindOnce(&FullChecker::BuildAndCheck, std::move(data)),
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

  Profile* const profile_;
};

BorealisFeatures::BorealisFeatures(Profile* profile)
    : profile_(profile),
      async_checker_(std::make_unique<AsyncAllowChecker>(profile_)) {
  // Issue a request for the status immediately upon creation, in case
  // it's needed later.
  IsAllowed(base::DoNothing());
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
  if (!base::FeatureList::IsEnabled(features::kBorealis)) {
    return AllowStatus::kFeatureDisabled;
  }

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

  version_info::Channel c = chrome::GetChannel();
  if (c == version_info::Channel::STABLE) {
    return AllowStatus::kBlockedOnStable;
  }

  if (!base::FeatureList::IsEnabled(ash::features::kBorealisPermitted)) {
    return AllowStatus::kBlockedByFlag;
  }

  return AllowStatus::kAllowed;
}

bool BorealisFeatures::IsEnabled() {
  if (MightBeAllowed() != AllowStatus::kAllowed) {
    return false;
  }
  return profile_->GetPrefs()->GetBoolean(prefs::kBorealisInstalledOnDevice);
}

void BorealisFeatures::SetVmToken(
    std::string token,
    base::OnceCallback<void(AllowStatus)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::MayBlock(),
      base::BindOnce(&TokenHardwareChecker::H, std::move(token),
                     kSaltForPrefStorage),
      base::BindOnce(&BorealisFeatures::OnVmTokenDetermined,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BorealisFeatures::OnVmTokenDetermined(
    base::OnceCallback<void(AllowStatus)> callback,
    std::string hashed_token) {
  // The user has given a new password, so we invalidate the old status first,
  // that way things which monitor the below pref don't accidentally re-use the
  // old status.
  async_checker_->Invalidate();
  // This has the effect that you could overwrite the correct token and disable
  // borealis. Adding extra code to avoid that is not worth while because end
  // users aren't supposed to have the correct token anyway.
  profile_->GetPrefs()->SetString(prefs::kBorealisVmTokenHash, hashed_token);
  // Finally, re-issue an allowedness check.
  IsAllowed(std::move(callback));
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
    case AllowStatus::kBlockedOnStable:
      return os << "Your ChromeOS channel must be set to Beta or Dev to run "
                   "Borealis";
    case AllowStatus::kBlockedByFlag:
      return os << "Borealis is still being worked on. You must set the "
                   "#borealis-enabled feature flag.";
    case AllowStatus::kUnsupportedModel:
      return os << "Borealis is not supported on this model hardware";
    case AllowStatus::kHardwareChecksFailed:
      return os << "Insufficient CPU/Memory to run Borealis";
    case AllowStatus::kIncorrectToken:
      return os << "Borealis needs a valid permission token";
  }
}
