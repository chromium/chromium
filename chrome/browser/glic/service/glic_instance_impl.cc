// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_impl.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_zero_state_suggestions_manager.h"
#include "chrome/browser/glic/host/context/glic_active_pinned_focused_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_empty_focused_browser_manager.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/glic/widget/glic_floating_ui.h"
#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui.h"
#include "chrome/browser/glic/widget/glic_side_panel_ui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace glic {

namespace {
EmbedderKey CreateSidePanelEmbedderKey(tabs::TabInterface* tab) {
  CHECK(tab);
  return EmbedderKey(tab);
}
}  // namespace

// Web Contents Observer for the tab bound with its respective glic
// embedder.
class GlicTabContentsObserver : public content::WebContentsObserver {
 public:
  GlicTabContentsObserver(content::WebContents* web_contents,
                          GlicInstanceImpl* instance)
      : content::WebContentsObserver(web_contents), instance_(instance) {}

  // content::WebContentsObserver:
  // This is called whenever a navigation happens from clicking a link within
  // the observed web contents.
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override {
    if (!new_contents) {
      return;
    }

    tabs::TabInterface* tab_to_bind =
        tabs::TabInterface::MaybeGetFromContents(new_contents);

    if (!tab_to_bind ||
        (tab_to_bind->GetBrowserWindowInterface()->GetProfile() !=
         instance_->profile())) {
      return;
    }

    tabs::TabInterface* source_tab = tabs::TabInterface::GetFromContents(
        content::WebContents::FromRenderFrameHost(source_render_frame_host));
    auto* glic_embedder = instance_->GetEmbedderForTab(source_tab);

    // Only bind if the previous instance was active.
    if (glic_embedder && glic_embedder->IsShowing()) {
      auto show_options = ShowOptions{SidePanelShowOptions{*tab_to_bind}};
      show_options.focus_on_show = tab_to_bind->IsActivated();
      instance_->Show(show_options);
    }
  }

 private:
  raw_ptr<GlicInstanceImpl> instance_ = nullptr;
};

void GlicInstanceImpl::NotifyStateChange() {
  state_change_callback_list_.Notify(IsShowing(),
                                     host().GetPrimaryCurrentView());
  if (coordinator_delegate_) {
    coordinator_delegate_->OnInstanceVisibilityChanged(this, IsShowing());
  }
}

GlicInstanceImpl::EmbedderEntry::EmbedderEntry() = default;
GlicInstanceImpl::EmbedderEntry::~EmbedderEntry() = default;
GlicInstanceImpl::EmbedderEntry::EmbedderEntry(EmbedderEntry&&) = default;
GlicInstanceImpl::EmbedderEntry& GlicInstanceImpl::EmbedderEntry::operator=(
    EmbedderEntry&&) = default;

GlicInstanceImpl::GlicInstanceImpl(
    Profile* profile,
    InstanceId instance_id,
    base::WeakPtr<InstanceCoordinatorDelegate> coordinator_delegate,
    GlicMetrics* metrics,
    contextual_cueing::ContextualCueingService* contextual_cueing_service)
    : profile_(profile),
      service_(GlicKeyedService::Get(profile)),
      coordinator_delegate_(coordinator_delegate),
      id_(instance_id),
      host_(profile_, this, this, this),
      sharing_manager_(
          std::make_unique<GlicActivePinnedFocusedTabManager>(
              profile,
              &sharing_manager_),
          std::make_unique<GlicEmptyFocusedBrowserManager>(),
          std::make_unique<GlicPinnedTabManager>(profile, this, metrics),
          profile,
          metrics),
      last_non_hidden_panel_state_kind_(mojom::PanelStateKind::kAttached),
      zero_state_suggestions_manager_(
          std::make_unique<GlicZeroStateSuggestionsManager>(
              &sharing_manager_,
              this,
              contextual_cueing_service)),
      actor_task_manager_(std::make_unique<GlicActorTaskManager>(
          profile,
          actor::ActorKeyedServiceFactory::GetActorKeyedService(profile))) {
  CHECK(actor_task_manager_);
  browser_list_observation_.Observe(BrowserList::GetInstance());
  // Start warming the contents.
  host_.SetDelegate(&empty_embedder_delegate_);
  // TODO(crbug.com/448160018): Figure out how to signal the web contents
  // opening so that this can be set to `true`.
  host_.CreateContents(/*initially_hidden=*/false);
  host_observation_.Observe(&host_);
}

