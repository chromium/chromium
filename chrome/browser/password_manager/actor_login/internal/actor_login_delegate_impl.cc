// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/actor_login_delegate_impl.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "content/public/browser/web_contents_user_data.h"

namespace actor_login {
namespace {

Credential PasswordFormToCredential(
    const password_manager::PasswordForm& form) {
  CHECK(form.match_type);
  CHECK_NE(form.match_type.value(),
           password_manager::PasswordForm::MatchType::kGrouped);
  Credential credential;
  credential.username = form.username_value;
  // TODO(crbug.com/427171031): Clarify the format.
  credential.source_site_or_app =
      base::UTF8ToUTF16(form.url.GetWithEmptyPath().spec());
  // TODO(crbug.com/427171031): Use PasswordManager to set the real value here.
  credential.immediatelyAvailableToLogin = false;
  return credential;
}

}  // namespace

WEB_CONTENTS_USER_DATA_KEY_IMPL(ActorLoginDelegateImpl);

ActorLoginDelegateImpl::ActorLoginDelegateImpl(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client)
    : content::WebContentsUserData<ActorLoginDelegateImpl>(*web_contents),
      client_(client) {}

ActorLoginDelegateImpl::~ActorLoginDelegateImpl() = default;

// TODO(crbug.com/434156135): move to components/ as much as possible.
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
  if (!base::FeatureList::IsEnabled(password_manager::features::kActorLogin)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::vector<Credential>()));
    return;
  }
  // Store the callback to mark as active
  pending_get_credentials_callback_ = std::move(callback);
  password_manager::PasswordFormDigest form_digest(
      password_manager::PasswordForm::Scheme::kHtml,
      password_manager::GetSignonRealm(GetWebContents().GetLastCommittedURL()),
      GetWebContents().GetLastCommittedURL());
  form_fetcher_ = std::make_unique<password_manager::FormFetcherImpl>(
      std::move(form_digest), client_,
      /*should_migrate_http_passwords=*/false);
  form_fetcher_->Fetch();
  form_fetcher_->AddConsumer(this);
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
  if (!base::FeatureList::IsEnabled(password_manager::features::kActorLogin)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  base::unexpected(ActorLoginError::kUnknown)));
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

void ActorLoginDelegateImpl::OnAttemptLoginCompleted() {
  // There shouldn't be a pending request without a pending callback.
  CHECK(pending_attempt_login_callback_);
  std::move(pending_attempt_login_callback_).Run(LoginStatusResult(false));
}

void ActorLoginDelegateImpl::OnFetchCompleted() {
  std::vector<Credential> result;
  std::ranges::transform(form_fetcher_->GetBestMatches(),
                         std::back_inserter(result), &PasswordFormToCredential);

  form_fetcher_.reset();
  std::move(pending_get_credentials_callback_).Run(std::move(result));
}

}  // namespace actor_login
