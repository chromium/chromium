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
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/password_manager/actor_login/actor_login_permission_cleaning_service_factory.h"
#include "chrome/browser/password_manager/actor_login/actor_login_permission_service_factory.h"
#include "chrome/browser/password_manager/actor_login/internal/actor_login_federated_credentials_fetcher.h"
#include "chrome/browser/password_manager/actor_login/internal/actor_login_metrics_helper.h"
#include "chrome/browser/password_manager/actor_login/internal/actor_login_siwg_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/buildflags.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_credential_filler.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_credentials_fetcher.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_get_credentials_helper.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_password_credentials_fetcher.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_permission_cleaning_service.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/webid/federated_embedder_login_request.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "content/public/common/content_features.h"
#include "url/origin.h"

using password_manager::ContentPasswordManagerDriver;
using password_manager::PasswordForm;
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
    bool has_sign_in_with_google_button,
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
    CredentialsOrErrorReply callback) {
  CHECK(callback);

  // One request at a time mechanism using pending callbacks.
  // Check if either callback is currently active.
  if (get_credentials_helper_ || pending_attempt_login_done_callback_) {
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

  // A new login attempt will be made. Reset the state from the previous
  // attempt.
  ResetState();

  metrics_helper_ = std::make_unique<ActorLoginMetricsHelper>(
      GetWebContents().GetPrimaryMainFrame()->GetPageUkmSourceId());
  metrics_helper_->OnGetCredentialsStarted();

  PasswordManagerDriver* driver = driver_supplier_.Run(&GetWebContents());
  CHECK(driver);

  const url::Origin request_origin =
      GetWebContents().GetPrimaryMainFrame()->GetLastCommittedOrigin();
  mqls_logger->SetDomainAndLanguage(
      ChromeTranslateClient::GetManagerFromWebContents(&GetWebContents()),
      request_origin.GetURL());

  std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers;
  fetchers.push_back(std::make_unique<ActorLoginPasswordCredentialsFetcher>(
      request_origin, client_, driver->GetPasswordManager(), mqls_logger));

  if (has_sign_in_with_google_button) {
    ActorLoginPermissionService* permission_service =
        ActorLoginPermissionServiceFactory::GetForProfile(
            Profile::FromBrowserContext(GetWebContents().GetBrowserContext()));
    // This can be nullptr for incognito and guest profiles but these profiles
    // cannot use actor login.
    CHECK(permission_service);
    auto federated_fetcher =
        std::make_unique<ActorLoginFederatedCredentialsFetcher>(
            request_origin,
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
    federated_fetcher->SetMetricsHelper(metrics_helper_.get());
    fetchers.push_back(std::move(federated_fetcher));
  }

  get_credentials_helper_ = std::make_unique<ActorLoginGetCredentialsHelper>(
      std::move(fetchers), metrics_helper_.get(),
      base::BindOnce(&ActorLoginDelegateImpl::OnGetCredentialsCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ActorLoginDelegateImpl::AttemptLogin(
    const Credential& credential,
    bool should_store_permission,
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
    base::TimeTicks attempt_login_tool_start_time,
    LoginStatusResultOrErrorReply done_callback,
    base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate) {
  CHECK(done_callback);

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kActorLoginNoPermanentPermissionsAndroid)) {
    should_store_permission = false;
  }
#endif

  // One request at a time mechanism using pending callbacks.
  // Check if either callback is currently active.
  if (get_credentials_helper_ || pending_attempt_login_done_callback_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(done_callback),
                       base::unexpected(ActorLoginError::kServiceBusy)));
    return;
  }

  if (!base::FeatureList::IsEnabled(password_manager::features::kActorLogin)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(done_callback),
                       base::unexpected(ActorLoginError::kFeatureDisabled)));
    return;
  }

  last_attempted_credential_ = std::make_unique<Credential>(credential);

  // Store the callback to mark as active
  pending_attempt_login_done_callback_ = std::move(done_callback);
  action_sequence_delegate_ = std::move(action_sequence_delegate);

  const url::Origin origin =
      GetWebContents().GetPrimaryMainFrame()->GetLastCommittedOrigin();
  mqls_logger->SetDomainAndLanguage(
      ChromeTranslateClient::GetManagerFromWebContents(&GetWebContents()),
      origin.GetURL());

  if (!metrics_helper_) {
    metrics_helper_ = std::make_unique<ActorLoginMetricsHelper>(
        GetWebContents().GetPrimaryMainFrame()->GetPageUkmSourceId());
  }
  RecordAttemptLoginMetrics(credential);

  if (credential.type == CredentialType::kFederated) {
    actor::ActorKeyedService* actor_service =
        actor::ActorKeyedService::Get(GetWebContents().GetBrowserContext());
    CHECK(actor_service);
    actor_task_state_subscription_ = actor_service->AddTaskStateChangedCallback(
        base::BindRepeating(&ActorLoginDelegateImpl::OnActorTaskStateChanged,
                            weak_ptr_factory_.GetWeakPtr()));
    const actor::ActorTask* acting_task =
        actor_service->GetActingActorTaskForWebContents(&GetWebContents());
    CHECK(acting_task);
    CHECK(acting_task->IsUnderActorControl());
    acting_task_id_ = acting_task->id();

    ActorLoginPermissionService* permission_service =
        ActorLoginPermissionServiceFactory::GetForProfile(
            Profile::FromBrowserContext(GetWebContents().GetBrowserContext()));
    CHECK(permission_service);

    siwg_controller_ = std::make_unique<ActorLoginSiwgController>(
        &GetWebContents(), credential, should_store_permission,
        *permission_service,
        base::BindPostTaskToCurrentDefault(
            base::BindOnce(&ActorLoginDelegateImpl::OnAttemptLoginCompleted,
                           weak_ptr_factory_.GetWeakPtr())),
        action_sequence_delegate_, mqls_logger, attempt_login_tool_start_time,
        base::BindPostTaskToCurrentDefault(base::BindOnce(
            &ActorLoginDelegateImpl::OnFederatedLoginCompletedPostButtonClick,
            weak_ptr_factory_.GetWeakPtr())));
    siwg_controller_->StartFederatedLogin(std::move(metrics_helper_));
    return;
  }

  PasswordManagerDriver* driver = driver_supplier_.Run(&GetWebContents());
  CHECK(driver);
  PasswordManagerInterface* password_manager = driver->GetPasswordManager();
  CHECK(password_manager);
  credential_filler_ = std::make_unique<ActorLoginCredentialFiller>(
      origin, credential, should_store_permission, client_, mqls_logger,
      attempt_login_tool_start_time,
      base::BindRepeating(&ActorLoginDelegateImpl::IsTaskInFocus,
                          base::Unretained(this)),
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&ActorLoginDelegateImpl::OnAttemptLoginCompleted,
                         weak_ptr_factory_.GetWeakPtr())));
  credential_filler_->AttemptLogin(password_manager);
}

