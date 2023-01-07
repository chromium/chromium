// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_LOCK_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_LOCK_HANDLER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

namespace ash {
class UserContext;
}  // namespace ash

namespace chromeos {

// A thin wrapper around |SessionControllerClientImpl| and
// |ScreenLocker| to allow easier mocking for tests. Also manages the
// |unlock_in_progress| state.
class LoginApiLockHandler {
 public:
  // Gets the global instance of |LoginApiLockHandler|, and creates one if
  // there is none.
  static LoginApiLockHandler* Get();

  static void SetInstanceForTesting(LoginApiLockHandler* instance);

  LoginApiLockHandler();

  LoginApiLockHandler(const LoginApiLockHandler&) = delete;

  LoginApiLockHandler& operator=(const LoginApiLockHandler&) = delete;

  virtual ~LoginApiLockHandler();

  virtual void RequestLockScreen();

  virtual void Authenticate(const ash::UserContext& user_context,
                            base::OnceCallback<void(bool auth_success)>);

  virtual bool IsUnlockInProgress() const;

 private:
  void AuthenticateCallback(bool auth_success);

  bool unlock_in_progress_ = false;
  base::OnceCallback<void(bool auth_success)> callback_;
  base::WeakPtrFactory<LoginApiLockHandler> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_LOCK_HANDLER_H_
