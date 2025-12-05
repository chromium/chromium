// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_login_tool.h"

#include "base/barrier_closure.h"
#include "base/containers/flat_set.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor_webui.mojom-data-view.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/favicon/core/favicon_service.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace actor {

namespace {

content::RenderFrameHost& GetPrimaryMainFrameOfTab(tabs::TabHandle tab_handle) {
  return *tab_handle.Get()->GetContents()->GetPrimaryMainFrame();
}

mojom::ActionResultCode LoginErrorToActorError(
    actor_login::ActorLoginError login_error) {
  switch (login_error) {
    case actor_login::ActorLoginError::kServiceBusy:
      return mojom::ActionResultCode::kLoginTooManyRequests;
    case actor_login::ActorLoginError::kInvalidTabInterface:
      return mojom::ActionResultCode::kTabWentAway;
    case actor_login::ActorLoginError::kFillingNotAllowed:
      return mojom::ActionResultCode::kLoginFillingNotAllowed;
    case actor_login::ActorLoginError::kFeatureDisabled:
      return mojom::ActionResultCode::kLoginFeatureDisabled;
  }
}

mojom::ActionResultCode LoginResultToActorResult(
    actor_login::LoginStatusResult login_result) {
  // TODO(crbug.com/427817201): Re-assess whether all success statuses should
  // map to kOk or if differentiation is needed.
  switch (login_result) {
    case actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled:
    case actor_login::LoginStatusResult::kSuccessUsernameFilled:
    case actor_login::LoginStatusResult::kSuccessPasswordFilled:
      return mojom::ActionResultCode::kOk;
    case actor_login::LoginStatusResult::kErrorNoSigninForm:
      return mojom::ActionResultCode::kLoginNotLoginPage;
    case actor_login::LoginStatusResult::kErrorInvalidCredential:
      return mojom::ActionResultCode::kLoginNoCredentialsAvailable;
    case actor_login::LoginStatusResult::kErrorNoFillableFields:
      return mojom::ActionResultCode::kLoginNoFillableFields;
    case actor_login::LoginStatusResult::kErrorDeviceReauthRequired:
      return mojom::ActionResultCode::kLoginDeviceReauthRequired;
    case actor_login::LoginStatusResult::kErrorDeviceReauthFailed:
      return mojom::ActionResultCode::kLoginDeviceReauthFailed;
  }
}

}  // namespace

AttemptLoginTool::AttemptLoginTool(TaskId task_id,
                                   ToolDelegate& tool_delegate,
                                   tabs::TabInterface& tab)
    : Tool(task_id, tool_delegate), tab_handle_(tab.GetHandle()) {}

AttemptLoginTool::~AttemptLoginTool() {
  // Uploading the quality log on the destruction of the tool.
  tabs::TabInterface* tab = tab_handle_.Get();
  Profile* profile =
      tab ? Profile::FromBrowserContext(tab->GetContents()->GetBrowserContext())
          : nullptr;
  // TODO(crbug,com/459397449): Update where the log is uploaded and
  // send a pointer to the profile/service when creating the log instead
  // of at the moment of uploading.
  if (!profile) {
    return;
  }
  OptimizationGuideKeyedService* opt_guide_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (opt_guide_service &&
      base::FeatureList::IsEnabled(
          password_manager::features::kActorLoginQualityLogs)) {
    // TODO(crbug.com/459393643): Add a check for filtering out logs of
    // enterprise users.
    quality_logger_.UploadFinalLog(
        opt_guide_service->GetModelQualityLogsUploaderService());
  }
}

void AttemptLoginTool::Validate(ToolCallback callback) {
  if (!base::FeatureList::IsEnabled(password_manager::features::kActorLogin)) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kToolUnknown));
    return;
  }

  PostResponseTask(std::move(callback), MakeOkResult());
}