GlicInstanceImpl::~GlicInstanceImpl() {
  // Destroying the web contents may result in calls back here, so do it first.
  host_.Shutdown();
}

bool GlicInstanceImpl::IsShowing() const {
  return active_embedder_key_.has_value();
}

bool GlicInstanceImpl::IsAttached() {
  return GetPanelState().kind == mojom::PanelStateKind::kAttached;
}

gfx::Size GlicInstanceImpl::GetPanelSize() {
  if (auto* embedder = GetActiveEmbedder()) {
    return embedder->GetPanelSize();
  }
  return gfx::Size();
}

void GlicInstanceImpl::Show(const ShowOptions& options) {
  if (const auto* side_panel_options =
          std::get_if<SidePanelShowOptions>(&options.embedder_options);
      side_panel_options && !side_panel_options->tab->IsActivated()) {
    ShowInactiveSidePanelEmbedderFor(&side_panel_options->tab.get());
    return;
  }

  EmbedderKey new_key = GetEmbedderKey(options);

  GlicUiEmbedder* embedder_to_show = nullptr;

  if (active_embedder_key_.has_value() &&
      active_embedder_key_.value() == new_key) {
    embedder_to_show = GetActiveEmbedder();
  } else {
    DeactivateCurrentEmbedder();
    embedder_to_show = CreateActiveEmbedder(options);
    CHECK(embedder_to_show);
    host_.SetDelegate(embedder_to_show->GetHostEmbedderDelegate());
    SetActiveEmbedderAndNotifyStateChange(new_key);
  }

  MaybeShowHostUi(embedder_to_show);
  embedder_to_show->Show();
  if (options.focus_on_show) {
    embedder_to_show->Focus();
  }
}

void GlicInstanceImpl::Detach(tabs::TabInterface* tab) {
  if (coordinator_delegate_) {
    coordinator_delegate_->OnDetachRequested(this, tab);
  }
  auto show_options =
      ShowOptions::ForFloating(tab->GetBrowserWindowInterface());
  show_options.focus_on_show = true;
  Show(show_options);
  Close(CreateSidePanelEmbedderKey(tab));
}

void GlicInstanceImpl::Close(EmbedderKey key) {
  auto* embedder = GetEmbedderForKey(key);
  if (embedder) {
    embedder->Close();
  }
  MaybeDeactivateEmbedderAndCloseHostUi(key);
}

void GlicInstanceImpl::Toggle(ShowOptions&& options, bool prevent_close) {
  EmbedderKey key = GetEmbedderKey(options);
  if (active_embedder_key_.has_value() && active_embedder_key_.value() == key) {
    if (!prevent_close) {
      Close(key);
    }
  } else {
    // We assume that a toggle is user initiated so focus on show.
    options.focus_on_show = true;
    Show(options);
  }
}

GlicUiEmbedder* GlicInstanceImpl::GetEmbedderForTab(tabs::TabInterface* tab) {
  return GetEmbedderForKey(EmbedderKey(tab));
}

GlicUiEmbedder* GlicInstanceImpl::GetEmbedderForKey(EmbedderKey key) {
  auto it = embedders_.find(key);
  if (it != embedders_.end()) {
    return it->second.embedder.get();
  }
  return nullptr;
}

GlicSharingManager& GlicInstanceImpl::sharing_manager() {
  return sharing_manager_;
}

void GlicInstanceImpl::CloseInstanceAndShutdown() {
  NOTIMPLEMENTED();
}

void GlicInstanceImpl::RegisterConversation(
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::RegisterConversationCallback callback) {
  if (!info) {
    // This point shouldn't be hit, because empty info triggers switching to a
    // new conversation and the glic api enforces non-empty conversation info
    // for `registerConversation`.
    LOG(ERROR) << "RegisterConversation called with null info.";
    std::move(callback).Run(
        mojom::RegisterConversationErrorReason::kDefaultValue);
    return;
  }

  if (conversation_info_ &&
      conversation_info_->conversation_id != info->conversation_id) {
    std::move(callback).Run(mojom::RegisterConversationErrorReason::
                                kInstanceAlreadyHasConversationId);
    return;
  }

  conversation_info_ =
      ConversationInfo{info->conversation_id, info->conversation_title};
  std::move(callback).Run(std::nullopt);
}

