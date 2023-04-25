// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_impl.h"
#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller_impl.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_urls.h"

using signin::ConsentLevel;
using signin::IdentityManager;
using signin::PrimaryAccountChangeEvent;

class BoundSessionCookieRefreshServiceImpl::BoundSessionStateTracker
    : public IdentityManager::Observer {
 public:
  BoundSessionStateTracker(IdentityManager* identity_manager,
                           base::RepeatingCallback<void()> callback);
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
  bool ComputeIsBoundSession();
  void UpdateIsBoundSession();
  void SetIsBoundSession(bool new_value);

  // Assumes the session is bound until proven otherwise to avoid unauthorized
  // requests on startup.
  bool is_bound_session_ = true;
  const raw_ptr<IdentityManager> identity_manager_;
  base::RepeatingCallback<void()> callback_;

  base::ScopedObservation<IdentityManager, IdentityManager::Observer>
      identity_manager_observation_{this};
};

BoundSessionCookieRefreshServiceImpl::BoundSessionStateTracker::
    BoundSessionStateTracker(IdentityManager* identity_manager,
                             base::RepeatingCallback<void()> callback)
    : identity_manager_(identity_manager), callback_(callback) {
  DCHECK(callback);
  identity_manager_observation_.Observe(identity_manager_.get());
  // Set initial value.
  is_bound_session_ = ComputeIsBoundSession();
}

BoundSessionCookieRefreshServiceImpl::BoundSessionStateTracker::
    ~BoundSessionStateTracker() = default;

bool BoundSessionCookieRefreshServiceImpl::BoundSessionStateTracker::
    ComputeIsBoundSession() {
  if (!identity_manager_->HasPrimaryAccount(ConsentLevel::kSignin)) {
    return false;
  }

  if (!identity_manager_->AreRefreshTokensLoaded()) {
    return is_bound_session_;
  }

  const CoreAccountId primary_account_id =
      identity_manager_->GetPrimaryAccountId(ConsentLevel::kSignin);
  bool is_primary_account_valid =
      identity_manager_->HasAccountWithRefreshToken(primary_account_id) &&
      !identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id);

  // TODO: Add extra check that primary account is actually bound
  // `TokenBindingService::HasBindingKeyForAccount()`.
  return is_primary_account_valid;
}

void BoundSessionCookieRefreshServiceImpl::BoundSessionStateTracker::
    UpdateIsBoundSession() {
  SetIsBoundSession(ComputeIsBoundSession());
}

void BoundSessionCookieRefreshServiceImpl::BoundSessionStateTracker::
    SetIsBoundSession(bool new_value) {
  if (is_bound_session_ == new_value) {
    return;
  }

  is_bound_session_ = new_value;
  callback_.Run();
}

void BoundSessionCookieRefreshServiceImpl::BoundSessionStateTracker::
    OnPrimaryAccountChanged(const PrimaryAccountChangeEvent& event_details) {
  if (event_details.GetEventTypeFor(ConsentLevel::kSignin) ==
      PrimaryAccountChangeEvent::Type::kNone) {
    // Upgrade consent to sync has no impact on bound session.
    return;
  }
  UpdateIsBoundSession();
}

void BoundSessionCookieRefreshServiceImpl::BoundSessionStateTracker::
    OnEndBatchOfRefreshTokenStateChanges() {
  UpdateIsBoundSession();
}

void BoundSessionCookieRefreshServiceImpl::BoundSessionStateTracker::
    OnErrorStateOfRefreshTokenUpdatedForAccount(
        const CoreAccountInfo& account_info,
        const GoogleServiceAuthError& error) {
  if (account_info.account_id !=
      identity_manager_->GetPrimaryAccountId(ConsentLevel::kSignin)) {
    return;
  }
  UpdateIsBoundSession();
}

void BoundSessionCookieRefreshServiceImpl::BoundSessionStateTracker::
    OnRefreshTokensLoaded() {
  UpdateIsBoundSession();
}

void BoundSessionCookieRefreshServiceImpl::BoundSessionStateTracker::
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

bool BoundSessionCookieRefreshServiceImpl::BoundSessionStateTracker::
    is_bound_session() const {
  return is_bound_session_;
}

