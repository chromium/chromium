// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/dice_response_handler.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/account_manager/signin_helper_lacros.h"
#include "google_apis/gaia/core_account_id.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Stop the `SigninManager` from removing the primary account and revoke all
// tokens when there is a valid refresh token. The account reconcilor is
// expected to rebuild cookies. Used as a kill switch.
BASE_FEATURE(kPreventSignoutIfAccountValid,
             "PreventSignoutIfAccountValid",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  //  BUILDFLAG(ENABLE_DICE_SUPPORT)

namespace {

class AccountSelectionInProgressHandleInternal
    : public AccountSelectionInProgressHandle {
 public:
  explicit AccountSelectionInProgressHandleInternal(
      base::OnceClosure on_destroy)
      : on_destroy_(std::move(on_destroy)) {
    DCHECK(on_destroy_);
  }

  AccountSelectionInProgressHandleInternal(
      const AccountSelectionInProgressHandleInternal&) = delete;
  AccountSelectionInProgressHandleInternal& operator=(
      const AccountSelectionInProgressHandleInternal&) = delete;

  ~AccountSelectionInProgressHandleInternal() override {
    std::move(on_destroy_).Run();
  }

 private:
  base::OnceClosure on_destroy_;
};
}  // namespace

SigninManager::SigninManager(PrefService& prefs,
                             signin::IdentityManager& identity_manager,
                             SigninClient& client)
    : prefs_(prefs),
      signin_client_(client),
      identity_manager_(identity_manager) {
  signin_allowed_.Init(
      prefs::kSigninAllowed, &prefs,
      base::BindRepeating(&SigninManager::OnSigninAllowedPrefChanged,
                          base::Unretained(this)));
  UpdateUnconsentedPrimaryAccount();
  identity_manager_observation_.Observe(&identity_manager_.get());
}

SigninManager::~SigninManager() = default;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void SigninManager::StartLacrosSigninFlow(
    const base::FilePath& profile_path,
    AccountProfileMapper* account_profile_mapper,
    signin::ConsistencyCookieManager* consistency_cookie_manager,
    account_manager::AccountManagerFacade::AccountAdditionSource source,
    base::OnceCallback<void(const CoreAccountId&)> on_completion_callback) {
  // If there is already a flow in progress, cancel it.
  signin_helper_lacros_.reset();

  signin_helper_lacros_ = std::make_unique<SigninHelperLacros>(
      profile_path, account_profile_mapper, &identity_manager_.get(),
      consistency_cookie_manager, source,
      // Using `base::Unretained()` is fine because this owns the helper.
      base::BindOnce(&SigninManager::OnSigninHelperLacrosComplete,
                     base::Unretained(this),
                     std::move(on_completion_callback)));
}
#endif

std::unique_ptr<AccountSelectionInProgressHandle>
SigninManager::CreateAccountSelectionInProgressHandle() {
  ++live_account_selection_handles_count_;
  return std::make_unique<AccountSelectionInProgressHandleInternal>(
      base::BindOnce(
          &SigninManager::OnAccountSelectionInProgressHandleDestroyed,
          weak_ptr_factory_.GetWeakPtr()));
}

void SigninManager::UpdateUnconsentedPrimaryAccount() {
  if (!signin::IsImplicitBrowserSigninOrExplicitDisabled(
          &identity_manager_.get(), &prefs_.get())) {
    // Only update the primary account implicitly if the user hasn't explicitly
    // signed in or `switches::kExplicitBrowserSigninUIOnDesktop` is disabled.
    return;
  }

  if (live_account_selection_handles_count_ > 0) {
    // Don't update the unconsented primary account while some UI flow is also
    // manipulating it.
    return;
  }

  // Only update the unconsented primary account only after accounts are loaded.
  if (!identity_manager_->AreRefreshTokensLoaded()) {
    return;
  }

  CoreAccountInfo account = ComputeUnconsentedPrimaryAccountInfo();

  if (!account.IsEmpty()) {
    if (identity_manager_->GetPrimaryAccountInfo(
            signin::ConsentLevel::kSignin) != account) {
      DCHECK(
          !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync));
      // The access point is the point from where the last authentication
      // happened, either through adding the account or a reauth. If it is
      // unknown, report `ACCESS_POINT_DESKTOP_SIGNIN_MANAGER` instead.
      signin_metrics::AccessPoint access_point =
          identity_manager_->FindExtendedAccountInfo(account).access_point;
      if (access_point == signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN) {
        access_point =
            signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER;
      }
      base::UmaHistogramEnumeration(
          "Signin.SigninManager.SigninAccessPoint", access_point,
          signin_metrics::AccessPoint::ACCESS_POINT_MAX);
      identity_manager_->GetPrimaryAccountMutator()->SetPrimaryAccount(
          account.account_id, signin::ConsentLevel::kSignin, access_point);
    }
  } else if (identity_manager_->HasPrimaryAccount(
                 signin::ConsentLevel::kSignin)) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // On Lacros, the `SigninManager` only clears the primary account if it is
    // no longer on the device.
    signin_metrics::ProfileSignout source =
        signin_metrics::ProfileSignout::kAccountRemovedFromDevice;
#else
    DCHECK(!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync));
    signin_metrics::ProfileSignout source =
        signin_metrics::ProfileSignout::kSigninManagerUpdateUPA;
