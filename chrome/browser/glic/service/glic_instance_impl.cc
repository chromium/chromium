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
#include "base/trace_event/named_trigger.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_reporting_controller_factory.h"
#endif
#include "chrome/browser/glic/common/future_browser_features.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_zero_state_suggestions_manager.h"
#include "chrome/browser/glic/host/context/glic_active_pinned_focused_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_empty_focused_browser_manager.h"
#include "chrome/browser/glic/host/context/glic_empty_focused_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_pin_candidate_provider.h"
#include "chrome/browser/glic/host/context/glic_pinned_tab_manager_impl.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_coordinator.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/host/glic_skills_manager_impl.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/glic/service/glic_ui_types.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/metrics/profile_metrics_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
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
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_delegate.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/host/context/glic_empty_pinned_tab_manager.h"
#include "chrome/browser/glic/widget/conversions.h"
#include "chrome/browser/glic/widget/glic_floating_ui_android.h"
#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui_android.h"
#include "chrome/browser/glic/widget/glic_side_panel_ui_android.h"
#else
#include "chrome/browser/glic/host/context/glic_focused_tab_manager.h"
#include "chrome/browser/glic/widget/glic_floating_ui.h"
#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui.h"
#include "chrome/browser/glic/widget/glic_side_panel_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace glic {

BASE_FEATURE(kGlicBindOnlyForDaisyChainingFromFloatingUi,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGlicActorDaisyChainingFromFloatingUiDoesntClose,
             base::FEATURE_ENABLED_BY_DEFAULT);
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

#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
BASE_FEATURE(kGlicUnbindOnClose, base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kGlicUnbindOnClose, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

namespace {

EmbedderKey CreateSidePanelEmbedderKey(tabs::TabInterface* tab) {
  CHECK(tab);
  return EmbedderKey(tab);
}

enterprise_reporting::SaasUsageReportingController*
GetSaasUsageReportingController(Profile* profile) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return enterprise_reporting::SaasUsageReportingControllerFactory::
      GetForProfile(profile);
#else
  return nullptr;
#endif
}
}  // namespace

void GlicInstanceImpl::MaybeDaisyChainToTab(tabs::TabInterface* source_tab,
                                            tabs::TabInterface* target_tab,
                                            DaisyChainSource source) {
  // Ideally we would like to unify this daisy chaining logic with what is in
  // `CreateTab`, but doing so would require a more involved plumbing change in
  // order to play nicely with detached mode.
  if (is_creating_tab_from_glic_panel_link_click_) {
    return;
  }

  auto* glic_embedder = GetEmbedderForTab(source_tab);

  if (base::FeatureList::IsEnabled(kGlicRemoveDaisyChainingWhenFreShowing)) {
    if (!GlicEnabling::HasConsentedForProfile(profile())) {
      return;
    }
  }

  // Only bind if the previous instance was showing or backgrounded.
  if (glic_embedder && glic_embedder->IsShowingOrBackgrounded()) {
    SidePanelShowOptions side_panel_options{*target_tab};
    side_panel_options.suppress_opening_animation = true;
    side_panel_options.pin_trigger = GlicPinTrigger::kDaisyChain;
    side_panel_options.prefer_peek = true;
    auto show_options = ShowOptions{side_panel_options};
    instance_metrics().OnDaisyChain(source,
                                    /*success=*/true, target_tab, source_tab);
    Show(show_options);
  } else {
    // Record the failure.
    instance_metrics().OnDaisyChain(source,
                                    /*success=*/false, target_tab, source_tab);
  }
}

void GlicInstanceImpl::NotifyVisibilityChange() {
  instance_metrics_.OnVisibilityChanged(HasActiveEmbedder());
  if (coordinator_delegate_) {
    coordinator_delegate_->OnInstanceVisibilityChanged(this, IsShowing());
  }
}

