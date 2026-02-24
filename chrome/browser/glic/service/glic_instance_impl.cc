// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_impl.h"

#include <memory>
#include <optional>
#include <sstream>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/common/future_browser_features.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_zero_state_suggestions_manager.h"
#include "chrome/browser/glic/host/context/glic_active_pinned_focused_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_empty_focused_browser_manager.h"
#include "chrome/browser/glic/host/context/glic_empty_focused_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_pinned_tab_manager_impl.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_coordinator.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/glic/service/glic_ui_types.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/user_education_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/host/context/glic_empty_pinned_tab_manager.h"
#include "chrome/browser/glic/widget/glic_floating_ui_android.h"
#include "chrome/browser/glic/widget/glic_inactive_floating_ui_android.h"
#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui_android.h"
#include "chrome/browser/glic/widget/glic_side_panel_ui_android.h"
#else
#include "chrome/browser/glic/host/context/glic_focused_tab_manager.h"
#include "chrome/browser/glic/widget/glic_floating_ui.h"
#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui.h"
#include "chrome/browser/glic/widget/glic_side_panel_ui.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace glic {

BASE_FEATURE(kGlicBindOnlyForDaisyChainingFromFloatingUi,
             base::FEATURE_ENABLED_BY_DEFAULT);
#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kGlicActorDaisyChainingFromFloatingUiDoesntClose,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif
BASE_FEATURE(kGlicBindOnPinFromFloatingUiDoesntShowSidePanel,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGlicRemoveBlankInstancesOnClose,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGlicAlwaysBindOnPin, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicAvoidReactivatingActiveEmbedder,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicUnpinOnUnbindIfUnused, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSuppressFocusOnReady, base::FEATURE_ENABLED_BY_DEFAULT);

constexpr size_t kMaxRecentConversationsForPanel = 3;

const base::FeatureParam<base::TimeDelta> kRemoveBlankInstanceDelay{
    &kGlicRemoveBlankInstancesOnClose, "delay", base::Seconds(1)};
