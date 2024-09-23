// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"

namespace policy {

namespace {

constexpr base::TimeDelta kMinimumSuspendDuration = base::Minutes(1);

}  // namespace

ManagedSessionService::ManagedSessionService(base::Clock* clock)
    : clock_(clock), session_manager_(session_manager::SessionManager::Get()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  SetLoginStatus();
  if (session_manager_) {
    // To alleviate tight coupling in unit tests to DeviceStatusCollector.
    session_manager_observation_.Observe(session_manager_.get());
    is_session_locked_ = session_manager_->IsScreenLocked();
  }
  if (user_manager::UserManager::IsInitialized()) {
    authenticator_observation_.Observe(ash::UserSessionManager::GetInstance());
    user_manager_observation_.Observe(user_manager::UserManager::Get());
  }
  power_manager_observation_.Observe(chromeos::PowerManagerClient::Get());
}

ManagedSessionService::~ManagedSessionService() {
  if (ash::ExistingUserController::current_controller()) {
    ash::ExistingUserController::current_controller()
        ->RemoveLoginStatusConsumer(this);
  }

  // `ManagedSessionService` is part of the profile and the kiosk (launch)
  // controller must be destroyed before the profile, so we can not call
  // `RemoveKioskProfileLoadFailedObserver` observer here.

  if (ash::SessionTerminationManager::Get()) {
    ash::SessionTerminationManager::Get()->RemoveObserver(this);
  }
}

void ManagedSessionService::AddObserver(
    ManagedSessionService::Observer* observer) {
  observers_.AddObserver(observer);
  if (is_logged_in_observed_) {
    auto* const profile = ash::ProfileHelper::Get()->GetProfileByUser(
        user_manager::UserManager::Get()->GetPrimaryUser());
    observer->OnLogin(profile);
  }
}

void ManagedSessionService::RemoveObserver(
    ManagedSessionService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ManagedSessionService::OnSessionStateChanged() {
  TRACE_EVENT0("ui", "ManagedSessionService::OnSessionStateChanged");
  bool is_session_locked = session_manager_->IsScreenLocked();
  if (is_session_locked_ == is_session_locked) {
    return;
  }
  is_session_locked_ = is_session_locked;

  if (is_session_locked_) {
    for (auto& observer : observers_) {
      observer.OnLocked();
    }
  } else {
    for (auto& observer : observers_) {
      observer.OnUnlocked();
    }
  }
}

void ManagedSessionService::OnUserProfileLoaded(const AccountId& account_id) {
  Profile* profile =
      ash::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  bool is_primary_profile =
      ash::ProfileHelper::Get()->IsPrimaryProfile(profile);
  if (is_logged_in_observed_ && is_primary_profile) {
    return;
  } else if (!is_primary_profile) {
    profile_observations_.AddObservation(profile);
  }
  SetLoginStatus();
  for (auto& observer : observers_) {
    observer.OnLogin(profile);
  }
}

void ManagedSessionService::OnUnlockScreenAttempt(
    const bool success,
    const session_manager::UnlockType unlock_type) {
  for (auto& observer : observers_) {
    observer.OnUnlockAttempt(success, unlock_type);
  }
}

void ManagedSessionService::OnProfileWillBeDestroyed(Profile* profile) {
  is_session_locked_ = false;
  for (auto& observer : observers_) {
    observer.OnLogout(profile);
  }
  profile_observations_.RemoveObservation(profile);
}

void ManagedSessionService::OnSessionWillBeTerminated() {
  if (!user_manager::UserManager::Get() ||
      !user_manager::UserManager::Get()->GetPrimaryUser()) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnSessionTerminationStarted(
        user_manager::UserManager::Get()->GetPrimaryUser());
  }
  ash::SessionTerminationManager::Get()->RemoveObserver(this);
}

void ManagedSessionService::OnUserToBeRemoved(const AccountId& account_id) {
  for (auto& observer : observers_) {
    observer.OnUserToBeRemoved(account_id);
  }
}

void ManagedSessionService::OnUserRemoved(
    const AccountId& account_id,
    user_manager::UserRemovalReason reason) {
  for (auto& observer : observers_) {
    observer.OnUserRemoved(account_id, reason);
  }
}

void ManagedSessionService::SuspendDone(base::TimeDelta sleep_duration) {
  if (sleep_duration < kMinimumSuspendDuration) {
    return;
  }
  for (auto& observer : observers_) {
    observer.OnResumeActive(clock_->Now() - sleep_duration);
  }
}

void ManagedSessionService::OnAuthAttemptStarted() {
  if (ash::ExistingUserController::current_controller()) {
    ash::ExistingUserController::current_controller()
        ->RemoveLoginStatusConsumer(this);
    ash::ExistingUserController::current_controller()->AddLoginStatusConsumer(
        this);
  }

  ash::KioskController& kiosk_controller = ash::KioskController::Get();
  if (kiosk_controller.IsSessionStarting()) {
    // Remove observer first in case the auth attempt is because of a retry, and
    // the observation was added, if it was not added removing the observer will
    // be a no-op.
    kiosk_controller.RemoveProfileLoadFailedObserver(this);
    kiosk_controller.AddProfileLoadFailedObserver(this);
  }
}

void ManagedSessionService::OnAuthFailure(const ash::AuthFailure& error) {
  for (auto& observer : observers_) {
    observer.OnLoginFailure(error);
  }
}

void ManagedSessionService::OnKioskProfileLoadFailed() {
  for (auto& observer : observers_) {
    observer.OnKioskLoginFailure();
  }
}

void ManagedSessionService::SetLoginStatus() {
  if (is_logged_in_observed_ || !user_manager::UserManager::Get() ||
      !user_manager::UserManager::Get()->IsUserLoggedIn()) {
    return;
  }

  auto* const primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user || !primary_user->is_profile_created()) {
    return;
  }

  auto* const profile =
      ash::ProfileHelper::Get()->GetProfileByUser(primary_user);
  if (!profile) {
    // Profile is not fully initialized yet.
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_logged_in_observed_ = true;
  profile_observations_.AddObservation(profile);
  if (ash::SessionTerminationManager::Get()) {
    ash::SessionTerminationManager::Get()->AddObserver(this);
  }
}
}  // namespace policy
