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
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/common/actor/action_result.h"
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
      return mojom::ActionResultCode::kError;
    case actor_login::ActorLoginError::kInvalidTabInterface:
      return mojom::ActionResultCode::kTabWentAway;
    case actor_login::ActorLoginError::kUnknown:
    default:
      return mojom::ActionResultCode::kError;
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
    case actor_login::LoginStatusResult::kErrorFillingNotAllowed:
      return mojom::ActionResultCode::kLoginFillingNotAllowed;
    case actor_login::LoginStatusResult::kErrorDeviceReauthRequired:
      // TODO(crbug.com/449923972): Handle this error: draw attention of the
      // user to the tab and retry once the tab is in the foreground.
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

AttemptLoginTool::~AttemptLoginTool() = default;

void AttemptLoginTool::Validate(ValidateCallback callback) {
  if (!base::FeatureList::IsEnabled(password_manager::features::kActorLogin)) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kToolUnknown));
    return;
  }

  PostResponseTask(std::move(callback), MakeOkResult());
}

void AttemptLoginTool::Invoke(InvokeCallback callback) {
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
  const std::optional<actor_login::Credential> user_selected_credential =
      tool_delegate().GetUserSelectedCredential(current_origin);
  if (user_selected_credential.has_value()) {
    GetActorLoginService().AttemptLogin(
        tab, *user_selected_credential, /*should_store_permission=*/false,
        base::BindOnce(&AttemptLoginTool::OnAttemptLogin,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  GetActorLoginService().GetCredentials(
      tab, base::BindOnce(&AttemptLoginTool::OnGetCredentials,
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

  // Cache the user selected credential for reuse.
  tool_delegate().SetUserSelectedCredential(*selected_credential);

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

  GetActorLoginService().AttemptLogin(
      tab, *selected_credential,
      response->permission_duration ==
          webui::mojom::UserGrantedPermissionDuration::kAlwaysAllow,
      base::BindOnce(&AttemptLoginTool::OnAttemptLogin,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AttemptLoginTool::OnAttemptLogin(
    actor_login::LoginStatusResultOrError login_status) {
  if (!login_status.has_value()) {
    PostResponseTask(std::move(invoke_callback_),
                     MakeResult(LoginErrorToActorError(login_status.error())));
    return;
  }

  PostResponseTask(std::move(invoke_callback_),
                   MakeResult(LoginResultToActorResult(login_status.value())));
}

std::string AttemptLoginTool::DebugString() const {
  return "AttemptLoginTool";
}

std::string AttemptLoginTool::JournalEvent() const {
  return "AttemptLogin";
}

std::unique_ptr<ObservationDelayController>
AttemptLoginTool::GetObservationDelayer(
    std::optional<ObservationDelayController::PageStabilityConfig>
        page_stability_config) {
  return std::make_unique<ObservationDelayController>(
      GetPrimaryMainFrameOfTab(tab_handle_), task_id(), journal(),
      page_stability_config);
}

void AttemptLoginTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                              InvokeCallback callback) const {
  task.AddTab(tab_handle_, std::move(callback));
}

tabs::TabHandle AttemptLoginTool::GetTargetTab() const {
  return tab_handle_;
}

actor_login::ActorLoginService& AttemptLoginTool::GetActorLoginService() {
  return tool_delegate().GetActorLoginService();
}

}  // namespace actor
