// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_member.h"
#include "components/signin/public/identity_manager/identity_manager.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/account_manager_core/account_manager_facade.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace base {
class FilePath;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
namespace signin {
class ConsistencyCookieManager;
}

class AccountProfileMapper;
class SigninHelperLacros;
class SigninClient;
struct CoreAccountId;
#endif

class PrefService;
class SigninClient;


#if BUILDFLAG(ENABLE_DICE_SUPPORT)
BASE_DECLARE_FEATURE(kPreventSignoutIfAccountValid);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// See `SigninManager::CreateAccountSelectionInProgressHandle()`.
class AccountSelectionInProgressHandle {
 public:
  AccountSelectionInProgressHandle(const AccountSelectionInProgressHandle&) =
      delete;
  AccountSelectionInProgressHandle& operator=(
      const AccountSelectionInProgressHandle&) = delete;

  virtual ~AccountSelectionInProgressHandle() = default;

 protected:
  AccountSelectionInProgressHandle() = default;
};

class SigninManager : public KeyedService,
                      public signin::IdentityManager::Observer {
 public:
  SigninManager(PrefService& prefs,
                signin::IdentityManager& identity_manager,
                SigninClient& client);
  ~SigninManager() override;

  SigninManager(const SigninManager&) = delete;
  SigninManager& operator=(const SigninManager&) = delete;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void StartLacrosSigninFlow(
      const base::FilePath& profile_path,
      AccountProfileMapper* account_profile_mapper,
      signin::ConsistencyCookieManager* consistency_cookie_manager,
      account_manager::AccountManagerFacade::AccountAdditionSource source,
      base::OnceCallback<void(const CoreAccountId&)> on_completion_callback =
          base::DoNothing());
#endif

  // Returns a scoped handle that prevents `SigninManager` from changing the
  // unconsented primary account.
  virtual std::unique_ptr<AccountSelectionInProgressHandle>
  CreateAccountSelectionInProgressHandle();

 private:
  // Updates the cached version of unconsented primary account and notifies the
  // observers if there is any change.
  void UpdateUnconsentedPrimaryAccount();

  // Computes and returns the unconsented primary account (UPA).
  // - If a primary account with sync consent exists, the UPA is equal to it.
  // - The UPA is the first account in cookies and must have a refresh token.
  // For the UPA to be computed, it needs fresh cookies and tokens to be loaded.
  // - If tokens are not loaded or cookies are not fresh, the UPA can't be
  // computed but if one already exists it might be invalid. That can happen if
  // cookies are fresh but are empty or the first account is different than the
  // current UPA, the other cases are if tokens are not loaded but the current
  // UPA's refresh token has been rekoved or tokens are loaded but the current
  // UPA does not have a refresh token. If the UPA is invalid, it needs to be
  // cleared, an empty account is returned. If it is still valid, returns the
  // valid UPA.
  CoreAccountInfo ComputeUnconsentedPrimaryAccountInfo() const;

  // Checks wheter |account| is a valid account that can be used as an
  // unconsented primary account.
  bool IsValidUnconsentedPrimaryAccount(const CoreAccountInfo& account) const;

  // KeyedService implementation.
  void Shutdown() override;

  // signin::IdentityManager::Observer implementation.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
  void OnEndBatchOfRefreshTokenStateChanges() override;
  void OnRefreshTokensLoaded() override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;

  void OnSigninAllowedPrefChanged();

  void OnAccountSelectionInProgressHandleDestroyed();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void OnSigninHelperLacrosComplete(
      base::OnceCallback<void(const CoreAccountId&)> on_completion_callback,
      const CoreAccountId& account_id);
#endif

  const raw_ref<PrefService> prefs_;
  const raw_ref<SigninClient> signin_client_;
  const raw_ref<signin::IdentityManager> identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // Helper object to listen for changes to the signin allowed preference.
  BooleanPrefMember signin_allowed_;

  // The number of handles currently active, that indicates the number of UIs
  // currently manipulating the unconsented primary account.
  // We should not reset the UPA while it's not `0`.
  int live_account_selection_handles_count_ = 0;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<SigninHelperLacros> signin_helper_lacros_;
#endif

  base::WeakPtrFactory<SigninManager> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_H_