BASE_FEATURE(kGlicSuppressAnimationsOnDetach, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicRemoveDaisyChainingWhenFreShowing,
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {
EmbedderKey CreateSidePanelEmbedderKey(tabs::TabInterface* tab) {
  CHECK(tab);
  return EmbedderKey(tab);
}

bool IsTrustFirstOnboardingPending(Profile* profile) {
  return GlicEnabling::IsTrustFirstOnboardingEnabledForProfile(profile);
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

    if (!tab_to_bind) {
      LOG(ERROR) << "Invalid tab_to_bind, null";
      return;
    }
    if (tab_to_bind->GetBrowserWindowInterface()->GetProfile() !=
        instance_->profile()) {
      LOG(ERROR) << "Invalid tab_to_bind, wrong profile";
      return;
    }
    if (!GlicInstanceHelper::From(tab_to_bind)) {
      LOG(ERROR) << "Invalid tab_to_bind, no GlicInstanceHelper in its "
                    "UnownedUserData";
      return;
    }

    if (!tab_to_bind ||
        (tab_to_bind->GetBrowserWindowInterface()->GetProfile() !=
         instance_->profile()) ||
        !GlicInstanceHelper::From(tab_to_bind)) {
      LOG(ERROR) << "Invalid tab_to_bind, null or wrong profile or no "
                    "GlicInstanceHelper in its UnownedUserData";
      return;
    }

    tabs::TabInterface* source_tab = tabs::TabInterface::GetFromContents(
        content::WebContents::FromRenderFrameHost(source_render_frame_host));

    instance_->MaybeDaisyChainToTab(source_tab, tab_to_bind);
  }

 private:
  raw_ptr<GlicInstanceImpl> instance_ = nullptr;
};

void GlicInstanceImpl::MaybeDaisyChainToTab(tabs::TabInterface* source_tab,
                                            tabs::TabInterface* target_tab) {
  auto* glic_embedder = GetEmbedderForTab(source_tab);

  if (base::FeatureList::IsEnabled(kGlicRemoveDaisyChainingWhenFreShowing)) {
    if (service()->IsFreShowing() ||
        (IsTrustFirstOnboardingPending(profile()) &&
         features::kGlicTrustFirstOnboardingArmParam.Get() != 1)) {
      return;
    }
  }

  // Only bind if the previous instance was active.
  if (glic_embedder && glic_embedder->IsShowing()) {
    SidePanelShowOptions side_panel_options{*target_tab};
    side_panel_options.suppress_opening_animation = true;
    side_panel_options.pin_trigger = GlicPinTrigger::kDaisyChain;
    auto show_options = ShowOptions{side_panel_options};
    metrics()->OnDaisyChain(DaisyChainSource::kTabContents,
                            /*success=*/true, target_tab, source_tab);
    Show(show_options);
  } else {
    // Record the failure.
    metrics()->OnDaisyChain(DaisyChainSource::kTabContents,
                            /*success=*/false, target_tab, source_tab);
  }
}

void GlicInstanceImpl::NotifyStateChange() {
  instance_metrics_.OnVisibilityChanged(IsShowing());
  state_change_callback_list_.Notify(IsShowing());
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
      sharing_manager_coordinator_(profile, this, metrics),
      instance_metrics_(
          &sharing_manager_coordinator_.GetActiveSharingManager()),
      zero_state_suggestions_manager_(
          std::make_unique<GlicZeroStateSuggestionsManager>(
              &sharing_manager(),
              this,
              contextual_cueing_service)),
      last_activation_timestamp_(base::Time::Now()),
      last_deactivation_timestamp_(base::TimeTicks::Now()) {
  if (auto* actor_keyed_service =
          actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_)) {
    actor_task_manager_ = std::make_unique<GlicActorTaskManager>(
        profile, actor_keyed_service, service_->actor_policy_checker());
  }

  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
  host_.SetDelegate(&empty_embedder_delegate_);
  if (!base::FeatureList::IsEnabled(features::kGlicWebContentsWarming)) {
    // Start warming the contents.
    host_.CreateContents(/*initially_hidden=*/false);
  }
  host_observation_.Observe(&host_);
  if (base::FeatureList::IsEnabled(features::kGlicBindPinnedUnboundTab)) {
    pinned_tabs_change_subscription_ =
        sharing_manager().AddTabPinningStatusEventCallback(
            base::BindRepeating(&GlicInstanceImpl::OnTabPinningStatusEvent,
                                weak_ptr_factory_.GetWeakPtr()));
  }
}

GlicInstanceImpl::~GlicInstanceImpl() {
  // Destroying the web contents may result in calls back here, so do it first.
  host_.Shutdown();

  // Unbind from all embedders to close side panels and prevent dangling ptrs.
  std::vector<EmbedderKey> keys;
  for (const auto& [key, _] : embedders_) {
    keys.push_back(key);
  }
  for (const auto& key : keys) {
    UnbindEmbedder(key);
  }

  for (auto* web_contents : sharing_manager().GetPinnedTabs()) {
    if (auto* tab = tabs::TabInterface::GetFromContents(web_contents)) {
      if (auto* helper = GlicInstanceHelper::From(tab)) {
        helper->OnUnpinnedByInstance(this);
      }
    }
  }
}

glic::GlicInstanceMetrics* GlicInstanceImpl::instance_metrics() {
  return &instance_metrics_;
}

glic::GlicInstanceMetricsBackwardsCompatibility&
GlicInstanceImpl::instance_metrics_backwards_compatibility() {
  return instance_metrics_;
}

bool GlicInstanceImpl::IsShowing() const {
  return active_embedder_key_.has_value();
}

bool GlicInstanceImpl::IsAttached() {
  return GetPanelState().kind == mojom::PanelStateKind::kAttached;
}

bool GlicInstanceImpl::IsDetached() {
  return GetPanelState().kind == mojom::PanelStateKind::kDetached;
}

gfx::Size GlicInstanceImpl::GetPanelSize() {
  if (auto* embedder = GetActiveEmbedder()) {
    return embedder->GetPanelSize();
  }
  return gfx::Size();
}

bool GlicInstanceImpl::IsActuating() const {
  return actor_task_manager_ && actor_task_manager_->IsActuating();
}

bool GlicInstanceImpl::IsLiveMode() {
  return interaction_mode_ == mojom::WebClientMode::kAudio;
}

void GlicInstanceImpl::Show(const ShowOptions& options) {
  if (const auto* side_panel_options =
          std::get_if<SidePanelShowOptions>(&options.embedder_options);
      side_panel_options && !side_panel_options->tab->IsActivated()) {
    ShowInactiveSidePanelEmbedderFor(*side_panel_options);
    return;
  }

  EmbedderKey new_key = GetEmbedderKey(options);

  GlicUiEmbedder* embedder_to_show = nullptr;

  if (IsActiveEmbedder(new_key)) {
    if (base::FeatureList::IsEnabled(kGlicAvoidReactivatingActiveEmbedder) &&
        !options.reinitialize_if_already_active) {
      return;
    } else {
      embedder_to_show = GetActiveEmbedder();
    }
  } else {
    DeactivateCurrentEmbedder();
    // Ensure that there is a WebContents for the embedder to use.
    host_.CreateContents(/*initially_hidden=*/false);
    embedder_to_show = CreateActiveEmbedder(options);
    CHECK(embedder_to_show);
    host_.SetDelegate(embedder_to_show->GetHostEmbedderDelegate());
    SetActiveEmbedderAndNotifyStateChange(new_key);
  }

  MaybeShowHostUi(embedder_to_show, options.invocation_source,
                  options.prompt_suggestion, options.auto_send);
  embedder_to_show->Show(options);
  if (options.focus_on_show) {
    embedder_to_show->Focus();
  }
}

void GlicInstanceImpl::Detach(tabs::TabInterface& tab) {
  instance_metrics_.OnDetach();
  auto show_options =
      ShowOptions::ForFloating(tab.GetHandle(), interaction_mode_);
  show_options.focus_on_show = true;
  Show(show_options);
  Close(CreateSidePanelEmbedderKey(&tab),
        {.suppress_animations =
             base::FeatureList::IsEnabled(kGlicSuppressAnimationsOnDetach)});
}

void GlicInstanceImpl::Attach(tabs::TabHandle tab) {
  tabs::TabInterface* tab_to_attach_to = tab.Get();
  if (base::FeatureList::IsEnabled(features::kGlicOrphanedReattachment) &&
      !tab_to_attach_to) {
    // No source tab given. Activate the first side panel embedder.
    for (const auto& [key, entry] : embedders_) {
      if (auto* const* tab_ptr = std::get_if<tabs::TabInterface*>(&key)) {
        tab_to_attach_to = *tab_ptr;
        break;
      }
    }
  }

  if (!tab_to_attach_to) {
    DLOG(ERROR) << "Attach called with no valid tab and no fallback available.";
    return;
  }

  if (auto* contents = tab_to_attach_to->GetContents()) {
    if (auto* delegate = contents->GetDelegate()) {
      delegate->ActivateContents(contents);
    }
  }
  Show(ShowOptions::ForSidePanel(*tab_to_attach_to));
}

void GlicInstanceImpl::Close(EmbedderKey key, const CloseOptions& options) {
  auto* embedder = GetEmbedderForKey(key);
  if (!embedder) {
    return;
  }
  if (GlicEnabling::IsTrustFirstOnboardingEnabledForProfile(profile_)) {
    service_->metrics()->OnTrustFirstOnboardingDismissed();
  }
  instance_metrics_.OnClose();
  embedder->Close(options);
}

bool GlicInstanceImpl::Toggle(ShowOptions&& options,
                              bool prevent_close,
                              glic::mojom::InvocationSource source,
                              std::optional<std::string> prompt_suggestion,
                              bool auto_send) {
  if (GlicEnabling::IsTrustFirstOnboardingEnabledForProfile(profile_)) {
    service_->metrics()->OnTrustFirstOnboardingShown();
  }

  instance_metrics_.OnToggle(source, options, IsShowing());
  EmbedderKey key = GetEmbedderKey(options);
  // Close instance on toggle when it has an active embedder.
  if (IsActiveEmbedder(key)) {
    if (!prevent_close) {
      Close(key);
    }
    return false;
  }
  // We assume that a toggle is user initiated so focus on show.
  options.focus_on_show = true;
  options.prompt_suggestion = prompt_suggestion;
  options.auto_send = auto_send;
  options.invocation_source = source;
  Show(options);
  return true;
}

GlicUiEmbedder* GlicInstanceImpl::GetEmbedderForTab(tabs::TabInterface* tab) {
  return GetEmbedderForKey(EmbedderKey(tab));
}

bool GlicInstanceImpl::ContextAccessIndicatorEnabled() {
  return host().IsContextAccessIndicatorEnabled();
}

GlicUiEmbedder* GlicInstanceImpl::GetEmbedderForKey(EmbedderKey key) {
  auto it = embedders_.find(key);
  if (it != embedders_.end()) {
    return it->second.embedder.get();
  }
  return nullptr;
}

GlicSharingManager& GlicInstanceImpl::sharing_manager() {
  return sharing_manager_coordinator_.GetActiveSharingManager();
}

void GlicInstanceImpl::CloseInstanceAndShutdown() {
  if (actor_task_manager_) {
    // We have to do this here before the ActorKeyedService is shutdown.
    actor_task_manager_->CancelTask();
  }
}

void GlicInstanceImpl::RegisterConversation(
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::RegisterConversationCallback callback) {
  instance_metrics_.OnRegisterConversation(info->conversation_id);

  if (!conversation_info_->conversation_id.empty() &&
      conversation_info_->conversation_id != info->conversation_id) {
    std::move(callback).Run(mojom::RegisterConversationErrorReason::
                                kInstanceAlreadyHasConversationId);
    return;
  }

  conversation_info_ = std::move(info);
  std::move(callback).Run(std::nullopt);
}

tabs::TabInterface* GlicInstanceImpl::CreateTab(
    const ::GURL& url,
    bool open_in_background,
    const std::optional<int32_t>& window_id,
    glic::mojom::WebClientHandler::CreateTabCallback callback) {
  instance_metrics_.OnCreateTab();
  auto* active_embedder = GetActiveEmbedder();
  bool embedder_has_focus = active_embedder && active_embedder->HasFocus();

  tabs::TabInterface* source_tab = nullptr;
  if (active_embedder_key_.has_value()) {
    if (auto* tab_ptr =
            std::get_if<tabs::TabInterface*>(&active_embedder_key_.value())) {
      source_tab = *tab_ptr;
    }
  }

  bool is_onboarding = IsTrustFirstOnboardingPending(profile_);

  tabs::TabInterface* created_tab = service_->CreateTab(
      url, open_in_background || is_onboarding, window_id, std::move(callback));

  // Prevent links clicked inside the side panel during the onboarding from
  // being daisy chained.
  if (is_onboarding) {
    return nullptr;
  }

  if (!created_tab) {
    instance_metrics_.OnDaisyChain(DaisyChainSource::kGlicContents,
                                   /*success=*/false, nullptr, source_tab);
    return nullptr;
  }

  // If the floating UI is active and the feature flag is enabled, we only bind
  // the tab instead of showing it to avoid closing the floating UI.
  if (base::FeatureList::IsEnabled(
          kGlicBindOnlyForDaisyChainingFromFloatingUi) &&
      IsDetached()) {
    BindTab(created_tab, GlicPinTrigger::kDaisyChain, /*pin_on_bind=*/true);
    if (embedder_has_focus) {
      GetActiveEmbedder()->Focus();
    }
  } else {
    SidePanelShowOptions side_panel_options{*created_tab};
    side_panel_options.suppress_opening_animation = true;
    auto show_options = ShowOptions{side_panel_options};
    Show(show_options);
  }
  instance_metrics_.OnDaisyChain(DaisyChainSource::kGlicContents,
                                 /*success=*/true, created_tab, source_tab);
  return nullptr;
}

void GlicInstanceImpl::CreateTask(
    base::WeakPtr<actor::ActorTaskDelegate> delegate,
    actor::webui::mojom::TaskOptionsPtr options,
    mojom::WebClientHandler::CreateTaskCallback callback) {
  if (!actor_task_manager_) {
    std::move(callback).Run(
        base::unexpected(mojom::CreateTaskErrorReason::kTaskSystemUnavailable));
    return;
  }
  instance_metrics_.OnCreateTask();
  actor_task_manager_->CreateTask(weak_ptr_factory_.GetWeakPtr(),
                                  std::move(options), std::move(callback));
}

void GlicInstanceImpl::PerformActions(
    const std::vector<uint8_t>& actions_proto,
    mojom::WebClientHandler::PerformActionsCallback callback) {
  CHECK(actor_task_manager_);
  instance_metrics_.OnPerformActions();
  actor_task_manager_->PerformActions(actions_proto, std::move(callback));
}

void GlicInstanceImpl::CancelActions(
    actor::TaskId task_id,
    mojom::WebClientHandler::CancelActionsCallback callback) {
  CHECK(actor_task_manager_);
  actor_task_manager_->CancelActions(task_id, std::move(callback));
}

void GlicInstanceImpl::StopActorTask(actor::TaskId task_id,
                                     mojom::ActorTaskStopReason stop_reason) {
  CHECK(actor_task_manager_);
  instance_metrics_.OnStopActorTask();
  actor_task_manager_->StopActorTask(task_id, stop_reason);
}

void GlicInstanceImpl::PauseActorTask(actor::TaskId task_id,
                                      mojom::ActorTaskPauseReason pause_reason,
                                      tabs::TabInterface::Handle tab_handle) {
  CHECK(actor_task_manager_);
  instance_metrics_.OnPauseActorTask();
  actor_task_manager_->PauseActorTask(task_id, pause_reason, tab_handle);
}

void GlicInstanceImpl::ResumeActorTask(
    actor::TaskId task_id,
    const mojom::GetTabContextOptions& context_options,
    glic::mojom::WebClientHandler::ResumeActorTaskCallback callback) {
  CHECK(actor_task_manager_);
  instance_metrics_.OnResumeActorTask();
  actor_task_manager_->ResumeActorTask(task_id, context_options,
                                       std::move(callback));
}

void GlicInstanceImpl::InterruptActorTask(actor::TaskId task_id) {
  CHECK(actor_task_manager_);
  instance_metrics_.InterruptActorTask();
  actor_task_manager_->InterruptActorTask(task_id);
}

void GlicInstanceImpl::UninterruptActorTask(actor::TaskId task_id) {
  CHECK(actor_task_manager_);
  instance_metrics_.UninterruptActorTask();
  actor_task_manager_->UninterruptActorTask(task_id);
}

void GlicInstanceImpl::CreateActorTab(
    actor::TaskId task_id,
    bool open_in_background,
    const std::optional<int32_t>& initiator_tab_id,
    const std::optional<int32_t>& initiator_window_id,
    glic::mojom::WebClientHandler::CreateActorTabCallback callback) {
  CHECK(actor_task_manager_);
  actor_task_manager_->CreateActorTab(task_id, open_in_background,
                                      initiator_tab_id, initiator_window_id,
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
  if (contextual_cueing_service && active_web_contents &&
      GlicEnabling::HasConsentedForProfile(profile_)) {
    contextual_cueing_service->PrepareToFetchContextualGlicZeroStateSuggestions(
        active_web_contents);
  }
}

void GlicInstanceImpl::OnInteractionModeChange(mojom::WebClientMode new_mode) {
  interaction_mode_ = new_mode;
  sharing_manager_coordinator_.UpdateState(GetPanelState().kind,
                                           interaction_mode_);
  ContextAccessIndicatorChanged(host().IsContextAccessIndicatorEnabled());
}

void GlicInstanceImpl::AddStateObserver(PanelStateObserver* observer) {
  state_observers_.AddObserver(observer);
}

void GlicInstanceImpl::RemoveStateObserver(PanelStateObserver* observer) {
  state_observers_.RemoveObserver(observer);
}

void GlicInstanceImpl::UnbindEmbedder(EmbedderKey key) {
  instance_metrics_.OnUnbindEmbedder(key);
  if (auto** tab = std::get_if<tabs::TabInterface*>(&key)) {
    auto tab_handle = (*tab)->GetHandle();
    std::optional<GlicPinnedTabUsage> usage =
        sharing_manager().GetPinnedTabUsage(tab_handle);
    // If the conversation hasn't had a turn since the tab was pinned and the
    // tab was not manually pinned, then we will unpin the tab.
    if (base::FeatureList::IsEnabled(kGlicUnpinOnUnbindIfUnused) && usage &&
        usage->pin_event.trigger != GlicPinTrigger::kRestore &&
        usage->Unused() && !usage->IsExplicitlyPinnedByUser()) {
      sharing_manager().UnpinTabs({tab_handle});
    }

    if (auto* helper = GlicInstanceHelper::From(*tab)) {
      helper->SetBoundInstance(nullptr);
    }
  }

  Close(key);
  // Deactivate if this was the active embedder. This ensures predictable state
  // for the other embedders and also cleans up the host delegate reference to
  // avoid a dangling raw_ptr.
  MaybeDeactivateEmbedder(key);
  embedders_.erase(key);

  UpdateFloatingPanelCanAttach();

  // Remove the instance if all embedders are gone.
  if (embedders_.empty() && coordinator_delegate_) {
    // This call will delete `this`.
    coordinator_delegate_->RemoveInstance(this);
  }
}

Host& GlicInstanceImpl::host() {
  return host_;
}

const InstanceId& GlicInstanceImpl::id() const {
  return id_;
}

void GlicInstanceImpl::SetIdForRestoration(InstanceId id) {
  id_ = id;
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
  if (!conversation_info_->conversation_id.empty()) {
    return conversation_info_->conversation_id;
  }
  return std::nullopt;
}

glic::mojom::ConversationInfoPtr GlicInstanceImpl::GetConversationInfo() const {
  return conversation_info_->Clone();
}

// Automatic activation should be suppressed if a floating embedder is active.
// The floating UI is a more deliberate user choice, and we don't want a
// tab switch to unexpectedly close the floating UI.
bool GlicInstanceImpl::ShouldDoAutomaticActivation() const {
  return !active_embedder_key_.has_value() ||
         !std::holds_alternative<FloatingEmbedderKey>(
             active_embedder_key_.value());
}

void GlicInstanceImpl::OnBrowserActivated(BrowserWindowInterface* browser) {
  if (!ShouldDoAutomaticActivation()) {
    return;
  }
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser)->GetActiveTab();
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

  EmbedderKey key = active_embedder_key_.value();
  // If SidePanel has focus when it's being closed, focus tab's webcontents.
  if (old_embedder->HasFocus() &&
      std::holds_alternative<tabs::TabInterface*>(key)) {
    auto* tab = std::get<tabs::TabInterface*>(key);
    if (auto* web_contents = (tab ? tab->GetContents() : nullptr)) {
      web_contents->Focus();
    }
  }

  auto it = embedders_.find(key);
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
                       return CreateActiveEmbedderForSidePanel(opts);
                     },
                     [&](const FloatingShowOptions& opts) {
                       return CreateActiveEmbedderForFloaty(opts.initial_bounds,
                                                            opts.source_tab);
                     }},
      options.embedder_options);
}

