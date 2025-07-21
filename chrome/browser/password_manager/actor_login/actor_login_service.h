// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_ACTOR_LOGIN_SERVICE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_ACTOR_LOGIN_SERVICE_H_

#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/tabs/public/tab_interface.h"

namespace actor_login {

// Interface for the `ActorLoginService`.
// This service provides methods for retrieving credentials and attempting
// login. It exists to facilitate mocking in tests.
class ActorLoginService {
 public:
  virtual ~ActorLoginService() = default;

  // Asynchronously retrieves credentials for the given `tab`.
  // The `callback` will be invoked with a `base::expected` containing either
  // a list of `Credential`s or an `ActorLoginError`.
  virtual void GetCredentials(tabs::TabInterface* tab,
                              CredentialsOrErrorReply callback) = 0;

  // Attempts to log in using the provided `credential` for the given `tab`.
  // The `callback` will be invoked with a `base::expected` containing either
  // a `LoginStatusResult` or an `ActorLoginError`.
  virtual void AttemptLogin(tabs::TabInterface* tab,
                            const Credential& credential,
                            LoginStatusResultOrErrorReply callback) = 0;
};

}  // namespace actor_login

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_ACTOR_LOGIN_SERVICE_H_
