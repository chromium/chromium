// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/time/default_clock.h"
#include "chrome/browser/ash/login/auth/chrome_safe_mode_delegate.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/profile_auth_data.h"
#include "chrome/browser/ash/login/saml/password_sync_token_fetcher.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_reauth_dialogs.h"
#include "chromeos/ash/components/login/auth/auth_session_authenticator.h"
#include "chromeos/ash/components/login/auth/password_update_flow.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager_base.h"
#include "content/public/browser/storage_partition.h"

namespace ash {

InSessionPasswordSyncManager::InSessionPasswordSyncManager(
    Profile* primary_profile)
    : primary_profile_(primary_profile),
      clock_(base::DefaultClock::GetInstance()),
      primary_user_(ProfileHelper::Get()->GetUserByProfile(primary_profile)) {
  DCHECK(primary_user_);
  auto* session_manager = session_manager::SessionManager::Get();
  // Extra check as SessionManager may be not initialized in some unit
  // tests
  if (session_manager) {
    session_manager->AddObserver(this);
  }

  screenlock_bridge_ = proximity_auth::ScreenlockBridge::Get();
  DCHECK(screenlock_bridge_);
}

InSessionPasswordSyncManager::~InSessionPasswordSyncManager() {
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager) {
    session_manager->RemoveObserver(this);
  }
}

bool InSessionPasswordSyncManager::IsLockReauthEnabled() {
  return primary_profile_->GetPrefs()->GetBoolean(
      prefs::kLockScreenReauthenticationEnabled);
}

void InSessionPasswordSyncManager::MaybeForceReauthOnLockScreen(
    ReauthenticationReason reauth_reason) {
  if (!IsLockReauthEnabled()) {
    // Reauth on lock is disabled by a policy.
    return;
  }
  if (lock_screen_reauth_reason_ == ReauthenticationReason::kInvalidToken) {
    // Re-authentication already enforced, no other action is needed.
    return;
  }
  if (lock_screen_reauth_reason_ == ReauthenticationReason::kPolicy &&
      reauth_reason == ReauthenticationReason::kInvalidToken) {
    // Re-authentication already enforced but need to reset it to trigger token
    // update. No other action is needed.
    lock_screen_reauth_reason_ = reauth_reason;
    return;
  }
  if (!primary_user_->force_online_signin()) {
    // force_online_signin flag is not set - do not update the screen.
    return;
  }
  if (screenlock_bridge_->IsLocked()) {
    // On the lock screen: need to update the UI.
    screenlock_bridge_->lock_handler()->SetAuthType(
        primary_user_->GetAccountId(),
        proximity_auth::mojom::AuthType::ONLINE_SIGN_IN, std::u16string());
  }
  lock_screen_reauth_reason_ = reauth_reason;
}

void InSessionPasswordSyncManager::SetClockForTesting(
    const base::Clock* clock) {
  clock_ = clock;
}

void InSessionPasswordSyncManager::Shutdown() {}

void InSessionPasswordSyncManager::OnSessionStateChanged() {
  if (!session_manager::SessionManager::Get()->IsScreenLocked()) {
    // We are unlocking the session, no further action required.
    return;
  }
  if (lock_screen_reauth_reason_ == ReauthenticationReason::kNone) {
    // locking the session but no re-auth flag set - show standard UI.
    return;
  }

  // Request re-auth immediately after locking the screen.
  screenlock_bridge_->lock_handler()->SetAuthType(
      primary_user_->GetAccountId(),
      proximity_auth::mojom::AuthType::ONLINE_SIGN_IN, std::u16string());
}

void InSessionPasswordSyncManager::UpdateOnlineAuth() {
  PrefService* prefs = primary_profile_->GetPrefs();
  const base::Time now = clock_->Now();
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime, now);

  user_manager::UserManager::Get()->SaveForceOnlineSignin(
      primary_user_->GetAccountId(), false);
  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetLastOnlineSignin(primary_user_->GetAccountId(), now);
}

void InSessionPasswordSyncManager::CreateTokenAsync() {
  password_sync_token_fetcher_ = std::make_unique<PasswordSyncTokenFetcher>(
      primary_profile_->GetURLLoaderFactory(), primary_profile_, this);
  password_sync_token_fetcher_->StartTokenCreate();
}

void InSessionPasswordSyncManager::OnTokenCreated(const std::string& token) {
  password_sync_token_fetcher_.reset();

  // Set token value in local state.
  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetPasswordSyncToken(primary_user_->GetAccountId(), token);
  lock_screen_reauth_reason_ = ReauthenticationReason::kNone;
}

void InSessionPasswordSyncManager::FetchTokenAsync() {
  password_sync_token_fetcher_ = std::make_unique<PasswordSyncTokenFetcher>(
      primary_profile_->GetURLLoaderFactory(), primary_profile_, this);
  password_sync_token_fetcher_->StartTokenGet();
}

void InSessionPasswordSyncManager::OnTokenFetched(const std::string& token) {
  password_sync_token_fetcher_.reset();
  if (!token.empty()) {
    // Set token fetched from the endpoint in local state.
    user_manager::KnownUser known_user(g_browser_process->local_state());
    known_user.SetPasswordSyncToken(primary_user_->GetAccountId(), token);
    lock_screen_reauth_reason_ = ReauthenticationReason::kNone;
  } else {
    // This is the first time a sync token is created for the user: we need to
    // initialize its value by calling the API and store it locally.
    CreateTokenAsync();
  }
}