GlicUiEmbedder* GlicInstanceImpl::CreateActiveEmbedderForSidePanel(
    const SidePanelShowOptions& options) {
  auto& entry =
      BindTab(&options.tab.get(), options.pin_trigger, options.pin_on_bind);
  entry.embedder = std::make_unique<GlicSidePanelUi>(
      profile_, options.tab->GetWeakPtr(), *this, instance_metrics_);
  return entry.embedder.get();
}

GlicUiEmbedder* GlicInstanceImpl::CreateActiveEmbedderForFloaty(
    const gfx::Rect& initial_bounds,
    tabs::TabInterface::Handle source_tab) {
  if (coordinator_delegate_) {
    coordinator_delegate_->OnWillCreateFloaty();
  }
  EmbedderKey key = FloatingEmbedderKey();
  auto [entry_iter, _] = embedders_.try_emplace(key);
  entry_iter->second.embedder = std::make_unique<GlicFloatingUi>(
      profile_, initial_bounds, source_tab, *this, instance_metrics_);
  return entry_iter->second.embedder.get();
}

void GlicInstanceImpl::ShowInactiveSidePanelEmbedderFor(
    const SidePanelShowOptions& options) {
  auto& entry =
      BindTab(&options.tab.get(), options.pin_trigger, options.pin_on_bind);
  entry.embedder = GlicInactiveSidePanelUi::CreateForBackgroundTab(
      options.tab.get().GetWeakPtr(), *this);
}

