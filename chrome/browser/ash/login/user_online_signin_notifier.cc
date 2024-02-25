// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/user_online_signin_notifier.h"

#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/browser_process.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

UserOnlineSigninNotifier::UserOnlineSigninNotifier(
    const user_manager::UserList& users)
    : users_(users),
      online_login_refresh_timer_(std::make_unique<base::OneShotTimer>()) {}

UserOnlineSigninNotifier::~UserOnlineSigninNotifier() = default;

void UserOnlineSigninNotifier::CheckForPolicyEnforcedOnlineSignin() {
  base::TimeDelta min_delta = base::TimeDelta::Max();
  user_manager::KnownUser known_user(g_browser_process->local_state());
  for (user_manager::User* user : users_) {
    const std::optional<base::TimeDelta> offline_signin_limit =
        known_user.GetOfflineSigninLimit(user->GetAccountId());
    if (!offline_signin_limit) {
      continue;
    }

    const base::Time last_online_signin =
        known_user.GetLastOnlineSignin(user->GetAccountId());
    base::TimeDelta time_to_next_online_signin = login::TimeToOnlineSignIn(
        last_online_signin, offline_signin_limit.value());
    if (time_to_next_online_signin.is_positive() &&
        time_to_next_online_signin < min_delta) {
      min_delta = time_to_next_online_signin;
    }
    if (time_to_next_online_signin <= base::TimeDelta() &&
        !user->force_online_signin()) {
      user_manager::UserManager::Get()->SaveForceOnlineSignin(
          user->GetAccountId(), true);
      NotifyObservers(user->GetAccountId());
    }
  }
  if (min_delta < base::TimeDelta::Max()) {
    // Schedule update task at the next policy timeout expiry.
    online_login_refresh_timer_->Start(
        FROM_HERE, min_delta,
        base::BindOnce(
            &UserOnlineSigninNotifier::CheckForPolicyEnforcedOnlineSignin,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void UserOnlineSigninNotifier::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void UserOnlineSigninNotifier::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void UserOnlineSigninNotifier::NotifyObservers(const AccountId& account_id) {
  for (auto& observer : observer_list_) {
    observer.OnOnlineSigninEnforced(account_id);
  }
}

}  // namespace ash
