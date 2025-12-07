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
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/buildflags.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_credential_filler.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_get_credentials_helper.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#endif  // BUILDFLAG(ENABLE_GLIC)

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
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ActorLoginDelegateImpl>(*web_contents),
      driver_supplier_(std::move(driver_supplier)),
      client_(client) {}

ActorLoginDelegateImpl::~ActorLoginDelegateImpl() = default;

// TODO(crbug.com/434156135): move to components/ as much as possible.
void ActorLoginDelegateImpl::GetCredentials(
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
    CredentialsOrErrorReply callback) {
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

  PasswordManagerDriver* driver = driver_supplier_.Run(&GetWebContents());
  CHECK(driver);

  const url::Origin request_origin =
      GetWebContents().GetPrimaryMainFrame()->GetLastCommittedOrigin();
  mqls_logger->SetDomainAndLanguage(
      ChromeTranslateClient::GetManagerFromWebContents(&GetWebContents()),
      request_origin.GetURL());
  get_credentials_helper_ = std::make_unique<ActorLoginGetCredentialsHelper>(
      request_origin, client_, driver->GetPasswordManager(), mqls_logger,
      base::BindOnce(&ActorLoginDelegateImpl::OnGetCredentialsCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ActorLoginDelegateImpl::AttemptLogin(
    const Credential& credential,
    bool should_store_permission,
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
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
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       base::unexpected(ActorLoginError::kFeatureDisabled)));
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
  mqls_logger->SetDomainAndLanguage(
      ChromeTranslateClient::GetManagerFromWebContents(&GetWebContents()),
      origin.GetURL());

  credential_filler_ = std::make_unique<ActorLoginCredentialFiller>(
      origin, credential, should_store_permission, client_, mqls_logger,
      base::BindRepeating(&ActorLoginDelegateImpl::IsTaskInFocus,
                          base::Unretained(this)),
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&ActorLoginDelegateImpl::OnAttemptLoginCompleted,
                         weak_ptr_factory_.GetWeakPtr())));
  credential_filler_->AttemptLogin(password_manager);
}

void ActorLoginDelegateImpl::WebContentsDestroyed() {
  get_credentials_helper_.reset();
  credential_filler_.reset();
  client_ = nullptr;
}

bool ActorLoginDelegateImpl::IsTaskInFocus() {
  // This `WebContents` comes from the `TabInterface` that
  // `ActorLoginService` is invoked with, so we know the `WebContents` is
  // attached to a tab.
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::GetFromContents(web_contents());
  BrowserWindowInterface* browser_window =
      tab_interface->GetBrowserWindowInterface();
  if (!browser_window->IsActive()) {
    return false;
  }
  if (tab_interface->IsActivated()) {
    return true;
  }
#if BUILDFLAG(ENABLE_GLIC)
  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedService::Get(web_contents()->GetBrowserContext());
  CHECK(glic_service);

  glic::GlicInstance* current_tab_instance =
      glic_service->GetInstanceForTab(tab_interface);
  glic::GlicInstance* active_tab_instance =
      glic_service->GetInstanceForActiveTab(
          tab_interface->GetBrowserWindowInterface());
  if (current_tab_instance != active_tab_instance) {
    return false;
  }

  return current_tab_instance->IsShowing();
#else
  NOTREACHED();
#endif
}

void ActorLoginDelegateImpl::OnGetCredentialsCompleted(
    CredentialsOrErrorReply callback,
    CredentialsOrError result) {
  get_credentials_helper_.reset();
  std::move(callback).Run(std::move(result));
}

void ActorLoginDelegateImpl::OnAttemptLoginCompleted(
    base::expected<LoginStatusResult, ActorLoginError> result) {
  // There shouldn't be a pending request without a pending callback.
  CHECK(pending_attempt_login_callback_);
  credential_filler_.reset();
  std::move(pending_attempt_login_callback_).Run(std::move(result));
}

}  // namespace actor_login
