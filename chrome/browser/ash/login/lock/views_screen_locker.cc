// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/lock/views_screen_locker.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/lock_screen_apps/state_controller.h"
#include "chrome/browser/ash/login/challenge_response_auth_keys_loader.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/lock_screen_utils.h"
#include "chrome/browser/ash/login/mojo_system_info_dispatcher.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/screens/chrome_user_selection_screen.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system/system_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/ime/ash/ime_keyboard.h"

// TODO(b/228873153): Remove after figuring out the root cause of the bug
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

ViewsScreenLocker::ViewsScreenLocker()
    : system_info_updater_(std::make_unique<MojoSystemInfoDispatcher>()),
      auth_performer_(UserDataAuthClient::Get()) {
  LoginScreenClientImpl::Get()->SetDelegate(this);
  user_selection_screen_ =
      std::make_unique<ChromeUserSelectionScreen>(DisplayedScreen::LOCK_SCREEN);
}

ViewsScreenLocker::~ViewsScreenLocker() {
  lock_screen_apps::StateController::Get()->SetFocusCyclerDelegate(nullptr);
  LoginScreenClientImpl::Get()->SetDelegate(nullptr);
}

void ViewsScreenLocker::Init(const user_manager::UserList& users) {
  VLOG(1) << "b/228873153 : ViewsScreenLocker::Init()";
  lock_time_ = base::TimeTicks::Now();
  user_selection_screen_->Init(users);

  // Reset Caps Lock state when lock screen is shown.
  input_method::InputMethodManager::Get()->GetImeKeyboard()->SetCapsLockEnabled(
      false);

  system_info_updater_->StartRequest();

  LoginScreen::Get()->GetModel()->SetUserList(
      user_selection_screen_->UpdateAndReturnUserListForAsh());
  LoginScreen::Get()->SetAllowLoginAsGuest(false /*show_guest*/);

  if (user_manager::UserManager::IsInitialized()) {
    // Enable pin and challenge-response authentication for any users who can
    // use them.
    for (user_manager::User* user :
         user_manager::UserManager::Get()->GetLoggedInUsers()) {
      UpdateAuthFactorsAvailability(user);
    }
  }

  user_selection_screen_->InitEasyUnlock();
  UMA_HISTOGRAM_TIMES("LockScreen.LockReady",
                      base::TimeTicks::Now() - lock_time_);
  lock_screen_apps::StateController::Get()->SetFocusCyclerDelegate(this);
}

void ViewsScreenLocker::OnAshLockAnimationFinished() {
  SessionControllerClientImpl::Get()->NotifyChromeLockAnimationsComplete();
}

void ViewsScreenLocker::HandleAuthenticateUserWithPasswordOrPin(
    const AccountId& account_id,
    const std::string& password,
    bool authenticated_by_pin,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_EQ(account_id.GetUserEmail(),
            gaia::SanitizeEmail(account_id.GetUserEmail()));
  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(account_id);
  DCHECK(user);
  auto user_context = std::make_unique<UserContext>(*user);
  user_context->SetKey(
      Key(Key::KEY_TYPE_PASSWORD_PLAIN, std::string(), password));
  if (!authenticated_by_pin) {
    user_context->SetLocalPasswordInput(LocalPasswordInput{password});
  }
  user_context->SetIsUsingPin(authenticated_by_pin);
  user_context->SetSyncPasswordData(password_manager::PasswordHashData(
      account_id.GetUserEmail(), base::UTF8ToUTF16(password),
      false /*force_update*/));
  if (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY) {
    LOG(FATAL) << "Incorrect Active Directory user type "
               << user_context->GetUserType();
  }

  auto on_authenticated = base::BindOnce(&ViewsScreenLocker::OnAuthenticated,
                                         weak_factory_.GetWeakPtr(), account_id,
                                         std::move(callback));
  ScreenLocker::default_screen_locker()->Authenticate(
      std::move(user_context), std::move(on_authenticated));
}

void ViewsScreenLocker::HandleAuthenticateUserWithEasyUnlock(
    const AccountId& account_id) {
  user_selection_screen_->AttemptEasyUnlock(account_id);
}

void ViewsScreenLocker::HandleAuthenticateUserWithChallengeResponse(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> callback) {
  ScreenLocker::default_screen_locker()->AuthenticateWithChallengeResponse(
      account_id, std::move(callback));
}

