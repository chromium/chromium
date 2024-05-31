// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/lock/online_reauth/lock_screen_reauth_manager.h"

#include <utility>

#include "ash/login/login_screen_controller.h"
#include "ash/public/cpp/reauth_reason.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/default_clock.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/login/auth/chrome_safe_mode_delegate.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/profile_auth_data.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_reauth_dialogs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/login/auth/auth_session_authenticator.h"
#include "chromeos/ash/components/login/auth/password_update_flow.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/storage_partition.h"

namespace ash {
namespace {
constexpr char kLockScreenReauthHistogram[] =
    "ChromeOS.LockScreenReauth.LockScreenReauthReason";
}  // namespace

LockScreenReauthManager::LockScreenReauthManager(Profile* primary_profile)
    : primary_profile_(primary_profile),
      primary_user_(ProfileHelper::Get()->GetUserByProfile(primary_profile)),
      clock_(base::DefaultClock::GetInstance()),
      in_session_password_sync_manager_(
          InSessionPasswordSyncManager(primary_profile_)) {
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

LockScreenReauthManager::~LockScreenReauthManager() {
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager) {
    session_manager->RemoveObserver(this);
  }
}

bool LockScreenReauthManager::ShouldPasswordSyncTriggerReauth() {
  return primary_profile_->GetPrefs()->GetBoolean(
      prefs::kLockScreenReauthenticationEnabled);
}

void LockScreenReauthManager::MaybeForceReauthOnLockScreen(
    ReauthReason reauth_reason) {
  if (reauth_reason == ReauthReason::kSamlPasswordSyncTokenValidationFailed &&
      !ShouldPasswordSyncTriggerReauth()) {
    // Reauth on lock for token dismatch is disabled by a policy.
    return;
  }

  // Record the reauth reason in case the user signed out without going through
  // lock screen online flow.
  RecordReauthReason(primary_user_->GetAccountId(), reauth_reason);

  if (reauth_reason == ReauthReason::kSamlPasswordSyncTokenValidationFailed) {
    is_reauth_required_by_saml_token_mismatch_ = true;
  } else if (reauth_reason == ReauthReason::kSamlLockScreenReauthPolicy) {
    is_reauth_required_by_saml_time_limit_policy_ = true;
  } else if (reauth_reason == ReauthReason::kGaiaLockScreenReauthPolicy) {
    is_reauth_required_by_gaia_time_limit_policy_ = true;
  }

  if (screenlock_bridge_->IsLocked()) {
    // On the lock screen: need to update the UI.
    ForceOnlineReauth();
  }
}

void LockScreenReauthManager::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
}

void LockScreenReauthManager::Shutdown() {}

void LockScreenReauthManager::OnSessionStateChanged() {
  TRACE_EVENT0("login", "LockScreenReauthManager::OnSessionStateChanged");
  if (!session_manager::SessionManager::Get()->IsScreenLocked()) {
    // We are unlocking the session, no further action required.
    return;
  }

  if (!is_reauth_required_by_gaia_time_limit_policy_ &&
      !is_reauth_required_by_saml_time_limit_policy_ &&
      !is_reauth_required_by_saml_token_mismatch_) {
    // locking the session but no re-auth flag set - show standard UI.
    return;
  }

  // Request re-auth immediately after locking the screen.
  ForceOnlineReauth();
}

void LockScreenReauthManager::ForceOnlineReauth() {
  const auto account_id = primary_user_->GetAccountId();
  screenlock_bridge_->lock_handler()->SetAuthType(
      account_id, proximity_auth::mojom::AuthType::ONLINE_SIGN_IN, u"");

  const bool auto_start_reauth = primary_profile_->GetPrefs()->GetBoolean(
      ::prefs::kLockScreenAutoStartOnlineReauth);
  if (auto_start_reauth) {
    // TODO(b/333882432): Remove this log after the bug fixed.
    LOG(WARNING) << "b/333882432: LoginScreenReauthManager::ForceOnlineReauth";
    Shell::Get()->login_screen_controller()->ShowGaiaSignin(
        /*prefilled_account=*/account_id);
  }
}

void LockScreenReauthManager::ResetOnlineReauth() {
  user_manager::UserManager::Get()->SaveForceOnlineSignin(
      primary_user_->GetAccountId(), false);
  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetLastOnlineSignin(primary_user_->GetAccountId(), clock_->Now());
}

