// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_FAKE_ACTOR_LOGIN_SIWG_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_FAKE_ACTOR_LOGIN_SIWG_CONTROLLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_siwg_controller_interface.h"

namespace actor_login {

// A fake implementation of `ActorLoginSiwgControllerInterface` used in unit
// tests to simulate the Sign-in with Google flow.
class FakeActorLoginSiwgController : public ActorLoginSiwgControllerInterface {
 public:
  explicit FakeActorLoginSiwgController(
      bool should_require_button_click,
      bool should_store_permission,
      LoginStatusResultOrErrorReply on_finished_callback,
      base::OnceCallback<void(bool)> post_button_click_login_result_callback);
  ~FakeActorLoginSiwgController() override;

  // Invoked when the user has clicked the button.
  void OnSiwgButtonClicked(bool will_succeed);

  // ActorLoginSiwgControllerInterface implementation:
  bool ShouldStorePermission() const override;
  void StartFederatedLogin(
      std::unique_ptr<ActorLoginMetricsHelper> metrics_helper) override;

 private:
  bool should_require_button_click_;
  bool should_store_permission_;
  LoginStatusResultOrErrorReply on_finished_callback_;
  base::OnceCallback<void(bool)> post_button_click_login_result_callback_;
};

}  // namespace actor_login

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_FAKE_ACTOR_LOGIN_SIWG_CONTROLLER_H_
