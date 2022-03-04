// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_features.h"
#include <memory>
#include <string>

#include "ash/components/settings/cros_settings_names.h"
#include "ash/constants/ash_features.h"
#include "base/base64.h"
#include "base/callback.h"
#include "base/cpu.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/guest_os/infra/cached_callback.h"
#include "chrome/browser/ash/guest_os/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "crypto/sha2.h"
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

// Used to make it difficult to tell what someone's token is based on their
// prefs.
constexpr char kSaltForPrefStorage[] = "/!RoFN8,nDxiVgTI6CvU";

// A prime number chosen to give ~0.1s of wait time on my DUT.
constexpr unsigned kHashIterations = 100129;

// Returns the Board's name according to /etc/lsb-release. Strips any variant
// except the "-borealis" variant.
//
// Note: the comment on GetLsbReleaseBoard() (rightly) points out that we're
// not supposed to use LsbReleaseBoard directly, but rather set a flag in
// the overlay. I am not doing that as the following check is only a
// temporary hack necessary while we release borealis, but will be removed
// shortly afterwards. This check can fail in either direction and we won't
// be too upset.
std::string GetBoardName() {
  // In a developer build, the name "volteer" or "volteer-borealis" will become
  // "volteer-signed-mp-blahblah" and "volteer-borealis-signed..." on a signed
  // build, so we want to stop everything after the "-" unless its "-borealis".
  //
  // This means a variant like "volteer-kernelnext" will be treated as "volteer"
  // by us.
  std::vector<std::string> pieces =
      base::SplitString(base::SysInfo::GetLsbReleaseBoard(), "-",
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (pieces.size() >= 2 && pieces[1] == "borealis") {
    return pieces[0] + "-" + pieces[1];
  }
  DCHECK(!pieces.empty());
  return pieces[0];
}

// The below mechanism is not secure, and is not intended to be. It is a
// temporary measure that does not warrant any more effort. You might say
// it can be gamed 😎.
//
// Reminder: Don't Roll Your Own Crypto! Security should be left to the
// experts.
//
// TODO(b/218403711): This mechanism is temporary. It exists to allow borealis
// developers to verify that borealis functions correctly on the target
// platforms before releasing borealis broadly. We only need it because the
// boards we are targeting are publicly available, and going forward we will
// verify borealis is functioning on hardware before its public release.
std::string H(std::string input, const std::string& salt) {
  // Hashing is not strictly "blocking" since the cpu is probably busy, but best
  // not to call this method if you're on a thread that disallows blocking.
  base::ScopedBlockingCall sbc(FROM_HERE, base::BlockingType::WILL_BLOCK);
  std::string ret = std::move(input);
  for (int i = 0; i < kHashIterations; ++i) {
    std::string raw_sha = crypto::SHA256HashString(ret + salt);
    base::Base64Encode(raw_sha, &ret);
  }
  return ret;
}

enum class TokenAuthority {
  kRejected,
  kAllowedRequiresHardwareChecks,
  kAllowedOverridesHardwareChecks,
};

// Returns the degree to which we authorize the user's provided token. Some
// tokens are intended to allow developers/tests to avoid the hardware checks.
//
// If you are supposed to know the correct token, then you will be able to
// find it ~if you go to the place we all know and love~.
//
// For the maintainer:
//
// H(H("token", kSaltForPrefStorage), "salt") =
//   "aT79k1Uv7v7D5s2/rpYUJYRXTUq4EkPN2FK4JBQJWgw=";
TokenAuthority GetAuthorityForToken(const std::string& board,
                                    const std::string& hash_of_current_token) {
  // The following table shows in what situations various boards require
  // hardware checks or skip them, based on what token was provided:
  //
  //                  | super | test | /board
  //    ------------------------------------
  //    *-borealis    | skip  | skip | skip
  //    volteer       | skip  | yes  | yes
  //    brya          | skip  | yes  | skip
  //    monkey_island | skip  | yes  | skip
  //
  // TODO(b/222388986): The test and /board tokens are intended to do the same
  // thing, so add hardware checks to brya/monkey_island once we know what those
  // are.

  // The "super" token.
  if (H(hash_of_current_token, "i9n6HT3+3Bo:C1p^_qk!\\") ==
      "X1391g+2yiuBQrceA3gRGrT7+DQcaYGR/GkmFscyOfQ=") {
    LOG(WARNING) << "Super-token provided, bypassing hardware checks.";
    return TokenAuthority::kAllowedOverridesHardwareChecks;
  }

  // The "test" token.
  if (H(hash_of_current_token, "MpOI9+d58she4,97rI") ==
      "Eec1m+UrIkLUu3L6mV+5zTYZId6HJ+vz+50MseJJaGw=") {
    bool bypass_hardware =
        base::EndsWith(board, kOverrideHardwareChecksBoardSuffix);
    LOG(WARNING) << "Test-token provided, bypass_hardware=" << bypass_hardware;
    return bypass_hardware ? TokenAuthority::kAllowedOverridesHardwareChecks
                           : TokenAuthority::kAllowedRequiresHardwareChecks;
  }

  // The board-specific tokens.
  if (base::EndsWith(board, kOverrideHardwareChecksBoardSuffix)) {
    return H(hash_of_current_token, "MXlY+SFZ!2,P_k^02]hK") ==
                   "FbxB2mxNa/uqskX4X+NqHhAE6ebHeWC0u+Y+UlGEB/4="
               ? TokenAuthority::kAllowedOverridesHardwareChecks
               : TokenAuthority::kRejected;
  } else if (board == "volteer") {
    return H(hash_of_current_token, "F9sOMmgrk9%C$poxLT.Eg") ==
                   "Gn5gDfMLbMrBI10zrVba6q/1QEGJilyEyUeNiOID0X8="
               ? TokenAuthority::kAllowedRequiresHardwareChecks
               : TokenAuthority::kRejected;
  } else if (board == "brya") {
    return H(hash_of_current_token, "tPl24iMxXNR,w$h6,g") ==
                   "LWULWUcemqmo6Xvdu2LalOYOyo/V4/CkljTmAneXF+U="
               ? TokenAuthority::kAllowedOverridesHardwareChecks
               : TokenAuthority::kRejected;
  } else if (board == "guybrush" || board == "majolica") {
    return H(hash_of_current_token, "^_GkTVWDP.FQo5KclS") ==
                   "ftqv2wT3qeJKajioXqd+VrEW34CciMsigH3MGfMiMsU="
               ? TokenAuthority::kAllowedOverridesHardwareChecks
               : TokenAuthority::kRejected;
  }
  return TokenAuthority::kRejected;
}

// Returns the model name of this device (either from its CustomizationId or by
// parsing its hardware class). Returns "" if it fails.
std::string GetModelName() {
  std::string ret;
  if (chromeos::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          chromeos::system::kCustomizationIdKey, &ret)) {
    return ret;
  }
  LOG(WARNING)
      << "CustomizationId unavailable, attempting to parse hardware class";

  // As a fallback when the CustomizationId is not available, we try to parse it
  // out of the hardware class. If The hardware class is unavailable, all bets
  // are off.
  std::string hardware_class;
  if (!chromeos::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          chromeos::system::kHardwareClassKey, &hardware_class)) {
    return "";
  }

  // Hardware classes for the "modelname" model might look like this:
  //
  //    MODELNAME-FFFF DEAD-BEEF-HEX-JUNK
  //
  // (or "unknown" if we can't find it). So we only care about converting the
  // stuff before the first "-" into lowercase.
  //
  // Naively searching for the first hyphen is fine until we start caring about
  // models with hyphens in the name.
  size_t hyphen_pos = hardware_class.find('-');
  if (hyphen_pos != std::string::npos)
    hardware_class = hardware_class.substr(0, hyphen_pos);
  return base::ToLowerASCII(hardware_class);
}