void GlicInstanceImpl::SetActiveEmbedderAndNotifyStateChange(
    std::optional<EmbedderKey> new_key) {
  active_embedder_key_ = new_key;
  sharing_manager_coordinator_.UpdateState(GetPanelState().kind,
                                           interaction_mode_);
  NotifyStateChange();
  NotifyPanelStateChanged();
}

void GlicInstanceImpl::ClearActiveEmbedderAndNotifyStateChange() {
  if (active_embedder_key_.has_value()) {
    active_embedder_key_.reset();
    NotifyStateChange();
    NotifyPanelStateChanged();
    host().PanelWasClosed();
#if !BUILDFLAG(IS_ANDROID)
    MaybeShowShortcutToastPromo();
    MaybeShowShortcutSnoozePromo();
#endif  // !BUILDFLAG(IS_ANDROID)
  }
  return;
}

void GlicInstanceImpl::MaybeShowShortcutToastPromo() {
  if (!g_browser_process->local_state()->GetBoolean(
          prefs::kGlicLauncherEnabled)) {
    // Hotkey might not be registered, skip the promo.
    return;
  }
// TODO(b/483455896): implement hotkey promo for android
#if !BUILDFLAG(IS_ANDROID)
  Browser* browser = chrome::FindTabbedBrowser(profile_, false);
  if (!browser) {
    // If there is no browser window open for the profile, skip the promo.
    return;
  }

  user_education::FeaturePromoParams params(
      feature_engagement::
          kIPHGlicTrustFirstOnboardingShortcutToastPromoFeature);
  params.body_params = l10n_util::GetStringFUTF16(
      IDS_GLIC_SHORTCUT_IPH_TEXT,
      glic::GlicLauncherConfiguration::GetGlobalHotkey().GetShortcutText());

  BrowserUserEducationInterface::From(browser)->MaybeShowFeaturePromo(
      std::move(params));
#endif
}

