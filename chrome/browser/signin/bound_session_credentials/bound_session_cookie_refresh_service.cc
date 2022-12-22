// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "google_apis/gaia/core_account_id.h"

using signin::ConsentLevel;
using signin::PrimaryAccountChangeEvent;

class BoundSessionCookieRefreshService::BoundSessionStateTracker
    : public IdentityManager::Observer {
 public:
  BoundSessionStateTracker(IdentityManager* identity_manager,
                           base::RepeatingCallback<void(bool)> callback);
  ~BoundSessionStateTracker() override;

  // IdentityManager::Observer
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnEndBatchOfRefreshTokenStateChanges() override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error) override;
  void OnRefreshTokensLoaded() override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  bool is_bound_session() const;

 private:
  void UpdateIsBoundSession();
  void SetIsBoundSession(bool new_value);

  // Assumes the session is bound until proven otherwise to avoid unauthorized
  // requests on startup.
  bool is_bound_session_ = true;
  const raw_ptr<IdentityManager> identity_manager_;
  base::RepeatingCallback<void(bool)> callback_;

  base::ScopedObservation<IdentityManager, IdentityManager::Observer>
      identity_manager_observation_{this};
};

BoundSessionCookieRefreshService::BoundSessionStateTracker::
    BoundSessionStateTracker(IdentityManager* identity_manager,
                             base::RepeatingCallback<void(bool)> callback)
    : identity_manager_(identity_manager), callback_(callback) {
  identity_manager_observation_.Observe(identity_manager_.get());
  UpdateIsBoundSession();
}

BoundSessionCookieRefreshService::BoundSessionStateTracker::
    ~BoundSessionStateTracker() = default;

void BoundSessionCookieRefreshService::BoundSessionStateTracker::
    UpdateIsBoundSession() {
  if (!identity_manager_->HasPrimaryAccount(ConsentLevel::kSignin)) {
    SetIsBoundSession(false);
    return;
  }

  if (!identity_manager_->AreRefreshTokensLoaded()) {
    return;
  }

  const CoreAccountId primary_account_id =
      identity_manager_->GetPrimaryAccountId(ConsentLevel::kSignin);
  bool is_primary_account_valid =
      identity_manager_->HasAccountWithRefreshToken(primary_account_id) &&
      !identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id);

  // TODO: Add extra check that primary account is actually bound
  // `TokenBindingService::HasBindingKeyForAccount()`.
  SetIsBoundSession(is_primary_account_valid);
}

void BoundSessionCookieRefreshService::BoundSessionStateTracker::
    SetIsBoundSession(bool new_value) {
  if (is_bound_session_ == new_value) {
    return;
  }

  is_bound_session_ = new_value;
  callback_.Run(new_value);
}

void BoundSessionCookieRefreshService::BoundSessionStateTracker::
    OnPrimaryAccountChanged(const PrimaryAccountChangeEvent& event_details) {
  if (event_details.GetEventTypeFor(ConsentLevel::kSignin) ==
      PrimaryAccountChangeEvent::Type::kNone) {
    // Upgrade consent to sync has no impact on bound session.
    return;
  }
  UpdateIsBoundSession();
}

void BoundSessionCookieRefreshService::BoundSessionStateTracker::
    OnEndBatchOfRefreshTokenStateChanges() {
  UpdateIsBoundSession();
}

void BoundSessionCookieRefreshService::BoundSessionStateTracker::
    OnErrorStateOfRefreshTokenUpdatedForAccount(
        const CoreAccountInfo& account_info,
        const GoogleServiceAuthError& error) {
  if (account_info.account_id !=
      identity_manager_->GetPrimaryAccountId(ConsentLevel::kSignin)) {
    return;
  }
  UpdateIsBoundSession();
}

void BoundSessionCookieRefreshService::BoundSessionStateTracker::
    OnRefreshTokensLoaded() {
  UpdateIsBoundSession();
}

void BoundSessionCookieRefreshService::BoundSessionStateTracker::
    OnAccountsInCookieUpdated(
        const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
        const GoogleServiceAuthError& error) {
  if (accounts_in_cookie_jar_info.accounts_are_fresh &&
      accounts_in_cookie_jar_info.signed_in_accounts.empty()) {
    DCHECK_EQ(error, GoogleServiceAuthError::AuthErrorNone());
    // No need to wait for `OnPrimaryAccountChanged`, update all renderers,
    // cancel any ongoing fetchers, and resume any blocked requests.
    SetIsBoundSession(false);
  } else {
    // Ensure the session stays bound even if list accounts request fails.
    UpdateIsBoundSession();
  }
  // TODO: May be cache last known default user.
}

bool BoundSessionCookieRefreshService::BoundSessionStateTracker::
    is_bound_session() const {
  return is_bound_session_;
}

BoundSessionCookieRefreshService::BoundSessionCookieRefreshService(
    signin::IdentityManager* identity_manager) {
  // `base::Unretained(this)` is safe because `this` owns
  // `bound_session_tracker_`.
  bound_session_tracker_ = std::make_unique<BoundSessionStateTracker>(
      identity_manager,
      base::BindRepeating(
          &BoundSessionCookieRefreshService::OnBoundSessionUpdated,
          base::Unretained(this)));
}

BoundSessionCookieRefreshService::~BoundSessionCookieRefreshService() = default;

bool BoundSessionCookieRefreshService::IsBoundSession() const {
  return bound_session_tracker_->is_bound_session();
}

void BoundSessionCookieRefreshService::OnBoundSessionUpdated(
    bool is_bound_session) {
  UpdateAllRenderers();
  if (!is_bound_session) {
    ResumeBlockedRequestsIfAny();
    CancelCookieRefreshIfAny();
  }
}