void ActorLoginDelegateImpl::OnLoginSuccessful(const PasswordForm& form) {
  // TODO(crbug.com/486089293): Double check that it's impossible to reach
  // this point if the credential is not a password and turn this into a CHECK.
  if (last_attempted_credential_->type != CredentialType::kPassword) {
    return;
  }

  if (ShouldCleanUpConflictingPermissions(form)) {
    ClearConflictingPermissions();
  }

  ResetState();
}

void ActorLoginDelegateImpl::WebContentsDestroyed() {
  get_credentials_helper_.reset();
  credential_filler_.reset();
  password_manager_observation_.Reset();
  client_ = nullptr;
}

void ActorLoginDelegateImpl::PrimaryPageChanged(content::Page& page) {
  // If the page changed while trying to fill in passwords,
  // signal this to the filler so it can interrupt its processes and
  // terminate the operation.
  if (credential_filler_) {
    credential_filler_->OnPrimaryPageChanged();
  }
}

void ActorLoginDelegateImpl::OnFederatedLoginCompletedPostButtonClick(
    bool success) {
  if (last_attempted_credential_->type != CredentialType::kFederated) {
    // The last login attempt wasn't a federated login, so this result
    // doesn't correspond to the `last_attempted_credential_`.
    return;
  }
  if (success && found_conflicting_permissions_ &&
      siwg_controller_->should_store_permission()) {
    ClearConflictingPermissions();
  }

  found_conflicting_permissions_ = false;
  last_attempted_credential_.reset();
  siwg_controller_.reset();
  ResetState();
}

