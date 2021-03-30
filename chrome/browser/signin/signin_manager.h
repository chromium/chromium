// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class SigninManager : public KeyedService,
                      public signin::IdentityManager::Observer {
 public:
  explicit SigninManager(signin::IdentityManager* identity_manger);
  ~SigninManager() override;

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
  // cleared, |base::nullopt| is returned. If it is still valid, returns the
  // valid UPA.
  base::Optional<CoreAccountInfo> ComputeUnconsentedPrimaryAccountInfo() const;

  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;
  void OnRefreshTokensLoaded() override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnAccountsCookieDeletedByUserAction() override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error) override;

  signin::IdentityManager* identity_manager_;
  bool unconsented_primary_account_revoked_during_load_ = false;

  base::WeakPtrFactory<SigninManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SigninManager);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_H_
