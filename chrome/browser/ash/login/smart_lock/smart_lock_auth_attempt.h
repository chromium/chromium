// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_SMART_LOCK_AUTH_ATTEMPT_H_
#define CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_SMART_LOCK_AUTH_ATTEMPT_H_

#include "base/functional/callback.h"
#include "components/account_id/account_id.h"

namespace ash {

// Class responsible for handling Smart Lock auth attempts. The auth protocol
// is started by calling `Start`, which creates a connection to
// ScreenLockBridge. When the auth result is available, `FinalizeUnlock` should
// be called. To cancel the in progress auth attempt, delete the
// `SmartLockAuthAttempt` object.
class SmartLockAuthAttempt {
 public:
  explicit SmartLockAuthAttempt(const AccountId& account_id);

  SmartLockAuthAttempt(const SmartLockAuthAttempt&) = delete;
  SmartLockAuthAttempt& operator=(const SmartLockAuthAttempt&) = delete;

  ~SmartLockAuthAttempt();

  // Ensures the device is currently locked and the unlock process is being
  // initiated by AuthType::USER_CLICK.
  bool Start();

  // Finalizes an unlock attempt. It unlocks the screen if `success` is true.
  // If `type_` is not TYPE_UNLOCK, calling this method will cause unlock
  // failure equivalent to cancelling the attempt.
  void FinalizeUnlock(const AccountId& account_id, bool success);

 private:
  // The internal attempt state.
  enum State { STATE_IDLE, STATE_RUNNING, STATE_DONE };

  // Cancels the attempt.
  void Cancel(const AccountId& account_id);

  State state_;
  const AccountId account_id_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_SMART_LOCK_AUTH_ATTEMPT_H_
