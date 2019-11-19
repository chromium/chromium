// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/lock/views_screen_locker.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/authpolicy/authpolicy_helper.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"
#include "chrome/browser/chromeos/login/challenge_response_auth_keys_loader.h"
#include "chrome/browser/chromeos/login/lock_screen_utils.h"
#include "chrome/browser/chromeos/login/mojo_system_info_dispatcher.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_backend.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/screens/chrome_user_selection_screen.h"
#include "chrome/browser/chromeos/login/user_board_view_mojo.h"
#include "chrome/browser/chromeos/system/system_clock.h"
#include "chrome/browser/ui/ash/session_controller_client_impl.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/dbus/media_perception/media_perception.pb.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"

namespace chromeos {

namespace {
constexpr char kLockDisplay[] = "lock";
constexpr char kExternalBinaryAuth[] = "external_binary_auth";
constexpr char kExternalBinaryEnrollment[] = "external_binary_enrollment";
constexpr char kWebCameraDeviceContext[] = "WebCamera: WebCamera";
constexpr base::TimeDelta kExternalBinaryAuthTimeout =
    base::TimeDelta::FromSeconds(2);

// Starts the graph specified by |configuration| if the current graph
// is SUSPENDED or if the current configuration is different.
void StartGraphIfNeeded(chromeos::MediaAnalyticsClient* client,
                        const std::string& configuration,
                        base::Optional<mri::State> maybe_state) {
  if (!maybe_state)
    return;

  if (maybe_state->status() == mri::State::SUSPENDED) {
    // Start the specified graph
    mri::State new_state;
    new_state.set_status(mri::State::RUNNING);
    new_state.set_device_context(kWebCameraDeviceContext);
    new_state.set_configuration(configuration);
    client->SetState(new_state, base::DoNothing());
  } else if (maybe_state->configuration() != configuration) {
    // Suspend and restart with new graph
    mri::State suspend_state;
    suspend_state.set_status(mri::State::SUSPENDED);
    suspend_state.set_configuration(configuration);
    client->SetState(suspend_state, base::BindOnce(&StartGraphIfNeeded, client,
                                                   configuration));
  }
}

}  // namespace

ViewsScreenLocker::ViewsScreenLocker(ScreenLocker* screen_locker)
    : screen_locker_(screen_locker),
      system_info_updater_(std::make_unique<MojoSystemInfoDispatcher>()),
      media_analytics_client_(chromeos::MediaAnalyticsClient::Get()) {
  LoginScreenClient::Get()->SetDelegate(this);
  user_board_view_mojo_ = std::make_unique<UserBoardViewMojo>();
  user_selection_screen_ =
      std::make_unique<ChromeUserSelectionScreen>(kLockDisplay);
  user_selection_screen_->SetView(user_board_view_mojo_.get());

  if (base::FeatureList::IsEnabled(ash::features::kUnlockWithExternalBinary))
    scoped_observer_.Add(media_analytics_client_);
}

ViewsScreenLocker::~ViewsScreenLocker() {
  lock_screen_apps::StateController::Get()->SetFocusCyclerDelegate(nullptr);
  LoginScreenClient::Get()->SetDelegate(nullptr);
}

void ViewsScreenLocker::Init() {
  lock_time_ = base::TimeTicks::Now();
  user_selection_screen_->Init(screen_locker_->users());
  if (!ime_state_.get())
    ime_state_ = input_method::InputMethodManager::Get()->GetActiveIMEState();

  // Reset Caps Lock state when lock screen is shown.
  input_method::InputMethodManager::Get()->GetImeKeyboard()->SetCapsLockEnabled(
      false);

  system_info_updater_->StartRequest();

  ash::LoginScreen::Get()->GetModel()->SetUserList(
      user_selection_screen_->UpdateAndReturnUserListForAsh());
  ash::LoginScreen::Get()->SetAllowLoginAsGuest(false /*show_guest*/);

  if (user_manager::UserManager::IsInitialized()) {
    // Enable pin and challenge-response authentication for any users who can
    // use them.
    for (user_manager::User* user :
         user_manager::UserManager::Get()->GetLoggedInUsers()) {
      UpdatePinKeyboardState(user->GetAccountId());
      UpdateChallengeResponseAuthAvailability(user->GetAccountId());
    }
  }

  user_selection_screen_->InitEasyUnlock();
  UMA_HISTOGRAM_TIMES("LockScreen.LockReady",
                      base::TimeTicks::Now() - lock_time_);
  screen_locker_->ScreenLockReady();
  lock_screen_apps::StateController::Get()->SetFocusCyclerDelegate(this);

  allowed_input_methods_subscription_ =
      CrosSettings::Get()->AddSettingsObserver(
          kDeviceLoginScreenInputMethods,
          base::Bind(&ViewsScreenLocker::OnAllowedInputMethodsChanged,
                     base::Unretained(this)));
  OnAllowedInputMethodsChanged();
}

void ViewsScreenLocker::ShowErrorMessage(
    int error_msg_id,
    HelpAppLauncher::HelpTopic help_topic_id) {
  // TODO(xiaoyinh): Complete the implementation here.
  NOTIMPLEMENTED();
}

void ViewsScreenLocker::ClearErrors() {
  NOTIMPLEMENTED();
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
  UserContext user_context(*user);
  user_context.SetKey(
      Key(chromeos::Key::KEY_TYPE_PASSWORD_PLAIN, std::string(), password));
  user_context.SetIsUsingPin(authenticated_by_pin);
  user_context.SetSyncPasswordData(password_manager::PasswordHashData(
      account_id.GetUserEmail(), base::UTF8ToUTF16(password),
      false /*force_update*/));
  if (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY &&
      (user_context.GetUserType() !=
       user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY)) {
    LOG(FATAL) << "Incorrect Active Directory user type "
               << user_context.GetUserType();
  }
  ScreenLocker::default_screen_locker()->Authenticate(user_context,
                                                      std::move(callback));
  UpdatePinKeyboardState(account_id);
}

void ViewsScreenLocker::HandleAuthenticateUserWithExternalBinary(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> callback) {
  authenticate_with_external_binary_callback_ = std::move(callback);
  external_binary_timer_.Start(
      FROM_HERE, kExternalBinaryAuthTimeout,
      base::BindOnce(&ViewsScreenLocker::OnExternalBinaryAuthTimeout,
                     weak_factory_.GetWeakPtr()));
  media_analytics_client_->GetState(base::BindOnce(
      &StartGraphIfNeeded, media_analytics_client_, kExternalBinaryAuth));
}

void ViewsScreenLocker::HandleEnrollUserWithExternalBinary(
    base::OnceCallback<void(bool)> callback) {
  enroll_user_with_external_binary_callback_ = std::move(callback);
  external_binary_timer_.Start(
      FROM_HERE, kExternalBinaryAuthTimeout,
      base::BindOnce(&ViewsScreenLocker::OnExternalBinaryEnrollmentTimeout,
                     weak_factory_.GetWeakPtr()));
  media_analytics_client_->GetState(base::BindOnce(
      &StartGraphIfNeeded, media_analytics_client_, kExternalBinaryEnrollment));
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

void ViewsScreenLocker::HandleHardlockPod(const AccountId& account_id) {
  user_selection_screen_->HardLockPod(account_id);
}

void ViewsScreenLocker::HandleOnFocusPod(const AccountId& account_id) {
  proximity_auth::ScreenlockBridge::Get()->SetFocusedUser(account_id);
  if (user_selection_screen_)
    user_selection_screen_->CheckUserStatus(account_id);

  focused_pod_account_id_ = base::Optional<AccountId>(account_id);

  lock_screen_utils::SetUserInputMethod(account_id.GetUserEmail(),
                                        ime_state_.get());
  lock_screen_utils::SetKeyboardSettings(account_id);
  WallpaperControllerClient::Get()->ShowUserWallpaper(account_id);

  bool use_24hour_clock = false;
  if (user_manager::known_user::GetBooleanPref(
          account_id, prefs::kUse24HourClock, &use_24hour_clock)) {
    g_browser_process->platform_part()
        ->GetSystemClock()
        ->SetLastFocusedPodHourClockType(use_24hour_clock ? base::k24HourClock
                                                          : base::k12HourClock);
  }
}

void ViewsScreenLocker::HandleOnNoPodFocused() {
  focused_pod_account_id_.reset();
  lock_screen_utils::EnforcePolicyInputMethods(std::string());
}

bool ViewsScreenLocker::HandleFocusLockScreenApps(bool reverse) {
  if (lock_screen_app_focus_handler_.is_null())
    return false;

  lock_screen_app_focus_handler_.Run(reverse);
  return true;
}

void ViewsScreenLocker::HandleFocusOobeDialog() {
  NOTREACHED();
}

void ViewsScreenLocker::HandleLaunchPublicSession(
    const AccountId& account_id,
    const std::string& locale,
    const std::string& input_method) {
  NOTREACHED();
}

void ViewsScreenLocker::SuspendDone(const base::TimeDelta& sleep_duration) {
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
  ash::LoginScreen::Get()->GetModel()->HandleFocusLeavingLockScreenApps(
      reverse);
}

void ViewsScreenLocker::OnDetectionSignal(
    const mri::MediaPerception& media_perception) {
  if (authenticate_with_external_binary_callback_) {
    const mri::FramePerception& frame = media_perception.frame_perception(0);
    if (frame.frame_id() != 1)
      return;

    mri::State new_state;
    new_state.set_status(mri::State::SUSPENDED);
    media_analytics_client_->SetState(new_state, base::DoNothing());

    external_binary_timer_.Stop();
    std::move(authenticate_with_external_binary_callback_)
        .Run(true /*auth_success*/);
    ScreenLocker::Hide();
  } else if (enroll_user_with_external_binary_callback_) {
    const mri::FramePerception& frame = media_perception.frame_perception(0);

    external_binary_timer_.Stop();
    mri::State new_state;
    new_state.set_status(mri::State::SUSPENDED);
    media_analytics_client_->SetState(new_state, base::DoNothing());
    std::move(enroll_user_with_external_binary_callback_)
        .Run(frame.frame_id() == 1 /*enrollment_success*/);
  }
}

void ViewsScreenLocker::UpdatePinKeyboardState(const AccountId& account_id) {
  quick_unlock::PinBackend::GetInstance()->CanAuthenticate(
      account_id, base::BindOnce(&ViewsScreenLocker::OnPinCanAuthenticate,
                                 weak_factory_.GetWeakPtr(), account_id));
}

void ViewsScreenLocker::UpdateChallengeResponseAuthAvailability(
    const AccountId& account_id) {
  const bool enable_challenge_response =
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id);
  ash::LoginScreen::Get()->GetModel()->SetChallengeResponseAuthEnabledForUser(
      account_id, enable_challenge_response);
}

void ViewsScreenLocker::OnAllowedInputMethodsChanged() {
  if (focused_pod_account_id_) {
    std::string user_input_method = lock_screen_utils::GetUserLastInputMethod(
        focused_pod_account_id_->GetUserEmail());
    lock_screen_utils::EnforcePolicyInputMethods(user_input_method);
  } else {
    lock_screen_utils::EnforcePolicyInputMethods(std::string());
  }
}

void ViewsScreenLocker::OnPinCanAuthenticate(const AccountId& account_id,
                                             bool can_authenticate) {
  ash::LoginScreen::Get()->GetModel()->SetPinEnabledForUser(account_id,
                                                            can_authenticate);
}

void ViewsScreenLocker::OnExternalBinaryAuthTimeout() {
  std::move(authenticate_with_external_binary_callback_)
      .Run(false /*auth_success*/);
  mri::State new_state;
  new_state.set_status(mri::State::SUSPENDED);
  media_analytics_client_->SetState(new_state, base::DoNothing());
}

void ViewsScreenLocker::OnExternalBinaryEnrollmentTimeout() {
  std::move(enroll_user_with_external_binary_callback_)
      .Run(false /*auth_success*/);
  mri::State new_state;
  new_state.set_status(mri::State::SUSPENDED);
  media_analytics_client_->SetState(new_state, base::DoNothing());
}

}  // namespace chromeos
