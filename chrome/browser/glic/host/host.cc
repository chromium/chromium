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
#include "chrome/browser/glic/public/glic_instance_metrics_backwards_compatibility.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/chrome_features.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
#include "components/guest_view/browser/guest_view_base.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#endif

namespace glic {
BASE_FEATURE(kGlicReloadUsesFreshWebContents, base::FEATURE_ENABLED_BY_DEFAULT);

class EmptyInstanceDelegate : public Host::InstanceDelegate {
 public:
  EmptyInstanceDelegate() = default;
  ~EmptyInstanceDelegate() override = default;

  tabs::TabInterface* CreateTab(
      const ::GURL& url,
      bool open_in_background,
      const std::optional<int32_t>& window_id,
      glic::mojom::WebClientHandler::CreateTabCallback callback) override {
    std::move(callback).Run(nullptr);
    return nullptr;
  }
  void CreateTask(
      base::WeakPtr<actor::ActorTaskDelegate> delegate,
      actor::webui::mojom::TaskOptionsPtr options,
      mojom::WebClientHandler::CreateTaskCallback callback) override {
    std::move(callback).Run(
        base::unexpected(mojom::CreateTaskErrorReason::kUnknown));
  }
  void PerformActions(
      const std::vector<uint8_t>& actions_proto,
      mojom::WebClientHandler::PerformActionsCallback callback) override {
    std::move(callback).Run(
        base::unexpected(mojom::PerformActionsErrorReason::kUnknown));
  }
  void CancelActions(
      actor::TaskId task_id,
      mojom::WebClientHandler::CancelActionsCallback callback) override {
    std::move(callback).Run(mojom::CancelActionsResult::kFailed);
  }
  void StopActorTask(actor::TaskId task_id,
                     mojom::ActorTaskStopReason stop_reason) override {}
  void PauseActorTask(actor::TaskId task_id,
                      mojom::ActorTaskPauseReason pause_reason,
                      tabs::TabInterface::Handle tab_handle) override {}
  void ResumeActorTask(actor::TaskId task_id,
                       const mojom::GetTabContextOptions& context_options,
                       glic::mojom::WebClientHandler::ResumeActorTaskCallback
                           callback) override {
    std::move(callback).Run(mojom::GetContextResultWithActionResultCode::New());
  }
  void InterruptActorTask(actor::TaskId task_id) override {}
  void UninterruptActorTask(actor::TaskId task_id) override {}
  void CreateActorTab(
      actor::TaskId task_id,
      bool open_in_background,
      const std::optional<int32_t>& initiator_tab_id,
      const std::optional<int32_t>& initiator_window_id,
      glic::mojom::WebClientHandler::CreateActorTabCallback callback) override {
    std::move(callback).Run(nullptr);
  }
  void FetchZeroStateSuggestions(
      bool is_first_run,
      std::optional<std::vector<std::string>> supported_tools,
      glic::mojom::WebClientHandler::
          GetZeroStateSuggestionsForFocusedTabCallback callback) override {
    std::move(callback).Run(nullptr);
  }
  void GetZeroStateSuggestionsAndSubscribe(
      bool has_active_subscription,
      const mojom::ZeroStateSuggestionsOptions& options,
      mojom::WebClientHandler::GetZeroStateSuggestionsAndSubscribeCallback
          callback) override {
    std::move(callback).Run(nullptr);
  }
  void RegisterConversation(
      glic::mojom::ConversationInfoPtr info,
      mojom::WebClientHandler::RegisterConversationCallback callback) override {
    std::move(callback).Run(mojom::RegisterConversationErrorReason::kUnknown);
  }
  void OnWebClientCleared() override {}
  void PrepareForOpen() override {}
  void OnInteractionModeChange(mojom::WebClientMode new_mode) override {}
  GlicInstanceMetrics* instance_metrics() override { return nullptr; }
  GlicInstanceMetricsBackwardsCompatibility&
  instance_metrics_backwards_compatibility() override {
    return metrics_backwards_compatibility_stub_;
  }
  bool IsActive() override { return true; }

