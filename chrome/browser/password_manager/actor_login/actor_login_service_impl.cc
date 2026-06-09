// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/actor_login_service_impl.h"

#include "base/functional/bind.h"
#include "chrome/browser/password_manager/actor_login/internal/chrome_actor_login_delegate_client.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_delegate.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_delegate_impl.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_metrics.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace actor_login {

namespace {

ActorLoginDelegate* GetOrCreateDelegate(content::WebContents* web_contents) {
  ActorLoginDelegateImpl* delegate =
      ActorLoginDelegateImpl::FromUserData(web_contents);
  if (!delegate) {
    ActorLoginDelegateClient* client =
        ChromeActorLoginDelegateClient::GetOrCreateForWebContents(web_contents);
    delegate = ActorLoginDelegateImpl::CreateForUserData(web_contents, client);
  }
  return delegate;
}

void OnGetCredentialsResult(CredentialsOrErrorReply callback,
                            CredentialsOrError result) {
  RecordGetCredentialsResult(result);
  std::move(callback).Run(std::move(result));
}

void OnAttemptLoginResult(LoginStatusResultOrErrorReply done_callback,
                          LoginStatusResultOrError result) {
  RecordAttemptLoginResult(result);
  std::move(done_callback).Run(std::move(result));
}
}  // namespace

ActorLoginServiceImpl::ActorLoginServiceImpl() {
  actor_login_delegate_factory_ = base::BindRepeating(&GetOrCreateDelegate);
}

ActorLoginServiceImpl::~ActorLoginServiceImpl() = default;

void ActorLoginServiceImpl::GetCredentials(
    tabs::TabInterface* tab,
    bool has_sign_in_with_google_button,
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
    CredentialsOrErrorReply callback) {
  CHECK(tab);

  content::WebContents* web_contents = tab->GetContents();
  if (!web_contents) {
    OnGetCredentialsResult(
        std::move(callback),
        base::unexpected(ActorLoginError::kInvalidTabInterface));
    return;
  }

  // A single instance per WebContents to ensure that all service method calls
  // for a tab are managed through the same delegate instance.
  ActorLoginDelegate* delegate =
      actor_login_delegate_factory_.Run(web_contents);

  // Delegate the call to the `WebContents`-scoped delegate.
  delegate->GetCredentials(
      has_sign_in_with_google_button, mqls_logger,
      base::BindOnce(&OnGetCredentialsResult, std::move(callback)));
}

void ActorLoginServiceImpl::AttemptLogin(
    tabs::TabInterface* tab,
    const Credential& credential,
    bool should_store_permission,
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
    base::TimeTicks attempt_login_tool_start_time,
    LoginStatusResultOrErrorReply done_callback,
    base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate) {
  CHECK(tab);

  content::WebContents* web_contents = tab->GetContents();
  if (!web_contents) {
    OnAttemptLoginResult(
        std::move(done_callback),
        base::unexpected(ActorLoginError::kInvalidTabInterface));
    return;
  }

  // A single instance per WebContents to ensure that all service method calls
  // for a tab are managed through the same delegate instance.
  ActorLoginDelegate* delegate =
      actor_login_delegate_factory_.Run(web_contents);

  // Delegate the call to the `WebContents`-scoped delegate.
  delegate->AttemptLogin(
      credential, should_store_permission, mqls_logger,
      attempt_login_tool_start_time,
      base::BindOnce(&OnAttemptLoginResult, std::move(done_callback)),
      std::move(action_sequence_delegate));
}

void ActorLoginServiceImpl::SetActorLoginDelegateFactoryForTesting(
    base::RepeatingCallback<ActorLoginDelegate*(content::WebContents*)>
        factory) {
  actor_login_delegate_factory_ = factory;
}

}  // namespace actor_login
