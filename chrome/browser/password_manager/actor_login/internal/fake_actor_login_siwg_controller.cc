// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/fake_actor_login_siwg_controller.h"

#include <utility>

#include "components/password_manager/core/browser/actor_login/internal/actor_login_metrics_helper.h"

namespace actor_login {

FakeActorLoginSiwgController::FakeActorLoginSiwgController(
    bool should_require_button_click,
    bool should_store_permission,
    LoginStatusResultOrErrorReply on_finished_callback,
    base::OnceCallback<void(bool)> post_button_click_login_result_callback)
    : should_require_button_click_(should_require_button_click),
      should_store_permission_(should_store_permission),
      on_finished_callback_(std::move(on_finished_callback)),
      post_button_click_login_result_callback_(
          std::move(post_button_click_login_result_callback)) {}

FakeActorLoginSiwgController::~FakeActorLoginSiwgController() = default;

void FakeActorLoginSiwgController::OnSiwgButtonClicked(bool will_succeed) {
  if (post_button_click_login_result_callback_) {
    std::move(post_button_click_login_result_callback_).Run(will_succeed);
  }
}

bool FakeActorLoginSiwgController::ShouldStorePermission() const {
  return should_store_permission_;
}

void FakeActorLoginSiwgController::StartFederatedLogin(
    std::unique_ptr<ActorLoginMetricsHelper> metrics_helper) {
  std::move(on_finished_callback_)
      .Run(should_require_button_click_
               ? LoginStatusResult::kRequiresButtonClick
               : LoginStatusResult::kSuccessFederated);
}

}  // namespace actor_login