AllowStatus GetAsyncAllowStatus(const std::string& hash_of_current_token) {
  // First, check the token.
  std::string board = GetBoardName();
  TokenAuthority auth = GetAuthorityForToken(board, hash_of_current_token);
  if (auth == TokenAuthority::kRejected) {
    LOG(WARNING) << "Incorrect token for board=" << board;
    return AllowStatus::kIncorrectToken;
  } else if (auth == TokenAuthority::kAllowedOverridesHardwareChecks) {
    return AllowStatus::kAllowed;
  }

  // Next, exclude variants of the boards that we don't expect to work on.
  std::string model_name = GetModelName();
  bool found = false;
  for (const char* allowed_model : kAllowedModelNames) {
    if (model_name == allowed_model) {
      found = true;
      break;
    }
  }
  if (!found) {
    LOG(WARNING) << "Borealis is not supported on \"" << model_name
                 << "\" models";
    return AllowStatus::kUnsupportedModel;
  }

  // Finally, check system requirements.
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

    chromeos::system::StatisticsProvider::GetInstance()
        ->ScheduleOnMachineStatisticsLoaded(base::BindOnce(
            [](RealCallback callback, const std::string& token_hash) {
              base::ThreadPool::PostTaskAndReplyWithResult(
                  FROM_HERE, base::MayBlock(),
                  base::BindOnce(&GetAsyncAllowStatus, token_hash),
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
            std::move(callback),
            profile_->GetPrefs()->GetString(prefs::kBorealisVmTokenHash)));
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

  version_info::Channel c = chrome::GetChannel();
  if (c == version_info::Channel::STABLE || c == version_info::Channel::BETA)
    return AllowStatus::kBlockedOnBetaStable;

  if (!base::FeatureList::IsEnabled(chromeos::features::kBorealisPermitted))
    return AllowStatus::kBlockedByFlag;

  return AllowStatus::kAllowed;
}

bool BorealisFeatures::IsEnabled() {
  if (MightBeAllowed() != AllowStatus::kAllowed)
    return false;
  return profile_->GetPrefs()->GetBoolean(prefs::kBorealisInstalledOnDevice);
}

void BorealisFeatures::SetVmToken(
    std::string token,
    base::OnceCallback<void(AllowStatus)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::MayBlock(),
      base::BindOnce(&H, std::move(token), kSaltForPrefStorage),
      base::BindOnce(&BorealisFeatures::OnVmTokenDetermined,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BorealisFeatures::OnVmTokenDetermined(
    base::OnceCallback<void(AllowStatus)> callback,
    std::string hashed_token) {
  // This has the effect that you could overwrite the correct token and disable
  // borealis. Adding extra code to avoid that is not worth while because end
  // users aren't supposed to have the correct token anyway.
  profile_->GetPrefs()->SetString(prefs::kBorealisVmTokenHash, hashed_token);
  // The user has given a new password, so we invalidate the old status.
  async_checker_->Invalidate();
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
    case AllowStatus::kBlockedOnBetaStable:
      return os << "Your ChromeOS channel must be set to Dev or Canary "
                   "to run Borealis";
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
