// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_switching_manager.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/notreached.h"
#include "base/numerics/clamped_math.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/public/base/consent_level.h"
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
  active_promo_.reset();
  active_switch_.reset();
}

bool AccountSwitchingManager::CanShowPromo() const {
  return GetEligiblePromoPair().has_value();
}

bool AccountSwitchingManager::ShowPromo(
    BrowserWindowInterface* browser_window) {
  if (!browser_window) {
    return false;
  }

  std::optional<AccountPair> promo_pair = GetEligiblePromoPair();
  if (!promo_pair.has_value()) {
    return false;
  }

  active_promo_ = std::move(*promo_pair);

  // TODO(crbug.com/585131565): Trigger the Smart Account Switching prompt
  // bubble via `SigninViewController::From(browser_window)` using a `WeakPtr`
  // invalidated in `Shutdown()` bound to `OnPromoResult()`, and ensure
  // never-shown bubble aborts report `SigninInterceptionResult::kNotDisplayed`.
  return true;
}

void AccountSwitchingManager::OnPromoResult(SigninInterceptionResult result) {
  CHECK(active_promo_.has_value());
  AccountPair promo = std::move(*active_promo_);
  active_promo_.reset();

  switch (result) {
    case SigninInterceptionResult::kAccepted:
      StartSwitching(std::move(promo));
      break;
    case SigninInterceptionResult::kDeclined:
      RecordPromoExplicitlyDismissed(promo);
      break;
    case SigninInterceptionResult::kIgnored:
    case SigninInterceptionResult::kDismissed:
      RecordPromoIgnored();
      break;
    case SigninInterceptionResult::kNotDisplayed:
      break;
    case SigninInterceptionResult::kAcceptedWithExistingProfile:
      NOTREACHED();
  }
}

void AccountSwitchingManager::ResolvePromoForTesting(
    SigninInterceptionResult result) {
  OnPromoResult(result);
}

void AccountSwitchingManager::CompleteSwitchForTesting(bool success) {
  OnSwitchFinished(success);
}

bool AccountSwitchingManager::IsPromoActiveForTesting() const {
  return active_promo_.has_value();
}

bool AccountSwitchingManager::IsSwitchInProgressForTesting() const {
  return active_switch_.has_value();
}

std::optional<AccountSwitchingManager::AccountPair>
AccountSwitchingManager::GetEligiblePromoPair(
    bool check_promo_eligibility) const {
  if ((check_promo_eligibility && active_promo_.has_value()) ||
      !IsProfileEligibleForSwitching() ||
      !identity_manager_->AreRefreshTokensLoaded()) {
    return std::nullopt;
  }

  std::optional<CoreAccountInfo> primary_account = GetEligiblePrimaryAccount();
  if (!primary_account.has_value()) {
    return std::nullopt;
  }

  std::optional<AccountPreviewDataService::AccountPreviewPreference> candidate =
      GetEligibleAccountForSwitching();
  if (!candidate.has_value() || candidate->gaia_id == primary_account->gaia) {
    return std::nullopt;
  }

  AccountPair pair{primary_account->gaia, candidate->gaia_id};
  if (check_promo_eligibility && !CanShowPromoForPair(pair)) {
    return std::nullopt;
  }

  return pair;
}

std::optional<CoreAccountInfo>
AccountSwitchingManager::GetEligiblePrimaryAccount() const {
  const CoreAccountInfo primary_account =
      identity_manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin);
  if (primary_account.IsEmpty() || primary_account.gaia.empty() ||
      !IsAccountEligible(
          identity_manager_->FindExtendedAccountInfo(primary_account))) {
    return std::nullopt;
  }

  return primary_account;
}

