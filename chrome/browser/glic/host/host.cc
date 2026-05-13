// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/host.h"

#include <algorithm>
#include <memory>
#include <ranges>

#include "base/containers/to_vector.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_provider.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_page_handler.h"
#include "chrome/browser/glic/host/glic_skills_manager_impl.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/host/host_metrics.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_instance_metrics_backwards_compatibility.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/chrome_features.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/base/proto_wrapper.h"

#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#endif

namespace glic {
BASE_FEATURE(kGlicReloadUsesFreshWebContents, base::FEATURE_ENABLED_BY_DEFAULT);

bool EmptyEmbedderDelegate::IsShowing() const {
  return true;
}

void EmptyEmbedderDelegate::Resize(const gfx::Size& size,
                                   base::TimeDelta duration,
                                   base::OnceClosure callback) {
  std::move(callback).Run();
}

void EmptyEmbedderDelegate::SwitchConversation(
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::SwitchConversationCallback callback) {
  std::move(callback).Run(std::nullopt);
}

void EmptyEmbedderDelegate::CaptureScreenshot(
    glic::mojom::WebClientHandler::CaptureScreenshotCallback callback) {
  std::move(callback).Run(nullptr);
}

Host::PageHandlerInfo::PageHandlerInfo() = default;
Host::PageHandlerInfo::~PageHandlerInfo() = default;
Host::PageHandlerInfo::PageHandlerInfo(PageHandlerInfo&&) = default;
Host::PageHandlerInfo& Host::PageHandlerInfo::operator=(PageHandlerInfo&&) =
    default;

Host::Host(Profile* profile,
           GlicSharingManagerProvider* sharing_manager_provider,
           GlicInstance* glic_instance,
           InstanceDelegate* instance_delegate)
    : profile_(profile),
      instance_delegate_(instance_delegate),
      glic_instance_(glic_instance),
      sharing_manager_provider_(sharing_manager_provider),
      metrics_(this) {
  VLOG(1) << "Glic [Host] Constructor";
}

Host::~Host() {
  VLOG(1) << "Glic [Host] Destructor";
  // Destroying the web contents results in calls back to the host, so do that
  // first.
  Shutdown();
}

void Host::SetDelegate(EmbedderDelegate* new_delegate) {
  CHECK(new_delegate);
  delegate_ = new_delegate;
}

void Host::Shutdown() {
  VLOG(1) << "Glic [Host] Shutdown";
  metrics_.Shutdown();

  handler_info_.reset();
  contents_.reset();
}

bool Host::IsWebContentPresentAndMatches(
    content::RenderFrameHost* render_frame_host) {
  auto* contents = webui_contents();
  if (contents && contents->GetPrimaryMainFrame() == render_frame_host) {
    return true;
  }
  auto* handler = page_handler();
  if (handler) {
    if (handler->GetGuestMainFrame() == render_frame_host) {
      return true;
    }
  }
  return false;
}

void Host::NotifyActorTaskListRowClicked(int32_t task_id) {
  if (auto* client = GetPrimaryWebClient()) {
    client->NotifyActorTaskListRowClicked(task_id);
  }
}

void Host::NotifySkillToInvokeChanged(mojom::SkillPtr skill) {
  if (auto* client = GetPrimaryWebClient()) {
    client->NotifySkillToInvokeChanged(std::move(skill));
  }
}

void Host::NotifyIsInvoking(bool is_invoking) {
  if (auto* client = GetPrimaryWebClient()) {
    client->NotifyIsInvoking(is_invoking);
  }
}

void Host::NotifyContextualSkillsChanged(
    std::vector<mojom::SkillPreviewPtr> contextual_skill_previews) {
  if (auto* client = GetPrimaryWebClient()) {
    client->NotifyContextualSkillPreviewsChanged(
        std::move(contextual_skill_previews));
  } else {
    pending_contextual_skills_ = std::move(contextual_skill_previews);
  }
}

void Host::GetExperimentalTriggeringUpdates(
    mojo::PendingRemote<mojom::ExperimentalTriggeringUpdatesHandler> handler,
    base::OnceCallback<void(bool)> success_status_callback) {
  if (auto* client = GetPrimaryWebClient()) {
    client->GetExperimentalTriggeringUpdates(
        std::move(handler), std::move(success_status_callback));
  } else {
    std::move(success_status_callback).Run(false);
  }
}

void Host::Invoke(mojom::InvokeOptionsPtr options, base::OnceClosure callback) {
  CHECK(!options->auto_submit) << "Use InvokeWithAutoSubmit instead.";
  InvokeInternal(std::move(options), std::move(callback));
}

void Host::InvokeWithAutoSubmit(InvokeWithAutoSubmitPasskey auto_submit_passkey,
                                mojom::InvokeOptionsPtr options,
                                base::OnceClosure callback) {
  InvokeInternal(std::move(options), std::move(callback));
}

void Host::InvokeInternal(mojom::InvokeOptionsPtr options,
                          base::OnceClosure callback) {
  if (auto* client = GetPrimaryWebClient()) {
    client->Invoke(std::move(options), std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void Host::Close() {
  delegate_->ClosePanel();
}

void Host::Reload() {
  auto* contents = webui_contents();
  if (!contents) {
    return;
  }

  if (base::FeatureList::IsEnabled(kGlicReloadUsesFreshWebContents)) {
    if (handler_info_ && handler_info_->web_client) {
      UnsetWebClient(handler_info_->web_client);
    }
    Shutdown();
    CreateContents();
    delegate_->OnReload();
  } else {
    contents->GetController().Reload(content::ReloadType::BYPASSING_CACHE,
                                     /*check_for_repost=*/false);
  }
}

void Host::OnWebContentsNavigated() {
  if (delegate_) {
    delegate_->OnReload();
  }
}

void Host::CreateContents() {
  if (contents_) {
    return;
  }

  VLOG(1) << "Glic [Host] CreateContents";

  glic_service().fre_controller().RecordFrameworkStartTime();
  contents_ = instance_delegate_->CreateWebUIContentsContainer();
  contents_->AttachToHost(this);

  metrics_.StartRecording();
}

Host::PanelWillOpenOptions::PanelWillOpenOptions() = default;
Host::PanelWillOpenOptions::~PanelWillOpenOptions() = default;
Host::PanelWillOpenOptions::PanelWillOpenOptions(PanelWillOpenOptions&&) =
    default;
Host::PanelWillOpenOptions& Host::PanelWillOpenOptions::operator=(
    PanelWillOpenOptions&&) = default;

void Host::PanelWillOpen(mojom::InvocationSource invocation_source,
                         PanelWillOpenOptions options) {
  VLOG(1) << "Glic [Host] PanelWillOpen";
  CHECK(delegate_);
  panel_open_ = true;
  invocation_source_ = invocation_source;
  if (handler_info_ && handler_info_->web_client) {
    handler_info_->web_client->PanelWillOpen(
        glic_instance_
            ? mojom::PanelOpeningData::New(
                  glic_instance_->GetPanelState().Clone(), invocation_source,
                  std::move(options.prompt_suggestion), options.auto_send,
                  /*skill_to_invoke=*/nullptr,
                  std::move(options.recently_active_conversations),
                  std::move(options.conversation_info), options.fre_override)
            : mojom::PanelOpeningData::New(),
        base::BindOnce(
            &Host::PanelWillOpenComplete,
            // Unretained is safe because web client is owned by `contents_`.
            base::Unretained(this), handler_info_->web_client.get()));
  } else {
    pending_panel_open_options_ = std::move(options);
  }
}

void Host::PanelWasClosed() {
  VLOG(1) << "Glic [Host] PanelWasClosed";
  panel_open_ = false;
  if (handler_info_ && handler_info_->web_client) {
    handler_info_->web_client->PanelWasClosed(base::DoNothing());
    handler_info_->open_complete = false;
  }
}

void Host::StopMicrophone(base::OnceClosure done) {
  if (auto* client = GetPrimaryWebClient()) {
    client->StopMicrophone(std::move(done));
  } else {
    std::move(done).Run();
  }
}

void Host::SwitchConversation(
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::SwitchConversationCallback callback) {
  delegate_->SwitchConversation(std::move(info), std::move(callback));
}

void Host::RegisterConversation(
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::RegisterConversationCallback callback) {
  instance_delegate().RegisterConversation(std::move(info),
                                           std::move(callback));
}

void Host::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void Host::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void Host::AddPanelStateObserver(PanelStateObserver* observer) {
  if (glic_instance_) {
    glic_instance_->AddStateObserver(observer);
  }
}

void Host::RemovePanelStateObserver(PanelStateObserver* observer) {
  if (glic_instance_) {
    glic_instance_->RemoveStateObserver(observer);
  }
}

void Host::WebUIPageHandlerAdded(GlicPageHandler* page_handler) {
  CHECK(!contents_ ||
        contents_->web_contents() == page_handler->webui_contents());
  if (handler_info_) {
    // The glic window supports right-click->Reload. When this happens, there
    // is momentarily two page handlers for the same web contents. Since this
    // can affect real users, it needs to be handled specially here.
    WebUiStateChanged(handler_info_->page_handler,
                      mojom::WebUiState::kUninitialized);
    if (handler_info_->web_client) {
      UnsetWebClient(handler_info_->web_client);
    }
  }
  handler_info_ = PageHandlerInfo();
  handler_info_->page_handler = page_handler;
}

void Host::WebUIPageHandlerRemoved(GlicPageHandler* page_handler) {
  if (!handler_info_ || handler_info_->page_handler != page_handler) {
    return;
  }
  handler_info_ = std::nullopt;
  WebUiStateChanged(page_handler, mojom::WebUiState::kUninitialized);
}

void Host::LoginPageCommitted(GlicPageHandler* page_handler) {
  observers_.Notify(&Observer::LoginPageCommitted);
}

GlicKeyedService& Host::glic_service() {
  return *GlicKeyedService::Get(profile_);
}

GlicSharingManager& Host::sharing_manager() {
  return sharing_manager_provider_->sharing_manager();
}

GlicSkillsManager& Host::skills_manager() {
  if (!skills_manager_) {
    skills_manager_ = std::make_unique<GlicSkillsManagerImpl>(this);
  }
  return *skills_manager_;
}

Host::InstanceDelegate& Host::instance_delegate() {
  CHECK(instance_delegate_);
  return *instance_delegate_;
}

GlicPageHandler* Host::page_handler() const {
  return handler_info_ ? handler_info_->page_handler : nullptr;
}

Host::PageHandlerInfo* Host::FindInfo(GlicPageHandler* handler) {
  if (handler_info_) {
    if (handler_info_->page_handler == handler) {
      return &*handler_info_;
    }
  }
  return nullptr;
}

Host::PageHandlerInfo* Host::FindInfoForClient(GlicWebClientAccess* client) {
  if (handler_info_) {
    if (handler_info_->web_client == client) {
      return &handler_info_.value();
    }
  }
  return nullptr;
}

Host::PageHandlerInfo* Host::FindInfoForWebUiContents(
    content::WebContents* web_contents) {
  if (handler_info_) {
    if (handler_info_->page_handler->webui_contents() == web_contents) {
      return &handler_info_.value();
    }
  }
  return nullptr;
}

GlicPageHandler* Host::FindPageHandlerForWebUiContents(
    const content::WebContents* webui_contents) {
  if (handler_info_) {
    if (handler_info_->page_handler->webui_contents() == webui_contents) {
      return handler_info_->page_handler;
    }
  }
  return nullptr;
}

void Host::NotifyWindowIntentToShow() {
  if (handler_info_) {
    handler_info_->page_handler->NotifyWindowIntentToShow();
  }
}

void Host::Zoom(mojom::ZoomAction zoom_action) {
  if (GlicPageHandler* handler = page_handler()) {
    handler->Zoom(zoom_action);
  }
}

void Host::UnsetWebClient(GlicWebClientAccess* web_client) {
  if (!handler_info_ || handler_info_->web_client != web_client) {
    return;
  }

  // Revert any observed state from the web client.
  if (handler_info_->context_access_indicator_enabled) {
    observers_.Notify(&Observer::ContextAccessIndicatorChanged, false);
  }
  handler_info_->web_client = nullptr;
  instance_delegate().OnWebClientCleared();
}

void Host::SetWebClient(GlicWebClientAccess* web_client) {
  CHECK(handler_info_);
  CHECK(web_client);
  handler_info_->web_client = web_client;

  // TODO(b/507074189): Refactor Skills to use the invoke API.
  if (!pending_contextual_skills_.empty()) {
    web_client->NotifyContextualSkillPreviewsChanged(
        std::move(pending_contextual_skills_));
    pending_contextual_skills_.clear();
  }

  for (auto& [source, context] : pending_additional_contexts_) {
    web_client->NotifyAdditionalContext(std::move(context));
  }
  pending_additional_contexts_.clear();

  if (is_manually_resizing_) {
    web_client->ManualResizeChanged(true);
  }
  if (invocation_source_ && web_client) {
    std::optional<std::string> prompt_suggestion;
    std::optional<std::vector<mojom::ConversationInfoPtr>>
        recently_active_conversations;
    auto conversation_info = mojom::ConversationInfo::New();

    bool auto_send = false;
    mojom::FreOverride fre_override = mojom::FreOverride::kUnspecified;
    if (pending_panel_open_options_) {
      prompt_suggestion =
          std::move(pending_panel_open_options_->prompt_suggestion);
      recently_active_conversations =
          std::move(pending_panel_open_options_->recently_active_conversations);
      conversation_info =
          std::move(pending_panel_open_options_->conversation_info);
      auto_send = pending_panel_open_options_->auto_send;
      fre_override = pending_panel_open_options_->fre_override;
      pending_panel_open_options_.reset();
    }

    // Note: we're sending the open call even if the panel as since been closed.
    // This ensure we don't drop the invocation information. Finally, a call to
    // PanelWasClosed() resolves the discrepancy.
    web_client->PanelWillOpen(
        mojom::PanelOpeningData::New(
            glic_instance_ ? glic_instance_->GetPanelState().Clone()
                           : mojom::PanelState::New(),
            *invocation_source_, std::move(prompt_suggestion), auto_send,
            /*skill_to_invoke=*/nullptr,
            std::move(recently_active_conversations),
            std::move(conversation_info), fre_override),
        base::BindOnce(
            &Host::PanelWillOpenComplete,
            // Unretained is safe because web client is owned by `contents_`.
            base::Unretained(this),
            // Unretained is safe because web_client is calling us.
            base::Unretained(web_client)));
    if (!panel_open_) {
      web_client->PanelWasClosed(base::DoNothing());
    }
  }
  skills_manager().UpdateSkillPreviews(std::nullopt);

  observers_.Notify(&Observer::WebClientConnected);
}

void Host::WebClientInitializeFailed(GlicWebClientAccess* web_client) {
  if (handler_info_ && handler_info_->web_client == web_client) {
    observers_.Notify(&Observer::WebClientInitializeFailed);
  }
}

void Host::SetContextAccessIndicator(GlicPageHandler* page_handler,
                                     bool enabled) {
  CHECK(handler_info_);
  if (handler_info_->context_access_indicator_enabled == enabled) {
    return;
  }
  handler_info_->context_access_indicator_enabled = enabled;
  observers_.Notify(&Observer::ContextAccessIndicatorChanged, enabled);
}

bool Host::IsContextAccessIndicatorEnabled() const {
  return handler_info_ ? handler_info_->context_access_indicator_enabled
                       : false;
}

GlicWebClientAccess* Host::GetPrimaryWebClient() {
  return handler_info_ ? handler_info_->web_client : nullptr;
}

void Host::ManualResizeChanged(bool resizing) {
  is_manually_resizing_ = resizing;
  if (auto* client = GetPrimaryWebClient()) {
    client->ManualResizeChanged(resizing);
  }
}

bool Host::IsPrimaryClientOpen() {
  return handler_info_ ? handler_info_->open_complete : false;
}

InstanceId Host::GetInstanceId() const {
  return glic_instance_ ? glic_instance_->id() : InstanceId::CreateNullId();
}

content::WebContents* Host::webui_contents() const {
  if (contents_) {
    return contents_->web_contents();
  }
  if (page_handler()) {
    return page_handler()->webui_contents();
  }
  return nullptr;
}

content::WebContents* Host::web_client_contents() const {
  return content::WebContents::FromRenderFrameHost(GetGuestMainFrame());
}

bool Host::IsGlicWebUiHost(content::RenderProcessHost* host) const {
  if (handler_info_) {
    if (handler_info_->page_handler->webui_contents()
            ->GetPrimaryMainFrame()
            ->GetProcess() == host) {
      return true;
    }
  }
  return false;
}

content::RenderFrameHost* Host::GetGuestMainFrame() const {
  if (page_handler()) {
    return page_handler()->GetGuestMainFrame();
  }
  return nullptr;
}

bool Host::IsGlicWebUi(content::WebContents* contents) const {
  return FindInfoForWebUiContents(contents) != nullptr;
}

std::vector<GlicPageHandler*> Host::GetPageHandlersForTesting() {
  if (!handler_info_) {
    return {};
  }
  return {handler_info_->page_handler};
}

GlicPageHandler* Host::GetPrimaryPageHandlerForTesting() {
  return handler_info_ ? handler_info_->page_handler : nullptr;
}

void Host::PanelWillOpenComplete(GlicWebClientAccess* client,
                                 mojom::OpenPanelInfoPtr open_info) {
  CHECK(client);
  // If the panel was closed before opening finished, return early.
  if (!panel_open_) {
    return;
  }
  if (handler_info_ && handler_info_->web_client == client) {
    handler_info_->open_complete = true;
    observers_.Notify(&Observer::ClientReadyToShow, *open_info);
  }
}

bool Host::IsWebClientConnected() const {
  if (handler_info_) {
    return handler_info_->web_client != nullptr;
  }
  return false;
}

void Host::WebUiStateChanged(GlicPageHandler* page_handler,
                             mojom::WebUiState new_state) {
  base::UmaHistogramEnumeration("Glic.PanelWebUiState", new_state);
  // UI State has changed
  primary_webui_state_ = new_state;
  observers_.Notify(&Observer::WebUiStateChanged, primary_webui_state_);
}

void Host::NotifyZeroStateSuggestion(
    mojom::ZeroStateSuggestionsV2Ptr suggestions,
    mojom::ZeroStateSuggestionsOptions options) {
  if (handler_info_) {
    handler_info_->page_handler->ZeroStateSuggestionChanged(
        std::move(suggestions), std::move(options));
  }
}

void Host::NotifyInstanceActivationChanged(bool is_active) {
  if (handler_info_ && handler_info_->web_client) {
    handler_info_->web_client->NotifyInstanceActivationChanged(is_active);
  }
}

void Host::NotifyAdditionalContext(mojom::AdditionalContextPtr context) {
  if (auto* client = GetPrimaryWebClient()) {
    client->NotifyAdditionalContext(std::move(context));
  } else {
    pending_additional_contexts_[context->source] = std::move(context);
  }
}

content::RenderProcessHost* Host::GetWebClientRenderProcessHost() const {
  auto* guest_frame = GetGuestMainFrame();
  if (guest_frame) {
    return guest_frame->GetProcess();
  }
  return nullptr;
}

void Host::OnInteractionModeChange(GlicPageHandler* page_handler,
                                   mojom::WebClientMode new_mode) {
  instance_delegate_->OnInteractionModeChange(new_mode);
}

void Host::OnMicrophoneStatusChanged(mojom::MicrophoneStatus status) {
  microphone_status_ = status;
  delegate_->OnMicrophoneStatusChanged(status);
}

void Host::ResizePanel(GlicPageHandler* page_handler,
                       const gfx::Size& size,
                       base::TimeDelta duration,
                       base::OnceClosure callback) {
  delegate_->Resize(size, duration, std::move(callback));
}

void Host::EnableDragResize(GlicPageHandler* page_handler, bool enabled) {
  if (handler_info_ && handler_info_->page_handler == page_handler) {
    delegate_->EnableDragResize(enabled);
  }
}

void Host::AttachPanel(GlicPageHandler* page_handler) {
  if (handler_info_ && handler_info_->page_handler == page_handler) {
    delegate_->Attach();
  }
}

void Host::DetachPanel(GlicPageHandler* page_handler) {
  if (handler_info_ && handler_info_->page_handler == page_handler) {
    delegate_->Detach();
  }
}

void Host::ClosePanel(GlicPageHandler* page_handler) {
  delegate_->ClosePanel();
}

void Host::SetMinimumWidgetSize(GlicPageHandler* page_handler,
                                const gfx::Size& size) {
  if (handler_info_ && handler_info_->page_handler == page_handler) {
    delegate_->SetMinimumWidgetSize(size);
  }
}

void Host::CaptureScreenshot(
    glic::mojom::WebClientHandler::CaptureScreenshotCallback callback) {
  delegate_->CaptureScreenshot(std::move(callback));
}

bool Host::IsWidgetShowing(GlicWebClientAccess* client) const {
  return delegate_->IsShowing();
}

mojom::PanelState Host::GetPanelState(GlicWebClientAccess* client) const {
  return glic_instance_ ? glic_instance_->GetPanelState() : mojom::PanelState();
}

void Host::RequestToShowCredentialSelectionDialog(
    actor::TaskId task_id,
    const base::flat_map<std::string, gfx::Image>& icons,
    const std::vector<actor_login::Credential>& credentials,
    actor::ActorTaskDelegate::CredentialSelectedCallback callback) {
  if (!IsWebClientConnected()) {
    std::move(callback).Run(
        actor::webui::mojom::SelectCredentialDialogResponse::New());
    return;
  }
  handler_info_->web_client->RequestToShowCredentialSelectionDialog(
      task_id, icons, credentials, std::move(callback));
}

void Host::RequestToShowUserConfirmationDialog(
    actor::TaskId task_id,
    const url::Origin& navigation_origin,
    bool for_blocklisted_origin,
    actor::ActorTaskDelegate::UserConfirmationDialogCallback callback) {
  if (!IsWebClientConnected()) {
    std::move(callback).Run(
        actor::webui::mojom::UserConfirmationDialogResponse::New(
            actor::webui::mojom::ConfirmationRequestResult::
                NewPermissionGranted(/*value=*/false)));
    return;
  }
  handler_info_->web_client->RequestToShowUserConfirmationDialog(
      task_id, navigation_origin, for_blocklisted_origin, std::move(callback));
}

void Host::RequestToConfirmNavigation(
    actor::TaskId task_id,
    const url::Origin& navigation_origin,
    actor::ActorTaskDelegate::NavigationConfirmationCallback callback) {
  if (!IsWebClientConnected()) {
    std::move(callback).Run(
        actor::webui::mojom::NavigationConfirmationResponse::New(
            actor::webui::mojom::ConfirmationRequestResult::
                NewPermissionGranted(/*value=*/false)));
    return;
  }
  handler_info_->web_client->RequestToConfirmNavigation(
      task_id, navigation_origin, std::move(callback));
}

void Host::RequestToShowAutofillSuggestionsDialog(
    actor::TaskId task_id,
    std::vector<autofill::ActorFormFillingRequest> requests,
    base::WeakPtr<actor::AutofillSelectionDialogEventHandler> event_handler,
    actor::ActorTaskDelegate::AutofillSuggestionSelectedCallback callback) {
  if (!IsWebClientConnected()) {
    std::move(callback).Run(
        actor::webui::mojom::SelectAutofillSuggestionsDialogResponse::New(
            task_id.value(),
            actor::webui::mojom::SelectAutofillSuggestionsDialogResult::
                NewErrorReason(actor::webui::mojom::
                                   SelectAutofillSuggestionsDialogErrorReason::
                                       kDialogPromiseNoSubscriber)));
    return;
  }
  handler_info_->web_client->RequestToShowAutofillSuggestionsDialog(
      task_id, std::move(requests), std::move(event_handler),
      std::move(callback));
}

void Host::FloatingPanelCanAttachChanged(bool can_attach) {
  if (!IsWebClientConnected()) {
    return;
  }
  handler_info_->web_client->FloatingPanelCanAttachChanged(can_attach);
}

}  // namespace glic