void InSessionPasswordSyncManager::OnTokenVerified(bool is_valid) {
  // InSessionPasswordSyncManager does not verify the sync token.
}

void InSessionPasswordSyncManager::OnApiCallFailed(
    PasswordSyncTokenFetcher::ErrorType error_type) {
  // If error_type == kGetNoList || kGetNoToken the token API is not
  // initialized yet and we can fix it by creating a new token on lock
  // screen re-authentication.
  // All other API errors will be ignored since they are logged by
  // TokenFetcher and will be re-tried.
  password_sync_token_fetcher_.reset();
  if (error_type == PasswordSyncTokenFetcher::ErrorType::kGetNoList ||
      error_type == PasswordSyncTokenFetcher::ErrorType::kGetNoToken) {
    CreateTokenAsync();
  }
}

void InSessionPasswordSyncManager::CheckCredentials(
    const UserContext& user_context,
    PasswordChangedCallback callback) {
  user_context_ = user_context;
  password_changed_callback_ = std::move(callback);
  content::StoragePartition* lock_screen_partition =
      login::GetLockScreenPartition();
  if (!lock_screen_partition) {
    LOG(ERROR) << "The lock screen partition is not available yet";
    OnCookiesTransfered();
    return;
  }

  bool transfer_saml_auth_cookies_on_subsequent_login = false;
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(primary_profile_);
  if (user->IsAffiliated()) {
    CrosSettings::Get()->GetBoolean(
        kAccountsPrefTransferSAMLCookies,
        &transfer_saml_auth_cookies_on_subsequent_login);
  }

  ProfileAuthData::Transfer(
      lock_screen_partition, primary_profile_->GetDefaultStoragePartition(),
      false /*transfer_auth_cookies_on_first_login*/,
      transfer_saml_auth_cookies_on_subsequent_login,
      base::BindOnce(&InSessionPasswordSyncManager::OnCookiesTransfered,
                     weak_factory_.GetWeakPtr()));
}

void InSessionPasswordSyncManager::OnCookiesTransfered() {
  if (!auth_session_authenticator_) {
    auth_session_authenticator_ =
        base::MakeRefCounted<AuthSessionAuthenticator>(
            this, std::make_unique<ChromeSafeModeDelegate>(),
            /*user_recorder=*/base::DoNothing(),
            g_browser_process->local_state());
  }
  // Perform a fast ("verify-only") check of the current password. This is an
  // optimization: if the password wasn't actually changed the check will
  // finish faster. However, it also implies that if the password was actually
  // changed, we'll need to start a new cryptohome AuthSession for updating
  // the password auth factor (in `password_update_flow_`).
  auth_session_authenticator_->AuthenticateToUnlock(
      user_manager::UserManager::Get()->IsEphemeralAccountId(
          user_context_.GetAccountId()),
      std::make_unique<UserContext>(user_context_));
}

void InSessionPasswordSyncManager::UpdateUserPassword(
    const std::string& old_password) {
  if (!password_update_flow_)
    password_update_flow_ = std::make_unique<PasswordUpdateFlow>();
  // TODO(b/258638651): The old password might be checked quicker using a
  // "verify-only" mode, before we go into the more expensive full update flow.
  password_update_flow_->Start(
      std::make_unique<UserContext>(user_context_), old_password,
      base::BindOnce(&InSessionPasswordSyncManager::OnPasswordUpdateSuccess,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&InSessionPasswordSyncManager::OnPasswordUpdateFailure,
                     weak_factory_.GetWeakPtr()));
}

// TODO(crbug.com/1163777): Add UMA histograms for lockscreen online
// re-authentication.
void InSessionPasswordSyncManager::OnAuthFailure(const AuthFailure& error) {
  password_changed_callback_.Run();
}

void InSessionPasswordSyncManager::OnAuthSuccess(
    const UserContext& user_context) {
  if (user_context.GetAccountId() != primary_user_->GetAccountId()) {
    // Tried to re-authenicate with non-primary user: the authentication was
    // successful but we are allowed to unlock only with valid credentials of
    // the user who locked the screen. In this case show customized version
    // of first re-auth flow dialog with an error message.
    // TODO(crbug.com/1090341)
    return;
  }

  UpdateOnlineAuth();
  if (lock_screen_reauth_reason_ == ReauthenticationReason::kInvalidToken) {
    FetchTokenAsync();
  } else {
    lock_screen_reauth_reason_ = ReauthenticationReason::kNone;
  }
  if (screenlock_bridge_->IsLocked()) {
    screenlock_bridge_->lock_handler()->Unlock(user_context.GetAccountId());
  }
  LockScreenStartReauthDialog::Dismiss();
}

void InSessionPasswordSyncManager::OnPasswordUpdateSuccess(
    std::unique_ptr<UserContext> user_context) {
  DCHECK(user_context);
  OnAuthSuccess(*user_context);
}

void InSessionPasswordSyncManager::OnPasswordUpdateFailure(
    std::unique_ptr<UserContext> /*user_context*/,
    AuthenticationError /*error*/) {
  OnAuthFailure(AuthFailure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME));
}

}  // namespace ash