std::optional<AccountPreviewDataService::AccountPreviewPreference>
AccountSwitchingManager::GetEligibleAccountForSwitching() const {
  if (!account_preview_data_service_) {
    return std::nullopt;
  }

  // TODO(crbug.com/585132231): Confirm with product whether a suppressed
  // `(A, B)` pair should fall back to a third eligible secondary account `C`.
  std::optional<AccountPreviewDataService::AccountPreviewPreference> candidate =
      account_preview_data_service_->GetPreferredAccountForSwitching();
  if (!candidate.has_value() || candidate->gaia_id.empty()) {
    return std::nullopt;
  }

  if (!IsAccountEligible(identity_manager_->FindExtendedAccountInfoByGaiaId(
          candidate->gaia_id))) {
    return std::nullopt;
  }

  if (signin_client_->IsAccountSignedInOnOtherProfile(candidate->gaia_id)) {
    return std::nullopt;
  }

  return candidate;
}

bool AccountSwitchingManager::IsAccountEligible(
    const AccountInfo& account_info) const {
  return !account_info.IsEmpty() &&
         identity_manager_->HasAccountWithRefreshToken(
             account_info.GetAccountId()) &&
         !identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
             account_info.GetAccountId()) &&
         account_info.IsManaged() != Tribool::kTrue &&
         !account_info.IsMemberOfFlexOrg() &&
         !(account_info.IsManaged() == Tribool::kUnknown &&
           AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
               account_info.GetEmail())) &&
         account_info.IsChildAccount() != Tribool::kTrue;
}

bool AccountSwitchingManager::CanShowPromoForPair(
    const AccountPair& pair) const {
  if (switches::kEnableAccountPreviewSwitchingAccountSkipCooldown.Get()) {
    return true;
  }

  const base::Time now = base::Time::Now();
  const SigninPrefs signin_prefs(*prefs_);

  // 28-day primary-account sign-in/switch grace period.
  std::optional<base::Time> last_signin_time =
      signin_prefs.GetChromeLastSigninTime(pair.primary_gaia_id);
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

  // Bi-directional per-pair suppression rules in `SigninPrefs`.
  if (signin_prefs.IsAccountSwitchingPromoSuppressedBySwitch(
          pair.primary_gaia_id, pair.target_gaia_id)) {
    return false;
  }
  if (signin_prefs.GetAccountSwitchingPromoExplicitDismissalCount(
          pair.primary_gaia_id, pair.target_gaia_id) >=
      kMaxPairExplicitDismissals) {
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
    const AccountPair& pair) {
  ScopedDictPrefUpdate update(*prefs_, prefs::kAccountSwitchingPromoState);
  update->Set(kLastExplicitDismissalTimeKey,
              base::TimeToValue(base::Time::Now()));

  SigninPrefs(*prefs_).IncrementAccountSwitchingPromoExplicitDismissalCount(
      pair.primary_gaia_id, pair.target_gaia_id);
}

bool AccountSwitchingManager::IsProfileEligibleForSwitching() const {
  if (!IsFeatureEnabled()) {
    return false;
  }
  if (!prefs_->GetBoolean(prefs::kSigninAllowed)) {
    return false;
  }
  return !active_switch_.has_value() && !IsSwitchBackWindowActive();
}

bool AccountSwitchingManager::IsSwitchBackWindowActive() const {
  // TODO(crbug.com/585132769): Query the 7-day Profile Menu switch-back row
  // pref state in `prefs_` once implemented.
  return false;
}

bool AccountSwitchingManager::StartSwitching(AccountPair pair) {
  std::optional<AccountPair> eligible_pair =
      GetEligiblePromoPair(/*check_promo_eligibility=*/false);
  if (!eligible_pair.has_value() || *eligible_pair != pair) {
    return false;
  }

  active_switch_ = std::move(pair);

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

void AccountSwitchingManager::OnSwitchFinished(bool success) {
  CHECK(active_switch_.has_value());
  AccountPair completed_switch = std::move(*active_switch_);
  active_switch_.reset();

  if (success) {
    // Record switch-based pair suppression bi-directionally in `SigninPrefs`
    // so accepting a switch for `A -> B` prevents promoting either direction
    // while both accounts remain in the profile.
    SigninPrefs(*prefs_).SetAccountSwitchingPromoSuppressedBySwitch(
        completed_switch.primary_gaia_id, completed_switch.target_gaia_id);
  }
}

}  // namespace signin