void GlicInstanceImpl::MaybeShowShortcutSnoozePromo() {
  if (!g_browser_process->local_state()->GetBoolean(
          prefs::kGlicLauncherEnabled)) {
    // Hotkey might not be registered, skip the promo.
    return;
  }

  // TODO(b/483455896): implement hotkey promo for android.
#if !BUILDFLAG(IS_ANDROID)
  Browser* browser = chrome::FindTabbedBrowser(profile_, false);
  if (!browser) {
    // If there is no browser window open for the profile, skip the promo.
    return;
  }

  user_education::FeaturePromoParams params(
      feature_engagement::
          kIPHGlicTrustFirstOnboardingShortcutSnoozePromoFeature);
  params.body_params = l10n_util::GetStringFUTF16(
      IDS_GLIC_SHORTCUT_IPH_TEXT,
      glic::GlicLauncherConfiguration::GetGlobalHotkey().GetShortcutText());

  BrowserUserEducationInterface::From(browser)->MaybeShowFeaturePromo(
      std::move(params));
#endif
}

void GlicInstanceImpl::UpdateFloatingPanelCanAttach() {
  host().FloatingPanelCanAttachChanged(embedders_.size() > 1);
}

void GlicInstanceImpl::MaybeShowHostUi(
    GlicUiEmbedder* embedder,
    mojom::InvocationSource invocation_source,
    std::optional<std::string> prompt_suggestion,
    bool auto_send) {
  Host::EmbedderDelegate* delegate = embedder->GetHostEmbedderDelegate();
  if (!delegate) {
    return;
  }

  host_.SetDelegate(delegate);
  host_.webui_contents()->UpdateWebContentsVisibility(
      content::Visibility::VISIBLE);
  host_.NotifyWindowIntentToShow();

  NotifyPanelWillOpen(invocation_source, prompt_suggestion, auto_send);
}

