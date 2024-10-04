// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/session_controller_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/session/scoped_screen_lock_blocker.h"
#include "ash/public/cpp/session/session_activation_observer.h"
#include "ash/public/cpp/session/session_controller_client.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/session/fullscreen_controller.h"
#include "ash/session/multiprofiles_intro_dialog.h"
#include "ash/session/session_aborted_dialog.h"
#include "ash/session/teleport_warning_dialog.h"
#include "ash/shell.h"
#include "ash/system/power/power_event_observer.h"
#include "ash/system/privacy/screen_switch_check_controller.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_type.h"
#include "ui/message_center/message_center.h"

using session_manager::SessionState;

namespace ash {
namespace {

void SetTimeOfLastSessionActivation(PrefService* user_pref_service) {
  if (!user_pref_service) {
    return;
  }

  // NOTE: Round down to the nearest day since Windows epoch to reduce syncs.
  const base::Time time_of_last_session_activation =
      base::Time::FromDeltaSinceWindowsEpoch(
          base::Days(base::Time::Now().ToDeltaSinceWindowsEpoch().InDays()));

  if (user_pref_service->GetTime(prefs::kTimeOfLastSessionActivation) !=
      time_of_last_session_activation) {
    user_pref_service->SetTime(prefs::kTimeOfLastSessionActivation,
                               time_of_last_session_activation);
  }
}

}  // namespace

class SessionControllerImpl::ScopedScreenLockBlockerImpl
    : public ScopedScreenLockBlocker {
 public:
  explicit ScopedScreenLockBlockerImpl(
      base::WeakPtr<SessionControllerImpl> session_controller)
      : session_controller_(session_controller) {
    DCHECK(session_controller_);
  }

  ~ScopedScreenLockBlockerImpl() override {
    if (session_controller_) {
      session_controller_->RemoveScopedScreenLockBlocker();
    }
  }

 private:
  base::WeakPtr<SessionControllerImpl> session_controller_;
};

SessionControllerImpl::SessionControllerImpl()
    : fullscreen_controller_(std::make_unique<FullscreenController>(this)) {}

SessionControllerImpl::~SessionControllerImpl() {
  // Abort pending start lock request.
  if (!start_lock_callback_.is_null())
    std::move(start_lock_callback_).Run(false /* locked */);
}

// static
void SessionControllerImpl::RegisterUserProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(
      prefs::kTimeOfLastSessionActivation, base::Time(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterTimePref(ash::prefs::kAshLoginSessionStartedTime,
                             base::Time());
  registry->RegisterBooleanPref(
      ash::prefs::kAshLoginSessionStartedIsFirstSession, false);
}

int SessionControllerImpl::NumberOfLoggedInUsers() const {
  return static_cast<int>(user_sessions_.size());
}

AccountId SessionControllerImpl::GetActiveAccountId() const {
  return user_sessions_.empty() ? AccountId()
                                : user_sessions_[0]->user_info.account_id;
}

AddUserSessionPolicy SessionControllerImpl::GetAddUserPolicy() const {
  return add_user_session_policy_;
}

bool SessionControllerImpl::IsActiveAccountManaged() const {
  CHECK(!user_sessions_.empty());
  return user_sessions_[0]->user_info.is_managed;
}

bool SessionControllerImpl::IsActiveUserSessionStarted() const {
  return !user_sessions_.empty();
}

bool SessionControllerImpl::CanLockScreen() const {
  return scoped_screen_lock_blocker_count_ == 0 &&
         IsActiveUserSessionStarted() && can_lock_;
}

bool SessionControllerImpl::ShouldLockScreenAutomatically() const {
  return should_lock_screen_automatically_;
}

bool SessionControllerImpl::IsRunningInAppMode() const {
  return is_running_in_app_mode_;
}

bool SessionControllerImpl::IsDemoSession() const {
  return is_demo_session_;
}

bool SessionControllerImpl::IsUserSessionBlocked() const {
  // User sessions are blocked when session state is not ACTIVE, with two
  // exceptions:
  // - LOGGED_IN_NOT_ACTIVE state. This is needed so that browser windows
  //   created by session restore (or a default new browser window) are properly
  //   activated before session state changes to ACTIVE.
  // - LOCKED state with a running unlocking animation. This is needed because
  //   the unlocking animation hides the lock container at the end. During the
  //   unlock animation, IsUserSessionBlocked needs to return unblocked so that
  //   user windows are deemed activatable and ash correctly restores the active
  //   window before locking.
  return state_ != SessionState::ACTIVE &&
         state_ != SessionState::LOGGED_IN_NOT_ACTIVE &&
         !(state_ == SessionState::LOCKED && is_unlocking_);
}

bool SessionControllerImpl::IsInSecondaryLoginScreen() const {
  return state_ == SessionState::LOGIN_SECONDARY;
}

SessionState SessionControllerImpl::GetSessionState() const {
  return state_;
}

bool SessionControllerImpl::ShouldEnableSettings() const {
  // Settings opens a web UI window, so it is only available at active session
  // at the moment.
  return !IsUserSessionBlocked();
}

bool SessionControllerImpl::ShouldShowNotificationTray() const {
  return IsActiveUserSessionStarted() && !IsInSecondaryLoginScreen();
}

const SessionControllerImpl::UserSessions&
SessionControllerImpl::GetUserSessions() const {
  return user_sessions_;
}

const UserSession* SessionControllerImpl::GetUserSession(
    UserIndex index) const {
  if (index < 0 || index >= static_cast<UserIndex>(user_sessions_.size()))
    return nullptr;

  return user_sessions_[index].get();
}

const UserSession* SessionControllerImpl::GetUserSessionByAccountId(
    const AccountId& account_id) const {
  auto it = base::ranges::find(user_sessions_, account_id,
                               [](const std::unique_ptr<UserSession>& session) {
                                 return session->user_info.account_id;
                               });
  if (it == user_sessions_.end())
    return nullptr;

  return (*it).get();
}

const UserSession* SessionControllerImpl::GetPrimaryUserSession() const {
  auto it = base::ranges::find(user_sessions_, primary_session_id_,
                               &UserSession::session_id);
  if (it == user_sessions_.end())
    return nullptr;

  return (*it).get();
}

bool SessionControllerImpl::IsUserChild() const {
  if (!IsActiveUserSessionStarted())
    return false;

  user_manager::UserType active_user_type = GetUserSession(0)->user_info.type;
  return active_user_type == user_manager::UserType::kChild;
}

bool SessionControllerImpl::IsUserGuest() const {
  if (!IsActiveUserSessionStarted()) {
    return false;
  }

  user_manager::UserType active_user_type = GetUserSession(0)->user_info.type;
  return active_user_type == user_manager::UserType::kGuest;
}

bool SessionControllerImpl::IsUserPublicAccount() const {
  if (!IsActiveUserSessionStarted())
    return false;

  user_manager::UserType active_user_type = GetUserSession(0)->user_info.type;
  return active_user_type == user_manager::UserType::kPublicAccount;
}

std::optional<user_manager::UserType> SessionControllerImpl::GetUserType()
    const {
  if (!IsActiveUserSessionStarted())
    return std::nullopt;

  return std::make_optional(GetUserSession(0)->user_info.type);
}

bool SessionControllerImpl::IsUserPrimary() const {
  if (!IsActiveUserSessionStarted())
    return false;

  return GetUserSession(0)->session_id == primary_session_id_;
}

bool SessionControllerImpl::IsUserFirstLogin() const {
  if (!IsActiveUserSessionStarted())
    return false;

  return GetUserSession(0)->user_info.is_new_profile;
}

std::optional<int> SessionControllerImpl::GetExistingUsersCount() const {
  return client_ ? std::optional<int>(client_->GetExistingUsersCount())
                 : std::nullopt;
}

void SessionControllerImpl::NotifyFirstSessionReady() {
  CHECK(IsActiveUserSessionStarted());

  for (auto& observer : observers_) {
    observer.OnFirstSessionReady();
  }
}

void SessionControllerImpl::NotifyUserToBeRemoved(const AccountId& account_id) {
  observers_.Notify(&SessionObserver::OnUserToBeRemoved, account_id);
}

bool SessionControllerImpl::ShouldDisplayManagedUI() const {
  if (!IsActiveUserSessionStarted())
    return false;

  return GetUserSession(0)->user_info.should_display_managed_ui;
}

void SessionControllerImpl::LockScreen() {
  if (client_)
    client_->RequestLockScreen();
}

void SessionControllerImpl::HideLockScreen() {
  if (client_)
    client_->RequestHideLockScreen();
}

void SessionControllerImpl::RequestSignOut() {
  if (client_) {
    client_->RequestSignOut();
  }
}

void SessionControllerImpl::RequestRestartForUpdate() {
  if (client_) {
    client_->RequestRestartForUpdate();
  }
}

void SessionControllerImpl::AttemptRestartChrome() {
  if (client_)
    client_->AttemptRestartChrome();
}

void SessionControllerImpl::SwitchActiveUser(const AccountId& account_id) {
  if (client_)
    client_->SwitchActiveUser(account_id);
}

void SessionControllerImpl::CycleActiveUser(CycleUserDirection direction) {
  if (client_)
    client_->CycleActiveUser(direction);
}

void SessionControllerImpl::ShowMultiProfileLogin() {
  if (client_)
    client_->ShowMultiProfileLogin();
}

PrefService* SessionControllerImpl::GetSigninScreenPrefService() const {
  return client_ ? client_->GetSigninScreenPrefService() : nullptr;
}

PrefService* SessionControllerImpl::GetUserPrefServiceForUser(
    const AccountId& account_id) const {
  return client_ ? client_->GetUserPrefService(account_id) : nullptr;
}

base::FilePath SessionControllerImpl::GetProfilePath(
    const AccountId& account_id) const {
  return client_ ? client_->GetProfilePath(account_id) : base::FilePath();
}

std::tuple<bool, bool> SessionControllerImpl::IsEligibleForSeaPen(
    const AccountId& account_id) const {
  return client_ ? client_->IsEligibleForSeaPen(account_id)
                 : std::make_tuple(false, false);
}

PrefService* SessionControllerImpl::GetPrimaryUserPrefService() const {
  const UserSession* session = GetPrimaryUserSession();
  return session ? GetUserPrefServiceForUser(session->user_info.account_id)
                 : nullptr;
}

PrefService* SessionControllerImpl::GetLastActiveUserPrefService() const {
  return last_active_user_prefs_;
}

PrefService* SessionControllerImpl::GetActivePrefService() const {
  // Use the active user prefs once they become available. Check the PrefService
  // object instead of session state because prefs load is async after login.
  if (last_active_user_prefs_)
    return last_active_user_prefs_;
  return GetSigninScreenPrefService();
}

std::unique_ptr<ScopedScreenLockBlocker>
SessionControllerImpl::GetScopedScreenLockBlocker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++scoped_screen_lock_blocker_count_;
  return std::make_unique<SessionControllerImpl::ScopedScreenLockBlockerImpl>(
      weak_ptr_factory_.GetWeakPtr());
}

void SessionControllerImpl::AddObserver(SessionObserver* observer) {
  observers_.AddObserver(observer);
}

void SessionControllerImpl::RemoveObserver(SessionObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool SessionControllerImpl::IsScreenLocked() const {
  return state_ == SessionState::LOCKED;
}

void SessionControllerImpl::SetClient(SessionControllerClient* client) {
  client_ = client;
}

void SessionControllerImpl::SetSessionInfo(const SessionInfo& info) {
  can_lock_ = info.can_lock_screen;
  should_lock_screen_automatically_ = info.should_lock_screen_automatically;
  is_running_in_app_mode_ = info.is_running_in_app_mode;
  if (info.is_demo_session)
    SetIsDemoSession();
  add_user_session_policy_ = info.add_user_session_policy;
  SetSessionState(info.state);
}

void SessionControllerImpl::UpdateUserSession(const UserSession& user_session) {
  auto it = base::ranges::find(user_sessions_, user_session.session_id,
                               &UserSession::session_id);
  if (it == user_sessions_.end()) {
    AddUserSession(user_session);
    return;
  }

  *it = std::make_unique<UserSession>(user_session);
  for (auto& observer : observers_)
    observer.OnUserSessionUpdated((*it)->user_info.account_id);

  UpdateLoginStatus();
}

void SessionControllerImpl::SetUserSessionOrder(
    const std::vector<uint32_t>& user_session_order) {
  DCHECK_EQ(user_sessions_.size(), user_session_order.size());

  AccountId last_active_account_id;
  if (user_sessions_.size())
    last_active_account_id = user_sessions_[0]->user_info.account_id;

  // Adjusts |user_sessions_| to match the given order.
  std::vector<std::unique_ptr<UserSession>> sessions;
  for (const auto& session_id : user_session_order) {
    auto it = base::ranges::find_if(
        user_sessions_,
        [session_id](const std::unique_ptr<UserSession>& session) {
          return session && session->session_id == session_id;
        });
    if (it == user_sessions_.end()) {
      LOG(ERROR) << "Unknown session id =" << session_id;
      continue;
    }

    sessions.push_back(std::move(*it));
  }

  user_sessions_.swap(sessions);

  // Check active user change and notifies observers.
  if (user_sessions_[0]->session_id != active_session_id_) {
    const bool is_first_session = active_session_id_ == 0u;
    active_session_id_ = user_sessions_[0]->session_id;

    if (is_first_session) {
      for (auto& observer : observers_)
        observer.OnFirstSessionStarted();
    }

    session_activation_observer_holder_.NotifyActiveSessionChanged(
        last_active_account_id, user_sessions_[0]->user_info.account_id);

    // When switching to a user for whose PrefService is not ready,
    // |last_active_user_prefs_| continues to point to the PrefService of the
    // most-recently active user with a loaded PrefService.
    PrefService* user_pref_service =
        GetUserPrefServiceForUser(user_sessions_[0]->user_info.account_id);
    if (user_pref_service && last_active_user_prefs_ != user_pref_service) {
      last_active_user_prefs_ = user_pref_service;
      MaybeNotifyOnActiveUserPrefServiceChanged();
    }

    for (auto& observer : observers_) {
      observer.OnActiveUserSessionChanged(
          user_sessions_[0]->user_info.account_id);
    }

    // NOTE: This pref is intentionally set *after* notifying observers of
    // active user session changes so observers can use time of last activation
    // during event handling.
    if (state_ == SessionState::ACTIVE) {
      SetTimeOfLastSessionActivation(user_pref_service);
    }

    UpdateLoginStatus();
  }
}

void SessionControllerImpl::PrepareForLock(PrepareForLockCallback callback) {
  fullscreen_controller_->MaybeExitFullscreenBeforeLock(std::move(callback));
}

void SessionControllerImpl::StartLock(StartLockCallback callback) {
  DCHECK(start_lock_callback_.is_null());
  start_lock_callback_ = std::move(callback);

  LockStateController* const lock_state_controller =
      Shell::Get()->lock_state_controller();

  lock_state_controller->SetLockScreenDisplayedCallback(
      base::BindOnce(&SessionControllerImpl::OnLockAnimationFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SessionControllerImpl::NotifyChromeLockAnimationsComplete() {
  Shell::Get()->power_event_observer()->OnLockAnimationsComplete();
}

void SessionControllerImpl::RunUnlockAnimation(
    RunUnlockAnimationCallback callback) {
  is_unlocking_ = true;

  // Shell could have no instance in tests.
  if (Shell::HasInstance())
    Shell::Get()->lock_state_controller()->OnLockScreenHide(
        std::move(callback));
}

void SessionControllerImpl::NotifyChromeTerminating() {
  for (auto& observer : observers_)
    observer.OnChromeTerminating();
}

void SessionControllerImpl::SetSessionLengthLimit(base::TimeDelta length_limit,
                                                  base::Time start_time) {
  session_length_limit_ = length_limit;
  session_start_time_ = start_time;
  for (auto& observer : observers_)
    observer.OnSessionLengthLimitChanged();
}

void SessionControllerImpl::CanSwitchActiveUser(
    CanSwitchActiveUserCallback callback) {
  Shell::Get()->screen_switch_check_controller()->CanSwitchAwayFromActiveUser(
      std::move(callback));
}

void SessionControllerImpl::ShowMultiprofilesIntroDialog(
    ShowMultiprofilesIntroDialogCallback callback) {
  MultiprofilesIntroDialog::Show(std::move(callback));
}

void SessionControllerImpl::ShowTeleportWarningDialog(
    ShowTeleportWarningDialogCallback callback) {
  TeleportWarningDialog::Show(std::move(callback));
}

void SessionControllerImpl::ShowMultiprofilesSessionAbortedDialog(
    const std::string& user_email) {
  SessionAbortedDialog::Show(user_email);
}

void SessionControllerImpl::AddSessionActivationObserverForAccountId(
    const AccountId& account_id,
    SessionActivationObserver* observer) {
  bool locked = state_ == SessionState::LOCKED;
  observer->OnLockStateChanged(locked);
  observer->OnSessionActivated(user_sessions_.size() &&
                               user_sessions_[0]->user_info.account_id ==
                                   account_id);
  session_activation_observer_holder_.AddForAccountId(account_id, observer);
}

void SessionControllerImpl::RemoveSessionActivationObserverForAccountId(
    const AccountId& account_id,
    SessionActivationObserver* observer) {
  session_activation_observer_holder_.RemoveForAccountId(account_id, observer);
}

void SessionControllerImpl::ClearUserSessionsForTest() {
  user_sessions_.clear();
  last_active_user_prefs_ = nullptr;
  active_session_id_ = 0u;
  primary_session_id_ = 0u;
}

void SessionControllerImpl::SetIsDemoSession() {
  if (is_demo_session_)
    return;

  is_demo_session_ = true;
  Shell::Get()->metrics()->StartDemoSessionMetricsRecording();
  // Notifications should be silenced during demo sessions.
  message_center::MessageCenter::Get()->SetQuietMode(true);
}

void SessionControllerImpl::SetSessionState(SessionState state) {
  if (state_ == state)
    return;

  const bool was_user_session_blocked = IsUserSessionBlocked();
  const bool was_locked = state_ == SessionState::LOCKED;
  state_ = state;
  for (auto& observer : observers_)
    observer.OnSessionStateChanged(state_);

  // NOTE: This pref is intentionally set *after* notifying observers of state
  // changes so observers can use time of last activation during event handling.
  if (state_ == SessionState::ACTIVE) {
    SetTimeOfLastSessionActivation(
        GetUserPrefServiceForUser(GetActiveAccountId()));
  }

  UpdateLoginStatus();

  const bool locked = state_ == SessionState::LOCKED;
  if (was_locked != locked) {
    if (!locked)
      is_unlocking_ = false;

    for (auto& observer : observers_)
      observer.OnLockStateChanged(locked);

    session_activation_observer_holder_.NotifyLockStateChanged(locked);
  }

  EnsureSigninScreenPrefService();

  if (was_user_session_blocked && !IsUserSessionBlocked())
    EnsureActiveWindowAfterUnblockingUserSession();
}

void SessionControllerImpl::AddUserSession(const UserSession& user_session) {
  if (primary_session_id_ == 0u)
    primary_session_id_ = user_session.session_id;

  user_sessions_.push_back(std::make_unique<UserSession>(user_session));

  const AccountId account_id(user_session.user_info.account_id);
  PrefService* user_prefs = GetUserPrefServiceForUser(account_id);
  // |user_prefs| could be null in tests.
  if (user_prefs)
    OnProfilePrefServiceInitialized(account_id, user_prefs);

  UpdateLoginStatus();
  for (auto& observer : observers_)
    observer.OnUserSessionAdded(account_id);
}

LoginStatus SessionControllerImpl::CalculateLoginStatus() const {
  // TODO(jamescook|xiyuan): There is not a 1:1 mapping of SessionState to
  // LoginStatus. Fix the cases that don't match. http://crbug.com/701193
  switch (state_) {
    case SessionState::UNKNOWN:
    case SessionState::OOBE:
    case SessionState::LOGIN_PRIMARY:
    case SessionState::LOGGED_IN_NOT_ACTIVE:
    case SessionState::RMA:
      return LoginStatus::NOT_LOGGED_IN;

    case SessionState::ACTIVE:
      return CalculateLoginStatusForActiveSession();

    case SessionState::LOCKED:
      return LoginStatus::LOCKED;

    case SessionState::LOGIN_SECONDARY:
      // TODO(jamescook): There is no LoginStatus for this.
      return LoginStatus::USER;
  }
  NOTREACHED();
}

LoginStatus SessionControllerImpl::CalculateLoginStatusForActiveSession()
    const {
  DCHECK(state_ == SessionState::ACTIVE);

  if (user_sessions_.empty())  // Can be empty in tests.
    return LoginStatus::USER;

  switch (user_sessions_[0]->user_info.type) {
    case user_manager::UserType::kRegular:
      return LoginStatus::USER;
    case user_manager::UserType::kGuest:
      return LoginStatus::GUEST;
    case user_manager::UserType::kPublicAccount:
      return LoginStatus::PUBLIC;
    case user_manager::UserType::kChild:
      return LoginStatus::CHILD;
    case user_manager::UserType::kKioskApp:
    case user_manager::UserType::kWebKioskApp:
    case user_manager::UserType::kKioskIWA:
      return LoginStatus::KIOSK_APP;
  }
  NOTREACHED();
}

void SessionControllerImpl::UpdateLoginStatus() {
  const LoginStatus new_login_status = CalculateLoginStatus();
  if (new_login_status == login_status_)
    return;

  login_status_ = new_login_status;
  for (auto& observer : observers_)
    observer.OnLoginStatusChanged(login_status_);
}

void SessionControllerImpl::OnLockAnimationFinished() {
  if (!start_lock_callback_.is_null())
    std::move(start_lock_callback_).Run(true /* locked */);
}

void SessionControllerImpl::EnsureSigninScreenPrefService() {
  // Obtain and notify signin profile prefs only once.
  if (signin_screen_prefs_obtained_)
    return;

  PrefService* const signin_prefs = GetSigninScreenPrefService();
  if (!signin_prefs)
    return;

  OnSigninScreenPrefServiceInitialized(signin_prefs);
}

void SessionControllerImpl::OnSigninScreenPrefServiceInitialized(
    PrefService* pref_service) {
  DCHECK(pref_service);
  DCHECK(!signin_screen_prefs_obtained_);

  signin_screen_prefs_obtained_ = true;

  for (auto& observer : observers_)
    observer.OnSigninScreenPrefServiceInitialized(pref_service);

  if (on_active_user_prefs_changed_notify_deferred_) {
    // Notify obsevers with the deferred OnActiveUserPrefServiceChanged(). Do
    // this in a separate loop from the above since observers might depend on
    // each other and we want to avoid having inconsistent states.
    for (auto& observer : observers_)
      observer.OnActiveUserPrefServiceChanged(last_active_user_prefs_);
    on_active_user_prefs_changed_notify_deferred_ = false;
  }
}

void SessionControllerImpl::OnProfilePrefServiceInitialized(
    const AccountId& account_id,
    PrefService* pref_service) {
  // |pref_service| can be null in tests.
  if (!pref_service)
    return;

  DCHECK(!user_sessions_.empty());
  if (account_id == user_sessions_[0]->user_info.account_id) {
    last_active_user_prefs_ = pref_service;

    MaybeNotifyOnActiveUserPrefServiceChanged();
  }
}

void SessionControllerImpl::MaybeNotifyOnActiveUserPrefServiceChanged() {
  DCHECK(last_active_user_prefs_);

  if (!signin_screen_prefs_obtained_) {
    // We must guarantee that OnSigninScreenPrefServiceInitialized() is called
    // before OnActiveUserPrefServiceChanged(), so defer notifying the
    // observers until the sign in prefs are received.
    on_active_user_prefs_changed_notify_deferred_ = true;
    return;
  }

  for (auto& observer : observers_)
    observer.OnActiveUserPrefServiceChanged(last_active_user_prefs_);
}

void SessionControllerImpl::EnsureActiveWindowAfterUnblockingUserSession() {
  // This happens only in tests (See SessionControllerImplTest).
  if (!Shell::HasInstance())
    return;

  auto mru_list =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  if (!mru_list.empty())
    mru_list.front()->Focus();
}

void SessionControllerImpl::RemoveScopedScreenLockBlocker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(scoped_screen_lock_blocker_count_, 0);
  --scoped_screen_lock_blocker_count_;
}

}  // namespace ash
