// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/chrome_actor_login_delegate_client.h"

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/password_manager/actor_login/actor_login_permission_cleaning_service_factory.h"
#include "chrome/browser/password_manager/actor_login/actor_login_permission_service_factory.h"
#include "chrome/browser/password_manager/actor_login/internal/actor_login_federated_credentials_fetcher.h"
#include "chrome/browser/password_manager/actor_login/internal/actor_login_siwg_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/buildflags.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_web_content_interface.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/webid/federated_embedder_login_request.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "content/public/common/content_features.h"

namespace actor_login {

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeActorLoginDelegateClient);

ChromeActorLoginDelegateClient::ChromeActorLoginDelegateClient(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ChromeActorLoginDelegateClient>(
          *web_contents) {}

ChromeActorLoginDelegateClient::~ChromeActorLoginDelegateClient() = default;

void ChromeActorLoginDelegateClient::SetActorLoginWebContentInterface(
    ActorLoginWebContentInterface* web_interface) {
  CHECK(web_interface);
  web_interface_ = web_interface;
}

PrefService* ChromeActorLoginDelegateClient::GetPrefs() {
  return Profile::FromBrowserContext(GetWebContents().GetBrowserContext())
      ->GetPrefs();
}

password_manager::PasswordManagerClient*
ChromeActorLoginDelegateClient::GetPasswordManagerClient() {
  if (auto* driver_factory =
          password_manager::ContentPasswordManagerDriverFactory::
              FromWebContents(&GetWebContents())) {
    return driver_factory->password_client();
  }
  return nullptr;
}

password_manager::PasswordManagerDriver*
ChromeActorLoginDelegateClient::GetPasswordManagerDriverForMainFrame() {
  if (content::RenderFrameHost* rfh = GetWebContents().GetPrimaryMainFrame()) {
    return password_manager::ContentPasswordManagerDriver::
        GetForRenderFrameHost(rfh);
  }
  return nullptr;
}

ukm::SourceId ChromeActorLoginDelegateClient::GetPageUkmSourceIdForMainFrame() {
  return GetWebContents().GetPrimaryMainFrame()->GetPageUkmSourceId();
}

url::Origin
ChromeActorLoginDelegateClient::GetLastCommittedOriginForMainFrame() {
  return GetWebContents().GetPrimaryMainFrame()->GetLastCommittedOrigin();
}

translate::TranslateManager*
ChromeActorLoginDelegateClient::GetTranslateManager() {
  return ChromeTranslateClient::GetManagerFromWebContents(&GetWebContents());
}

ActorLoginPermissionCleaningService*
ChromeActorLoginDelegateClient::GetPermissionCleaningService() {
  return ActorLoginPermissionCleaningServiceFactory::GetForProfile(
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext()));
}

std::unique_ptr<ActorLoginCredentialsFetcher>
ChromeActorLoginDelegateClient::CreateFederatedCredentialsFetcher(
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
    ActorLoginMetricsHelper* metrics_helper) {
  ActorLoginPermissionService* permission_service =
      ActorLoginPermissionServiceFactory::GetForProfile(
          Profile::FromBrowserContext(GetWebContents().GetBrowserContext()));
  // This can be nullptr for incognito and guest profiles but these profiles
  // cannot use actor login.
  CHECK(permission_service);
  auto federated_fetcher =
      std::make_unique<ActorLoginFederatedCredentialsFetcher>(
          GetLastCommittedOriginForMainFrame(),
          base::BindRepeating(
              [](base::WeakPtr<content::WebContents> web_contents)
                  -> content::webid::IdentityCredentialSource* {
                if (!web_contents) {
                  return nullptr;
                }
                return content::webid::IdentityCredentialSource::FromPage(
                    web_contents->GetPrimaryPage());
              },
              GetWebContents().GetWeakPtr()),
          *permission_service, mqls_logger);
  federated_fetcher->SetMetricsHelper(metrics_helper);
  return federated_fetcher;
}

std::unique_ptr<ActorLoginSiwgControllerInterface>
ChromeActorLoginDelegateClient::CreateSiwgController(
    const Credential& credential,
    bool should_store_permission,
    LoginStatusResultOrErrorReply on_finished_callback,
    base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate,
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
    base::TimeTicks attempt_login_tool_start_time,
    base::OnceCallback<void(bool)> post_button_click_login_result_callback) {
  ActorLoginPermissionService* permission_service =
      ActorLoginPermissionServiceFactory::GetForProfile(
          Profile::FromBrowserContext(GetWebContents().GetBrowserContext()));
  CHECK(permission_service);

  return std::make_unique<ActorLoginSiwgController>(
      &GetWebContents(), credential, should_store_permission,
      *permission_service, std::move(on_finished_callback),
      action_sequence_delegate, mqls_logger, attempt_login_tool_start_time,
      std::move(post_button_click_login_result_callback));
}

bool ChromeActorLoginDelegateClient::IsTaskInFocus() {
  // This `WebContents` comes from the `TabInterface` that
  // `ActorLoginService` is invoked with, so we know the `WebContents` is
  // attached to a tab.
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::GetFromContents(&GetWebContents());
// TODO(crbug.com/482430429): Reconsider the use of BrowserWindowInterface on
// Android.
#if !BUILDFLAG(IS_ANDROID)
  BrowserWindowInterface* browser_window =
      tab_interface->GetBrowserWindowInterface();
  if (!browser_window->IsActive()) {
    return false;
  }
#endif
  if (tab_interface->IsActivated()) {
    return true;
  }
  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedService::Get(GetWebContents().GetBrowserContext());
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
}

bool ChromeActorLoginDelegateClient::SupportsFedCmEmbedderInitiatedLogin() {
  return base::FeatureList::IsEnabled(features::kFedCmEmbedderInitiatedLogin);
}

void ChromeActorLoginDelegateClient::RemoveFederatedEmbedderLoginRequest() {
  content::webid::FederatedEmbedderLoginRequest::Remove(&GetWebContents());
}

void ChromeActorLoginDelegateClient::ObserveControlStateForCurrentTask(
    base::OnceClosure on_released_callback) {
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(GetWebContents().GetBrowserContext());
  CHECK(actor_service);

  const actor::ActorTask* acting_task =
      actor_service->GetActingActorTaskForWebContents(&GetWebContents());
  CHECK(acting_task);
  CHECK(acting_task->IsUnderActorControl());

  on_task_control_released_callback_ = std::move(on_released_callback);
  acting_task_id_ = acting_task->id();
  actor_task_state_subscription_ =
      actor_service->AddTaskStateChangedCallback(base::BindRepeating(
          &ChromeActorLoginDelegateClient::OnActorTaskStateChanged,
          weak_ptr_factory_.GetWeakPtr()));
}

void ChromeActorLoginDelegateClient::OnActorTaskStateChanged(
    actor::ActorTask& task) {
  if (acting_task_id_ != task.id()) {
    return;
  }
  if (!task.IsUnderActorControl()) {
    acting_task_id_ = actor::TaskId();
    actor_task_state_subscription_ = {};
    if (on_task_control_released_callback_) {
      std::move(on_task_control_released_callback_).Run();
    }
  }
}

base::WeakPtr<ActorLoginDelegateClient>
ChromeActorLoginDelegateClient::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ChromeActorLoginDelegateClient::PrimaryPageChanged(content::Page& page) {
  if (web_interface_) {
    web_interface_->OnPrimaryPageChanged();
  }
}

void ChromeActorLoginDelegateClient::WebContentsDestroyed() {
  if (web_interface_) {
    web_interface_->OnContextDestroyed();
  }
  web_interface_ = nullptr;
}
}  // namespace actor_login
