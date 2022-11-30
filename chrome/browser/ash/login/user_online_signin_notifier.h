// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USER_ONLINE_SIGNIN_NOTIFIER_H_
#define CHROME_BROWSER_ASH_LOGIN_USER_ONLINE_SIGNIN_NOTIFIER_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"

namespace ash {

// Checks when online signin conditions are met for any login screen pods and
// notifies observers.
class UserOnlineSigninNotifier {
 public:
  // Observers of OnlineLoginNotifier are notified when user pod needs to be
  // updatated to enforce online signin.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnOnlineSigninEnforced(const AccountId& account_id) = 0;
  };

  explicit UserOnlineSigninNotifier(const user_manager::UserList& users);
  ~UserOnlineSigninNotifier();

  UserOnlineSigninNotifier(const UserOnlineSigninNotifier&) = delete;
  UserOnlineSigninNotifier& operator=(const UserOnlineSigninNotifier&) = delete;

  // Check for policy timer expiry..
  void CheckForPolicyEnforcedOnlineSignin();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class UserOnlineSigninNotifierTest;

  void NotifyObservers(const AccountId& account_id);

  base::ObserverList<Observer> observer_list_;
  user_manager::UserList users_;

  // Timer to update login screen when online user authenticztion os enforced
  // by SAMLOfflineSigninTimeLimit policy.
  std::unique_ptr<base::OneShotTimer> online_login_refresh_timer_;

  base::WeakPtrFactory<UserOnlineSigninNotifier> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USER_ONLINE_SIGNIN_NOTIFIER_H_
