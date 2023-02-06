// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/account_manager/signin_helper_lacros.h"
#include "google_apis/gaia/core_account_id.h"
#endif

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

SigninManager::SigninManager(PrefService* prefs,
                             signin::IdentityManager* identity_manager,
                             SigninClient* client)
    : prefs_(prefs),
      signin_client_(client),
      identity_manager_(identity_manager) {
  signin_allowed_.Init(
      prefs::kSigninAllowed, prefs_,
      base::BindRepeating(&SigninManager::OnSigninAllowedPrefChanged,
                          base::Unretained(this)));
  UpdateUnconsentedPrimaryAccount();
  identity_manager_observation_.Observe(identity_manager_);
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
      profile_path, account_profile_mapper, identity_manager_,
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
      identity_manager_->GetPrimaryAccountMutator()->SetPrimaryAccount(
          account.account_id, signin::ConsentLevel::kSignin,
          // TODO(crbug.com/1261772): Attribute this to actual access points.
          signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER);
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
        signin_metrics::ProfileSignout::kUserDeletedAccountCookies;
#endif
    identity_manager_->GetPrimaryAccountMutator()->ClearPrimaryAccount(
        source, signin_metrics::SignoutDelete::kIgnoreMetric);
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
  DCHECK(!signin_client_->GetInitialPrimaryAccount().has_value());

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
  if (!signin_allowed_.GetValue())
    return CoreAccountInfo();

  signin::AccountsInCookieJarInfo cookie_info =
      identity_manager_->GetAccountsInCookieJar();

  std::vector<gaia::ListedAccount> cookie_accounts =
      cookie_info.signed_in_accounts;

  // Fresh cookies and loaded tokens are needed to compute the UPA.
  if (cookie_info.accounts_are_fresh) {
    // Cookies are fresh and tokens are loaded, UPA is the first account
    // in cookies if it exists and has a refresh token.
    if (cookie_accounts.empty()) {
      // Cookies are empty, the UPA is empty.
      return CoreAccountInfo();
    }

    AccountInfo account_info =
        identity_manager_->FindExtendedAccountInfoByAccountId(
            cookie_accounts[0].id);
    return IsValidUnconsentedPrimaryAccount(account_info) ? account_info
                                                          : CoreAccountInfo();
  }

  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin))
    return CoreAccountInfo();

  // If cookies or tokens are not loaded, it is not possible to fully compute
  // the unconsented primary account. However, if the current unconsented
  // primary account is no longer valid, it has to be removed.
  return IsValidUnconsentedPrimaryAccount(current_primary_account)
             ? current_primary_account
             : CoreAccountInfo();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

bool SigninManager::IsValidUnconsentedPrimaryAccount(
    const CoreAccountInfo& account) const {
  DCHECK(identity_manager_->AreRefreshTokensLoaded());
  if (account.IsEmpty())
    return false;

  const CoreAccountId& account_id = account.account_id;
  return identity_manager_->HasAccountWithRefreshToken(account_id) &&
         !identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
             account_id);
}

void SigninManager::Shutdown() {
  // Unsubscribe to all notifications to stop calling the identity manager.
  signin_allowed_.Destroy();
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;
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

void SigninManager::OnAccountsCookieDeletedByUserAction() {
  UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error) {
  CoreAccountInfo current_account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  bool should_update = false;
  if (error == GoogleServiceAuthError::AuthErrorNone()) {
    should_update = current_account.IsEmpty();
  } else {
    // In error state, update if the account in error is the current UPA.
    should_update = (account_info == current_account);
  }

  if (should_update)
    UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnSigninAllowedPrefChanged() {
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