void GlicInstanceImpl::OnBoundTabDestroyed(tabs::TabInterface* tab) {
  instance_metrics_.OnBoundTabDestroyed();
  // This call may delete `this`.
  UnbindEmbedder(tab);
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
  instance_metrics_.OnSwitchFromConversation(options, active_embedder_key_);

  if (coordinator_delegate_) {
    coordinator_delegate_->SwitchConversation(*this, options, std::move(info),
                                              std::move(callback));
  } else {
    std::move(callback).Run(mojom::SwitchConversationErrorReason::kUnknown);
  }
}

bool GlicInstanceImpl::IsActiveEmbedder(EmbedderKey key) const {
  return active_embedder_key_.has_value() &&
         active_embedder_key_.value() == key;
}

void GlicInstanceImpl::MaybeDeactivateEmbedder(EmbedderKey key) {
  if (IsActiveEmbedder(key)) {
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

bool GlicInstanceImpl::ShouldPinOnBind() const {
  PrefService* user_prefs = profile_->GetPrefs();
  // Check both the setting exists, the pin-on-bind kill switch (*PinOnBind
  // feature flag) is enabled, and then that actual value of the settings prefs
  // to determine whether to pin on bind.
  if (base::FeatureList::IsEnabled(features::kGlicDefaultTabContextSetting)) {
    return base::FeatureList::IsEnabled(
               features::kGlicDefaultContextPinOnBind) &&
           user_prefs &&
           user_prefs->GetBoolean(prefs::kGlicDefaultTabContextEnabled);
  } else {
    // If kGlicDefaultTabContextSetting is not enabled (old settings page) keep
    // the old behavior.
    return true;
  }
}

GlicInstanceImpl::EmbedderEntry& GlicInstanceImpl::BindTab(
    tabs::TabInterface* tab,
    GlicPinTrigger pin_trigger,
    bool pin_on_bind) {
  EmbedderKey key = CreateSidePanelEmbedderKey(tab);
  auto [it, inserted] = embedders_.try_emplace(key);

  if (!inserted) {
    return it->second;
  }

  if (coordinator_delegate_) {
    coordinator_delegate_->UnbindTabFromAnyInstance(tab);
  }

  instance_metrics_.OnBind();

  EmbedderEntry& new_entry = it->second;
  auto* helper = GlicInstanceHelper::From(tab);
  CHECK(helper);
  helper->SetBoundInstance(this);
  new_entry.destruction_subscription = helper->SubscribeToDestruction(
      base::BindRepeating(&GlicInstanceImpl::OnBoundTabDestroyed,
                          weak_ptr_factory_.GetWeakPtr()));
  new_entry.tab_activation_subscription = tab->RegisterDidActivate(
      base::BindRepeating(&GlicInstanceImpl::OnBoundTabActivated,
                          weak_ptr_factory_.GetWeakPtr()));
  if (!base::FeatureList::IsEnabled(features::kGlicDaisyChainViaCoordinator)) {
    new_entry.tab_web_contents_observer =
        std::make_unique<GlicTabContentsObserver>(tab->GetContents(), this);
  }

  if (pin_on_bind && ShouldPinOnBind()) {
    // Auto-pin on bind.
    sharing_manager().PinTabs({tab->GetHandle()}, pin_trigger);
  }

  UpdateFloatingPanelCanAttach();

  return new_entry;
}

void GlicInstanceImpl::BindTabWithoutShowing(tabs::TabInterface* tab,
                                             bool pin_on_bind) {
  BindTab(tab, GlicPinTrigger::kUnknown, pin_on_bind);
}

void GlicInstanceImpl::WillCloseFor(EmbedderKey key) {
  MaybeDeactivateEmbedder(key);
}

void GlicInstanceImpl::ClientReadyToShow(
    const mojom::OpenPanelInfo& open_info) {
  if (auto* embedder = GetActiveEmbedder()) {
    embedder->OnClientReady();
  }
}

void GlicInstanceImpl::WebUiStateChanged(mojom::WebUiState state) {
  instance_metrics_.OnWebUiStateChanged(state);
  if (state == mojom::WebUiState::kReady &&
      !base::FeatureList::IsEnabled(kSuppressFocusOnReady)) {
    if (auto* embedder = GetActiveEmbedder()) {
      embedder->Focus();
    }
  }
}

void GlicInstanceImpl::ContextAccessIndicatorChanged(bool enabled) {
  if (coordinator_delegate_) {
    coordinator_delegate_->ContextAccessIndicatorChanged(*this, enabled);
  }
}

void GlicInstanceImpl::OnEmbedderWindowActivationChanged(bool has_focus) {
  NotifyInstanceActivationChanged(has_focus);
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
      if (entry.embedder && entry.embedder->IsShowing() &&
          (*tab)->IsActivated()) {
        Show(ShowOptions::ForSidePanel(**tab));
        return;
      }
    }
  }

  OnAllEmbeddersInactive();
}

