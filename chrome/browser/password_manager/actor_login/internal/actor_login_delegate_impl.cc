// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/actor_login_delegate_impl.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_credential_filler.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_get_credentials_helper.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_user_data.h"

using password_manager::ContentPasswordManagerDriver;
using password_manager::PasswordManagerDriver;
using password_manager::PasswordManagerInterface;

namespace actor_login {

namespace {

password_manager::PasswordManagerDriver*
GetPasswordManagerDriverForPrimaryMainFrame(
    content::WebContents* web_contents) {
  if (content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame()) {
    return password_manager::ContentPasswordManagerDriver::
        GetForRenderFrameHost(rfh);
  }
  return nullptr;  // No driver without primary main frame.
}

}  // namespace

WEB_CONTENTS_USER_DATA_KEY_IMPL(ActorLoginDelegateImpl);

// static
ActorLoginDelegate* ActorLoginDelegateImpl::GetOrCreate(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client) {
  CHECK(web_contents);
  return ActorLoginDelegateImpl::GetOrCreateForWebContents(
      web_contents, client,
      base::BindRepeating(GetPasswordManagerDriverForPrimaryMainFrame));
}

// static
ActorLoginDelegate* ActorLoginDelegateImpl::GetOrCreateForTesting(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    PasswordDriverSupplierForPrimaryMainFrame driver_supplier) {
  CHECK(web_contents);

  return ActorLoginDelegateImpl::GetOrCreateForWebContents(
      web_contents, client, std::move(driver_supplier));
}

ActorLoginDelegateImpl::ActorLoginDelegateImpl(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    PasswordDriverSupplierForPrimaryMainFrame driver_supplier)
    : content::WebContentsUserData<ActorLoginDelegateImpl>(*web_contents),
      driver_supplier_(std::move(driver_supplier)),
      client_(client) {}

ActorLoginDelegateImpl::~ActorLoginDelegateImpl() = default;

// TODO(crbug.com/434156135): move to components/ as much as possible.
void ActorLoginDelegateImpl::GetCredentials(CredentialsOrErrorReply callback) {
  CHECK(callback);

  // One request at a time mechanism using pending callbacks.
  // Check if either callback is currently active.
  if (get_credentials_helper_ || pending_attempt_login_callback_) {
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

  get_credentials_helper_ = std::make_unique<ActorLoginGetCredentialsHelper>(
      GetWebContents().GetLastCommittedURL(), client_,
      std::move(callback).Then(
          base::BindOnce(&ActorLoginDelegateImpl::OnGetCredentialsCompleted,
                         weak_ptr_factory_.GetWeakPtr())));
}

void ActorLoginDelegateImpl::AttemptLogin(
    const Credential& credential,
    LoginStatusResultOrErrorReply callback) {
  CHECK(callback);

  // One request at a time mechanism using pending callbacks.
  // Check if either callback is currently active.
  if (get_credentials_helper_ || pending_attempt_login_callback_) {
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

  PasswordManagerDriver* driver = driver_supplier_.Run(&GetWebContents());
  CHECK(driver);
  PasswordManagerInterface* password_manager = driver->GetPasswordManager();
  CHECK(password_manager);

  const url::Origin origin =
      GetWebContents().GetPrimaryMainFrame()->GetLastCommittedOrigin();

  credential_filler_ = std::make_unique<ActorLoginCredentialFiller>(
      origin, credential,
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&ActorLoginDelegateImpl::OnAttemptLoginCompleted,
                         weak_ptr_factory_.GetWeakPtr())));
  credential_filler_->AttemptLogin(password_manager);
}

void ActorLoginDelegateImpl::OnGetCredentialsCompleted() {
  get_credentials_helper_.reset();
}

void ActorLoginDelegateImpl::OnAttemptLoginCompleted(
    base::expected<LoginStatusResult, ActorLoginError> result) {
  // There shouldn't be a pending request without a pending callback.
  CHECK(pending_attempt_login_callback_);
  std::move(pending_attempt_login_callback_).Run(std::move(result));
  credential_filler_.reset();
}


}  // namespace actor_login