tabs::TabInterface* GlicInstanceImpl::CreateTab(
    const ::GURL& url,
    bool open_in_background,
    const std::optional<int32_t>& window_id,
    glic::mojom::WebClientHandler::CreateTabCallback callback) {
  tabs::TabInterface* created_tab = service_->CreateTab(
      url, open_in_background, window_id, std::move(callback));
  if (!created_tab) {
    return nullptr;
  }

  auto show_options = ShowOptions::ForSidePanel(*created_tab);
  show_options.focus_on_show = created_tab->IsActivated();
  Show(show_options);
  return nullptr;
}

void GlicInstanceImpl::CreateTask(
    base::WeakPtr<actor::ActorTaskDelegate> delegate,
    actor::webui::mojom::TaskOptionsPtr options,
    mojom::WebClientHandler::CreateTaskCallback callback) {
  actor_task_manager_->CreateTask(weak_ptr_factory_.GetWeakPtr(),
                                  std::move(options), std::move(callback));
}

void GlicInstanceImpl::PerformActions(
    const std::vector<uint8_t>& actions_proto,
    mojom::WebClientHandler::PerformActionsCallback callback) {
  actor_task_manager_->PerformActions(actions_proto, std::move(callback));
}

void GlicInstanceImpl::StopActorTask(actor::TaskId task_id,
                                     mojom::ActorTaskStopReason stop_reason) {
  actor_task_manager_->StopActorTask(task_id, stop_reason);
}

void GlicInstanceImpl::PauseActorTask(actor::TaskId task_id,
                                      mojom::ActorTaskPauseReason pause_reason,
                                      tabs::TabInterface::Handle tab_handle) {
  actor_task_manager_->PauseActorTask(task_id, pause_reason, tab_handle);
}

void GlicInstanceImpl::ResumeActorTask(
    actor::TaskId task_id,
    const mojom::GetTabContextOptions& context_options,
    glic::mojom::WebClientHandler::ResumeActorTaskCallback callback) {
  actor_task_manager_->ResumeActorTask(task_id, context_options,
                                       std::move(callback));
}

void GlicInstanceImpl::GetZeroStateSuggestionsAndSubscribe(
    bool has_active_subscription,
    const mojom::ZeroStateSuggestionsOptions& options,
    mojom::WebClientHandler::GetZeroStateSuggestionsAndSubscribeCallback
        callback) {
  zero_state_suggestions_manager_->ObserveZeroStateSuggestions(
      has_active_subscription, options.is_first_run, options.supported_tools,
      std::move(callback));
}

void GlicInstanceImpl::PrepareForOpen() {
  GlicKeyedServiceFactory::GetGlicKeyedService(profile_)
      ->fre_controller()
      .MaybePreconnect();

  // TODO(crbug.com/444463509): Update this when we have per-instance
  // sharing managers set up without auto-focus.
  auto* active_web_contents =
      sharing_manager().GetFocusedTabData().focus()
          ? sharing_manager().GetFocusedTabData().focus()->GetContents()
          : nullptr;
  contextual_cueing::ContextualCueingService* contextual_cueing_service =
      contextual_cueing::ContextualCueingServiceFactory::GetForProfile(
          profile_);
  if (contextual_cueing_service && active_web_contents) {
    contextual_cueing_service->PrepareToFetchContextualGlicZeroStateSuggestions(
        active_web_contents);
  }
}

void GlicInstanceImpl::AddStateObserver(PanelStateObserver* observer) {
  state_observers_.AddObserver(observer);
}

void GlicInstanceImpl::RemoveStateObserver(PanelStateObserver* observer) {
  state_observers_.RemoveObserver(observer);
}

void GlicInstanceImpl::UnbindEmbedder(EmbedderKey key) {
  MaybeDeactivateEmbedderAndCloseHostUi(key);
  embedders_.erase(key);
}

Host& GlicInstanceImpl::host() {
  return host_;
}

const InstanceId& GlicInstanceImpl::id() const {
  return id_;
}

base::CallbackListSubscription GlicInstanceImpl::RegisterStateChange(
    StateChangeCallback callback) {
  return state_change_callback_list_.Add(std::move(callback));
}