#endif
    identity_manager_->GetPrimaryAccountMutator()->ClearPrimaryAccount(source);
  }
}

CoreAccountInfo SigninManager::ComputeUnconsentedPrimaryAccountInfo() const {
  DCHECK(identity_manager_->AreRefreshTokensLoaded());
  CoreAccountInfo current_primary_account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  bool has_sync_account =
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync);
  if (!signin_client_->IsClearPrimaryAccountAllowed(has_sync_account)) {
    if (current_primary_account.IsEmpty()) {
      DLOG(ERROR)
          << "Clear primary account is not allowed, the primary account "
             "should exist";
    }
    return current_primary_account;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool has_primary_account_with_refresh_token =
      identity_manager_->HasPrimaryAccountWithRefreshToken(
          signin::ConsentLevel::kSignin);

  // The device account should not be removed from the main profile. Therefore,
  // `signin_client_->IsClearPrimaryAccountAllowed()` is expected to always
  // return `false` for the main profile and this function to early return the
  // current primary account.
  DCHECK(signin_client_
             ->is_clear_primary_account_allowed_for_testing() ||  // IN-TEST
         !signin_client_->GetInitialPrimaryAccount().has_value());

  // Secondary profile.
  // Unless the user signs out, removes the account, the UPA will stay the same.
  if (has_primary_account_with_refresh_token) {
    return current_primary_account;
  }

  // No primary account or the user has turned sync off or signed out.
  if (identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // Clear primary account.
    return CoreAccountInfo();
  }

  // Local profile.
  std::vector<CoreAccountInfo> accounts =
      identity_manager_->GetAccountsWithRefreshTokens();
  return accounts.empty() ? CoreAccountInfo() : accounts[0];
#else
  // UPA is equal to the primary account with sync consent if it exists.
  if (has_sync_account) {
    return identity_manager_->GetPrimaryAccountInfo(
        signin::ConsentLevel::kSync);
  }

  // Clearing the primary sync account when sign-in is not allowed is handled
  // by PrimaryAccountPolicyManager. That flow is extremely hard to follow
  // especially for the case when the user is syncing with a managed account
  // as in that case the whole profile needs to be deleted.
  //
  // It was considered simpler to keep the logic to update the unconsented
  // primary account in a single place.
  if (!signin_allowed_.GetValue()) {
    return CoreAccountInfo();
  }

  bool is_current_primary_account_valid =
      IsValidUnconsentedPrimaryAccount(current_primary_account);
  if (is_current_primary_account_valid) {
    AccountInfo extended_account_info =
        identity_manager_->FindExtendedAccountInfo(current_primary_account);
    bool is_subject_to_parental_controls =
        extended_account_info.capabilities.is_subject_to_parental_controls() ==
        signin::Tribool::kTrue;
    if (is_subject_to_parental_controls) {
      // For supervised users, in some cases like clear browsing data including
      // cookies, they shouldn't be signed out. If the refresh token is valid
      // and not in error state, the account reconcilor will rebuild cookies.
      return current_primary_account;
    }
  }

  signin::AccountsInCookieJarInfo cookie_info =
      identity_manager_->GetAccountsInCookieJar();

  std::vector<gaia::ListedAccount> cookie_accounts =
      cookie_info.GetPotentiallyInvalidSignedInAccounts();

  // Fresh cookies and loaded tokens are needed to compute the UPA.
  if (cookie_info.AreAccountsFresh()) {
    // Cookies are fresh and tokens are loaded, UPA is the first account
    // in cookies if it exists and has a refresh token.
    if (!cookie_accounts.empty()) {
      AccountInfo account_info =
          identity_manager_->FindExtendedAccountInfoByAccountId(
              cookie_accounts[0].id);
      return IsValidUnconsentedPrimaryAccount(account_info) ? account_info
                                                            : CoreAccountInfo();
    }
    // Cookie accounts are empty.
    // If `kPreventSignoutIfAccountValid` is enabled, only clear primary account
    // if the account is not valid. Allow the reconcilor to rebuild cookies.
    if (!base::FeatureList::IsEnabled(kPreventSignoutIfAccountValid)) {
      // Cookies are empty, the UPA is empty.
      return CoreAccountInfo();
    }
  }

  // If cookie accounts are empty and the primary account is valid allow the
  // reconcilor to rebuild cookies.
  // If cookies are not fresh, it is not possible to fully compute
  // the unconsented primary account.
  // However, if the current unconsented primary account is no longer valid, it
  // has to be removed.
  return is_current_primary_account_valid ? current_primary_account
                                          : CoreAccountInfo();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

bool SigninManager::IsValidUnconsentedPrimaryAccount(
    const CoreAccountInfo& account) const {
  DCHECK(identity_manager_->AreRefreshTokensLoaded());
  if (account.IsEmpty()) {
    return false;
  }

  const CoreAccountId& account_id = account.account_id;
  return identity_manager_->HasAccountWithRefreshToken(account_id) &&
         !identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
             account_id);
}

