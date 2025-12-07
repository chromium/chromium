// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_ACTOR_LOGIN_SERVICE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_ACTOR_LOGIN_SERVICE_IMPL_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_delegate.h"
#include "content/public/browser/web_contents.h"

namespace actor_login {

class ActorLoginServiceImpl : public ActorLoginService {
 public:
  ActorLoginServiceImpl();
  ~ActorLoginServiceImpl() override;

  ActorLoginServiceImpl(const ActorLoginServiceImpl&) = delete;
  ActorLoginServiceImpl& operator=(const ActorLoginServiceImpl&) = delete;

  // `ActorLoginService` implementation:
  void GetCredentials(
      tabs::TabInterface* tab,
      base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
      CredentialsOrErrorReply callback) override;
  void AttemptLogin(tabs::TabInterface* tab,
                    const Credential& credential,
                    bool should_store_permission,
                    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
                    LoginStatusResultOrErrorReply callback) override;

  void SetActorLoginDelegateFactoryForTesting(
      base::RepeatingCallback<ActorLoginDelegate*(content::WebContents*)>
          factory);

 private:
  // Factory callback returning a new instance of `ActorLoginDelegate` or
  // an existing one if there is one already attached to the provided
  // `WebContents`. Used to facilitate testing.
  base::RepeatingCallback<ActorLoginDelegate*(content::WebContents*)>
      actor_login_delegate_factory_;
};

}  // namespace actor_login

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_ACTOR_LOGIN_SERVICE_IMPL_H_