void GlicInstanceImpl::FetchZeroStateSuggestions(
    bool is_first_run,
    std::optional<std::vector<std::string>> supported_tools,
    mojom::WebClientHandler::GetZeroStateSuggestionsForFocusedTabCallback
        callback) {
  // TODO(crbug.com/444463509): Update this when we have per-instance
  // sharing managers set up without auto-focus.
  auto* active_web_contents =
      sharing_manager().GetFocusedTabData().focus()
          ? sharing_manager().GetFocusedTabData().focus()->GetContents()
          : nullptr;

  contextual_cueing::ContextualCueingService* contextual_cueing_service =
      contextual_cueing::ContextualCueingServiceFactory::GetForProfile(
          profile_);

  if (contextual_cueing_service && active_web_contents && IsShowing()) {
    auto suggestions = mojom::ZeroStateSuggestions::New();
    suggestions->tab_id = GetTabId(active_web_contents);
    suggestions->tab_url = active_web_contents->GetLastCommittedURL();
    contextual_cueing_service
        ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
            active_web_contents, is_first_run, supported_tools,
            mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                base::BindOnce(&GlicInstanceImpl::OnZeroStateSuggestionsFetched,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(suggestions), std::move(callback)),
                std::vector<std::string>({})));

  } else {
    std::move(callback).Run(nullptr);
  }
}

void GlicInstanceImpl::OnZeroStateSuggestionsFetched(
    mojom::ZeroStateSuggestionsPtr suggestions,
    mojom::WebClientHandler::GetZeroStateSuggestionsForFocusedTabCallback
        callback,
    std::vector<std::string> returned_suggestions) {
  std::vector<mojom::SuggestionContentPtr> output_suggestions;
  for (const std::string& suggestion_string : returned_suggestions) {
    output_suggestions.push_back(
        mojom::SuggestionContent::New(suggestion_string));
  }
  suggestions->suggestions = std::move(output_suggestions);

  std::move(callback).Run(std::move(suggestions));
}

std::optional<std::string> GlicInstanceImpl::conversation_id() const {
  if (conversation_info_) {
    return conversation_info_->conversation_id;
  }
  return std::nullopt;
}

// Automatic activation should be suppressed if a floating embedder is active.
// The floating UI is a more deliberate user choice, and we don't want a
// tab switch to unexpectedly close the floating UI.
bool GlicInstanceImpl::ShouldDoAutomaticActivation() const {
  return !active_embedder_key_.has_value() ||
         !std::holds_alternative<FloatingEmbedderKey>(
             active_embedder_key_.value());
}

void GlicInstanceImpl::OnBrowserSetLastActive(Browser* browser) {
  if (!ShouldDoAutomaticActivation()) {
    return;
  }
  tabs::TabInterface* active_tab = browser->GetActiveTabInterface();
  if (!active_tab) {
    return;
  }
  auto* embedder = GetEmbedderForTab(active_tab);
  if (embedder && embedder->IsShowing()) {
    Show(ShowOptions::ForSidePanel(*active_tab));
  }
}

GlicUiEmbedder* GlicInstanceImpl::GetActiveEmbedder() {
  if (!active_embedder_key_.has_value()) {
    return nullptr;
  }
  auto it = embedders_.find(active_embedder_key_.value());
  if (it != embedders_.end()) {
    return it->second.embedder.get();
  }
  return nullptr;
}

void GlicInstanceImpl::DeactivateCurrentEmbedder() {
  auto* old_embedder = GetActiveEmbedder();
  if (!old_embedder) {
    ClearActiveEmbedderAndNotifyStateChange();
    return;
  }

  auto it = embedders_.find(active_embedder_key_.value());
  CHECK(it != embedders_.end());
  // Avoid use-after-free.
  host_.SetDelegate(&empty_embedder_delegate_);
  it->second.embedder = old_embedder->CreateInactiveEmbedder();
  ClearActiveEmbedderAndNotifyStateChange();
}

GlicUiEmbedder* GlicInstanceImpl::CreateActiveEmbedder(
    const ShowOptions& options) {
  return std::visit(
      absl::Overload{[&](const SidePanelShowOptions& opts) {
                       return CreateActiveEmbedderForSidePanel(&opts.tab.get());
                     },
                     [&](const FloatingShowOptions& opts) {
                       return CreateActiveEmbedderForFloaty(
                           opts.initial_bounds);
                     }},
      options.embedder_options);
}

