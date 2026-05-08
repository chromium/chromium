// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_login_tool.h"

#include "base/barrier_closure.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor.mojom-shared.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/favicon/core/favicon_service.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

// TODO(crbug.com/482430429): Reconsider the use of BrowserWindowInterface on
// Android.
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#endif

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

std::string MaybeTargetDebugString(const std::optional<PageTarget>& target) {
  return target ? DebugString(*target) : "null";
}

}  // namespace

// static
mojom::ActionResultCode AttemptLoginTool::LoginResultToActorResult(
    actor_login::LoginStatusResult login_result) {
  // TODO(crbug.com/427817201): Re-assess whether all success statuses should
  // map to kOk or if differentiation is needed.
  switch (login_result) {
    case actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled:
    case actor_login::LoginStatusResult::kSuccessUsernameFilled:
    case actor_login::LoginStatusResult::kSuccessPasswordFilled:
    case actor_login::LoginStatusResult::kSuccessFederated:
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
    case actor_login::LoginStatusResult::kErrorFederatedContinuation:
      return mojom::ActionResultCode::kLoginFederatedContinuation;
    case actor_login::LoginStatusResult::kErrorFederatedAccountNotLoggedIn:
      return mojom::ActionResultCode::kLoginFederatedAccountNotLoggedIn;
    case actor_login::LoginStatusResult::kErrorFederatedAccountIsSignUp:
      return mojom::ActionResultCode::kLoginFederatedAccountIsSignUp;
    case actor_login::LoginStatusResult::kErrorFederatedAccountNotAvailable:
      return mojom::ActionResultCode::kLoginFederatedAccountNotAvailable;
    case actor_login::LoginStatusResult::kErrorFederatedIdpReturnedError:
      return mojom::ActionResultCode::kLoginFederatedIdpReturnedError;
    case actor_login::LoginStatusResult::kErrorFederatedIdpNetworkError:
      return mojom::ActionResultCode::kLoginFederatedIdpNetworkError;
    case actor_login::LoginStatusResult::kErrorFederatedTokenRequestAborted:
      return mojom::ActionResultCode::kLoginFederatedTokenRequestAborted;
    case actor_login::LoginStatusResult::kErrorFederatedFrameNotActive:
      return mojom::ActionResultCode::kLoginFederatedFrameNotActive;
    case actor_login::LoginStatusResult::
        kErrorFederatedExpectedAccountNotPresent:
      return mojom::ActionResultCode::kLoginFederatedExpectedAccountNotPresent;
    case actor_login::LoginStatusResult::kErrorFederatedTimeout:
      return mojom::ActionResultCode::kLoginFederatedTimeout;
    case actor_login::LoginStatusResult::kRequiresButtonClick:
      // TODO(crbug.com/479505793): Consider adding a more specific error code.
      return mojom::ActionResultCode::kArgumentsInvalid;
    case actor_login::LoginStatusResult::kErrorPageChangedDuringFilling:
      return mojom::ActionResultCode::kLoginPasswordFillingPageChanged;
  }
}

AttemptLoginTool::AttemptLoginTool(
    TaskId task_id,
    ToolDelegate& tool_delegate,
    tabs::TabInterface& tab,
    std::optional<PageTarget> password_button,
    std::optional<PageTarget> sign_in_with_google_button,
    bool requires_opening_web_contents)
    : Tool(task_id, tool_delegate),
      tab_handle_(tab.GetHandle()),
      password_button_(password_button),
      sign_in_with_google_button_(sign_in_with_google_button),
      requires_opening_web_contents_(requires_opening_web_contents),
      attempt_login_tool_start_time_(base::TimeTicks::Now()) {}