BoundSessionCookieRefreshServiceImpl::BoundSessionCookieRefreshServiceImpl(
    SigninClient* client,
    IdentityManager* identity_manager)
    : client_(client), identity_manager_(identity_manager) {}

BoundSessionCookieRefreshServiceImpl::~BoundSessionCookieRefreshServiceImpl() =
    default;

void BoundSessionCookieRefreshServiceImpl::Initialize() {
  // `base::Unretained(this)` is safe because `this` owns
  // `bound_session_tracker_`.
  bound_session_tracker_ = std::make_unique<BoundSessionStateTracker>(
      identity_manager_,
      base::BindRepeating(
          &BoundSessionCookieRefreshServiceImpl::OnBoundSessionUpdated,
          base::Unretained(this)));
  OnBoundSessionUpdated();
}

bool BoundSessionCookieRefreshServiceImpl::IsBoundSession() const {
  DCHECK(bound_session_tracker_);
  return bound_session_tracker_->is_bound_session();
}

chrome::mojom::BoundSessionParamsPtr
BoundSessionCookieRefreshServiceImpl::GetBoundSessionParams() const {
  if (!cookie_controller_) {
    return chrome::mojom::BoundSessionParamsPtr();
  }

  return chrome::mojom::BoundSessionParams::New(
      cookie_controller_->url().host(), cookie_controller_->url().path(),
      cookie_controller_->cookie_expiration_time());
}

void BoundSessionCookieRefreshServiceImpl::
    SetRendererBoundSessionParamsUpdaterDelegate(
        RendererBoundSessionParamsUpdaterDelegate renderer_updater) {
  renderer_updater_ = renderer_updater;
}

void BoundSessionCookieRefreshServiceImpl::
    AddBoundSessionRequestThrottledListenerReceiver(
        mojo::PendingReceiver<
            chrome::mojom::BoundSessionRequestThrottledListener> receiver) {
  renderer_request_throttled_listener_.Add(this, std::move(receiver));
}

void BoundSessionCookieRefreshServiceImpl::OnRequestBlockedOnCookie(
    OnRequestBlockedOnCookieCallback resume_blocked_request) {
  if (!IsBoundSession()) {
    // Session has been terminated.
    std::move(resume_blocked_request).Run();
    return;
  }
  DCHECK(cookie_controller_);
  cookie_controller_->OnRequestBlockedOnCookie(
      std::move(resume_blocked_request));
}

base::WeakPtr<BoundSessionCookieRefreshService>
BoundSessionCookieRefreshServiceImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BoundSessionCookieRefreshServiceImpl::OnCookieExpirationDateChanged() {
  UpdateAllRenderers();
}

std::unique_ptr<BoundSessionCookieController>
BoundSessionCookieRefreshServiceImpl::CreateBoundSessionCookieController(
    const GURL& url,
    const std::string& cookie_name) {
  return controller_factory_for_testing_.is_null()
             ? std::make_unique<BoundSessionCookieControllerImpl>(
                   client_, url, cookie_name, this)
             : controller_factory_for_testing_.Run(url, cookie_name, this);
}

void BoundSessionCookieRefreshServiceImpl::StartManagingBoundSessionCookie() {
  DCHECK(!cookie_controller_);
  constexpr char kSIDTSCookieName[] = "__Secure-1PSIDTS";

  cookie_controller_ = CreateBoundSessionCookieController(
      GaiaUrls::GetInstance()->secure_google_url(), kSIDTSCookieName);
  cookie_controller_->Initialize();
}

void BoundSessionCookieRefreshServiceImpl::StopManagingBoundSessionCookie() {
  cookie_controller_.reset();
}

void BoundSessionCookieRefreshServiceImpl::OnBoundSessionUpdated() {
  UpdateAllRenderers();
  if (!IsBoundSession()) {
    StopManagingBoundSessionCookie();
  } else {
    StartManagingBoundSessionCookie();
  }
}

void BoundSessionCookieRefreshServiceImpl::UpdateAllRenderers() {
  if (renderer_updater_) {
    renderer_updater_.Run();
  }
}