void GlicInstanceImpl::NotifyConversationTitleChanged() {
#if BUILDFLAG(IS_ANDROID)
  // Notify bound helpers that the instance info (title) changed.
  for (const auto& [key, entry] : embedders_) {
    if (auto* const* tab_ptr = std::get_if<tabs::TabInterface*>(&key)) {
      if (auto* helper = GlicInstanceHelper::From(*tab_ptr)) {
        helper->OnConversationTitleChanged();
      }
    }
  }
#endif
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
    ContextualCueingService* contextual_cueing_service)
    : profile_(profile),
      service_(GlicKeyedService::Get(profile)),
      coordinator_delegate_(coordinator_delegate),
      id_(instance_id),
      host_(profile_, this, this, this),
      sharing_manager_coordinator_(profile, this, metrics),
      instance_metrics_(ProfileMetricsServiceFactory::GetForProfile(profile),
                        &sharing_manager_coordinator_.GetActiveSharingManager(),
                        GetSaasUsageReportingController(profile),
                        profile->GetPrefs()),
      zero_state_suggestions_manager_(
          std::make_unique<GlicZeroStateSuggestionsManager>(
              &sharing_manager(),
              this,
              contextual_cueing_service)),
      last_activation_timestamp_(base::Time::Now()),
      last_deactivation_timestamp_(base::TimeTicks::Now()) {
  VLOG(1) << "Glic [InstanceImpl] Constructor, id=" << id_.value();
  base::trace_event::EmitNamedTrigger("glic-instance-created");
  TRACE_EVENT_INSTANT("glic", "GlicInstanceImpl::GlicInstanceImpl",
                      perfetto::Flow::FromPointer(this));
  if (auto* actor_keyed_service =
          actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_)) {
    actor_task_manager_ = std::make_unique<GlicActorTaskManager>(
        profile_, actor_keyed_service, service_->actor_policy_checker(),
        &instance_metrics_, &sharing_manager(), this);
  }

  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
  host_.SetDelegate(&empty_embedder_delegate_);
  host_observation_.Observe(&host_);
  if (base::FeatureList::IsEnabled(features::kGlicBindPinnedUnboundTab)) {
    pinned_tabs_change_subscription_ =
        sharing_manager().AddTabPinningStatusEventCallback(
            base::BindRepeating(&GlicInstanceImpl::OnTabPinningStatusEvent,
                                weak_ptr_factory_.GetWeakPtr()));
  }
}

GlicInstanceImpl::~GlicInstanceImpl() {
  VLOG(1) << "Glic [InstanceImpl] Destructor, id=" << id_.value();
  TRACE_EVENT_INSTANT("glic", "GlicInstanceImpl::~GlicInstanceImpl",
                      perfetto::TerminatingFlow::FromPointer(this));
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

  for (tabs::TabInterface* tab : sharing_manager().GetPinnedTabs()) {
    if (auto* helper = GlicInstanceHelper::From(tab)) {
      helper->OnUnpinnedByInstance(this);
    }
  }
}

glic::GlicInstanceMetrics& GlicInstanceImpl::instance_metrics() {
  return instance_metrics_;
}

glic::GlicInstanceMetricsBackwardsCompatibility&
GlicInstanceImpl::instance_metrics_backwards_compatibility() {
  return instance_metrics_;
}

GlicSkillsManager& GlicInstanceImpl::skills_manager() {
  if (!skills_manager_) {
    skills_manager_ = std::make_unique<GlicSkillsManagerImpl>(this, profile_);
  }
  return *skills_manager_;
}

std::unique_ptr<WebUIContentsContainer>
GlicInstanceImpl::CreateWebUIContentsContainer() {
  return coordinator_delegate_->CreateWebUIContentsContainer();
}

bool GlicInstanceImpl::IsShowing() const {
  if (HasActiveEmbedder()) {
    return true;
  }
  for (const auto& [key, entry] : embedders_) {
    if (entry.embedder && entry.embedder->IsShowing()) {
      return true;
    }
  }
  return false;
}