void GlicInstanceImpl::OnAllEmbeddersInactive() {
  NotifyInstanceActivationChanged(false);
  if (actor_task_manager_) {
    // Attempt to show toast on UI deactivated (and not replaced by anything
    // else).
    actor_task_manager_->MaybeShowDeactivationToastUi();
  }
  // This call might delete `this`.
  remove_blank_instance_timer_.Start(
      FROM_HERE, kRemoveBlankInstanceDelay.Get(), this,
      &GlicInstanceImpl::MaybeRemoveBlankInstanceOnClose);
}

void GlicInstanceImpl::MaybeRemoveBlankInstanceOnClose() {
  if (!base::FeatureList::IsEnabled(kGlicRemoveBlankInstancesOnClose)) {
    return;
  }
  // If the conversation id is set, then the instance isn't blank.
  if (conversation_id().has_value()) {
    return;
  }
  if (embedders_.size() != 1) {
    return;
  }

  const auto& [key, entry] = *embedders_.begin();
  tabs::TabInterface* const* tab = std::get_if<tabs::TabInterface*>(&key);
  if (!tab) {
    // The single embedder is not a side panel (e.g., it's a floating embedder).
    return;
  }

  GlicSidePanelCoordinator* coordinator =
      GlicSidePanelCoordinator::GetForTab(*tab);
  if (!coordinator) {
    return;
  }
  // Only delete the instance if the side panel is actually closed, as opposed
  // to just being on a backgrounded tab.
  if (coordinator->state() != GlicSidePanelCoordinator::State::kClosed) {
    return;
  }

  // Only remove the instance if there are no pinned tabs, or if the only pinned
  // tab is the one for this embedder.
  if (sharing_manager().GetNumPinnedTabs() > 1) {
    return;
  }
  if (sharing_manager().GetNumPinnedTabs() == 1 &&
      !sharing_manager().IsTabPinned((*tab)->GetHandle())) {
    return;
  }

  // This call will delete `this`.
  UnbindEmbedder(*tab);
}

void GlicInstanceImpl::NotifyInstanceActivationChanged(bool is_active) {
  is_active_ = is_active;
  instance_metrics_.OnActivationChanged(is_active);
  if (is_active) {
    last_activation_timestamp_ = base::Time::Now();
    inactivity_timer_.Stop();
    remove_blank_instance_timer_.Stop();
  } else {
    last_deactivation_timestamp_ = base::TimeTicks::Now();
    inactivity_timer_.Start(
        FROM_HERE, base::Hours(23),
        base::BindOnce(&GlicInstanceImpl::Hibernate, base::Unretained(this)));
  }

  sharing_manager_coordinator_.OnGlicWindowActivationChanged(is_active &&
                                                             IsDetached());
  if (coordinator_delegate_) {
    coordinator_delegate_->OnInstanceActivationChanged(this, is_active);
  }
  host_.NotifyInstanceActivationChanged(is_active);
}

bool GlicInstanceImpl::IsActive() {
  return is_active_;
}

base::Time GlicInstanceImpl::GetLastActivationTimestamp() const {
  return last_activation_timestamp_;
}

base::TimeDelta GlicInstanceImpl::GetTimeSinceLastActive() const {
  if (is_active_) {
    return base::TimeDelta();
  }
  return base::TimeTicks::Now() - last_deactivation_timestamp_;
}

bool GlicInstanceImpl::IsHibernated() const {
  return !host_.webui_contents();
}

void GlicInstanceImpl::Hibernate() {
  DeactivateCurrentEmbedder();
  host_.Shutdown();
}

void GlicInstanceImpl::OnTabPinningStatusEvent(tabs::TabInterface* tab,
                                               GlicPinningStatusEvent event) {
  bool pinned = std::holds_alternative<GlicPinEvent>(event);

  auto* helper = GlicInstanceHelper::From(tab);
  if (!helper) {
    return;
  }
  if (!pinned) {
    helper->OnUnpinnedByInstance(this);
    return;
  }
  helper->OnPinnedByInstance(this);

  // If the trigger is kRestore, we should not bind the tab to the instance
  // automatically. This prevents a circular dependency where restoring a
  // pinned tab would automatically bind it, which would then try to pin it
  // again, etc.
  if (std::get<GlicPinEvent>(event).trigger == GlicPinTrigger::kRestore) {
    return;
  }

  auto instance_id = helper->GetInstanceId();
  if (!base::FeatureList::IsEnabled(kGlicAlwaysBindOnPin) &&
      instance_id.has_value()) {
    return;
  }

  // Don't try to rebind/show if the tab is already bound to this instance.
  if (instance_id == id()) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          kGlicBindOnPinFromFloatingUiDoesntShowSidePanel) &&
      IsDetached()) {
    // Bind without showing if floaty is open. We pass in the unknown pin
    // trigger because the tab is already pinned, so we don't expect any
    // pinning to actually happen on bind.
    BindTab(tab, GlicPinTrigger::kUnknown, /*pin_on_bind=*/false);
  } else {
    ShowInactiveSidePanelEmbedderFor(SidePanelShowOptions(*tab));
  }
}