GlicUiEmbedder* GlicInstanceImpl::CreateActiveEmbedderForSidePanel(
    tabs::TabInterface* tab) {
  auto& entry = BindTab(tab);
  entry.embedder =
      std::make_unique<GlicSidePanelUi>(profile_, tab->GetWeakPtr(), *this);
  return entry.embedder.get();
}

GlicUiEmbedder* GlicInstanceImpl::CreateActiveEmbedderForFloaty(
    const gfx::Rect& initial_bounds) {
  EmbedderKey key = FloatingEmbedderKey();
  auto [entry_iter, _] = embedders_.try_emplace(key);
  entry_iter->second.embedder =
      std::make_unique<GlicFloatingUi>(profile_, initial_bounds, *this);
  return entry_iter->second.embedder.get();
}

void GlicInstanceImpl::ShowInactiveSidePanelEmbedderFor(
    tabs::TabInterface* tab) {
  auto& entry = BindTab(tab);
  entry.embedder = GlicInactiveSidePanelUi::CreateForBackgroundTab(
      tab->GetWeakPtr(), host().webui_contents(), *this);
}

void GlicInstanceImpl::SetActiveEmbedderAndNotifyStateChange(
    std::optional<EmbedderKey> new_key) {
  active_embedder_key_ = new_key;
  if (last_non_hidden_panel_state_kind_ != GetPanelState().kind &&
      GetPanelState().kind != mojom::PanelStateKind::kHidden) {
    last_non_hidden_panel_state_kind_ = GetPanelState().kind;
  }
  NotifyStateChange();
  NotifyPanelStateChanged();
}

void GlicInstanceImpl::ClearActiveEmbedderAndNotifyStateChange() {
  if (active_embedder_key_.has_value()) {
    active_embedder_key_.reset();
    NotifyStateChange();
    NotifyPanelStateChanged();
    host().PanelWasClosed();
  }
  return;
}

void GlicInstanceImpl::MaybeShowHostUi(GlicUiEmbedder* embedder) {
  Host::EmbedderDelegate* delegate = embedder->GetHostEmbedderDelegate();
  if (!delegate) {
    return;
  }

  host_.SetDelegate(delegate);

  // Create the WebContents if it's not already created.
  host_.CreateContents(/*initially_hidden=*/false);
  host_.webui_contents()->UpdateWebContentsVisibility(
      content::Visibility::VISIBLE);
  host_.NotifyWindowIntentToShow();

  // TODO: NotifyPanelStateChanged() here
  // TODO: pass in the correct invocation source
  Host::PanelWillOpenOptions options;
  options.conversation_id = conversation_id();
  host_.PanelWillOpen(mojom::InvocationSource::kTopChromeButton,
                      std::move(options));
}

void GlicInstanceImpl::OnBoundTabDestroyed(tabs::TabInterface* tab,
                                           const InstanceId& instance_id) {
  UnbindEmbedder(tab);
  if (embedders_.empty() && coordinator_delegate_) {
    // This call will delete `this`.
    coordinator_delegate_->RemoveInstance(this);
  }
}

void GlicInstanceImpl::OnBoundTabActivated(tabs::TabInterface* tab) {
  if (!ShouldDoAutomaticActivation()) {
    return;
  }
  auto* embedder = GetEmbedderForTab(tab);
  if (embedder && embedder->IsShowing()) {
    // Ensure that the side panel in this tab becomes the active embedder.
    Show(ShowOptions::ForSidePanel(*tab));
  }
}

void GlicInstanceImpl::SwitchConversation(
    const ShowOptions& options,
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::SwitchConversationCallback callback) {
  if (coordinator_delegate_) {
    coordinator_delegate_->SwitchConversation(*this, options, std::move(info),
                                              std::move(callback));
  } else {
    std::move(callback).Run(mojom::SwitchConversationErrorReason::kUnknown);
  }
}