bool ActorLoginDelegateImpl::IsTaskInFocus() {
  // This `WebContents` comes from the `TabInterface` that
  // `ActorLoginService` is invoked with, so we know the `WebContents` is
  // attached to a tab.
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::GetFromContents(web_contents());
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
}

void ActorLoginDelegateImpl::OnGetCredentialsCompleted(
    CredentialsOrErrorReply callback,
    CredentialsOrError result,
    bool conflicting_permissions) {
  get_credentials_helper_.reset();
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kActorLoginConflictingPermissionCleanup)) {
    found_conflicting_permissions_ = conflicting_permissions;
  }

  RecordGetCredentialsMetricsAndResetHelper(result);

  std::move(callback).Run(std::move(result));
}

void ActorLoginDelegateImpl::OnAttemptLoginCompleted(
    base::expected<LoginStatusResult, ActorLoginError> result) {
  // There shouldn't be a pending request without a pending callback.
  CHECK(pending_attempt_login_done_callback_);

  // If this is the end of the login flow, the `last_attempted_credential_`
  // will be reset.
  if (last_attempted_credential_->type == CredentialType::kFederated) {
    ProcessFederatedResult(result);
  } else if (last_attempted_credential_->type == CredentialType::kPassword) {
    ProcessPasswordResult(result);
  }

  // Record metrics by resetting the metrics helper.
  metrics_helper_.reset();

  std::move(pending_attempt_login_done_callback_).Run(std::move(result));
}

void ActorLoginDelegateImpl::ProcessFederatedResult(
    base::expected<LoginStatusResult, ActorLoginError> result) {
  // `kRequiresButtonClick` means that the federated login is not yet done.
  // We need to keep the controller alive so it can receive the result of the
  // login and store permissions if needed. It will be cleaned up together with
  // the delegate.
  if (result.has_value() &&
      result.value() == LoginStatusResult::kRequiresButtonClick) {
    return;
  }

  // While the continuation flow is an error for the model, we are still
  // expecting the success/failure status once the user resolves the
  // continuation prompt. So we do not reset the state here.
  if (result.has_value() &&
      result.value() == LoginStatusResult::kErrorFederatedContinuation) {
    return;
  }

  // If the federated login doesn't require a button click, there is no
  // action sequence to wait for, so we can clean up the controller and
  // clean up conflicting permissions if needed.
  if (result.has_value() &&
      result.value() == LoginStatusResult::kSuccessFederated &&
      found_conflicting_permissions_ &&
      siwg_controller_->should_store_permission()) {
    ClearConflictingPermissions();
  }
  // This is the end of the federated login flow if it didn't require a button
  // click. Flows requiring a button click end in
  // `OnFederatedLoginCompletedPostButtonClick`
  ResetState();
  siwg_controller_.reset();
}

void ActorLoginDelegateImpl::ProcessPasswordResult(
    base::expected<LoginStatusResult, ActorLoginError> result) {
  bool should_store_permission = credential_filler_->should_store_permission();
  credential_filler_.reset();

  // Only listen for successful login if:
  // - a new permission has been granted
  // - conflicting permissions existed prior to that
  // - filling succeeded (otherwise there can be no successful password login)
  // This ensures that we only try to clear conflicting permissions if
  // the conflict was resolved by the newly granted one.
  if (!should_store_permission || !found_conflicting_permissions_) {
    return;
  }

  if (!result.has_value() ||
      (result.value() != LoginStatusResult::kSuccessPasswordFilled &&
       result.value() != LoginStatusResult::kSuccessUsernameFilled &&
       result.value() !=
           LoginStatusResult::kSuccessUsernameAndPasswordFilled)) {
    return;
  }
  // Don't reset state here. The password login flow ends when
  // `OnLoginSucceeded` is called or if that doesn't happen, at the latest
  // when a new request comes in.
  password_manager_observation_.Observe(client_->GetPasswordManager());
}