void GlicInstanceImpl::NotifyPanelWillOpen(
    mojom::InvocationSource invocation_source,
    std::optional<std::string> prompt_suggestion,
    bool auto_send) {
  Host::PanelWillOpenOptions options;
  options.conversation_info = GetConversationInfo();
  if (coordinator_delegate_) {
    options.recently_active_conversations =
        coordinator_delegate_->GetRecentlyActiveConversations(
            kMaxRecentConversationsForPanel);
  }
  options.prompt_suggestion = prompt_suggestion;
  options.auto_send = auto_send;
  host_.PanelWillOpen(invocation_source, std::move(options));
}

void GlicInstanceImpl::OnWebClientCleared() {
  if (actor_task_manager_) {
    actor_task_manager_->CancelTask();
  }
  NotifyPanelWillOpen(mojom::InvocationSource::kDefaultValue, std::nullopt,
                      /*auto_send=*/false);
}

void GlicInstanceImpl::CloseAllEmbedders() {
  // Copy the keys before iterating because Close() might modify `embedders_`.
  std::vector<EmbedderKey> keys;
  for (auto& [key, entry] : embedders_) {
    keys.push_back(key);
  }
  for (const auto& key : keys) {
    Close(key);
  }
}

#if !BUILDFLAG(IS_ANDROID)
views::View* GlicInstanceImpl::GetActiveEmbedderGlicViewForTesting() {
  auto* embedder = GetActiveEmbedder();
  if (!embedder) {
    return nullptr;
  }
  return embedder->GetView().get();
}
#endif

void GlicInstanceImpl::OnTabAddedToTask(
    actor::TaskId task_id,
    const tabs::TabInterface::Handle& tab_handle) {
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL: actor not yet ported
  tabs::TabInterface* tab = tab_handle.Get();
  if (!tab || !task_id) {
    instance_metrics_.OnDaisyChain(DaisyChainSource::kActorAddTab,
                                   /*success=*/false);
    return;
  }
  if (base::FeatureList::IsEnabled(features::kGlicGetTabByIdApi)) {
    service_->OnTabAddedToTask(task_id, tab_handle);
  }
  SidePanelShowOptions side_panel_options{*tab};
  side_panel_options.suppress_opening_animation = true;

  if (base::FeatureList::IsEnabled(
          kGlicActorDaisyChainingFromFloatingUiDoesntClose) &&
      IsDetached()) {
    side_panel_options.pin_trigger = GlicPinTrigger::kActuation;
    ShowInactiveSidePanelEmbedderFor(side_panel_options);
  } else {
    Show(ShowOptions{side_panel_options});
  }
  instance_metrics_.OnDaisyChain(DaisyChainSource::kActorAddTab,
                                 /*success=*/true, tab);
#endif
}

void GlicInstanceImpl::RequestToShowCredentialSelectionDialog(
    actor::TaskId task_id,
    const base::flat_map<std::string, gfx::Image>& icons,
    const std::vector<actor_login::Credential>& credentials,
    actor::ActorTaskDelegate::CredentialSelectedCallback callback) {
  host_.RequestToShowCredentialSelectionDialog(task_id, icons, credentials,
                                               std::move(callback));
}

void GlicInstanceImpl::RequestToShowUserConfirmationDialog(
    actor::TaskId task_id,
    const url::Origin& navigation_origin,
    bool for_blocklisted_origin,
    actor::ActorTaskDelegate::UserConfirmationDialogCallback callback) {
  host_.RequestToShowUserConfirmationDialog(
      task_id, navigation_origin, for_blocklisted_origin, std::move(callback));
}

void GlicInstanceImpl::RequestToConfirmNavigation(
    actor::TaskId task_id,
    const url::Origin& navigation_origin,
    actor::ActorTaskDelegate::NavigationConfirmationCallback callback) {
  host_.RequestToConfirmNavigation(task_id, navigation_origin,
                                   std::move(callback));
}

void GlicInstanceImpl::RequestToShowAutofillSuggestionsDialog(
    actor::TaskId task_id,
    std::vector<autofill::ActorFormFillingRequest> requests,
    base::WeakPtr<actor::AutofillSelectionDialogEventHandler> event_handler,
    AutofillSuggestionSelectedCallback callback) {
  host_.RequestToShowAutofillSuggestionsDialog(task_id, std::move(requests),
                                               std::move(event_handler),
                                               std::move(callback));
}

bool GlicInstanceImpl::HasFocus() {
  if (auto* web_contents = host().webui_contents()) {
    content::RenderWidgetHostView* rwhv =
        web_contents->GetRenderWidgetHostView();
    return rwhv->HasFocus();
  }
  return false;
}

tabs::TabInterface* GlicInstanceImpl::GetActiveEmbedderTabForTesting() {
  if (!active_embedder_key_.has_value()) {
    return nullptr;
  }
  if (tabs::TabInterface* const* tab =
          std::get_if<tabs::TabInterface*>(&active_embedder_key_.value())) {
    return *tab;
  }
  return nullptr;
}

std::string GlicInstanceImpl::DescribeForTesting() {
  std::stringstream ss;
  ss << "GlicInstance[" << id_ << "]:\n";
  for (const auto& entry : embedders_) {
    ss << "  Embedder ["
       << DescribeEmbedderKeyForTesting(entry.first)            // IN-TEST
       << "]: " << entry.second.embedder->DescribeForTesting()  // IN-TEST
       << '\n';
  }

  return ss.str();
}

}  // namespace glic