void ViewsScreenLocker::HandleOnFocusPod(const AccountId& account_id) {
  user_selection_screen_->HandleFocusPod(account_id);

  WallpaperControllerClientImpl::Get()->ShowUserWallpaper(account_id);
}

bool ViewsScreenLocker::HandleFocusLockScreenApps(bool reverse) {
  if (lock_screen_app_focus_handler_.is_null())
    return false;

  lock_screen_app_focus_handler_.Run(reverse);
  return true;
}

void ViewsScreenLocker::HandleFocusOobeDialog() {
  NOTREACHED_IN_MIGRATION();
}

void ViewsScreenLocker::HandleLaunchPublicSession(
    const AccountId& account_id,
    const std::string& locale,
    const std::string& input_method) {
  NOTREACHED_IN_MIGRATION();
}

void ViewsScreenLocker::SuspendDone(base::TimeDelta sleep_duration) {
  for (user_manager::User* user :
       user_manager::UserManager::Get()->GetUnlockUsers()) {
    UpdatePinKeyboardState(user->GetAccountId());
  }
}

void ViewsScreenLocker::RegisterLockScreenAppFocusHandler(
    const LockScreenAppFocusCallback& focus_handler) {
  lock_screen_app_focus_handler_ = focus_handler;
}

void ViewsScreenLocker::UnregisterLockScreenAppFocusHandler() {
  lock_screen_app_focus_handler_.Reset();
}

void ViewsScreenLocker::HandleLockScreenAppFocusOut(bool reverse) {
  LoginScreen::Get()->GetModel()->HandleFocusLeavingLockScreenApps(reverse);
}

void ViewsScreenLocker::OnAuthenticated(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> success_callback,
    bool success) {
  std::move(success_callback).Run(success);

  if (!success) {
    // Asynchronously update pin keyboard state. The pin might be locked due to
    // too many attempts, in which case we might hide the pin keyboard.
    UpdatePinKeyboardState(account_id);
  }
}

void ViewsScreenLocker::UpdateAuthFactorsAvailability(
    const user_manager::User* user) {
  auto user_context = std::make_unique<UserContext>(*user);
  const bool ephemeral =
      user_manager::UserManager::Get()->IsUserCryptohomeDataEphemeral(
          user->GetAccountId());
  auth_performer_.StartAuthSession(
      std::move(user_context), ephemeral, ash::AuthSessionIntent::kVerifyOnly,
      base::BindOnce(&ViewsScreenLocker::OnAuthSessionStarted,
                     weak_factory_.GetWeakPtr()));
}

void ViewsScreenLocker::UpdatePinKeyboardState(const AccountId& account_id) {
  quick_unlock::PinBackend::GetInstance()->CanAuthenticate(
      account_id, quick_unlock::Purpose::kUnlock,
      base::BindOnce(&ViewsScreenLocker::OnPinCanAuthenticate,
                     weak_factory_.GetWeakPtr(), account_id));
}

void ViewsScreenLocker::UpdateChallengeResponseAuthAvailability(
    const AccountId& account_id) {
  const bool enable_challenge_response =
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id);
  LoginScreen::Get()->GetModel()->SetChallengeResponseAuthEnabledForUser(
      account_id, enable_challenge_response);
}

void ViewsScreenLocker::OnAuthSessionStarted(
    bool user_exists,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to start auth session, code "
               << error->get_cryptohome_error();
    return;
  }
  const AccountId& account_id = user_context->GetAccountId();
  const auto& auth_factors = user_context->GetAuthFactorsData();

  PrefService* pref_service = nullptr;
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (profile) {
    pref_service = profile->GetPrefs();
  }
  const bool is_pin_disabled_by_policy =
      pref_service && quick_unlock::IsPinDisabledByPolicy(
                          pref_service, quick_unlock::Purpose::kUnlock);

  login::SetAuthFactorsForUser(account_id, auth_factors,
                               is_pin_disabled_by_policy,
                               LoginScreen::Get()->GetModel());
  if (!auth_factors.FindPinFactor()) {
    // Check for pref-based PIN.
    UpdatePinKeyboardState(account_id);
  }
  auth_performer_.InvalidateAuthSession(std::move(user_context),
                                        base::DoNothing());
}

void ViewsScreenLocker::OnPinCanAuthenticate(
    const AccountId& account_id,
    bool can_authenticate,
    cryptohome::PinLockAvailability available_at) {
  LoginScreen::Get()->GetModel()->SetPinEnabledForUser(
      account_id, can_authenticate, available_at);
}

}  // namespace ash
