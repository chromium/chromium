// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_ACTOR_LOGIN_SERVICE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_ACTOR_LOGIN_SERVICE_IMPL_H_

#include "chrome/browser/password_manager/actor_login/actor_login_service.h"

namespace actor_login {

class ActorLoginServiceImpl : public ActorLoginService {
 public:
  ActorLoginServiceImpl();
  ~ActorLoginServiceImpl() override;

  ActorLoginServiceImpl(const ActorLoginServiceImpl&) = delete;
  ActorLoginServiceImpl& operator=(const ActorLoginServiceImpl&) = delete;

  // `ActorLoginService` implementation:
  void GetCredentials(tabs::TabInterface* tab,
                      CredentialsOrErrorReply callback) override;
  void AttemptLogin(tabs::TabInterface* tab,
                    const Credential& credential,
                    LoginStatusResultOrErrorReply callback) override;
};

}  // namespace actor_login

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_ACTOR_LOGIN_SERVICE_IMPL_H_
