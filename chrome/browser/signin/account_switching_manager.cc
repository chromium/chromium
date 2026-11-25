// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_switching_manager.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/numerics/clamped_math.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"

namespace signin {

namespace {

// Schema of the profile dictionary preference
// `prefs::kAccountSwitchingPromoState`:
//
// {
//   // Total number of times the promo was ignored without explicit action.
//   "ignore_count": 2,
//   // Timestamp of the last ignore.
//   "last_ignored_time": "<base::Time>",
//   // Timestamp of the last explicit dismissal ("Not now").
//   "last_explicit_dismissal_time": "<base::Time>"
// }
constexpr char kGlobalIgnoreCountKey[] = "ignore_count";
constexpr char kLastIgnoredTimeKey[] = "last_ignored_time";
constexpr char kLastExplicitDismissalTimeKey[] = "last_explicit_dismissal_time";

bool IsCooldownActive(const base::DictValue& state_dict,
                      std::string_view timestamp_key,
                      base::Time now,
                      base::TimeDelta cooldown) {
  std::optional<base::Time> timestamp =
      base::ValueToTime(state_dict.Find(timestamp_key));
  // Ignore null timestamps and timestamps in the future (e.g. after backward
  // wall-clock adjustment) so promos do not wedge indefinitely.
  return timestamp.has_value() && !timestamp->is_null() && *timestamp <= now &&
         now - *timestamp < cooldown;
}

}  // namespace

// static
bool AccountSwitchingManager::IsFeatureEnabled() {
  return base::FeatureList::IsEnabled(switches::kEnableAccountPreviewData) &&
         base::FeatureList::IsEnabled(
             switches::kEnableAccountPreviewSwitchingAccount);
}

// static
void AccountSwitchingManager::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kAccountSwitchingPromoState);
}

AccountSwitchingManager::AccountSwitchingManager(
    PrefService* prefs,
    SigninClient* signin_client,
    IdentityManager* identity_manager,
    AccountPreviewDataService* account_preview_data_service)
    : prefs_(CHECK_DEREF(prefs)),
      signin_client_(CHECK_DEREF(signin_client)),
      identity_manager_(CHECK_DEREF(identity_manager)),
      account_preview_data_service_(account_preview_data_service) {}

AccountSwitchingManager::~AccountSwitchingManager() = default;

void AccountSwitchingManager::Shutdown() {
  active_switch_.reset();
}

std::optional<AccountPreviewDataService::AccountPreviewPreference>
AccountSwitchingManager::GetEligibleAccountForSwitching() const {
  if (!IsFeatureEnabled() || !account_preview_data_service_) {
    return std::nullopt;
  }

  if (!identity_manager_->AreRefreshTokensLoaded()) {
    return std::nullopt;
  }

  // TODO(crbug.com/585132231): Confirm with product whether a suppressed
  // `(A, B)` pair should fall back to a third eligible secondary account `C`.
  std::optional<AccountPreviewDataService::AccountPreviewPreference> candidate =
      account_preview_data_service_->GetPreferredAccountForSwitching();
  if (!candidate.has_value() || candidate->gaia_id.empty()) {
    return std::nullopt;
  }

  // Validate prompt-time candidate freshness in `IdentityManager`: the target
  // account must remain non-managed, non-child, and have a valid refresh token
  // in this profile.
  const AccountInfo target_account_info =
      identity_manager_->FindExtendedAccountInfoByGaiaId(candidate->gaia_id);
  if (target_account_info.IsEmpty() ||
      !identity_manager_->HasAccountWithRefreshToken(
          target_account_info.GetAccountId()) ||
      identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          target_account_info.GetAccountId()) ||
      target_account_info.IsManaged() == Tribool::kTrue ||
      target_account_info.IsMemberOfFlexOrg() ||
      (target_account_info.IsManaged() == Tribool::kUnknown &&
       AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
           target_account_info.GetEmail())) ||
      target_account_info.IsChildAccount() == Tribool::kTrue) {
    return std::nullopt;
  }

  // Prompt-time multi-profile freshness check: never surface a candidate that
  // has since been signed in as the primary account of another profile.
  if (signin_client_->IsAccountSignedInOnOtherProfile(candidate->gaia_id)) {
    return std::nullopt;
  }

  return candidate;
}

