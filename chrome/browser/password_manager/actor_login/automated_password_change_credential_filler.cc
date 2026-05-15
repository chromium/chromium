// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/automated_password_change_credential_filler.h"

#include <utility>

#include "base/functional/concurrent_closures.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_form_finder.h"
#include "components/password_manager/core/browser/password_form_manager.h"

namespace actor_login {

AutomatedPasswordChangeCredentialFiller::
    AutomatedPasswordChangeCredentialFiller(
        const url::Origin& main_frame_origin,
        const Credential& credential,
        password_manager::PasswordManagerClient* client,
        base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
        base::TimeTicks attempt_login_start_time,
        IsTaskInFocus is_task_in_focus,
        LoginStatusResultOrErrorReply callback,
        std::u16string username,
        std::u16string password)
    : ActorLoginCredentialFiller(main_frame_origin,
                                 credential,
                                 /*should_store_permission=*/false,
                                 client,
                                 std::move(mqls_logger),
                                 attempt_login_start_time,
                                 std::move(is_task_in_focus),
                                 std::move(callback)),
      username_(std::move(username)),
      password_(std::move(password)) {}

AutomatedPasswordChangeCredentialFiller::
    ~AutomatedPasswordChangeCredentialFiller() = default;

const password_manager::PasswordForm*
AutomatedPasswordChangeCredentialFiller::GetMatchingStoredCredential(
    const password_manager::PasswordFormManager& signin_form_manager) {
  automated_form_.username_value = username_;
  automated_form_.password_value = password_;
  return &automated_form_;
}

bool AutomatedPasswordChangeCredentialFiller::
    DoesStoredCredentialBelongToManager(
        const password_manager::PasswordFormManager* manager,
        const password_manager::PasswordForm& stored_credential) {
  // TODO(crbug.com/511114240): Check if the stored credential is part of all
  // matches.
  return true;
}

bool AutomatedPasswordChangeCredentialFiller::IsReauthBeforeFillingRequired() {
  return false;
}

}  // namespace actor_login