void SigninManager::Shutdown() {
  // Unsubscribe to all notifications to stop calling the identity manager.
  signin_allowed_.Destroy();
  identity_manager_observation_.Reset();
}

// Lacros does not use cookies to compute the unconsented primary account.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
void SigninManager::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  // This is needed for the case where the user chooses to start syncing
  // with an account that is different from the unconsented primary account
  // (not the first in cookies) but then cancels. In that case, the tokens stay
  // the same. In all the other cases, either the token will be revoked which
  // will trigger an update for the unconsented primary account or the
  // primary account stays the same but the sync consent is revoked.
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSync) !=
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    return;
  }

  // It is important to update the primary account after all observers process
  // the current OnPrimaryAccountChanged() as all observers should see the same
  // value for the unconsented primary account. Schedule the potential update
  // on the next run loop.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SigninManager::UpdateUnconsentedPrimaryAccount,
                                weak_ptr_factory_.GetWeakPtr()));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

void SigninManager::OnEndBatchOfRefreshTokenStateChanges() {
  UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnRefreshTokensLoaded() {
  UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  CoreAccountInfo current_account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  bool should_update = false;
  if (error == GoogleServiceAuthError::AuthErrorNone()) {
    should_update = current_account.IsEmpty();
  } else {
    // In error state, update if the account in error is the current UPA.
    should_update = (account_info == current_account);
  }

  if (should_update) {
    UpdateUnconsentedPrimaryAccount();
  }
}

void SigninManager::OnSigninAllowedPrefChanged() {
  // TODO(crbug.com/350699437): Use kUserDisabledAllowChromeSignIn instead of
  // kSigninManagerUpdateUPA in UpdateUnconsentedPrimaryAccount().
  UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnAccountSelectionInProgressHandleDestroyed() {
  DCHECK_GT(live_account_selection_handles_count_, 0);
  --live_account_selection_handles_count_;

  // We should reset the primary account in case we missed some relevant events.
  UpdateUnconsentedPrimaryAccount();
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void SigninManager::OnSigninHelperLacrosComplete(
    base::OnceCallback<void(const CoreAccountId&)> on_completion_callback,
    const CoreAccountId& account_id) {
  std::move(on_completion_callback).Run(account_id);
  signin_helper_lacros_.reset();
}
#endif