void ActorLoginDelegateImpl::OnActorTaskStateChanged(actor::ActorTask& task) {
  if (acting_task_id_ != task.id()) {
    return;
  }

  if (!task.IsUnderActorControl()) {
    acting_task_id_ = actor::TaskId();
    content::webid::FederatedEmbedderLoginRequest::Remove(web_contents());
    actor_task_state_subscription_ = {};
  }
}

bool ActorLoginDelegateImpl::ShouldCleanUpConflictingPermissions(
    const PasswordForm& form) const {
  // If the latest request didn't find conflicting permissions, there is
  // nothing to clean up for the current credential configuration.
  if (!found_conflicting_permissions_) {
    return false;
  }

  // If the signal we got doesn't correspond to the latest attempted credential
  // or if the logged in credential didn't contain a new permission, don't
  // perform a cleanup.
  if (!form.actor_login_approved ||
      form.username_value != last_attempted_credential_->username ||
      form.signon_realm != last_attempted_credential_->signon_realm) {
    return false;
  }
  return true;
}

void ActorLoginDelegateImpl::ClearConflictingPermissions() {
  auto* cleaning_service =
      ActorLoginPermissionCleaningServiceFactory::GetForProfile(
          Profile::FromBrowserContext(GetWebContents().GetBrowserContext()));
  cleaning_service->ClearConflictingPermissions(
      *last_attempted_credential_,
      base::FeatureList::IsEnabled(features::kFedCmEmbedderInitiatedLogin),
      base::DoNothing());
}

void ActorLoginDelegateImpl::ResetState() {
  // TODO(crbug.com/500388451): Extract the fields and the logic in a
  // permissions cleaner tracker to make the lifetime clearer.
  password_manager_observation_.Reset();
  found_conflicting_permissions_ = false;
}

void ActorLoginDelegateImpl::RecordGetCredentialsMetricsAndResetHelper(
    const CredentialsOrError& result) {
  if (!metrics_helper_) {
    return;
  }

  metrics_helper_->OnGetCredentialsCompleted();
  if (result.has_value()) {
    bool has_password = false;
    bool has_federated = false;
    for (const auto& credential : result.value()) {
      if (credential.type == CredentialType::kPassword) {
        has_password = true;
      } else if (credential.type == CredentialType::kFederated) {
        has_federated = true;
      }
    }
    ActorLoginAccountTypes types = ActorLoginAccountTypes::kNone;
    if (has_password && has_federated) {
      types = ActorLoginAccountTypes::kPasswordAndFederated;
    } else if (has_password) {
      types = ActorLoginAccountTypes::kPassword;
    } else if (has_federated) {
      types = ActorLoginAccountTypes::kFederated;
    }
    metrics_helper_->RecordAccountTypesShown(types);
    metrics_helper_->RecordNumAccountsShown(result.value().size());
  } else {
    metrics_helper_->RecordAccountTypesShown(ActorLoginAccountTypes::kNone);
  }

  if (!result.has_value() || result.value().empty()) {
    metrics_helper_.reset();
  }
}

void ActorLoginDelegateImpl::RecordAttemptLoginMetrics(
    const Credential& credential) {
  CHECK(metrics_helper_);
  metrics_helper_->OnAccountChosen();
  metrics_helper_->RecordSelectedAccountType(
      credential.type == CredentialType::kFederated
          ? ActorLoginSelectedAccountType::kFederated
          : ActorLoginSelectedAccountType::kPassword);
  metrics_helper_->RecordAccountAutoSelected(
      credential.has_persistent_permission);
}

}  // namespace actor_login
