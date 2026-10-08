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
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/attempt_login_tool_request.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/actor_login/chrome_actor_login_delegate_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/actor.mojom-shared.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/favicon/core/favicon_service.h"
#include "components/password_manager/core/browser/actor_login/actor_login_service.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

// TODO(crbug.com/482430429): Reconsider the use of BrowserWindowInterface on
// Android.
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/password_manager/password_change/features.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "ui/base/base_window.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace actor {

using actor_login::ChromeActorLoginDelegateClient;

namespace {

content::RenderFrameHost& GetPrimaryMainFrameOfActorSurface(
    ActorSurfaceHandle actor_surface_handle) {
  return *actor_surface_handle.Get()->GetWebContents()->GetPrimaryMainFrame();
}

std::string MaybeTargetDebugString(const std::optional<PageTarget>& target) {
  return target ? DebugString(*target) : "null";
}

// Returns the service the quality log is uploaded to, or null if quality
// logging is disabled, in which case the log is collected but never uploaded.
optimization_guide::ModelQualityLogsUploaderService*
GetModelQualityLogsUploader(Profile& profile) {
  // TODO(crbug.com/562029939): Clean up kActorLoginQualityLogs.
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kActorLoginQualityLogs)) {
    return nullptr;
  }

  // Disable MQLS upload if Password Change is enabled while prototyping to
  // avoid uploading incorrect logs.
  // TODO(crbug.com/485620841): Remove this check once the prototyping is
  // complete for Automated Password Change.
#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          password_change::features::kPasswordChangeWithGlic)) {
    return nullptr;
  }
#endif

  OptimizationGuideKeyedService* opt_guide_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(&profile);
  return opt_guide_service
             ? opt_guide_service->GetModelQualityLogsUploaderService()
             : nullptr;
}

bool IsSuccessfulPasswordCredentialFilling(
    actor_login::LoginStatusResult login_result) {
  switch (login_result) {
    case actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled:
    case actor_login::LoginStatusResult::kSuccessUsernameFilled:
    case actor_login::LoginStatusResult::kSuccessPasswordFilled:
      return true;
    case actor_login::LoginStatusResult::kSuccessFederated:
    case actor_login::LoginStatusResult::kErrorNoSigninForm:
    case actor_login::LoginStatusResult::kErrorInvalidCredential:
    case actor_login::LoginStatusResult::kErrorNoFillableFields:
    case actor_login::LoginStatusResult::kErrorDeviceReauthRequired:
    case actor_login::LoginStatusResult::kErrorDeviceReauthFailed:
    case actor_login::LoginStatusResult::kErrorFederatedContinuation:
    case actor_login::LoginStatusResult::kErrorFederatedAccountNotLoggedIn:
    case actor_login::LoginStatusResult::kErrorFederatedAccountIsSignUp:
    case actor_login::LoginStatusResult::kErrorFederatedAccountNotAvailable:
    case actor_login::LoginStatusResult::kErrorFederatedIdpReturnedError:
    case actor_login::LoginStatusResult::kErrorFederatedIdpNetworkError:
    case actor_login::LoginStatusResult::kErrorFederatedTokenRequestAborted:
    case actor_login::LoginStatusResult::kErrorFederatedFrameNotActive:
    case actor_login::LoginStatusResult::
        kErrorFederatedExpectedAccountNotPresent:
    case actor_login::LoginStatusResult::kErrorFederatedTimeout:
    case actor_login::LoginStatusResult::kRequiresButtonClick:
    case actor_login::LoginStatusResult::kErrorPageChangedDuringFilling:
      return false;
  }
}

}  // namespace

AttemptLoginTool::AttemptLoginTool(
    TaskId task_id,
    ToolDelegate& tool_delegate,
    ActorSurface& actor_surface,
    std::optional<PageTarget> password_button,
    std::optional<PageTarget> sign_in_with_google_button,
    bool requires_opening_web_contents)
    : Tool(task_id, tool_delegate),
      actor_surface_handle_(actor_surface.GetHandle()),
      password_button_(password_button),
      sign_in_with_google_button_(sign_in_with_google_button),
      requires_opening_web_contents_(requires_opening_web_contents),
      attempt_login_tool_start_time_(base::TimeTicks::Now()),
      quality_logger_(base::MakeRefCounted<ActorLoginQualityLogger>(
          g_browser_process->variations_service(),
          GetModelQualityLogsUploader(tool_delegate.GetProfile()))) {}