void LockScreenReauthManager::CheckCredentials(
    const UserContext& user_context,
    PasswordChangedCallback callback) {
  user_context_ = user_context;
  password_changed_callback_ = std::move(callback);
  content::StoragePartition* lock_screen_partition =
      login::GetLockScreenPartition();
  if (!lock_screen_partition) {
    LOG(ERROR) << "The lock screen partition is not available yet";
    OnCookiesTransferred();
    return;
  }

  bool transfer_saml_auth_cookies_on_subsequent_login = false;
  if (primary_user_->IsAffiliated()) {
    CrosSettings::Get()->GetBoolean(
        kAccountsPrefTransferSAMLCookies,
        &transfer_saml_auth_cookies_on_subsequent_login);
  }

  ProfileAuthData::Transfer(
      lock_screen_partition, primary_profile_->GetDefaultStoragePartition(),
      false /*transfer_auth_cookies_on_first_login*/,
      transfer_saml_auth_cookies_on_subsequent_login,
      base::BindOnce(&LockScreenReauthManager::OnCookiesTransferred,
                     weak_factory_.GetWeakPtr()));
}

void LockScreenReauthManager::OnCookiesTransferred() {
  if (!auth_session_authenticator_) {
    auth_session_authenticator_ =
        base::MakeRefCounted<AuthSessionAuthenticator>(
            this, std::make_unique<ChromeSafeModeDelegate>(),
            /*user_recorder=*/base::DoNothing(),
            /* new_user_can_be_owner=*/false, g_browser_process->local_state());
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

void LockScreenReauthManager::UpdateUserPassword(
    const std::string& old_password) {
  if (!password_update_flow_) {
    password_update_flow_ = std::make_unique<PasswordUpdateFlow>();
  }
  // TODO(b/258638651): The old password might be checked quicker using a
  // "verify-only" mode, before we go into the more expensive full update flow.
  password_update_flow_->Start(
      std::make_unique<UserContext>(user_context_), old_password,
      base::BindOnce(&LockScreenReauthManager::OnPasswordUpdateSuccess,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&LockScreenReauthManager::OnPasswordUpdateFailure,
                     weak_factory_.GetWeakPtr()));
}

// TODO(crbug.com/40163256): Add UMA histograms for lockscreen online
// re-authentication.
void LockScreenReauthManager::OnAuthFailure(const AuthFailure& error) {
  password_changed_callback_.Run();
}

void LockScreenReauthManager::OnAuthSuccess(const UserContext& user_context) {
  if (user_context.GetAccountId() != primary_user_->GetAccountId()) {
    // Tried to re-authenicate with non-primary user: the authentication was
    // successful but we are allowed to unlock only with valid credentials of
    // the user who locked the screen. In this case show customized version
    // of first re-auth flow dialog with an error message.
    LOG(FATAL) << "Different user is unlocking the device";
  }

  ResetOnlineReauth();
  SendLockscreenReauthReason();
  if (is_reauth_required_by_saml_token_mismatch_) {
    in_session_password_sync_manager_.FetchTokenAsync();
  }

  // is_reauth_required_by_saml_token_mismatch_ shouldn't be reset until
  // SAML token is fetched.
  is_reauth_required_by_gaia_time_limit_policy_ =
      is_reauth_required_by_saml_time_limit_policy_ = false;

  if (screenlock_bridge_->IsLocked()) {
    screenlock_bridge_->lock_handler()->Unlock(user_context.GetAccountId());
  }
  LockScreenStartReauthDialog::Dismiss();
}

void LockScreenReauthManager::SendLockscreenReauthReason() {
  if (is_reauth_required_by_gaia_time_limit_policy_) {
    base::UmaHistogramEnumeration(kLockScreenReauthHistogram,
                                  ReauthReason::kGaiaLockScreenReauthPolicy,
                                  ReauthReason::kNumReauthFlowReasons);
  }

  if (is_reauth_required_by_saml_time_limit_policy_) {
    base::UmaHistogramEnumeration(kLockScreenReauthHistogram,
                                  ReauthReason::kSamlLockScreenReauthPolicy,
                                  ReauthReason::kNumReauthFlowReasons);
  }

  if (is_reauth_required_by_saml_token_mismatch_) {
    base::UmaHistogramEnumeration(
        kLockScreenReauthHistogram,
        ReauthReason::kSamlPasswordSyncTokenValidationFailed,
        ReauthReason::kNumReauthFlowReasons);
  }
}

void LockScreenReauthManager::OnPasswordUpdateSuccess(
    std::unique_ptr<UserContext> user_context) {
  DCHECK(user_context);
  OnAuthSuccess(*user_context);
}

void LockScreenReauthManager::OnPasswordUpdateFailure(
    std::unique_ptr<UserContext> /*user_context*/,
    AuthenticationError /*error*/) {
  OnAuthFailure(AuthFailure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME));
}

void LockScreenReauthManager::ResetReauthRequiredBySamlTokenDismatch() {
  is_reauth_required_by_saml_token_mismatch_ = false;
}

}  // namespace ash
