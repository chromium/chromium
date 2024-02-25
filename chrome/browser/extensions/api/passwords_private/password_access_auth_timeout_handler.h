// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORD_ACCESS_AUTH_TIMEOUT_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORD_ACCESS_AUTH_TIMEOUT_HANDLER_H_

#include "base/functional/callback.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/password_manager/core/common/password_manager_constants.h"

namespace extensions {

// This class only records metrics and manages the timer at the end of which the
// timeout callback is called. It does not manage authentication.
class PasswordAccessAuthTimeoutHandler {
 public:
  using TimeoutCallback = base::RepeatingCallback<void()>;

  PasswordAccessAuthTimeoutHandler();

  PasswordAccessAuthTimeoutHandler(const PasswordAccessAuthTimeoutHandler&) =
      delete;
  PasswordAccessAuthTimeoutHandler& operator=(
      const PasswordAccessAuthTimeoutHandler&) = delete;

  ~PasswordAccessAuthTimeoutHandler();

  // |timeout_call| is passed to |timeout_call_| and will be called when
  // |timeout_timer_| runs out.
  void Init(TimeoutCallback timeout_call);

  // Restarts the |timeout_timer_| if it is already running. Has no effect if
  // |timeout_timer_| is not running.
  void RestartAuthTimer();

#if defined(UNIT_TEST)
  // Use it in tests to mock starting |timeout_timer_|.
  void start_auth_timer(TimeoutCallback timeout_call) {
    timeout_call_ = timeout_call;
    timeout_timer_.Start(
        FROM_HERE, password_manager::constants::kPasswordManagerAuthValidity,
        base::BindRepeating(timeout_call_));
  }
#endif  // defined(UNIT_TEST)

  // Callback to start |timeout_timer_| after authentication.
  void OnUserReauthenticationResult(bool authenticated);

 private:
  // Fired after `kAuthValidityPeriod` after successful user authentication.
  TimeoutCallback timeout_call_;

  // Used only to keep track of time once the user passed the authentication
  // challenge, so that, after it runs out, |timeout_call_| is run. This
  // timer can be restarted using |RestartAuthTimer|. Restarting it merely means
  // delaying the call of |timeout_call_|. It DOES NOT extend
  // authentication validity period.
  base::RetainingOneShotTimer timeout_timer_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORD_ACCESS_AUTH_TIMEOUT_HANDLER_H_