bool GlicInstanceImpl::HasActiveEmbedder() const {
  return active_embedder_key_.has_value();
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

bool GlicInstanceImpl::ShouldShowInactiveSidePanel(
    const SidePanelShowOptions& options) const {
  if (!options.tab->IsActivated()) {
    return true;
  }

  if (!options.prefer_peek) {
    return false;
  }

  EmbedderKey key = CreateSidePanelEmbedderKey(&options.tab.get());
  if (IsActiveEmbedder(key)) {
    return false;
  }

  auto* coordinator = GlicSidePanelCoordinator::GetForTab(&options.tab.get());
  return coordinator && coordinator->SupportsPeek();
}

void GlicInstanceImpl::Show(const ShowOptions& options) {
  VLOG(1) << "Glic [InstanceImpl] Show, id=" << id_.value();

  TRACE_EVENT("glic", "GlicInstanceImpl::Show",
              perfetto::Flow::FromPointer(this));

  if (const auto* side_panel_options =
          std::get_if<SidePanelShowOptions>(&options.embedder_options);
      side_panel_options) {
    if (ShouldShowInactiveSidePanel(*side_panel_options)) {
      ShowInactiveSidePanelEmbedderFor(*side_panel_options);
      return;
    }
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
    host_.CreateContents();
    embedder_to_show = CreateActiveEmbedder(options);
    CHECK(embedder_to_show);
    host_.SetDelegate(embedder_to_show->GetHostEmbedderDelegate());
    SetActiveEmbedderAndNotifyVisibilityChange(new_key);
  }

  MaybeWarmZeroStateSuggestions();

  MaybeShowHostUi(embedder_to_show, options.invocation_source,
                  options.prompt_suggestion, options.fre_override);

  if (instance_metrics().MarkShownAndCheckIfFirstTime(new_key)) {
    instance_metrics().OnOpen(options.invocation_source, options);
    service_->metrics()->OnGlicWindowStartedOpening(/*attached=*/false,
                                                    options.invocation_source);
  }

  embedder_to_show->Show(options);
  // WARNING: Show() may result in deleting the embedder! Check if the embedder
  // is still active before calling Focus().
  if (options.focus_on_show && IsActiveEmbedder(new_key)) {
    if (auto* active_embedder = GetActiveEmbedder()) {
      active_embedder->Focus();
    }
  }
}

void GlicInstanceImpl::Detach(tabs::TabInterface& tab) {
  CHECK(GlicEnabling::IsLiveAndFloatyEnabledByFlags())
      << "Detach called when floaty is disabled by flags.";
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
  VLOG(1) << "Glic [InstanceImpl] Close, id=" << id_.value();
  auto* entry = GetEmbedderEntry(key);
  if (!entry || !entry->embedder) {
    return;
  }

  // May delete this.
  CloseInternal(key, *entry, options);
}

void GlicInstanceImpl::CloseInternal(EmbedderKey key,
                                     EmbedderEntry& entry,
                                     const CloseOptions& options) {
  service_->metrics()->OnInstanceClosed();
  instance_metrics_.OnClose();
  instance_metrics_.ResetShownState(key);
  if (entry.embedder) {
    // May delete this.
    entry.embedder->Close(options);
  }
}

bool GlicInstanceImpl::ShouldUnbindOnClose(EmbedderKey key,
                                           const EmbedderEntry& entry) {
  // Determines whether the instance should be unbound from the embedder when
  // closed. Unbinding occurs only when all of the following conditions are met:
  // - Both `kGlicUnbindOnClose` and `kGlicDefaultToLastActiveConversation`
  //   flags are enabled.
  // - The user has not submitted input (ie. sent a prompt) while the tab was
  //   bound.
  // - The instance is scoped to a tab (not a floating panel or window).
  // - The tab was pinned as a result of clicking an entrypoint (e.g., clicking
  //   the entrypoint), rather than being pinned via one of the other mechanisms
  //   (eg. actuation, daisy chaining, explicit pinning, etc.)
  if (!base::FeatureList::IsEnabled(kGlicUnbindOnClose)) {
    return false;
  }
  if (!base::FeatureList::IsEnabled(
          features::kGlicDefaultToLastActiveConversation)) {
    return false;
  }
  const auto* tab_key = std::get_if<tabs::TabInterface*>(&key);
  if (!tab_key) {
    return false;
  }
  auto usage = sharing_manager().GetPinnedTabUsage((**tab_key).GetHandle());
  // This is the pin trigger used for entrypoint clicks.
  // TODO(b/501090068): Figure out how to separate this from invoke pin
  // triggers.
  return usage &&
         (usage->pin_event.trigger == GlicPinTrigger::kInstanceCreation &&
          !entry.user_input_submitted_while_bound);
}

bool GlicInstanceImpl::Toggle(ShowOptions&& options,
                              bool prevent_close,
                              glic::mojom::InvocationSource source) {
  VLOG(1) << "Glic [InstanceImpl] Toggle, id=" << id_.value();
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
  if (auto* entry = GetEmbedderEntry(key)) {
    return entry->embedder.get();
  }
  return nullptr;
}

GlicInstanceImpl::EmbedderEntry* GlicInstanceImpl::GetEmbedderEntry(
    EmbedderKey key) {
  auto it = embedders_.find(key);
  if (it != embedders_.end()) {
    return &it->second;
  }
  return nullptr;
}

GlicSharingManager& GlicInstanceImpl::sharing_manager() {
  return sharing_manager_coordinator_.GetActiveSharingManager();
}

GlicPinCandidateProvider& GlicInstanceImpl::pin_candidate_provider() {
  return sharing_manager_coordinator_.pin_candidate_provider();
}

void GlicInstanceImpl::CloseInstanceAndShutdown() {
  VLOG(1) << "Glic [InstanceImpl] CloseInstanceAndShutdown, id=" << id_.value();
  will_be_destroyed_callbacks_.Notify(this);

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
  NotifyConversationTitleChanged();
  conversation_info_changed_callback_list_.Notify(*conversation_info_);

  std::move(callback).Run(std::nullopt);
}

void GlicInstanceImpl::CreateTab(
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

  bool is_onboarding = !GlicEnabling::HasConsentedForProfile(profile_);

  base::AutoReset<bool> auto_reset(&is_creating_tab_from_glic_panel_link_click_,
                                   true);
  tabs::TabInterface* created_tab = service_->CreateTab(
      url, open_in_background || is_onboarding, window_id, std::move(callback));

  // Prevent links clicked inside the side panel during the onboarding from
  // being daisy chained.
  if (is_onboarding) {
    return;
  }

  // TODO(b/501276046): Figure out how to ensure that instance helper is
  // initialized when we get to this point.
  if (!created_tab || !GlicInstanceHelper::From(created_tab)) {
    instance_metrics_.OnDaisyChain(DaisyChainSource::kGlicContents,
                                   /*success=*/false, nullptr, source_tab);
    return;
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
    side_panel_options.prefer_peek = true;
    auto show_options = ShowOptions{side_panel_options};
    Show(show_options);
  }
  instance_metrics_.OnDaisyChain(DaisyChainSource::kGlicContents,
                                 /*success=*/true, created_tab, source_tab);
}

void GlicInstanceImpl::CreateActorHandler(
    mojo::PendingReceiver<mojom::ActorHandler> receiver,
    mojo::PendingRemote<mojom::ActorClient> client) {
  if (actor_task_manager_) {
    actor_task_manager_->Bind(std::move(receiver), std::move(client));
  }
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
  // TODO(crbug.com/444463509): Update this when we have per-instance
  // sharing managers set up without auto-focus.
  auto* active_web_contents =
      sharing_manager().GetFocusedTabData().focus()
          ? sharing_manager().GetFocusedTabData().focus()->GetContents()
          : nullptr;
  ContextualCueingService* contextual_cueing_service =
      ContextualCueingServiceFactory::GetForProfile(profile_);
  if (contextual_cueing_service && active_web_contents &&
      GlicEnabling::HasConsentedForProfile(profile_)) {
    contextual_cueing_service->PrepareToFetchContextualGlicZeroStateSuggestions(
        active_web_contents);
  }
}

void GlicInstanceImpl::OnUserInputSubmitted(mojom::WebClientMode mode) {
  for (auto& [key, entry] : embedders_) {
    entry.user_input_submitted_while_bound = true;
  }
  last_prompt_submission_time_ = base::TimeTicks::Now();
  // TODO(harringtond): The only subscriber to this event is the tab underline
  // controller and I think it makes more sense for it to get that signal from
  // sharing manager instead of going through the keyed service.
  service_->OnUserInputSubmitted(mode);
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

  if (auto* entry = GetEmbedderEntry(key)) {
    base::WeakPtr<GlicInstanceImpl> weak_this = weak_ptr_factory_.GetWeakPtr();
    CloseInternal(key, *entry, {.suppress_animations = true});
    if (!weak_this) {
      return;
    }
  }

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

void GlicInstanceImpl::SendAdditionalContext(
    mojom::AdditionalContextPtr context) {
  host_.NotifyAdditionalContext(std::move(context));
}

void GlicInstanceImpl::FocusIfActive() {
  if (!IsActive()) {
    return;
  }
  content::WebContents* web_contents = host_.webui_contents();
  if (!web_contents) {
    return;
  }
  web_contents->Focus();
}

void GlicInstanceImpl::NotifyActorTaskListRowClicked(int32_t task_id) {
  host_.NotifyActorTaskListRowClicked(task_id);
}

void GlicInstanceImpl::GetExperimentalTriggeringUpdates(
    mojo::PendingRemote<mojom::ExperimentalTriggeringUpdatesHandler> handler,
    base::OnceCallback<void(bool)> success_status_callback) {
  host_.GetExperimentalTriggeringUpdates(std::move(handler),
                                         std::move(success_status_callback));
}

const InstanceId& GlicInstanceImpl::id() const {
  return id_;
}

void GlicInstanceImpl::SetIdForRestoration(InstanceId id) {
  id_ = id;
}

base::CallbackListSubscription GlicInstanceImpl::RegisterWillBeDestroyed(
    DestructionCallback callback) {
  return will_be_destroyed_callbacks_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicInstanceImpl::AddConversationInfoChangedCallback(
    base::RepeatingCallback<void(const mojom::ConversationInfo&)> callback) {
  return conversation_info_changed_callback_list_.Add(std::move(callback));
}

void GlicInstanceImpl::CancelTask() {
  if (actor_task_manager_) {
    actor_task_manager_->CancelTask();
  }
}

GlicActorTaskManager* GlicInstanceImpl::GetActorTaskManager() {
  return actor_task_manager_.get();
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

  ContextualCueingService* contextual_cueing_service =
      ContextualCueingServiceFactory::GetForProfile(profile_);

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

std::string GlicInstanceImpl::conversation_title() const {
  return conversation_info_->conversation_title;
}

std::vector<tabs::TabInterface*> GlicInstanceImpl::GetBoundTabs() const {
  std::vector<tabs::TabInterface*> tabs;
  for (const auto& [key, entry] : embedders_) {
    if (tabs::TabInterface* const* tab =
            std::get_if<tabs::TabInterface*>(&key)) {
      tabs.push_back(*tab);
    }
  }
  return tabs;
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
    SidePanelShowOptions side_panel_options{*active_tab};
    if (auto* coordinator = GlicSidePanelCoordinator::GetForTab(active_tab)) {
      side_panel_options.prefer_peek =
          coordinator->state() == GlicSidePanelCoordinator::State::kPeek;
    }
    Show(ShowOptions{side_panel_options});
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
    ClearActiveEmbedderAndNotifyVisibilityChange();
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
  ClearActiveEmbedderAndNotifyVisibilityChange();

  if (it->second.embedder) {
    it->second.embedder->InitializeAfterRegistration();
  }

  // Special case: call back to DidCloseFor if the embedder was closed by
  // deletion (eg. floating embedder).
  if (!it->second.embedder) {
    DidCloseFor(key, EmbedderCloseReason::kExplicitlyClosed);
  }
}

GlicUiEmbedder* GlicInstanceImpl::CreateActiveEmbedder(
    const ShowOptions& options) {
  return std::visit(
      absl::Overload{
          [&](const SidePanelShowOptions& opts) {
            return CreateActiveEmbedderForSidePanel(opts);
          },
          [&](const FloatingShowOptions& opts) {
            CHECK(base::FeatureList::IsEnabled(features::kGlicLiveMode));
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
  CHECK(GlicEnabling::IsLiveAndFloatyEnabledByFlags());
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
  CHECK(!IsActiveEmbedder(CreateSidePanelEmbedderKey(&options.tab.get())))
      << "ShowInactiveSidePanelEmbedderFor called for active embedder. "
         "Converting an active embedder to an inactive one needs to be done "
         "with DeactivateCurrentEmbedder.";
  auto& entry =
      BindTab(&options.tab.get(), options.pin_trigger, options.pin_on_bind);
  entry.embedder = GlicInactiveSidePanelUi::CreateForBackgroundTab(
      options.tab.get().GetWeakPtr(), *this);
  entry.embedder->Show(ShowOptions(options));
}

void GlicInstanceImpl::SetActiveEmbedderAndNotifyVisibilityChange(
    std::optional<EmbedderKey> new_key) {
  maybe_activate_foreground_embedder_timer_.Stop();
  active_embedder_key_ = new_key;
  sharing_manager_coordinator_.UpdateState(GetPanelState().kind,
                                           interaction_mode_);
  NotifyVisibilityChange();
  NotifyPanelStateChanged();
}

void GlicInstanceImpl::ClearActiveEmbedderAndNotifyVisibilityChange() {
  if (active_embedder_key_.has_value()) {
    active_embedder_key_.reset();
    NotifyVisibilityChange();
    NotifyPanelStateChanged();
    host().PanelWasClosed();
#if !BUILDFLAG(IS_ANDROID)
    MaybeShowShortcutSnoozePromo();
#endif  // !BUILDFLAG(IS_ANDROID)
  }
  return;
}

void GlicInstanceImpl::MaybeShowShortcutSnoozePromo() {
  if (!g_browser_process->local_state()->GetBoolean(
          prefs::kGlicLauncherEnabled)) {
    // Hotkey might not be registered, skip the promo.
    return;
  }

  // TODO(b/483455896): implement hotkey promo for android.
#if !BUILDFLAG(IS_ANDROID)
  BrowserWindowInterface* browser =
      ProfileBrowserCollection::GetForProfile(profile_)->FindTabbedBrowser();
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
    mojom::FreOverride fre_override) {
  Host::EmbedderDelegate* delegate = embedder->GetHostEmbedderDelegate();
  if (!delegate) {
    return;
  }
  VLOG(2) << "Glic [InstanceImpl] MaybeShowHostUi, id=" << id_.value();

  host_.SetDelegate(delegate);
  host_.SetWebContentsVisibility(content::Visibility::VISIBLE);
  host_.NotifyWindowIntentToShow();

  NotifyPanelWillOpen(invocation_source, prompt_suggestion, fre_override);
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
  if (embedder && embedder->IsShowingOrBackgrounded()) {
    // Ensure that the side panel in this tab becomes the active embedder.
    SidePanelShowOptions side_panel_options{*tab};
    side_panel_options.prefer_peek = true;
    Show(ShowOptions{side_panel_options});
  }
}

void GlicInstanceImpl::SwitchConversation(
    const ShowOptions& options,
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::SwitchConversationCallback callback) {
  instance_metrics_.OnSwitchFromConversation(options, active_embedder_key_);

  // For Floaty: reset the flag, so the conversation switch logs OnOpen when
  // switching from default Floaty state to a new conversation. This is an edge
  // case where Floaty embedder is reused without being closed.
  if (active_embedder_key_ &&
      std::holds_alternative<FloatingEmbedderKey>(*active_embedder_key_)) {
    instance_metrics_.ResetShownState(*active_embedder_key_);
  }

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
    // Start a timer to maybe activate another embedder. This is to avoid
    // a race condition where the deactivation of an old embedder (e.g. during a
    // tab/window switch) tries to show the new embedder before the browser's
    // own tab activation logic has had a chance to run. We allow the
    // synchronous activation logic to complete, and then this timer will fire.
    // The timer is canceled if an embedder becomes active before it fires.
    // TODO(crbug.com/451667367): Find another way to do this that doesn't
    // require a timer. Spoiler alert, it might not be possible.
    maybe_activate_foreground_embedder_timer_.Start(
        FROM_HERE, base::Milliseconds(30), this,
        &GlicInstanceImpl::MaybeActivateForegroundEmbedder);
  }
}

void GlicInstanceImpl::MaybeWarmZeroStateSuggestions() {
  if (conversation_id() ||
      !GlicEnabling::IsEnabledAndConsentForProfile(profile_) ||
      !IsZeroStateSuggestionsEnabled()) {
    return;
  }

  // Warm ZSS to reduce latency. But only do it for new conversations.
  // Conversations with an ID won't have ZSS.
  FetchZeroStateSuggestions(/*is_first_run=*/false, std::nullopt,
                            base::DoNothing());
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

  if (pin_on_bind && ShouldPinOnBind()) {
    // Auto-pin on bind.
    sharing_manager().PinTabs({tab->GetHandle()}, pin_trigger);
  }

  UpdateFloatingPanelCanAttach();

  return new_entry;
}

void GlicInstanceImpl::BindTabWithoutShowing(tabs::TabInterface* tab,
                                             GlicPinTrigger pin_trigger,
                                             bool pin_on_bind) {
  BindTab(tab, pin_trigger, pin_on_bind);
}

void GlicInstanceImpl::SuppressShowOnNextTabAddedToTask(bool suppress) {
  suppress_show_on_tab_added_to_task_ = suppress;
}

void GlicInstanceImpl::MaybeInitializeHiddenClient(
    mojom::InvocationSource invocation_source,
    mojom::FreOverride fre_override) {
  if (!host_.webui_contents()) {
    host_.SetDelegate(&empty_embedder_delegate_);
    host_.CreateContents();
  }

  NotifyPanelWillOpen(invocation_source, std::nullopt, fre_override);
}

void GlicInstanceImpl::DidCloseFor(EmbedderKey key,
                                   EmbedderCloseReason reason) {
  // Must be called early to avoid use-after-free if instance is deleted.
  if (reason == EmbedderCloseReason::kExplicitlyClosed) {
    instance_metrics().ResetShownState(key);
  }

  // Deactivation might delete 'this'
  MaybeDeactivateEmbedder(key);

  auto* entry = GetEmbedderEntry(key);
  if (reason == EmbedderCloseReason::kExplicitlyClosed && entry &&
      ShouldUnbindOnClose(key, *entry)) {
    // Unbind might delete 'this'
    UnbindEmbedder(key);
  }
}

void GlicInstanceImpl::ClientReadyToShow(
    const mojom::OpenPanelInfo& open_info) {
  if (auto* embedder = GetActiveEmbedder()) {
    embedder->OnClientReady();
  }
}

void GlicInstanceImpl::WebUiStateChanged(mojom::WebUiState state) {
  TRACE_EVENT_INSTANT("glic", "GlicInstanceImpl::WebUiStateChanged",
                      perfetto::Flow::FromPointer(this), "state", state);
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
  state_observers_.Notify(&PanelStateObserver::PanelStateChanged,
                          GetPanelState());
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
      if (entry.embedder && (*tab)->IsActivated()) {
        auto* coordinator = GlicSidePanelCoordinator::GetForTab(*tab);
        if (coordinator &&
            coordinator->state() == GlicSidePanelCoordinator::State::kShown) {
          // Note that this will only happen for full show, not peek.
          Show(ShowOptions::ForSidePanel(**tab));
          return;
        }
      }
    }
  }

  OnAllEmbeddersInactive();
}

void GlicInstanceImpl::OnAllEmbeddersInactive() {
  TRACE_EVENT("glic", "GlicInstanceImpl::OnAllEmbeddersInactive");

  if (base::FeatureList::IsEnabled(
          features::kGlicSetWebContentsVisibilityWhenToggling)) {
    // Make WebContents hidden to avoid frame production and reduce the priority
    // of its renderer processes.
    // Some actuations steps need the WebContents to be visible in order to
    // make progress, so we need to keep it visible in that case.
    // TODO(crbug.com/513209932): Hide WebContents when Glic is not showing,
    // regardless of whether it is actuating or not.
    if (!IsActuating()) {
      host_.SetWebContentsVisibility(content::Visibility::HIDDEN);
    }
  }

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

base::TimeDelta GlicInstanceImpl::GetTimeSinceLastPromptSubmission() const {
  if (last_prompt_submission_time_.is_null()) {
    return base::TimeDelta::Max();
  }
  return base::TimeTicks::Now() - last_prompt_submission_time_;
}

bool GlicInstanceImpl::IsHibernated() const {
  return !host_.webui_contents();
}

void GlicInstanceImpl::Hibernate() {
  VLOG(1) << "Glic [InstanceImpl] Hibernate, id=" << id_.value();
  DeactivateCurrentEmbedder();
  host_.Shutdown();
}

void GlicInstanceImpl::Shutdown() {
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
    mojom::FreOverride fre_override) {
  Host::PanelWillOpenOptions options;
  options.conversation_info = GetConversationInfo();
  if (coordinator_delegate_) {
    options.recently_active_conversations =
        coordinator_delegate_->GetRecentlyActiveConversations(
            kMaxRecentConversationsForPanel);
  }
  options.prompt_suggestion = prompt_suggestion;
  options.auto_send = false;
  options.fre_override = fre_override;
  host_.PanelWillOpen(invocation_source, std::move(options));

  if (base::FeatureList::IsEnabled(features::kGlicClearTurnIdOnPanelWillOpen)) {
    conversation_info_->turn_id = std::nullopt;
  }
}

void GlicInstanceImpl::OnWebClientCleared() {
  NotifyPanelWillOpen(mojom::InvocationSource::kDefaultValue, std::nullopt);
}

void GlicInstanceImpl::CloseAllEmbedders() {
  // Copy the keys before iterating because Close() might modify `embedders_`.
  std::vector<EmbedderKey> keys;
  for (auto& [key, entry] : embedders_) {
    keys.push_back(key);
  }
  base::WeakPtr<GlicInstanceImpl> weak_this = weak_ptr_factory_.GetWeakPtr();
  for (const auto& key : keys) {
    Close(key);
    if (!weak_this) {
      return;
    }
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

GlicFloatingUi* GlicInstanceImpl::GetFloatingUiForTesting() {
  if (!IsDetached()) {
    return nullptr;
  }
  auto* embedder = GetActiveEmbedder();
  if (!embedder) {
    return nullptr;
  }
  return static_cast<GlicFloatingUi*>(embedder);
}
#endif

void GlicInstanceImpl::OnTabAddedToTask(
    actor::TaskId task_id,
    const tabs::TabInterface::Handle& tab_handle) {
  tabs::TabInterface* tab = tab_handle.Get();
  if (!tab || !task_id) {
    instance_metrics_.OnDaisyChain(DaisyChainSource::kActorAddTab,
                                   /*success=*/false);
    return;
  }

  if (IsActiveEmbedder(CreateSidePanelEmbedderKey(tab))) {
    return;
  }

  SidePanelShowOptions side_panel_options{*tab};
  side_panel_options.suppress_opening_animation = true;
  side_panel_options.prefer_peek = true;

  if (base::FeatureList::IsEnabled(
          kGlicActorDaisyChainingFromFloatingUiDoesntClose) &&
      IsDetached()) {
    side_panel_options.pin_trigger = GlicPinTrigger::kActuation;
    ShowInactiveSidePanelEmbedderFor(side_panel_options);
  } else if (suppress_show_on_tab_added_to_task_) {
    BindTab(tab, GlicPinTrigger::kActuation, /*pin_on_bind=*/true);
    suppress_show_on_tab_added_to_task_ = false;
  } else {
    Show(ShowOptions{side_panel_options});
  }
  instance_metrics_.OnDaisyChain(DaisyChainSource::kActorAddTab,
                                 /*success=*/true, tab);
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