AttemptLoginTool::~AttemptLoginTool() = default;

void AttemptLoginTool::Validate(ToolCallback callback) {
  if (!base::FeatureList::IsEnabled(password_manager::features::kActorLogin)) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kToolUnknown));
    return;
  }

  PostResponseTask(std::move(callback), MakeOkResult());
}

void AttemptLoginTool::Invoke(ToolCallback callback) {
  ActorSurface* actor_surface = actor_surface_handle_.Get();
  if (!actor_surface) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  content::RenderFrameHost* main_rfh =
      actor_surface->GetWebContents()->GetPrimaryMainFrame();
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
      user_selected_credential_and_permission =
          tool_delegate().GetUserSelectedCredential(current_origin);
  if (user_selected_credential_and_permission.has_value()) {
    const bool should_store_permission =
        user_selected_credential_and_permission->permission_duration ==
        webui::mojom::UserGrantedPermissionDuration::kAlwaysAllow;

    GetActorLoginService().AttemptLogin(
        ChromeActorLoginDelegateClient::GetOrCreateForWebContents(
            actor_surface->GetWebContents()),
        user_selected_credential_and_permission->credential,
        should_store_permission, quality_logger_,
        attempt_login_tool_start_time_,
        GetFrameFillingStartedCallback(
            user_selected_credential_and_permission->credential),
        base::BindOnce(&AttemptLoginTool::OnAttemptLogin,
                       weak_ptr_factory_.GetWeakPtr(),
                       user_selected_credential_and_permission->credential,
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
      ChromeActorLoginDelegateClient::GetOrCreateForWebContents(
          actor_surface->GetWebContents()),
      sign_in_with_google_button_.has_value(), quality_logger_,
      base::BindOnce(&AttemptLoginTool::OnGetCredentials,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AttemptLoginTool::OnGetCredentials(
    actor_login::CredentialsOrError credentials) {
  if (!credentials.has_value()) {
    PostResponseTask(
        std::move(invoke_callback_),
        MakeResult(actor_login::LoginErrorToActorResult(credentials.error())));
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

  if (!actor_surface_handle_.Get()) {
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
    quality_logger_->SetPermissionPicked(
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
        quality_logger_->SetPermissionPicked(
            optimization_guide::proto::
                ActorLoginQuality_PermissionOption_ALLOW_ONCE);
        break;
      case webui::mojom::UserGrantedPermissionDuration::kAlwaysAllow:
        quality_logger_->SetPermissionPicked(
            optimization_guide::proto::
                ActorLoginQuality_PermissionOption_ALWAYS_ALLOW);
        break;
    }
  } else {
    quality_logger_->SetPermissionPicked(
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
  ActorSurface* actor_surface = actor_surface_handle_.Get();
  if (!actor_surface) {
    PostResponseTask(std::move(invoke_callback_),
                     MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  if (main_rfh_token_ != actor_surface->GetWebContents()
                             ->GetPrimaryMainFrame()
                             ->GetGlobalFrameToken()) {
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
      ChromeActorLoginDelegateClient::GetOrCreateForWebContents(
          actor_surface->GetWebContents()),
      selected_credential, should_store_permission, quality_logger_,
      attempt_login_tool_start_time_,
      GetFrameFillingStartedCallback(selected_credential),
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
    tool_delegate().GetActorOneTimeTokenFillingService().AbortLoginTracking();
    PostResponseTask(
        std::move(invoke_callback_),
        MakeResult(actor_login::LoginErrorToActorResult(login_status.error())));
    return;
  }

  if (!IsSuccessfulPasswordCredentialFilling(login_status.value())) {
    tool_delegate().GetActorOneTimeTokenFillingService().AbortLoginTracking();
  }

  if (login_status.value() ==
      actor_login::LoginStatusResult::kErrorDeviceReauthRequired) {
    if (!actor_surface_handle_.Get()) {
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
    if (!actor_surface_handle_.Get()) {
      PostResponseTask(std::move(invoke_callback_),
                       MakeResult(mojom::ActionResultCode::kTabWentAway));
      return;
    }
    tool_delegate().EnqueueFollowupAction(std::make_unique<ClickToolRequest>(
        actor_surface_handle_.GetTabHandle(), *sign_in_with_google_button_,
        mojom::ClickType::kLeft, mojom::ClickCount::kSingle,
        requires_opening_web_contents_,
        AttemptLoginToolRequest::GetLoginObservationPageStabilityConfig()));
    mojom::ActionResultPtr result =
        MakeOkResult(/*requires_page_stabilization=*/false);
    result->attempt_login_status = mojom::AttemptLoginStatus::kFederated;
    PostResponseTask(std::move(invoke_callback_), std::move(result));
    return;
  }

  // The availability of the password submit target is bundled with federated
  // support.
  if (base::FeatureList::IsEnabled(features::kFedCmEmbedderInitiatedLogin) &&
      IsSuccessfulPasswordCredentialFilling(login_status.value()) &&
      password_button_.has_value()) {
    CHECK_EQ(selected_credential.type, actor_login::CredentialType::kPassword);
    if (!actor_surface_handle_.Get()) {
      PostResponseTask(std::move(invoke_callback_),
                       MakeResult(mojom::ActionResultCode::kTabWentAway));
      return;
    }
    tool_delegate().EnqueueFollowupAction(std::make_unique<ClickToolRequest>(
        actor_surface_handle_.GetTabHandle(), *password_button_,
        mojom::ClickType::kLeft, mojom::ClickCount::kSingle,
        requires_opening_web_contents_,
        AttemptLoginToolRequest::GetLoginObservationPageStabilityConfig()));
    mojom::ActionResultPtr result =
        MakeOkResult(/*requires_page_stabilization=*/false);
    result->attempt_login_status = mojom::AttemptLoginStatus::kPasswordManager;
    PostResponseTask(std::move(invoke_callback_), std::move(result));
    return;
  }

  mojom::ActionResultCode code =
      actor_login::LoginResultToActorResult(login_status.value());
  mojom::ActionResultPtr result =
      IsOk(code) ? MakeOkResult() : MakeResult(code);
  if (IsOk(code)) {
    result->attempt_login_status =
        login_status.value() ==
                actor_login::LoginStatusResult::kSuccessFederated
            ? mojom::AttemptLoginStatus::kFederated
            : mojom::AttemptLoginStatus::kPasswordManager;
  }
  PostResponseTask(std::move(invoke_callback_), std::move(result));
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

#if BUILDFLAG(IS_ANDROID)
void AttemptLoginTool::OnBrowserActivated(BrowserWindowInterface* browser) {
  tabs::TabInterface* tab = actor_surface_handle_.GetTabHandle().Get();
  if (tab && tab->GetBrowserWindowInterface() == browser) {
    MaybeRetryCredentialNeedingFocus();
  }
}
#else
void AttemptLoginTool::HandleWindowActivatedChange(
    BrowserWindowInterface* browser_window) {
  MaybeRetryCredentialNeedingFocus();
}
#endif  // BUILDFLAG(IS_ANDROID)

void AttemptLoginTool::ObserveTabToAwaitFocus() {
  // TODO(b/567721071): This needs tab-specific activation and browser window
  // APIs, so it observes the tab currently backing the surface.
  tabs::TabInterface* tab = actor_surface_handle_.GetTabHandle().Get();
  CHECK(tab);

  will_detach_subscription_ = tab->RegisterWillDetach(base::BindRepeating(
      &AttemptLoginTool::OnWillDetach, base::Unretained(this)));
  tab_did_activate_subscription_ = tab->RegisterDidActivate(base::BindRepeating(
      &AttemptLoginTool::HandleTabActivatedChange, base::Unretained(this)));
// TODO(crbug.com/482430429): Reconsider the use of BrowserWindowInterface on
// Android.
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          password_manager::features::kBiometricTouchToFill)) {
    browser_observation_.Observe(GlobalBrowserCollection::GetInstance());
  }
#else
  BrowserWindowInterface* browser_window = tab->GetBrowserWindowInterface();
  // TODO(mcnee): Should we update the window subscription if the tab is moved?
  // The tab would probably be focused first which would cause us to stop
  // observing anyway.
  window_did_become_active_subscription_ =
      browser_window->RegisterDidBecomeActive(
          base::BindRepeating(&AttemptLoginTool::HandleWindowActivatedChange,
                              base::Unretained(this)));
#endif  // BUILDFLAG(IS_ANDROID)
}

void AttemptLoginTool::StopObservingTab() {
  will_detach_subscription_ = {};
  tab_did_activate_subscription_ = {};
#if BUILDFLAG(IS_ANDROID)
  browser_observation_.Reset();
#else
  window_did_become_active_subscription_ = {};
#endif  // BUILDFLAG(IS_ANDROID)
}

void AttemptLoginTool::MaybeRetryCredentialNeedingFocus() {
  if (!credential_awaiting_task_focus_.has_value()) {
    return;
  }

  // TODO(b/567721071): Handle surfaces demoted to headless while awaiting
  // focus.
  tabs::TabInterface* tab = actor_surface_handle_.GetTabHandle().Get();
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
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          password_manager::features::kBiometricTouchToFill)) {
    // On Android, a single Activity window can host multiple TabModels (e.g.
    // standard and incognito). Therefore, verifying window->IsActive() is not
    // sufficient; we also need to ensure the TabModel containing our tab is the
    // currently active model, and that our tab is the selected active tab
    // within it.
    BrowserWindowInterface* browser_window = tab->GetBrowserWindowInterface();
    ::ui::BaseWindow* window =
        browser_window ? browser_window->GetWindow() : nullptr;
    if (!window || !window->IsActive()) {
      return;
    }
    TabListInterface* tab_list =
        browser_window ? TabListInterface::From(browser_window) : nullptr;
    if (!tab_list || tab_list->GetActiveTab() != tab) {
      return;
    }
    // On Android, TabListInterface is always implemented by TabModel.
    TabModel* tab_model = static_cast<TabModel*>(tab_list);
    if (!tab_model->IsActiveModel()) {
      return;
    }
  }
#else
  BrowserWindowInterface* browser_window = tab->GetBrowserWindowInterface();
  if (!browser_window->IsActive()) {
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  StopObservingTab();
  tool_delegate().UninterruptFromTool();

  GetActorLoginService().AttemptLogin(
      ChromeActorLoginDelegateClient::GetOrCreateForWebContents(
          tab->GetContents()),
      credential_awaiting_task_focus_->first,
      credential_awaiting_task_focus_->second, quality_logger_,
      attempt_login_tool_start_time_,
      GetFrameFillingStartedCallback(credential_awaiting_task_focus_->first),
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
      GetPrimaryMainFrameOfActorSurface(actor_surface_handle_), task_id(),
      journal(), page_stability_config);
}

void AttemptLoginTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                              ToolCallback callback) const {
  task.AddActorSurface(actor_surface_handle_, /*stop_task_on_detach=*/true,
                       std::move(callback));
}

ActorSurfaceHandle AttemptLoginTool::GetTargetActorSurface() const {
  return actor_surface_handle_;
}

actor_login::ActorLoginService& AttemptLoginTool::GetActorLoginService() {
  return tool_delegate().GetActorLoginService();
}

actor_login::FrameFillingStartedCallback
AttemptLoginTool::GetFrameFillingStartedCallback(
    const actor_login::Credential& credential) {
  // TODO(b/567721071): Pass `actor_surface_handle_` once
  // ActorOneTimeTokenFillingService accepts surface handles.
  return base::BindOnce(
      &autofill::ActorOneTimeTokenFillingService::OnPasswordFillingStarted,
      tool_delegate().GetActorOneTimeTokenFillingService().GetWeakPtr(),
      actor_surface_handle_.GetTabHandle(), credential.request_origin,
      credential.has_persistent_permission);
}

}  // namespace actor
