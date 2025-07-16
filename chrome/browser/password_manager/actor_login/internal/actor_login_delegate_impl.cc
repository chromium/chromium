// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/actor_login_delegate_impl.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected.h"
#include "content/public/browser/web_contents_user_data.h"

namespace actor_login {

WEB_CONTENTS_USER_DATA_KEY_IMPL(ActorLoginDelegateImpl);

ActorLoginDelegateImpl::ActorLoginDelegateImpl(
    content::WebContents* web_contents)
    : content::WebContentsUserData<ActorLoginDelegateImpl>(*web_contents) {}

ActorLoginDelegateImpl::~ActorLoginDelegateImpl() = default;

void ActorLoginDelegateImpl::GetCredentials(CredentialsOrErrorReply callback) {
  CHECK(callback);

  // One request at a time mechanism using pending callbacks.
  // Check if either callback is currently active.
  if (pending_get_credentials_callback_ || pending_attempt_login_callback_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       base::unexpected(ActorLoginError::kServiceBusy)));
    return;
  }
  // Store the callback to mark as active
  pending_get_credentials_callback_ = std::move(callback);

  // Simulate asynchronous operation and return an empty list.
  // TODO(crbug.com/427171031): Implement actual logic.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ActorLoginDelegateImpl::OnGetCredentialsCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ActorLoginDelegateImpl::AttemptLogin(
    const Credential& credential,
    LoginStatusResultOrErrorReply callback) {
  CHECK(callback);

  // One request at a time mechanism using pending callbacks.
  // Check if either callback is currently active.
  if (pending_get_credentials_callback_ || pending_attempt_login_callback_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       base::unexpected(ActorLoginError::kServiceBusy)));
    return;
  }
  // Store the callback to mark as active
  pending_attempt_login_callback_ = std::move(callback);

  // Simulate asynchronous operation and return `false` indicating failure.
  // TODO(crbug.com/427170499) - Implement actual logic.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ActorLoginDelegateImpl::OnAttemptLoginCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

// Private helper methods for callbacks:
void ActorLoginDelegateImpl::OnGetCredentialsCompleted() {
  // There shouldn't be a pending request without a pending callback.
  CHECK(pending_get_credentials_callback_);
  std::move(pending_get_credentials_callback_).Run(std::vector<Credential>());
}

void ActorLoginDelegateImpl::OnAttemptLoginCompleted() {
  // There shouldn't be a pending request without a pending callback.
  CHECK(pending_attempt_login_callback_);
  std::move(pending_attempt_login_callback_).Run(LoginStatusResult(false));
}

}  // namespace actor_login