 private:
  class MetricsBackwardsCompatibilityStub
      : public GlicInstanceMetricsBackwardsCompatibility {
   public:
    void OnUserInputSubmitted(mojom::WebClientMode mode) override {}
    void DidRequestContextFromTab(tabs::TabInterface& tab) override {}
    void OnResponseStarted() override {}
    void OnResponseStopped(mojom::ResponseStopCause cause) override {}
    void OnTurnCompleted(mojom::WebClientModel model,
                         base::TimeDelta duration) override {}
    void OnReaction(mojom::MetricUserInputReactionType reaction_type) override {
    }
    void OnGlicScrollAttempt() override {}
    void OnGlicScrollComplete(bool success) override {}
  };
  MetricsBackwardsCompatibilityStub metrics_backwards_compatibility_stub_;
};

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
      metrics_(this) {}

Host::~Host() {
  // Destroying the web contents results in calls back to the host, so do that
  // first.
  Shutdown();
}

void Host::SetDelegate(EmbedderDelegate* new_delegate) {
  CHECK(new_delegate);
  delegate_ = new_delegate;
}

void Host::Shutdown() {
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

void Host::NotifyContextualSkillsChanged(
    std::vector<mojom::SkillPreviewPtr> contextual_skill_previews) {
  if (auto* client = GetPrimaryWebClient()) {
    client->NotifyContextualSkillPreviewsChanged(
        std::move(contextual_skill_previews));
  }
}

void Host::Invoke(mojom::InvokeOptionsPtr options, base::OnceClosure callback) {
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

  if (GlicEnabling::IsMultiInstanceEnabled() &&
      base::FeatureList::IsEnabled(kGlicReloadUsesFreshWebContents)) {
    Shutdown();
    CreateContents(/*initially_hidden=*/false);
    delegate_->OnReload();
  } else {
    contents->GetController().Reload(content::ReloadType::BYPASSING_CACHE,
                                     /*check_for_repost=*/false);
  }
}

void Host::CreateContents(bool initially_hidden) {
  if (contents_) {
    return;
  }

  glic_service().fre_controller().RecordFrameworkStartTime();
  if (base::FeatureList::IsEnabled(features::kGlicWebContentsWarming)) {
    contents_ = glic_service().web_contents_warming_pool().TakeContainer();
  } else {
    contents_ =
        std::make_unique<WebUIContentsContainer>(profile_, initially_hidden);
  }
  contents_->AttachToHost(this);
  glic::GlicProfileManager::GetInstance()->OnLoadingClientForService(
      &glic_service());

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
  CHECK(delegate_);
  invocation_source_ = invocation_source;
  if (handler_info_ && handler_info_->web_client) {
    handler_info_->web_client->PanelWillOpen(
        glic_instance_
            ? mojom::PanelOpeningData::New(
                  glic_instance_->GetPanelState().Clone(), invocation_source,
                  std::move(options.prompt_suggestion), options.auto_send,
                  /*skill_to_invoke=*/nullptr,
                  std::move(options.recently_active_conversations),
                  std::move(options.conversation_info))
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
  invocation_source_ = std::nullopt;
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
  return sharing_manager_provider_
             ? sharing_manager_provider_->sharing_manager()
             : glic_service().sharing_manager();
}

GlicSkillsManager& Host::skills_manager() {
  if (!skills_manager_) {
    skills_manager_ = std::make_unique<GlicSkillsManagerImpl>(this);
  }
  return *skills_manager_;
}

Host::InstanceDelegate& Host::instance_delegate() {
#if !BUILDFLAG(IS_ANDROID)
  return instance_delegate_ ? *instance_delegate_ : glic_service();
#else
  // Multi-instance always uses instance_delegate_.
  return *instance_delegate_;
#endif
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
  if (invocation_source_ && web_client) {
    std::optional<std::string> prompt_suggestion;
    std::optional<std::vector<mojom::ConversationInfoPtr>>
        recently_active_conversations;
    auto conversation_info = mojom::ConversationInfo::New();

    bool auto_send = false;
    if (pending_panel_open_options_) {
      prompt_suggestion =
          std::move(pending_panel_open_options_->prompt_suggestion);
      recently_active_conversations =
          std::move(pending_panel_open_options_->recently_active_conversations);
      conversation_info =
          std::move(pending_panel_open_options_->conversation_info);
      auto_send = pending_panel_open_options_->auto_send;
      pending_panel_open_options_.reset();
    }

    web_client->PanelWillOpen(
        mojom::PanelOpeningData::New(
            glic_instance_ ? glic_instance_->GetPanelState().Clone()
                           : mojom::PanelState::New(),
            *invocation_source_, std::move(prompt_suggestion), auto_send,
            /*skill_to_invoke=*/nullptr,
            std::move(recently_active_conversations),
            std::move(conversation_info)),
        base::BindOnce(
            &Host::PanelWillOpenComplete,
            // Unretained is safe because web client is owned by `contents_`.
            base::Unretained(this),
            // Unretained is safe because web_client is calling us.
            base::Unretained(web_client)));
  }
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
  skills_manager().UpdateSkillPreviews(std::nullopt);
#endif
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

bool Host::IsPrimaryClientOpen() {
  return handler_info_ ? handler_info_->open_complete : false;
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
  return web_client_contents_.get();
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
  if (!invocation_source_) {
    return;
  }
  if (handler_info_ && handler_info_->web_client == client) {
    handler_info_->open_complete = true;
    observers_.Notify(&Observer::ClientReadyToShow, *open_info);
  }
}

bool Host::IsReady() const {
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
  }
}

content::RenderProcessHost* Host::GetWebClientRenderProcessHost() const {
  if (content::WebContents* contents = web_client_contents()) {
    if (content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame()) {
      return rfh->GetProcess();
    }
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

void Host::SetPanelDraggableAreas(
    GlicPageHandler* page_handler,
    const std::vector<gfx::Rect>& draggable_areas) {
  // TODO(b:469211337): Remove it from glic api.
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
  if (!IsReady()) {
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
  if (!IsReady()) {
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
  if (!IsReady()) {
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
  if (!IsReady()) {
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
  if (!IsReady()) {
    return;
  }
  handler_info_->web_client->FloatingPanelCanAttachChanged(can_attach);
}

void Host::GuestAdded(content::WebContents* guest_contents) {
  web_client_contents_ = guest_contents->GetWeakPtr();
}

HostManager::HostManager(Profile* profile,
                         base::WeakPtr<GlicWindowController> window_controller)
    : profile_(profile),
      window_controller_(window_controller),
      empty_embedder_delegate_(std::make_unique<EmptyEmbedderDelegate>()),
      instance_delegate_stub_(std::make_unique<EmptyInstanceDelegate>()) {}

HostManager::~HostManager() = default;

void HostManager::Shutdown() {
  for (Host* host : GetAllHosts()) {
    host->Shutdown();
  }
}

void HostManager::GuestAdded(content::WebContents* guest_contents) {
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
  content::WebContents* top =
      guest_view::GuestViewBase::GetTopLevelWebContents(guest_contents);
#endif

  for (Host* host : GetPrimaryHosts()) {
    if (!host->webui_contents()) {
      continue;
    }

    host->GuestAdded(guest_contents);

#if !BUILDFLAG(IS_ANDROID)
    // TODO(harringtond): This looks wrong, either fix or document this.
    blink::web_pref::WebPreferences prefs(top->GetOrCreateWebPreferences());
    prefs.default_font_size =
        host->webui_contents()->GetOrCreateWebPreferences().default_font_size;
    top->SetWebPreferences(prefs);
#else
    // TODO(b/470059315): What do we do for Android?
#endif
    return;
  }
}

std::vector<Host*> HostManager::GetAllHosts() {
  std::vector<Host*> hosts = GetPrimaryHosts();
  for (std::unique_ptr<Host>& host : tab_hosts_) {
    hosts.push_back(host.get());
  }
  return hosts;
}

Host* HostManager::GetOrCreateHostForTab(content::WebContents* web_contents) {
  for (const auto& host : tab_hosts_) {
    if (host->webui_contents() == web_contents) {
      return host.get();
    }
  }

  if (!tabs::TabInterface::MaybeGetFromContents(web_contents)) {
    return nullptr;
  }

  // For backwards compatibility, tab hosts are tied to the window controller.
  // In multi-instance mode, no instance is used for now. We should consider
  // just creating new instances for these hosts.
  GlicInstance* glic_instance = nullptr;
#if !BUILDFLAG(IS_ANDROID)
  if (!GlicEnabling::IsMultiInstanceEnabled()) {
    glic_instance =
        static_cast<GlicWindowControllerInterface*>(window_controller_.get());
  }
#endif
  tab_hosts_.push_back(std::make_unique<Host>(profile_, nullptr, glic_instance,
                                              instance_delegate_stub_.get()));
  Host* new_host = tab_hosts_.back().get();
  new_host->SetDelegate(empty_embedder_delegate_.get());
  new_host->PanelWillOpen(mojom::InvocationSource::kOsButton, {});
  return new_host;
}

bool HostManager::IsGlicWebUi(content::WebContents* contents) {
  for (const Host* host : GetAllHosts()) {
    if (host->IsGlicWebUi(contents)) {
      return true;
    }
  }
  return false;
}

bool HostManager::IsGlicWebUiHost(content::RenderProcessHost* process_host) {
  for (const Host* host : GetAllHosts()) {
    if (host->IsGlicWebUiHost(process_host)) {
      return true;
    }
  }
  return false;
}

void HostManager::WebUIPageHandlerAdded(GlicPageHandler* page_handler,
                                        Host* host) {
  CHECK(host);
  host->WebUIPageHandlerAdded(page_handler);
}

void HostManager::WebUIPageHandlerRemoved(GlicPageHandler* page_handler) {
  std::vector<Host*> instance_hosts = GetPrimaryHosts();
  for (Host* host : GetAllHosts()) {
    if (host->page_handler() == page_handler) {
      host->WebUIPageHandlerRemoved(page_handler);
      if (std::ranges::contains(instance_hosts, host)) {
        std::erase_if(tab_hosts_, [host](std::unique_ptr<Host>& h) {
          return h.get() == host;
        });
      }
      break;
    }
  }
}

Host* HostManager::FindHostForTabForTesting(tabs::TabInterface& tab) {
  for (auto& host : tab_hosts_) {
    if (host->webui_contents() == tab.GetContents()) {
      return host.get();
    }
  }

  return nullptr;
}

std::vector<Host*> HostManager::GetPrimaryHosts() {
  if (!window_controller_) {
    return {};
  }
  std::vector<Host*> hosts;
  for (GlicInstance* instance : window_controller_->GetInstances()) {
    hosts.push_back(&instance->host());
  }
  return hosts;
}

}  // namespace glic
