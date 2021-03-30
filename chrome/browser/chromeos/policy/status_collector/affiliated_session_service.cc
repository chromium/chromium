// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_collector/affiliated_session_service.h"

#include "base/logging.h"
#include "chrome/browser/ash/profiles/profile_helper.h"

namespace policy {

namespace {

constexpr base::TimeDelta kMinimumSuspendDuration =
    base::TimeDelta::FromMinutes(1);

bool IsPrimaryAndAffiliated(Profile* profile) {
  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  bool is_primary = chromeos::ProfileHelper::Get()->IsPrimaryProfile(profile);
  bool is_affiliated = user && user->IsAffiliated();
  if (!is_primary || !is_affiliated) {
    VLOG(1) << "The profile for the primary user is not associated with an "
               "affiliated user.";
  }
  return is_primary && is_affiliated;
}

}  // namespace

AffiliatedSessionService::AffiliatedSessionService(base::Clock* clock)
    : clock_(clock), session_manager_(session_manager::SessionManager::Get()) {
  if (session_manager_) {
    // To alleviate tight coupling in unit tests to DeviceStatusCollector.
    session_manager_observer_.Add(session_manager_);
    is_session_locked_ = session_manager_->IsScreenLocked();
  }
  power_manager_observer_.Add(chromeos::PowerManagerClient::Get());
}

AffiliatedSessionService::~AffiliatedSessionService() = default;

void AffiliatedSessionService::AddObserver(
    AffiliatedSessionService::Observer* observer) {
  observers_.AddObserver(observer);
}

void AffiliatedSessionService::RemoveObserver(
    AffiliatedSessionService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AffiliatedSessionService::OnSessionStateChanged() {
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

void AffiliatedSessionService::OnUserProfileLoaded(
    const AccountId& account_id) {
  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (!IsPrimaryAndAffiliated(profile)) {
    return;
  }
  profile_observer_.Add(profile);
  for (auto& observer : observers_) {
    observer.OnAffiliatedLogin(profile);
  }
}

void AffiliatedSessionService::OnProfileWillBeDestroyed(Profile* profile) {
  is_session_locked_ = false;
  if (!IsPrimaryAndAffiliated(profile)) {
    return;
  }
  for (auto& observer : observers_) {
    observer.OnAffiliatedLogout(profile);
  }
  profile_observer_.Remove(profile);
}

void AffiliatedSessionService::SuspendDone(base::TimeDelta sleep_duration) {
  if (sleep_duration < kMinimumSuspendDuration) {
    return;
  }
  for (auto& observer : observers_) {
    observer.OnResumeActive(clock_->Now() - sleep_duration);
  }
}

}  // namespace policy