AttemptLoginTool::~AttemptLoginTool() {
  // Uploading the quality log on the destruction of the tool.
  tabs::TabInterface* tab = tab_handle_.Get();
  Profile* profile =
      tab ? Profile::FromBrowserContext(tab->GetContents()->GetBrowserContext())
          : nullptr;
  // TODO(crbug.com/459397449): Update where the log is uploaded and
  // send a pointer to the profile/service when creating the log instead
  // of at the moment of uploading.
  if (!profile) {
    return;
  }
  OptimizationGuideKeyedService* opt_guide_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);

  // Disable MQLS upload if Password Checkup is enabled while prototyping to
  // avoid uploading incorrect logs.
  // TODO(crbug.com/485620841): Remove this check once the prototyping is
  // complete for Automated Password Change.
  bool prototype_features_enabled = base::FeatureList::IsEnabled(
      password_manager::features::kPasswordCheckupPrototype);

  if (opt_guide_service &&
      base::FeatureList::IsEnabled(
          password_manager::features::kActorLoginQualityLogs) &&
      !prototype_features_enabled) {
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

  journal().Log(
      JournalURL(), task_id(), "LoginTargets",
      JournalDetailsBuilder()
          .Add("password_button", MaybeTargetDebugString(password_button_))
          .Add("sign_in_with_google_button",
               MaybeTargetDebugString(sign_in_with_google_button_))
          .Build());

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
        attempt_login_tool_start_time_,
        base::BindOnce(&AttemptLoginTool::OnAttemptLogin,
                       weak_ptr_factory_.GetWeakPtr(),
                       user_selected_credential_and_pemission->credential,
                       should_store_permission),
        tool_delegate().GetActionSequenceDelegate());
    return;
  }

  // Only false on Android.
  if (!affiliations_updated_) {
    affiliations::AffiliationService* affiliation_service =
        AffiliationServiceFactory::GetForProfile(&tool_delegate().GetProfile());
    if (affiliation_service) {
      affiliation_service->UpdateAffiliationsAndBranding(
          {affiliations::FacetURI::FromPotentiallyInvalidSpec(
              current_origin.GetURL().GetWithEmptyPath().spec())},
          base::BindOnce(&AttemptLoginTool::OnAffiliationsUpdated,
                         weak_ptr_factory_.GetWeakPtr()));
    } else {
      // Unblock the tool execution even if AffiliationService is not available.
      affiliations_updated_ = true;
    }
  }

  GetActorLoginService().GetCredentials(
      tab, sign_in_with_google_button_.has_value(), quality_logger_.AsWeakPtr(),
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

  // When federated credentials are supported, allow selection of passwords on
  // non-login pages. If the user selects a password in this case, it will be up
  // to the server to find the password form.
  if (!base::FeatureList::IsEnabled(features::kFedCmEmbedderInitiatedLogin)) {
    std::erase_if(credentials_, [](const actor_login::Credential& cred) {
      return !cred.immediatelyAvailableToLogin;
    });

    if (credentials_.empty()) {
      // Saved credentials exist, but none are available for login, which
      // means that this is not a signin page.
      PostResponseTask(std::move(invoke_callback_),
                       MakeResult(mojom::ActionResultCode::kLoginNotLoginPage));
      return;
    }
  }

  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab) {
    PostResponseTask(std::move(invoke_callback_),
                     MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  FetchIcons();
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
    if (cred.source_site_or_app.empty()) {
      continue;
    }
    if (cred.type == actor_login::CredentialType::kPassword) {
      unique_sites.insert(GURL(cred.source_site_or_app));
    } else if (cred.federation_detail &&
               !cred.federation_detail->brand_icon.IsEmpty()) {
      fetched_icons_[base::UTF16ToUTF8(cred.source_site_or_app)] =
          cred.federation_detail->brand_icon;
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

void AttemptLoginTool::OnAffiliationsUpdated() {
  affiliations_updated_ = true;
  if (on_affiliations_updated_callback_) {
    std::move(on_affiliations_updated_callback_).Run();
  }
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

  webui::mojom::UserGrantedPermissionDuration permission_duration =
      response->permission_duration.value_or(
          webui::mojom::UserGrantedPermissionDuration::kOneTime);

  SetUserSelectedCredential(*selected_credential, permission_duration);
}

void AttemptLoginTool::SetUserSelectedCredential(
    actor_login::Credential selected_credential,
    webui::mojom::UserGrantedPermissionDuration permission_duration) {
  // TODO(crbug.com/504897444): Test this once browser tests are available on
  // Android.
  if (!affiliations_updated_) {
    on_affiliations_updated_callback_ =
        base::BindOnce(&AttemptLoginTool::SetUserSelectedCredential,
                       weak_ptr_factory_.GetWeakPtr(), selected_credential,
                       permission_duration);
    return;
  }

  tool_delegate().SetUserSelectedCredential(
      ToolDelegate::CredentialWithPermission(selected_credential,
                                             permission_duration),
      base::BindOnce(&AttemptLoginTool::OnCredentialCachingDone,
                     weak_ptr_factory_.GetWeakPtr(), selected_credential,
                     permission_duration));
}

void AttemptLoginTool::OnCredentialCachingDone(
    actor_login::Credential selected_credential,
    webui::mojom::UserGrantedPermissionDuration permission_duration) {
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
      permission_duration ==
      webui::mojom::UserGrantedPermissionDuration::kAlwaysAllow;

  GetActorLoginService().AttemptLogin(
      tab, selected_credential, should_store_permission,
      quality_logger_.AsWeakPtr(), attempt_login_tool_start_time_,
      base::BindOnce(&AttemptLoginTool::OnAttemptLogin,
                     weak_ptr_factory_.GetWeakPtr(), selected_credential,
                     should_store_permission),
      tool_delegate().GetActionSequenceDelegate());
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

  if (login_status.value() ==
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

  if (login_status.value() ==
          actor_login::LoginStatusResult::kRequiresButtonClick &&
      selected_credential.type == actor_login::CredentialType::kFederated &&
      selected_credential.federation_detail->idp_origin ==
          GaiaUrls::GetInstance()->gaia_origin() &&
      sign_in_with_google_button_.has_value()) {
    tool_delegate().EnqueueFollowupAction(std::make_unique<ClickToolRequest>(
        tab_handle_, *sign_in_with_google_button_, mojom::ClickType::kLeft,
        mojom::ClickCount::kSingle, requires_opening_web_contents_));
    PostResponseTask(std::move(invoke_callback_),
                     MakeOkResult(/*requires_page_stabilization=*/false));
    return;
  }

  // The availability of the password submit target is bundled with federated
  // support.
  if (base::FeatureList::IsEnabled(features::kFedCmEmbedderInitiatedLogin) &&
      (login_status.value() ==
           actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled ||
       login_status.value() ==
           actor_login::LoginStatusResult::kSuccessUsernameFilled ||
       login_status.value() ==
           actor_login::LoginStatusResult::kSuccessPasswordFilled) &&
      password_button_.has_value()) {
    CHECK_EQ(selected_credential.type, actor_login::CredentialType::kPassword);
    tool_delegate().EnqueueFollowupAction(std::make_unique<ClickToolRequest>(
        tab_handle_, *password_button_, mojom::ClickType::kLeft,
        mojom::ClickCount::kSingle, requires_opening_web_contents_));
    PostResponseTask(std::move(invoke_callback_),
                     MakeOkResult(/*requires_page_stabilization=*/false));
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
// TODO(crbug.com/482430429): Reconsider the use of BrowserWindowInterface on
// Android.
#if !BUILDFLAG(IS_ANDROID)
  BrowserWindowInterface* browser_window = tab->GetBrowserWindowInterface();
  // TODO(mcnee): Should we update the window subscription if the tab is moved?
  // The tab would probably be focused first which would cause us to stop
  // observing anyway.
  window_did_become_active_subscription_ =
      browser_window->RegisterDidBecomeActive(
          base::BindRepeating(&AttemptLoginTool::HandleWindowActivatedChange,
                              base::Unretained(this)));
#endif
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

  // Note that this is more specific than the conditions checked in
  // `ActorLoginDelegateImpl::IsTaskInFocus`, but for simplicity we check for
  // the specific tab being activated, since the task nudge will take the user
  // there anyway.
  if (!tab->IsActivated()) {
    return;
  }

  // TODO(crbug.com/482430429): Reconsider the use of BrowserWindowInterface on
  // Android.
#if !BUILDFLAG(IS_ANDROID)
  BrowserWindowInterface* browser_window = tab->GetBrowserWindowInterface();
  if (!browser_window->IsActive()) {
    return;
  }
#endif

  StopObservingTab();
  tool_delegate().UninterruptFromTool();

  GetActorLoginService().AttemptLogin(
      tab, credential_awaiting_task_focus_->first,
      credential_awaiting_task_focus_->second, quality_logger_.AsWeakPtr(),
      attempt_login_tool_start_time_,
      base::BindOnce(&AttemptLoginTool::OnAttemptLogin,
                     weak_ptr_factory_.GetWeakPtr(),
                     credential_awaiting_task_focus_->first,
                     credential_awaiting_task_focus_->second),
      tool_delegate().GetActionSequenceDelegate());
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
  task.AddTab(tab_handle_, /*stop_task_on_detach=*/true, std::move(callback));
}

tabs::TabHandle AttemptLoginTool::GetTargetTab() const {
  return tab_handle_;
}

actor_login::ActorLoginService& AttemptLoginTool::GetActorLoginService() {
  return tool_delegate().GetActorLoginService();
}

}  // namespace actor