bool AccountSwitchingManager::CanShowPromo(const GaiaId& primary_gaia_id,
                                           const GaiaId& target_gaia_id) const {
  if (!CanStartSwitching(primary_gaia_id, target_gaia_id)) {
    return false;
  }

  if (switches::kEnableAccountPreviewSwitchingAccountSkipCooldown.Get()) {
    return true;
  }

  const base::Time now = base::Time::Now();
  const SigninPrefs signin_prefs(*prefs_);

  // 28-day primary-account sign-in/switch grace period.
  std::optional<base::Time> last_signin_time =
      signin_prefs.GetChromeLastSigninTime(primary_gaia_id);
  if (!last_signin_time.has_value() || last_signin_time->is_null() ||
      now - *last_signin_time < kPrimaryAccountSigninGracePeriod) {
    return false;
  }

  const base::DictValue& state_dict =
      prefs_->GetDict(prefs::kAccountSwitchingPromoState);

  // Global ignore cap (5) and 7-day ignore cooldown.
  if (state_dict.FindInt(kGlobalIgnoreCountKey).value_or(0) >=
      kMaxGlobalIgnores) {
    return false;
  }
  if (IsCooldownActive(state_dict, kLastIgnoredTimeKey, now,
                       kGlobalIgnoreCooldown)) {
    return false;
  }

  // Global 28-day cooldown after an explicit dismissal ("Not now").
  if (IsCooldownActive(state_dict, kLastExplicitDismissalTimeKey, now,
                       kGlobalExplicitDismissalCooldown)) {
    return false;
  }

  // Bi-directional per-pair `{primary_gaia_id, target_gaia_id}` suppression
  // rules in `SigninPrefs`.
  if (signin_prefs.IsAccountSwitchingPromoSuppressedBySwitch(primary_gaia_id,
                                                             target_gaia_id)) {
    return false;
  }
  if (signin_prefs.GetAccountSwitchingPromoExplicitDismissalCount(
          primary_gaia_id, target_gaia_id) >= kMaxPairExplicitDismissals) {
    return false;
  }

  // TODO(crbug.com/585132231): Check profile-level post-acceptance
  // suppression or post-new-profile-creation cooldown here once product
  // confirms re-prompting rules.
  return true;
}

void AccountSwitchingManager::RecordPromoIgnored() {
  ScopedDictPrefUpdate update(*prefs_, prefs::kAccountSwitchingPromoState);
  int new_ignore_count =
      base::ClampAdd(update->FindInt(kGlobalIgnoreCountKey).value_or(0), 1);
  update->Set(kGlobalIgnoreCountKey, new_ignore_count);
  update->Set(kLastIgnoredTimeKey, base::TimeToValue(base::Time::Now()));
}

void AccountSwitchingManager::RecordPromoExplicitlyDismissed(
    const GaiaId& primary_gaia_id,
    const GaiaId& target_gaia_id) {
  CHECK(!primary_gaia_id.empty());
  CHECK(!target_gaia_id.empty());
  CHECK_NE(primary_gaia_id, target_gaia_id);

  ScopedDictPrefUpdate update(*prefs_, prefs::kAccountSwitchingPromoState);
  update->Set(kLastExplicitDismissalTimeKey,
              base::TimeToValue(base::Time::Now()));

  // Stored bi-directionally in `SigninPrefs` for
  // `{primary_gaia_id, target_gaia_id}`.
  SigninPrefs(*prefs_).IncrementAccountSwitchingPromoExplicitDismissalCount(
      primary_gaia_id, target_gaia_id);
}

bool AccountSwitchingManager::StartSwitching(
    const GaiaId& primary_gaia_id,
    const GaiaId& target_gaia_id,
    base::OnceClosure on_switch_finished) {
  if (!CanStartSwitching(primary_gaia_id, target_gaia_id)) {
    return false;
  }

  active_switch_ = ActiveSwitch{primary_gaia_id, target_gaia_id,
                                std::move(on_switch_finished)};

  // TODO(crbug.com/585132231): Persist profile-level post-acceptance
  // suppression or post-new-profile-creation cooldown once product confirms
  // re-prompting rules.
  // TODO(crbug.com/585131182, crbug.com/585132335): Execute the in-profile
  // primary-account switch (which also starts a fresh 28-day grace period and
  // activates the 7-day switch-back window) or async new-profile creation
  // here, calling `OnSwitchFinished(success)` when the switch completes or
  // aborts.
  return true;
}

void AccountSwitchingManager::CompleteSwitchForTesting(bool success) {
  OnSwitchFinished(success);
}

bool AccountSwitchingManager::CanStartSwitching(
    const GaiaId& primary_gaia_id,
    const GaiaId& target_gaia_id) const {
  if (!IsFeatureEnabled()) {
    return false;
  }
  if (!prefs_->GetBoolean(prefs::kSigninAllowed)) {
    return false;
  }
  if (primary_gaia_id.empty() || target_gaia_id.empty() ||
      primary_gaia_id == target_gaia_id) {
    return false;
  }
  // Prompt-time / switch-time multi-profile freshness check: never promote or
  // switch to an account already signed in as primary on another profile.
  if (signin_client_->IsAccountSignedInOnOtherProfile(target_gaia_id)) {
    return false;
  }
  // In-progress switch and active 7-day switch-back grace window guards prevent
  // overlapping switches or inconsistent switch-back state and are never
  // bypassed by `SkipCooldown`.
  return !active_switch_.has_value() && !IsSwitchBackWindowActive();
}

bool AccountSwitchingManager::IsSwitchBackWindowActive() const {
  // TODO(crbug.com/585132769): Query the 7-day Profile Menu switch-back row
  // pref state in `prefs_` once implemented.
  return false;
}

void AccountSwitchingManager::OnSwitchFinished(bool success) {
  CHECK(active_switch_.has_value());
  ActiveSwitch completed_switch = std::move(*active_switch_);
  active_switch_.reset();

  if (success) {
    // Record switch-based pair suppression bi-directionally in `SigninPrefs`
    // so accepting a switch for `A -> B` prevents promoting either direction
    // while both accounts remain in the profile.
    SigninPrefs(*prefs_).SetAccountSwitchingPromoSuppressedBySwitch(
        completed_switch.primary_gaia_id, completed_switch.target_gaia_id);
  }

  if (completed_switch.on_finished_callback) {
    std::move(completed_switch.on_finished_callback).Run();
  }
}

}  // namespace signin
