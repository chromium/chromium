// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"

#include "base/logging.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/user_manager.h"

namespace policy {

namespace {

constexpr base::TimeDelta kMinimumSuspendDuration = base::Minutes(1);

}  // namespace

ManagedSessionService::ManagedSessionService(base::Clock* clock)
    : clock_(clock), session_manager_(session_manager::SessionManager::Get()) {
  if (session_manager_) {
    // To alleviate tight coupling in unit tests to DeviceStatusCollector.
    session_manager_observation_.Observe(session_manager_);
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

  if (ash::SessionTerminationManager::Get()) {
    ash::SessionTerminationManager::Get()->RemoveObserver(this);
  }
}

void ManagedSessionService::AddObserver(
    ManagedSessionService::Observer* observer) {
  observers_.AddObserver(observer);
}

void ManagedSessionService::RemoveObserver(
    ManagedSessionService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ManagedSessionService::OnSessionStateChanged() {
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
      chromeos::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  profile_observations_.AddObservation(profile);
  if (ash::SessionTerminationManager::Get() &&
      chromeos::ProfileHelper::Get()->IsPrimaryProfile(profile)) {
    ash::SessionTerminationManager::Get()->AddObserver(this);
  }
  for (auto& observer : observers_) {
    observer.OnLogin(profile);
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
}

void ManagedSessionService::OnAuthFailure(const ash::AuthFailure& error) {
  for (auto& observer : observers_) {
    observer.OnLoginFailure(error);
  }
}

}  // namespace policy