void GlicInstanceImpl::MaybeDeactivateEmbedderAndCloseHostUi(EmbedderKey key) {
  if (active_embedder_key_.has_value() && active_embedder_key_.value() == key) {
    // TODO: Figure out what else should go into host_.PanelWasClosed() and
    // maybe call it here.
    DeactivateCurrentEmbedder();
    // Post a delayed task to maybe activate another embedder. This is to avoid
    // a race condition where the deactivation of an old embedder (e.g. during a
    // tab/window switch) tries to show the new embedder before the browser's
    // own tab activation logic has had a chance to run. By posting, we allow
    // the synchronous activation logic to complete, and then this task will run
    // and activate a foreground embedder only if one isn't already active.
    // TODO(crbug.com/451667367): Find another way to do this that doesn't
    // require a delayed task. Spoiler alert, it might not be possible.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GlicInstanceImpl::MaybeActivateForegroundEmbedder,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Milliseconds(30));
  }
}

GlicInstanceImpl::EmbedderEntry& GlicInstanceImpl::BindTab(
    tabs::TabInterface* tab) {
  EmbedderKey key = CreateSidePanelEmbedderKey(tab);
  auto [it, inserted] = embedders_.try_emplace(key);

  if (!inserted) {
    return it->second;
  }

  if (coordinator_delegate_) {
    coordinator_delegate_->UnbindTabFromAnyInstance(tab);
  }

  EmbedderEntry& new_entry = it->second;
  auto* helper = GlicInstanceHelper::From(tab);
  CHECK(helper);
  helper->SetInstanceId(id_);
  new_entry.destruction_subscription = helper->SubscribeToDestruction(
      base::BindRepeating(&GlicInstanceImpl::OnBoundTabDestroyed,
                          weak_ptr_factory_.GetWeakPtr()));
  new_entry.tab_activation_subscription = tab->RegisterDidActivate(
      base::BindRepeating(&GlicInstanceImpl::OnBoundTabActivated,
                          weak_ptr_factory_.GetWeakPtr()));
  new_entry.tab_web_contents_observer =
      std::make_unique<GlicTabContentsObserver>(tab->GetContents(), this);
  // Auto-pin on bind.
  sharing_manager().PinTabs({tab->GetHandle()});

  return new_entry;
}

void GlicInstanceImpl::WillCloseFor(EmbedderKey key) {
  MaybeDeactivateEmbedderAndCloseHostUi(key);
}

void GlicInstanceImpl::WebUiStateChanged(mojom::WebUiState state) {
  if (state == mojom::WebUiState::kReady) {
    if (auto* embedder = GetActiveEmbedder()) {
      embedder->Focus();
    }
  }
}

void GlicInstanceImpl::OnEmbedderWindowActivationChanged(bool has_focus) {
  coordinator_delegate_->OnInstanceActivationChanged(this, has_focus);
}

void GlicInstanceImpl::NotifyPanelStateChanged() {
  state_observers_.Notify(
      &PanelStateObserver::PanelStateChanged, GetPanelState(),
      PanelStateContext{.attached_browser = nullptr, .glic_widget = nullptr});
}

mojom::PanelState GlicInstanceImpl::GetPanelState() {
  auto* embedder = GetActiveEmbedder();
  if (embedder) {
    return embedder->GetPanelState();
  }
  mojom::PanelState panel_state;
  panel_state.kind = mojom::PanelStateKind::kHidden;
  return panel_state;
}

// If no embedder is active, finds an embedder associated with an active
// tab and activates it. Note: The order is not guaranteed to be MRU.
void GlicInstanceImpl::MaybeActivateForegroundEmbedder() {
  if (active_embedder_key_.has_value()) {
    return;
  }
  for (auto const& [key, entry] : embedders_) {
    if (tabs::TabInterface* const* tab =
            std::get_if<tabs::TabInterface*>(&key)) {
      if (entry.embedder->IsShowing()) {
        Show(ShowOptions::ForSidePanel(**tab));
        return;
      }
    }
  }

  // If no embedder is showing, then the instance is inactive.
  coordinator_delegate_->OnInstanceActivationChanged(this, false);
}

void GlicInstanceImpl::OnTabAddedToTask(
    actor::TaskId task_id,
    const tabs::TabInterface::Handle& tab_handle) {
  tabs::TabInterface* tab = tab_handle.Get();
  if (!tab || !task_id) {
    return;
  }
  Show(ShowOptions::ForSidePanel(*tab));
}

}  // namespace glic
