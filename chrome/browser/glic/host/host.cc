// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/host.h"

#include <algorithm>
#include <memory>
#include <ranges>

#include "base/containers/to_vector.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/context/glic_pin_candidate_provider.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_provider.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_page_handler.h"
#include "chrome/browser/glic/host/glic_skills_manager.h"
#include "chrome/browser/glic/host/glic_ui.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/glic_web_client_handler.h"
#include "chrome/browser/glic/host/glic_web_client_manager.h"
#include "chrome/browser/glic/host/glic_web_contents_manager.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance_metrics_backwards_compatibility.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/chrome_features.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/base/proto_wrapper.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#else
#include "components/guest_view/browser/slim_web_view/slim_web_view_guest.h"  // nogncheck
#endif

namespace glic {
BASE_FEATURE(kGlicReloadUsesFreshWebContents, base::FEATURE_ENABLED_BY_DEFAULT);

void Host::EmbedderDelegate::Resize(const gfx::Size& size,
                                    base::TimeDelta duration,
                                    base::OnceClosure callback) {
  std::move(callback).Run();
}

void Host::EmbedderDelegate::EnableDragResize(bool enabled) {}

void Host::EmbedderDelegate::SetMinimumWidgetSize(const gfx::Size& size) {}

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


Host::Host(Profile* profile,
           GlicSharingManagerProvider* sharing_manager_provider,
           GlicInstance* glic_instance,
           InstanceDelegate* instance_delegate)
    : profile_(profile),
      instance_delegate_(instance_delegate),
      glic_instance_(glic_instance),
      sharing_manager_provider_(sharing_manager_provider) {
  VLOG(1) << "Glic [Host] Constructor";
}

Host::~Host() {
  VLOG(1) << "Glic [Host] Destructor";
  HibernateImpl(/*is_destroying=*/true);
}

void Host::SetDelegate(EmbedderDelegate* new_delegate) {
  CHECK(new_delegate);
  delegate_ = new_delegate;
}

void Host::HibernateImpl(bool is_destroying) {
  TRACE_EVENT("glic", "Host::Hibernate");
  VLOG(1) << "Glic [Host] Hibernate";

  if (is_destroying) {
    weak_ptr_factory_.InvalidateWeakPtrsAndDoom();
  }
  client_state_ = {};
  page_handler_ = nullptr;
  contents_changed_subscription_ = {};
  contents_.reset();
}

void Host::Hibernate() {
  HibernateImpl(/*is_destroying=*/false);
}

bool Host::IsAwake() const {
  return contents_ != nullptr;
}

bool Host::IsWebContentPresentAndMatches(
    content::RenderFrameHost* render_frame_host) {
  auto* contents = webui_contents();
  if (contents && contents->GetPrimaryMainFrame() == render_frame_host) {
    return true;
  }
  if (GetGuestMainFrame() == render_frame_host) {
    return true;
  }
  return false;
}

void Host::NotifyActorTaskListRowClicked(int32_t task_id) {
  if (auto* client = GetPrimaryWebClient()) {
    client->NotifyActorTaskListRowClicked(task_id);
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
    UnsetWebClient();
    Hibernate();
    Awaken();
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

void Host::Awaken() {
  if (contents_) {
    return;
  }
  TRACE_EVENT("glic", "Host::CreateContents");
  VLOG(1) << "Glic [Host] CreateContents";

  contents_ = instance_delegate_->CreateWebContentsManager();
  contents_changed_subscription_ =
      contents_->RegisterWebContentsChangedCallback(base::BindRepeating(
          &Host::OnActiveWebContentsChanged, base::Unretained(this)));
  contents_->AttachToHost(this);
}

void Host::OnActiveWebContentsChanged(content::WebContents* new_contents) {
  for (auto& observer : observers_) {
    observer.ActiveWebContentsChanged(new_contents);
  }
}

Host::PanelWillOpenOptions::PanelWillOpenOptions() = default;
Host::PanelWillOpenOptions::~PanelWillOpenOptions() = default;
Host::PanelWillOpenOptions::PanelWillOpenOptions(PanelWillOpenOptions&&) =
    default;
Host::PanelWillOpenOptions& Host::PanelWillOpenOptions::operator=(
    PanelWillOpenOptions&&) = default;

void Host::SetDebouncedVisibility(bool is_visible) {
  debounced_visibility_ = is_visible;
  UpdateVisibility();
}

void Host::PanelWillOpen(mojom::InvocationSource invocation_source,
                         PanelWillOpenOptions options) {
  VLOG(1) << "Glic [Host] PanelWillOpen";
  CHECK(delegate_);
  panel_open_ = true;
  invocation_source_ = invocation_source;
  if (auto* client = GetPrimaryWebClient()) {
    client->PanelWillOpen(
        glic_instance_
            ? mojom::PanelOpeningData::New(
                  glic_instance_->GetPanelState().Clone(), invocation_source,
                  std::move(options.prompt_suggestion), options.auto_send,
                  /*skill_to_invoke=*/nullptr,
                  std::move(options.recently_active_conversations),
                  std::move(options.conversation_info), options.fre_override)
            : mojom::PanelOpeningData::New(),
        base::BindOnce(&Host::PanelWillOpenComplete, GetWeakPtr(), client));
  } else {
    pending_panel_open_options_ = std::move(options);
  }
}

void Host::PanelWasClosed() {
  VLOG(1) << "Glic [Host] PanelWasClosed";
  panel_open_ = false;
  client_state_.open_complete = false;
  if (auto* client = GetPrimaryWebClient()) {
    client->PanelWasClosed(base::DoNothing());
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

void Host::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void Host::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void Host::WebUIPageHandlerAdded(GlicPageHandler* page_handler) {
  CHECK(!contents_ ||
        contents_->active_web_contents() == page_handler->webui_contents());
  if (page_handler_) {
    // The glic window supports right-click->Reload. When this happens, there
    // is momentarily two page handlers for the same web contents. Since this
    // can affect real users, it needs to be handled specially here.
    WebUiStateChanged(page_handler_, mojom::WebUiState::kUninitialized);
    // TODO(harringtond): Web client liveness needs detangled from the page
    // handler. This is currently needed because, on reload, the web client
    // isn't cleared soon enough otherwise.
    if (GetPrimaryWebClient()) {
      UnsetWebClient();
    }
  }
  page_handler_ = page_handler;
}

void Host::WebUIPageHandlerRemoved(GlicPageHandler* page_handler) {
  if (page_handler_ != page_handler) {
    return;
  }
  page_handler_ = nullptr;
  WebUiStateChanged(page_handler, mojom::WebUiState::kUninitialized);
  // TODO(harringtond): Web client liveness needs detangled from the page
  // handler. This is currently needed because, on reload, the web client
  // isn't cleared soon enough otherwise.
  if (GetPrimaryWebClient()) {
    UnsetWebClient();
  }
}

void Host::LoginPageCommitted(GlicPageHandler* page_handler) {
  observers_.Notify(&Observer::LoginPageCommitted);
}

GlicKeyedService& Host::glic_service() {
  return *GlicKeyedService::Get(profile_);
}

GlicSharingManagerInternal& Host::GetSharingManagerInternal() {
  return sharing_manager_provider_->GetSharingManagerInternal();
}

GlicPinCandidateProvider& Host::pin_candidate_provider() {
  return sharing_manager_provider_->pin_candidate_provider();
}

Host::InstanceDelegate& Host::instance_delegate() {
  CHECK(instance_delegate_);
  return *instance_delegate_;
}

GlicPageHandler* Host::page_handler() const {
  return page_handler_;
}

GlicPageHandler* Host::FindPageHandlerForWebUiContents(
    const content::WebContents* webui_contents) {
  if (page_handler_ && page_handler_->webui_contents() == webui_contents) {
    return page_handler_;
  }
  return nullptr;
}

void Host::NotifyWindowIntentToShow() {
  if (page_handler_) {
    page_handler_->NotifyWindowIntentToShow();
  }
}

void Host::Zoom(mojom::ZoomAction zoom_action, ZoomSource source) {
  if (GlicPageHandler* handler = page_handler()) {
    handler->Zoom(zoom_action, source);
  }
}

GlicWebClientAccess* Host::GetPrimaryWebClient() const {
  auto* manager = web_client_manager();
  return manager ? manager->web_client_access() : nullptr;
}

void Host::UnsetWebClient() {
  if (auto* manager = web_client_manager()) {
    manager->UnsetWebClient();
  }
}

void Host::CreateWebClient(
    mojo::PendingReceiver<glic::mojom::WebClientHandler> web_client_receiver) {
  if (auto* manager = web_client_manager()) {
    manager->CreateWebClient(std::move(web_client_receiver));
  }
}

void Host::WebClientInitialized() {
  if (auto* manager = web_client_manager()) {
    manager->WebClientInitialized();
  }
  auto* client = GetPrimaryWebClient();
  CHECK(client);

  for (auto& [source, context] : pending_additional_contexts_) {
    client->NotifyAdditionalContext(std::move(context));
  }
  pending_additional_contexts_.clear();

  if (is_manually_resizing_) {
    client->ManualResizeChanged(true);
  }
  if (invocation_source_) {
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

    client->PanelWillOpen(
        mojom::PanelOpeningData::New(
            glic_instance_ ? glic_instance_->GetPanelState().Clone()
                           : mojom::PanelState::New(),
            *invocation_source_, std::move(prompt_suggestion), auto_send,
            /*skill_to_invoke=*/nullptr,
            std::move(recently_active_conversations),
            std::move(conversation_info), fre_override),
        base::BindOnce(&Host::PanelWillOpenComplete, GetWeakPtr(), client));
    if (!panel_open_) {
      client->PanelWasClosed(base::DoNothing());
    }
  }
  instance_delegate().skills_manager().UpdateSkillPreviews(std::nullopt);

  observers_.Notify(&Observer::WebClientConnected);
}

void Host::WebClientInitializeFailed() {
  observers_.Notify(&Observer::WebClientInitializeFailed);
}

void Host::SetContextAccessIndicator(bool enabled) {
  if (client_state_.context_access_indicator_enabled == enabled) {
    return;
  }
  client_state_.context_access_indicator_enabled = enabled;
  observers_.Notify(&Observer::ContextAccessIndicatorChanged, enabled);
}

bool Host::IsContextAccessIndicatorEnabled() const {
  return client_state_.context_access_indicator_enabled;
}

void Host::ManualResizeChanged(bool resizing) {
  is_manually_resizing_ = resizing;
  if (auto* client = GetPrimaryWebClient()) {
    client->ManualResizeChanged(resizing);
  }
}

bool Host::IsPrimaryClientOpen() {
  return client_state_.open_complete;
}

InstanceId Host::GetInstanceId() const {
  return glic_instance_ ? glic_instance_->id() : InstanceId::CreateNullId();
}

std::unique_ptr<content::WebContents> Host::ReleaseWebContents() {
  CHECK(contents_);
  return contents_->ReleaseWebContents();
}

void Host::ReclaimWebContents(
    std::unique_ptr<content::WebContents> web_contents) {
  CHECK(contents_);
  contents_->ReclaimWebContents(std::move(web_contents));
}

content::WebContents* Host::webui_contents() const {
  return contents_ ? contents_->active_web_contents() : nullptr;
}

void Host::SetWebContentsVisibilityOverride(
    std::optional<content::Visibility> visibility_override) {
  visibility_override_ = visibility_override;
  UpdateVisibility();
}

void Host::UpdateVisibility() {
  content::Visibility visibility = GetExpectedVisibility();
  if (web_contents_visibility_ == visibility) {
    return;
  }
  web_contents_visibility_ = visibility;
  if (contents_) {
    contents_->SetVisibility(visibility);
  }
  if (content::WebContents* client_contents = web_client_contents()) {
    client_contents->UpdateWebContentsVisibility(visibility);
  }
}

content::Visibility Host::GetExpectedVisibility() const {
  if (visibility_override_.has_value()) {
    return visibility_override_.value();
  }
  if (!contents_) {
    return content::Visibility::HIDDEN;
  }
  if (debounced_visibility_) {
    return content::Visibility::VISIBLE;
  }
  return content::Visibility::HIDDEN;
}

content::WebContents* Host::web_client_contents() const {
  auto* manager = web_client_manager();
  return manager ? manager->web_client_contents() : nullptr;
}

void Host::OnGuestWebClientCleared(bool had_web_client) {
  if (had_web_client) {
    if (IsContextAccessIndicatorEnabled()) {
      observers_.Notify(&Observer::ContextAccessIndicatorChanged, false);
    }
    instance_delegate().OnWebClientCleared();
  }
  client_state_ = {};
  observers_.Notify(&Observer::WebClientDisconnected);
}

bool Host::IsGlicWebUiHost(content::RenderProcessHost* host) const {
  if (page_handler_ &&
      page_handler_->webui_contents()->GetPrimaryMainFrame()->GetProcess() ==
          host) {
    return true;
  }
  return false;
}

content::RenderFrameHost* Host::GetGuestMainFrame() const {
  auto* manager = web_client_manager();
  return manager ? manager->GetGuestMainFrame() : nullptr;
}

GlicWebClientManager* Host::web_client_manager() {
  if (contents_) {
    return &contents_->web_client_manager();
  }
  return nullptr;
}

const GlicWebClientManager* Host::web_client_manager() const {
  if (contents_) {
    return &contents_->web_client_manager();
  }
  return nullptr;
}

mojom::WebClientState Host::web_client_state() const {
  auto* manager = web_client_manager();
  return manager && manager->web_client_access()
             ? manager->web_client_access()->web_client_state()
             : mojom::WebClientState::kUninitialized;
}

bool Host::IsGlicWebUi(content::WebContents* contents) const {
  return page_handler_ && page_handler_->webui_contents() == contents;
}

std::vector<GlicPageHandler*> Host::GetPageHandlersForTesting() {
  if (!page_handler_) {
    return {};
  }
  return {page_handler_};
}

GlicPageHandler* Host::GetPrimaryPageHandlerForTesting() {
  return page_handler_;
}

void Host::OnWebClientStateChanged(mojom::WebClientState state) {
  observers_.Notify(&Observer::WebClientStateChanged, state);
}

void Host::PanelWillOpenComplete(GlicWebClientAccess* client,
                                 mojom::OpenPanelInfoPtr open_info) {
  CHECK(client);
  if (GetPrimaryWebClient() == client) {
    if (panel_open_) {
      client_state_.open_complete = true;
    }
    // Notify observers that the client is ready even if `panel_open_` is false
    // (e.g. if the user backgrounded or closed the panel during load) so that
    // metrics can record load completion and clear any pending timers.
    observers_.Notify(&Observer::ClientReadyToShow, *open_info);
  }
}

bool Host::IsWebClientConnected() const {
  return GetPrimaryWebClient() != nullptr;
}

void Host::WebUiStateChanged(GlicPageHandler* page_handler,
                             mojom::WebUiState new_state) {
  if (primary_webui_state_ == new_state) {
    return;
  }
  base::UmaHistogramEnumeration("Glic.PanelWebUiState", new_state);
  if (!GlicEnabling::HasConsentedForProfile(profile_)) {
    base::UmaHistogramEnumeration("Glic.Fre.PanelWebUiState", new_state);
  }
  // UI State has changed
  primary_webui_state_ = new_state;
  observers_.Notify(&Observer::WebUiStateChanged, primary_webui_state_);
}

void Host::NotifyInstanceActivationChanged(bool is_active) {
  if (auto* client = GetPrimaryWebClient()) {
    client->NotifyInstanceActivationChanged(is_active);
  }
}

void Host::OnActuatingChanged(bool actuating) {
  if (contents_) {
    contents_->OnActuatingChanged(actuating);
  }
}

void Host::OnTaskTabsVisibilityChanged(bool has_visible_tab) {
  if (contents_) {
    contents_->OnTaskTabsVisibilityChanged(has_visible_tab);
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

void Host::OnInteractionModeChange(mojom::WebClientMode new_mode) {
  instance_delegate_->OnInteractionModeChange(new_mode);
}

void Host::OnMicrophoneStatusChanged(mojom::MicrophoneStatus status) {
  microphone_status_ = status;
  delegate_->OnMicrophoneStatusChanged(status);
}

void Host::ResizePanel(const gfx::Size& size,
                       base::TimeDelta duration,
                       base::OnceClosure callback) {
  delegate_->Resize(size, duration, std::move(callback));
}

void Host::EnableDragResize(bool enabled) {
  delegate_->EnableDragResize(enabled);
}

void Host::AttachPanel() {
  delegate_->Attach();
}

void Host::DetachPanel() {
  delegate_->Detach();
}

void Host::ClosePanel() {
  delegate_->ClosePanel();
}

void Host::SetMinimumWidgetSize(const gfx::Size& size) {
  delegate_->SetMinimumWidgetSize(size);
}

void Host::CaptureScreenshot(
    glic::mojom::WebClientHandler::CaptureScreenshotCallback callback) {
  delegate_->CaptureScreenshot(std::move(callback));
}

bool Host::IsWidgetShowing(GlicWebClientAccess* client) const {
  return delegate_->IsShowing();
}

void Host::FloatingPanelCanAttachChanged(bool can_attach) {
  if (auto* client = GetPrimaryWebClient()) {
    client->FloatingPanelCanAttachChanged(can_attach);
  }
}

}  // namespace glic