void AttemptLoginTool::Invoke(ToolCallback callback) {
  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  content::RenderFrameHost* main_rfh =
      tab->GetContents()->GetPrimaryMainFrame();
  main_rfh_token_ = main_rfh->GetGlobalFrameToken();

  invoke_callback_ = std::move(callback);

  // First check if there is a user selected credential for the current request
  // origin. If so, use it immediately.
  const url::Origin& current_origin = main_rfh->GetLastCommittedOrigin();
  const std::optional<ToolDelegate::CredentialWithPermission>
      user_selected_credential_and_pemission =
          tool_delegate().GetUserSelectedCredential(current_origin);
  if (user_selected_credential_and_pemission.has_value()) {
    const bool should_store_permission =
        user_selected_credential_and_pemission->permission_duration ==
        webui::mojom::UserGrantedPermissionDuration::kAlwaysAllow;
    GetActorLoginService().AttemptLogin(
        tab, user_selected_credential_and_pemission->credential,
        should_store_permission, quality_logger_.AsWeakPtr(),
        base::BindOnce(&AttemptLoginTool::OnAttemptLogin,
                       weak_ptr_factory_.GetWeakPtr(),
                       user_selected_credential_and_pemission->credential,
                       should_store_permission));
    return;
  }

  GetActorLoginService().GetCredentials(
      tab, quality_logger_.AsWeakPtr(),
      base::BindOnce(&AttemptLoginTool::OnGetCredentials,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AttemptLoginTool::OnGetCredentials(
    actor_login::CredentialsOrError credentials) {
  if (!credentials.has_value()) {
    PostResponseTask(std::move(invoke_callback_),
                     MakeResult(LoginErrorToActorError(credentials.error())));
    return;
  }

  credentials_ = std::move(credentials.value());

  if (credentials_.empty()) {
    PostResponseTask(
        std::move(invoke_callback_),
        MakeResult(mojom::ActionResultCode::kLoginNoCredentialsAvailable));
    return;
  }

  if (base::FeatureList::IsEnabled(
          actor::kGlicEnableAutoLoginPersistedPermissions)) {
    const auto it_persistent_permission =
        std::find_if(credentials_.begin(), credentials_.end(),
                     [](const actor_login::Credential& cred) {
                       return cred.has_persistent_permission;
                     });
    if (it_persistent_permission != credentials_.end()) {
      OnCredentialSelected(webui::mojom::SelectCredentialDialogResponse::New(
          task_id().value(), /*error_reason=*/std::nullopt,
          webui::mojom::UserGrantedPermissionDuration::kAlwaysAllow,
          it_persistent_permission->id.value()));
      return;
    }
  }

  std::erase_if(credentials_, [](const actor_login::Credential& cred) {
    return !cred.immediatelyAvailableToLogin;
  });
  if (credentials_.empty()) {
    PostResponseTask(
        std::move(invoke_callback_),
        MakeResult(mojom::ActionResultCode::kLoginNoCredentialsAvailable));
    return;
  }

  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab) {
    PostResponseTask(std::move(invoke_callback_),
                     MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  // Unless the flag is enabled, always auto-select the first credential, which
  // is the credential that is most likely to be the correct one.
  if (base::FeatureList::IsEnabled(actor::kGlicEnableAutoLoginDialogs)) {
    FetchIcons();
  } else {
    // The task ID doesn't matter here because the task ID check is already
    // done at this point.
    auto response = webui::mojom::SelectCredentialDialogResponse::New();
    response->selected_credential_id = credentials_[0].id.value();
    OnCredentialSelected(std::move(response));
  }
}

void AttemptLoginTool::FetchIcons() {
  favicon::FaviconService* favicon_service =
      tool_delegate().GetFaviconService();
  if (!favicon_service) {
    // If there is no favicon service, just proceed without favicons.
    tool_delegate().PromptToSelectCredential(
        credentials_,
        /*icons=*/{},
        base::BindOnce(&AttemptLoginTool::OnCredentialSelected,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  base::flat_set<GURL> unique_sites;
  for (const auto& cred : credentials_) {
    if (!cred.source_site_or_app.empty()) {
      unique_sites.insert(GURL(cred.source_site_or_app));
    }
  }

  // OnAllIconsFetched is called immediately if unique_sites is empty.
  base::RepeatingClosure barrier = base::BarrierClosure(
      unique_sites.size(), base::BindOnce(&AttemptLoginTool::OnAllIconsFetched,
                                          weak_ptr_factory_.GetWeakPtr()));
  favicon_requests_tracker_ =
      std::vector<base::CancelableTaskTracker>(unique_sites.size());

  size_t i = 0u;
  for (const GURL& site : unique_sites) {
    favicon_service->GetFaviconImageForPageURL(
        site,
        base::BindOnce(&AttemptLoginTool::OnIconFetched,
                       weak_ptr_factory_.GetWeakPtr(), barrier, site),
        &favicon_requests_tracker_[i]);
    ++i;
  }
}

void AttemptLoginTool::OnIconFetched(
    base::RepeatingClosure barrier,
    GURL site,
    const favicon_base::FaviconImageResult& result) {
  if (!result.image.IsEmpty()) {
    fetched_icons_[site.GetWithEmptyPath().spec()] = result.image;
  }
  barrier.Run();
}

void AttemptLoginTool::OnAllIconsFetched() {
  tool_delegate().PromptToSelectCredential(
      credentials_, fetched_icons_,
      base::BindOnce(&AttemptLoginTool::OnCredentialSelected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AttemptLoginTool::OnCredentialSelected(
    webui::mojom::SelectCredentialDialogResponsePtr response) {
  std::optional<actor_login::Credential> selected_credential;
  std::vector<actor_login::Credential> credentials = std::move(credentials_);
  if (response->error_reason ==
      webui::mojom::SelectCredentialDialogErrorReason::
          kDialogPromiseNoSubscriber) {
    VLOG(1) << "selectCredentialDialogRequestHandler() has no subscriber. "
               "The web client is likely not set up correctly.";
  } else if (response->selected_credential_id.has_value()) {
    auto it = std::find_if(
        credentials.begin(), credentials.end(),
        [&](const actor_login::Credential& credential) {
          return credential.id ==
                 actor_login::Credential::Id(*response->selected_credential_id);
        });
    if (it != credentials.end()) {
      selected_credential = *it;
    } else {
      VLOG(1) << "Selected credential id " << *response->selected_credential_id
              << " not found in the credentials list.";
    }
  } else {
    quality_logger_.SetPermissionPicked(
        optimization_guide::proto::
            ActorLoginQuality_PermissionOption_TASK_STOPPED);
    VLOG(2) << "SelectCredentialDialogResponse has no selected "
               "credential id.";
  }
  if (!selected_credential.has_value()) {
    // We don't need to distinguish between no credentials being available and a
    // user declining the usage of a credential.
    PostResponseTask(
        std::move(invoke_callback_),
        MakeResult(mojom::ActionResultCode::kLoginNoCredentialsAvailable));
    return;
  }

  if (response->permission_duration.has_value()) {
    switch (response->permission_duration.value()) {
      case webui::mojom::UserGrantedPermissionDuration::kOneTime:
        quality_logger_.SetPermissionPicked(
            optimization_guide::proto::
                ActorLoginQuality_PermissionOption_ALLOW_ONCE);
        break;
      case webui::mojom::UserGrantedPermissionDuration::kAlwaysAllow:
        quality_logger_.SetPermissionPicked(
            optimization_guide::proto::
                ActorLoginQuality_PermissionOption_ALWAYS_ALLOW);
        break;
    }
  } else {
    quality_logger_.SetPermissionPicked(
        optimization_guide::proto::ActorLoginQuality_PermissionOption_UNKNOWN);
  }
  // Cache the user selected credential for reuse.
  tool_delegate().SetUserSelectedCredential(
      ToolDelegate::CredentialWithPermission(
          *selected_credential,
          response->permission_duration.value_or(
              webui::mojom::UserGrantedPermissionDuration::kOneTime)));

  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab) {
    PostResponseTask(std::move(invoke_callback_),
                     MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  if (main_rfh_token_ !=
      tab->GetContents()->GetPrimaryMainFrame()->GetGlobalFrameToken()) {
    // Don't proceed with the login attempt, if the page changed while we were
    // waiting for credential selection.
    PostResponseTask(
        std::move(invoke_callback_),
        MakeResult(mojom::ActionResultCode::kLoginPageChangedDuringSelection));
    return;
  }

  const bool should_store_permission =
      response->permission_duration ==
      webui::mojom::UserGrantedPermissionDuration::kAlwaysAllow;
  GetActorLoginService().AttemptLogin(
      tab, *selected_credential, should_store_permission,
      quality_logger_.AsWeakPtr(),
      base::BindOnce(&AttemptLoginTool::OnAttemptLogin,
                     weak_ptr_factory_.GetWeakPtr(), *selected_credential,
                     should_store_permission));
}

void AttemptLoginTool::OnAttemptLogin(
    actor_login::Credential selected_credential,
    bool should_store_permission,
    actor_login::LoginStatusResultOrError login_status) {
  if (!login_status.has_value()) {
    PostResponseTask(std::move(invoke_callback_),
                     MakeResult(LoginErrorToActorError(login_status.error())));
    return;
  }

  if (base::FeatureList::IsEnabled(
          password_manager::features::kActorLoginReauthTaskRefocus) &&
      login_status.value() ==
          actor_login::LoginStatusResult::kErrorDeviceReauthRequired) {
    if (!tab_handle_.Get()) {
      PostResponseTask(std::move(invoke_callback_),
                       MakeResult(mojom::ActionResultCode::kTabWentAway));
      return;
    }

    credential_awaiting_task_focus_ = {selected_credential,
                                       should_store_permission};
    ObserveTabToAwaitFocus();
    tool_delegate().InterruptFromTool();
    return;
  }

  mojom::ActionResultCode code = LoginResultToActorResult(login_status.value());
  PostResponseTask(std::move(invoke_callback_),
                   IsOk(code) ? MakeOkResult() : MakeResult(code));
}

void AttemptLoginTool::OnWillDetach(tabs::TabInterface* tab,
                                    tabs::TabInterface::DetachReason reason) {
  if (reason == tabs::TabInterface::DetachReason::kDelete &&
      credential_awaiting_task_focus_.has_value()) {
    PostResponseTask(std::move(invoke_callback_),
                     MakeResult(mojom::ActionResultCode::kTabWentAway));
  }
}

void AttemptLoginTool::HandleTabActivatedChange(tabs::TabInterface* tab) {
  MaybeRetryCredentialNeedingFocus();
}

void AttemptLoginTool::HandleWindowActivatedChange(
    BrowserWindowInterface* browser_window) {
  MaybeRetryCredentialNeedingFocus();
}

void AttemptLoginTool::ObserveTabToAwaitFocus() {
  tabs::TabInterface* tab = tab_handle_.Get();
  CHECK(tab);

  will_detach_subscription_ = tab->RegisterWillDetach(base::BindRepeating(
      &AttemptLoginTool::OnWillDetach, base::Unretained(this)));
  tab_did_activate_subscription_ = tab->RegisterDidActivate(base::BindRepeating(
      &AttemptLoginTool::HandleTabActivatedChange, base::Unretained(this)));
  BrowserWindowInterface* browser_window = tab->GetBrowserWindowInterface();
  // TODO(mcnee): Should we update the window subscription if the tab is moved?
  // The tab would probably be focused first which would cause us to stop
  // observing anyway.
  window_did_become_active_subscription_ =
      browser_window->RegisterDidBecomeActive(
          base::BindRepeating(&AttemptLoginTool::HandleWindowActivatedChange,
                              base::Unretained(this)));
}

void AttemptLoginTool::StopObservingTab() {
  will_detach_subscription_ = {};
  tab_did_activate_subscription_ = {};
  window_did_become_active_subscription_ = {};
}

void AttemptLoginTool::MaybeRetryCredentialNeedingFocus() {
  if (!credential_awaiting_task_focus_.has_value()) {
    return;
  }

  tabs::TabInterface* tab = tab_handle_.Get();
  CHECK(tab);
  BrowserWindowInterface* browser_window = tab->GetBrowserWindowInterface();

  // Note that this is more specific than the conditions checked in
  // `ActorLoginDelegateImpl::IsTaskInFocus`, but for simplicity we check for
  // the specific tab being activated, since the task nudge will take the user
  // there anyway.
  if (!browser_window->IsActive() || !tab->IsActivated()) {
    return;
  }

  StopObservingTab();
  tool_delegate().UninterruptFromTool();

  GetActorLoginService().AttemptLogin(
      tab, credential_awaiting_task_focus_->first,
      credential_awaiting_task_focus_->second, quality_logger_.AsWeakPtr(),
      base::BindOnce(&AttemptLoginTool::OnAttemptLogin,
                     weak_ptr_factory_.GetWeakPtr(),
                     credential_awaiting_task_focus_->first,
                     credential_awaiting_task_focus_->second));
}

std::string AttemptLoginTool::DebugString() const {
  return "AttemptLoginTool";
}

std::string AttemptLoginTool::JournalEvent() const {
  return "AttemptLogin";
}

std::unique_ptr<ObservationDelayController>
AttemptLoginTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  return std::make_unique<ObservationDelayController>(
      GetPrimaryMainFrameOfTab(tab_handle_), task_id(), journal(),
      page_stability_config);
}

void AttemptLoginTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                              ToolCallback callback) const {
  task.AddTab(tab_handle_, std::move(callback));
}

tabs::TabHandle AttemptLoginTool::GetTargetTab() const {
  return tab_handle_;
}

actor_login::ActorLoginService& AttemptLoginTool::GetActorLoginService() {
  return tool_delegate().GetActorLoginService();
}

}  // namespace actor
