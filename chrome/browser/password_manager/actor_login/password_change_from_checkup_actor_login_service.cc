// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/password_change_from_checkup_actor_login_service.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/concurrent_closures.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/password_manager/actor_login/automated_password_change_credential_filler.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_form_finder.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/tabs/public/tab_interface.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "url/origin.h"

namespace actor_login {

PasswordChangeFromCheckupActorLoginService::
    PasswordChangeFromCheckupActorLoginService(std::u16string username,
                                               std::u16string password,
                                               GURL url)
    : username_(std::move(username)),
      password_(std::move(password)),
      url_(std::move(url)) {}

PasswordChangeFromCheckupActorLoginService::
    ~PasswordChangeFromCheckupActorLoginService() = default;

// TODO(crbug.com/509823221): Actor Login MQLS logs should be uploaded and
// recorded as APC flows. Currently, MQLS is disabled for Actor Login
// when `kPasswordCheckupPrototype` is enabled. Instead, the MQLS logs
// should be marked as APC flows and uploaded.
void PasswordChangeFromCheckupActorLoginService::GetCredentials(
    tabs::TabInterface* tab,
    bool has_sign_in_with_google_button,
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
    CredentialsOrErrorReply callback) {
  CredentialsOrErrorReply async_callback =
      base::BindPostTaskToCurrentDefault(std::move(callback));
  CHECK(tab);
  content::WebContents* web_contents = tab->GetContents();
  if (!web_contents) {
    // TODO(crbug.com/511976430): Update metrics for Actor Login.
    std::move(async_callback)
        .Run(base::unexpected(ActorLoginError::kInvalidTabInterface));
    return;
  }
  if (!net::SchemefulSite::IsSameSite(
          url_, web_contents->GetPrimaryMainFrame()->GetLastCommittedURL())) {
    // TODO(crbug.com/511976430): Update metrics for Actor Login.
    std::move(async_callback)
        .Run(base::unexpected(ActorLoginError::kFillingNotAllowed));
    return;
  }

  password_manager::ContentPasswordManagerDriverFactory* driver_factory =
      password_manager::ContentPasswordManagerDriverFactory::FromWebContents(
          web_contents);
  if (!driver_factory || !driver_factory->password_client()) {
    // TODO(crbug.com/511976430): Update metrics for Actor Login.
    std::move(async_callback)
        .Run(base::unexpected(ActorLoginError::kInvalidTabInterface));
    return;
  }
  password_manager::PasswordManagerClient* client =
      driver_factory->password_client();

  login_form_finder_ = std::make_unique<ActorLoginFormFinder>(client);

  url::Origin request_origin =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  login_form_finder_->GetEligibleLoginFormManagersAsync(
      request_origin,
      base::BindOnce(&PasswordChangeFromCheckupActorLoginService::
                         OnEligibleLoginFormManagersRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), request_origin,
                     std::move(async_callback)));
}

void PasswordChangeFromCheckupActorLoginService::
    OnEligibleLoginFormManagersRetrieved(url::Origin request_origin,
                                         CredentialsOrErrorReply callback,
                                         FormFinderResult form_finder_result) {
  bool has_signin_form = ActorLoginFormFinder::GetSigninFormManager(
                             form_finder_result.eligible_managers) != nullptr;

  std::vector<Credential> credentials;
  Credential credential;
  credential.id = Credential::Id(1);
  credential.username = username_;
  credential.source_site_or_app =
      actor_login::ActorLoginFormFinder::GetSourceSiteOrAppFromUrl(url_);
  credential.request_origin = request_origin;
  credential.display_origin = url_formatter::FormatOriginForSecurityDisplay(
      credential.request_origin,
      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  credential.type = CredentialType::kPassword;
  credential.immediatelyAvailableToLogin = has_signin_form;
  // For the automated password change use case, the permission
  // is given at the moment the user agrees to start the flow.
  // This marks the permission as persistent so it is automatically
  // selected and does not trigger the credential picker.
  // It should not be saved to store, and just be available for this flow.
  credential.has_persistent_permission = true;
  credentials.push_back(credential);

  std::move(callback).Run(std::move(credentials));
}

void PasswordChangeFromCheckupActorLoginService::AttemptLogin(
    tabs::TabInterface* tab,
    const Credential& credential,
    bool should_store_permission,
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
    base::TimeTicks attempt_login_tool_start_time,
    LoginStatusResultOrErrorReply done_callback,
    base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate) {
  LoginStatusResultOrErrorReply async_callback =
      base::BindPostTaskToCurrentDefault(std::move(done_callback));
  CHECK(tab);
  CHECK(credential.username == username_);

  content::WebContents* web_contents = tab->GetContents();
  if (!web_contents) {
    std::move(async_callback)
        .Run(base::unexpected(ActorLoginError::kInvalidTabInterface));
    return;
  }

  if (!net::SchemefulSite::IsSameSite(
          url_, web_contents->GetPrimaryMainFrame()->GetLastCommittedURL())) {
    // TODO(crbug.com/511976430): Update metrics for Actor Login.
    std::move(async_callback).Run(LoginStatusResult::kErrorNoSigninForm);
    return;
  }

  password_manager::ContentPasswordManagerDriverFactory* driver_factory =
      password_manager::ContentPasswordManagerDriverFactory::FromWebContents(
          web_contents);
  if (!driver_factory || !driver_factory->password_client()) {
    // TODO(crbug.com/511976430): Update metrics for Actor Login.
    std::move(async_callback)
        .Run(base::unexpected(ActorLoginError::kInvalidTabInterface));
    return;
  }
  password_manager::PasswordManagerClient* client =
      driver_factory->password_client();
  password_manager::PasswordManagerInterface* password_manager =
      client->GetPasswordManager();

  if (!password_manager) {
    // TODO(crbug.com/511976430): Update metrics for Actor Login.
    std::move(async_callback).Run(LoginStatusResult::kErrorNoSigninForm);
    return;
  }

  // TODO(crbug.com/509823221): Actor Login MQLS logs should be uploaded and
  // recorded as APC flows. Currently, MQLS is disabled for Actor Login
  // when `kPasswordCheckupPrototype` is enabled. Instead, the MQLS logs
  // should be marked as APC flows and uploaded.
  credential_filler_ =
      std::make_unique<AutomatedPasswordChangeCredentialFiller>(
          web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
          credential, client, /*mqls_logger=*/nullptr,
          attempt_login_tool_start_time, base::NullCallback(),
          std::move(async_callback), username_, password_);

  credential_filler_->AttemptLogin(password_manager);
}

}  // namespace actor_login
