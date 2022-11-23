// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_LOGIN_OR_LOCK_SCREEN_VISIBLE_WAITER_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_LOGIN_OR_LOCK_SCREEN_VISIBLE_WAITER_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace ash {

// A waiter that blocks until the login or lock screen is shown.
class LoginOrLockScreenVisibleWaiter
    : public session_manager::SessionManagerObserver {
 public:
  LoginOrLockScreenVisibleWaiter();
  LoginOrLockScreenVisibleWaiter(const LoginOrLockScreenVisibleWaiter&) =
      delete;
  LoginOrLockScreenVisibleWaiter& operator=(
      const LoginOrLockScreenVisibleWaiter&) = delete;
  ~LoginOrLockScreenVisibleWaiter() override;

  void Wait();
  void WaitEvenIfShown();

  // session_manager::SessionManagerObserver:
  void OnLoginOrLockScreenVisible() override;

 private:
  // Waits for a signal. If `should_wait_if_already_shown` this will wait even
  // if the login or lock screen was already shown before.
  void WaitImpl(bool should_wait_if_already_shown);

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};

  base::RunLoop run_loop_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_LOGIN_OR_LOCK_SCREEN_VISIBLE_WAITER_H_
