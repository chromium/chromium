// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/memory_coordinator/utils.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/glic/common/future_browser_features.h"
#include "chrome/browser/glic/common/glic_tab_observer.h"
#include "chrome/browser/glic/common/instance_independent_hotkey_manager.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/context/glic_active_instance_sharing_manager.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_contents_manager.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/metrics/glic_instance_coordinator_metrics.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/glic/service/metrics/glic_invoke_metrics.h"
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/base_window.h"

namespace glic {

namespace {
constexpr base::TimeDelta kFloatyMaxRecency = base::Hours(3);

BASE_FEATURE(kGlicMaxRecency, base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<base::TimeDelta> kGlicMaxRecencyValue{
    &kGlicMaxRecency, "duration", base::Minutes(30)};

GlicTabRestoreData* GetTabRestoreData(const TabCreationEvent& creation_event) {
  if (!creation_event.new_tab) {
    return nullptr;
  }
  // TODO(b/448420873): Remove this once android guarantees non-null
  // `WebContents`.
  if (!creation_event.new_tab->GetContents()) {
    return nullptr;
  }
  return GlicTabRestoreData::FromWebContents(
      creation_event.new_tab->GetContents());
}

bool IsEligibleForHibernation(const GlicInstanceImpl* instance) {
  return !instance->IsHibernated() && !instance->IsActuating() &&
         !instance->IsShowing();
}

// Whether `target.live_mode_behavior` can have any effect on an invocation
// targeting `target.surface`. Apart from `kFail`, these behaviors work by
// choosing between a tab's side panel and the floaty, so they need a surface
// that resolves to one of those up front. `LastActiveOrNew` instead resolves
// to whichever surface the instance is already on, so nothing can steer it.
bool CanLiveModeBehaviorTakeEffect(const Target& target) {
  switch (target.live_mode_behavior) {
    case LiveModeBehavior::kProceedInLiveMode:
    case LiveModeBehavior::kFail:
      return true;
    case LiveModeBehavior::kForceSidePanelTextMode:
      return !std::holds_alternative<Floating>(target.surface) &&
             !std::holds_alternative<LastActiveOrNew>(target.surface);
    case LiveModeBehavior::kForceFloatingTextMode:
      return !std::holds_alternative<LastActiveOrNew>(target.surface);
  }
}

}  // namespace

GlicInstanceCoordinatorImpl::GlicInstanceCoordinatorImpl(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    GlicKeyedService* service,
    GlicEnabling* enabling,
    ContextualCueingService* contextual_cueing_service)
    : coordinator_uid_(
          base::RandGenerator(std::numeric_limits<int64_t>::max())),
      profile_(profile),
      service_(service),
      contextual_cueing_service_(contextual_cueing_service),
      memory_pressure_listener_registration_(
          FROM_HERE,
          base::MemoryPressureListenerTag::kGlicKeyedService,
          this),
      metrics_(this),
      web_contents_warming_pool_(
          std::make_unique<GlicWebContentsWarmingPool>(profile, enabling)),
      active_instance_sharing_manager_(
          std::make_unique<GlicActiveInstanceSharingManager>(profile,
                                                             enabling)) {
  if (GetMemoryLimit() <= base::kModerateMemoryPressureThreshold) {
    OnMemoryPressure(memory_pressure_level());
  }
  if (identity_manager) {
    identity_manager_observation_.Observe(identity_manager);
  }
  tab_observer_ = GlicTabObserver::Create(
      profile_, base::BindRepeating(&GlicInstanceCoordinatorImpl::OnTabEvent,
                                    weak_ptr_factory_.GetWeakPtr()));
  hotkey_manager_ =
      std::make_unique<InstanceIndependentHotkeyManager>(this, profile_, enabling);
  onboarding_tracker_ =
      std::make_unique<GlicOnboardingTracker>(profile_, enabling);
  metrics_.StartPeriodicMemoryMetricsRecording();
}

GlicInstanceCoordinatorImpl::~GlicInstanceCoordinatorImpl() {
  CHECK(active_instance_sharing_manager_);
  active_instance_sharing_manager_->SetActiveSharingManager(nullptr);

  for (auto& [id, instance] : instances_) {
    instance->CloseInstanceAndShutdown();
  }

  // Delete all instances before destruction. Destroying web contents can result
  // in various calls to dependencies.
  active_instance_ = nullptr;
  last_active_instance_ = nullptr;
  auto instances = std::exchange(instances_, {});
  instances.clear();
}

GlicWebContentsWarmingPool&
GlicInstanceCoordinatorImpl::GetWebContentsWarmingPoolForTesting() {
  return *web_contents_warming_pool_;
}

void GlicInstanceCoordinatorImpl::OnInstanceActivationChanged(
    GlicInstanceImpl* instance,
    bool is_active) {
  if (is_active && active_instance_ != instance) {
    active_instance_ = instance;
    last_active_instance_ = active_instance_;
    MaybeStopListeningFloaty(instance);
  } else if (!is_active && active_instance_ == instance) {
    active_instance_ = nullptr;
  } else {
    return;
  }
  if (active_instance_) {
    active_instance_sharing_manager_->SetActiveSharingManager(
        &active_instance_->GetSharingManagerInternal());
  } else {
    active_instance_sharing_manager_->SetActiveSharingManager(nullptr);
  }
  NotifyActiveInstanceChanged();
  ComputeContentAccessIndicator();
}

void GlicInstanceCoordinatorImpl::OnInstanceWillAwaken() {
  // Before an existing instance creates its WebUI container and awakens,
  // enforce the max awake limit so that older background instances are pruned
  // to make room for this one if we are already at capacity.
  ApplyMaxAwakeInstancesLimit();
}

void GlicInstanceCoordinatorImpl::OnInstanceVisibilityChanged(
    GlicInstanceImpl* instance,
    bool is_showing) {
  global_show_hide_callback_list_.Notify();
  if (instance == active_instance_) {
    ComputeContentAccessIndicator();
  }
  metrics_.OnInstanceVisibilityChanged();
}

bool GlicInstanceCoordinatorImpl::IsInvoking(
    const GlicInstanceImpl* instance) const {
  return invoke_handlers_.contains(const_cast<GlicInstanceImpl*>(instance));
}

void GlicInstanceCoordinatorImpl::CancelInvoke(GlicInstanceImpl* instance) {
  // Take ownership of the handlers first, as cancelling them re-enters
  // OnInvokeHandlerComplete().
  auto [begin, end] = invoke_handlers_.equal_range(instance);
  std::vector<std::unique_ptr<GlicInvokeHandler>> handlers;
  for (auto it = begin; it != end; ++it) {
    handlers.push_back(std::move(it->second));
  }
  invoke_handlers_.erase(begin, end);

  for (auto& handler : handlers) {
    if (handler) {
      handler->Cancel(GlicInvokeError::kCancelled);
    }
  }
}

void GlicInstanceCoordinatorImpl::OnInvoked(mojom::InvocationSource source,
                                            ukm::SourceId source_id) {
  if (onboarding_tracker_) {
    onboarding_tracker_->OnInvoke(source, source_id);
  }
}

void GlicInstanceCoordinatorImpl::OnUserInputSubmitted(
    ukm::SourceId source_id) {
  if (onboarding_tracker_) {
    onboarding_tracker_->OnPrompt(source_id);
  }
}

void GlicInstanceCoordinatorImpl::OnFreOptInShown(ukm::SourceId source_id) {
  if (onboarding_tracker_) {
    onboarding_tracker_->OnFreOptInShown(source_id);
  }
}

void GlicInstanceCoordinatorImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    // Close all instances on sign-out.
    RemoveAllInstances();
  }
}

void GlicInstanceCoordinatorImpl::NotifyActiveInstanceChanged() {
  active_instance_changed_callback_list_.Notify(active_instance_);
}

void GlicInstanceCoordinatorImpl::ComputeContentAccessIndicator() {
  if (active_instance_) {
    if (base::FeatureList::IsEnabled(features::kGlicLiveModeOnlyGlow)) {
      service_->SetContextAccessIndicator(
          active_instance_->IsShowing() && active_instance_->IsLiveMode() &&
          active_instance_->host().IsContextAccessIndicatorEnabled());
    } else {
      service_->SetContextAccessIndicator(
          active_instance_->IsShowing() &&
          active_instance_->host().IsContextAccessIndicatorEnabled());
    }
  } else {
    service_->SetContextAccessIndicator(false);
  }
}

GlicInstanceImpl* GlicInstanceCoordinatorImpl::GetInstanceImplForTab(
    const tabs::TabInterface* tab) const {
  if (!tab) {
    return nullptr;
  }

  auto* helper = GlicInstanceHelper::From(const_cast<tabs::TabInterface*>(tab));
  if (!helper) {
    return nullptr;
  }

  auto instance_id = helper->GetInstanceId();
  if (instance_id.has_value()) {
    if (auto* instance = GetInstanceImplFor(instance_id.value())) {
      return instance;
    }
  }

  return nullptr;
}

GlicInstanceImpl* GlicInstanceCoordinatorImpl::GetInstanceImplForTabGroup(
    tab_groups::TabGroupId group_id) const {
  for (const auto& [id, instance] : instances_) {
    if (instance->GetTabGroup() == group_id) {
      return instance.get();
    }
  }
  return nullptr;
}

std::vector<GlicInstanceImpl*> GlicInstanceCoordinatorImpl::GetInstances() {
  std::vector<GlicInstanceImpl*> instances;
  for (auto& entry : instances_) {
    instances.push_back(entry.second.get());
  }
  return instances;
}

int GlicInstanceCoordinatorImpl::GetVisibleInstanceCount() const {
  int count = 0;
  for (const auto& entry : instances_) {
    if (entry.second && entry.second->IsShowing()) {
      count++;
    }
  }
  return count;
}

std::vector<GlicInstanceCoordinatorMetrics::DataProvider::InstanceWebContents>
GlicInstanceCoordinatorImpl::GetAllUnhibernatedWebContents() {
  std::vector<GlicInstanceCoordinatorMetrics::DataProvider::InstanceWebContents>
      result;
  for (const auto& entry : instances_) {
    if (entry.second && !entry.second->IsHibernated()) {
      result.push_back({entry.second->host().webui_contents(),
                        entry.second->host().web_client_contents()});
    }
  }
  if (web_contents_warming_pool_) {
    if (auto warmed_contents =
            web_contents_warming_pool_->GetWarmedWebContents()) {
      result.push_back({warmed_contents->webui_contents.get(),
                        warmed_contents->guest_contents.get()});
    }
  }
  return result;
}

bool GlicInstanceCoordinatorImpl::IsAnyPanelShowing() const {
  for (const auto& entry : instances_) {
    if (entry.second && entry.second->IsShowing()) {
      return true;
    }
  }
  return false;
}

bool GlicInstanceCoordinatorImpl::IsConversationPresent(
    const std::string& conversation_id) const {
  return !!GetInstanceImplForConversationId(conversation_id);
}

GlicInstanceCoordinator::ActivateTabResult
GlicInstanceCoordinatorImpl::ActivateTabWithConversation(
    const std::string& conversation_id) {
  GlicInstanceImpl* instance =
      GetInstanceImplForConversationId(conversation_id);
  if (!instance) {
    return GlicInstanceCoordinator::ActivateTabResult::kConversationNotFound;
  }

  std::vector<tabs::TabInterface*> target_tabs;

  // Try to get tabs from the actor task manager first.
  GlicActorTaskManager* task_manager = instance->GetActorTaskManager();
  if (task_manager) {
    target_tabs = task_manager->GetLastActedTabs();
  }

  // If no tabs from actor, fallback to bound tabs.
  if (target_tabs.empty()) {
    target_tabs = instance->GetBoundTabs();
  }

  metrics_.RecordActivateTabCandidateTabCount(target_tabs.size());
  if (target_tabs.empty()) {
    return GlicInstanceCoordinator::ActivateTabResult::kNoBoundTabs;
  }

  tabs::TabInterface* target_tab = GetMostRecentlyActiveTab(target_tabs);

  BrowserWindowInterface* target_browser =
      target_tab->GetBrowserWindowInterface();
  if (!target_browser) {
    return GlicInstanceCoordinator::ActivateTabResult::kTabNotInWindow;
  }

  auto* tab_list = TabListInterface::From(target_browser);
  tab_list->ActivateTab(target_tab->GetHandle());
  target_browser->GetWindow()->Activate();

  return GlicInstanceCoordinator::ActivateTabResult::kSuccess;
}

GlicInstance* GlicInstanceCoordinatorImpl::GetInstanceForTab(
    const tabs::TabInterface* tab) const {
  return GetInstanceImplForTab(tab);
}

GlicInstance* GlicInstanceCoordinatorImpl::GetInstanceForTabGroup(
    tab_groups::TabGroupId group_id) const {
  return GetInstanceImplForTabGroup(group_id);
}

GlicInstance* GlicInstanceCoordinatorImpl::ShowInstanceForTabGroup(
    tab_groups::TabGroupId group_id) {
  GlicInstanceImpl* existing_instance = GetInstanceImplForTabGroup(group_id);

  if (existing_instance) {
    existing_instance->ShowForTabGroup(group_id, /*options=*/std::nullopt);
    return existing_instance;
  }

  GlicInstanceImpl* instance = CreateGlicInstance();
  instance->ShowForTabGroup(group_id, /*options=*/std::nullopt);
  return instance;
}

GlicInstance* GlicInstanceCoordinatorImpl::GetInstanceWithGlicWebContents(
    content::WebContents* glic_web_contents) const {
  if (!glic_web_contents) {
    return nullptr;
  }
  for (const auto& [id, instance] : instances_) {
    if (instance->host().IsWebContentPresentAndMatches(
            glic_web_contents->GetPrimaryMainFrame())) {
      return instance.get();
    }
  }
  return nullptr;
}

bool GlicInstanceCoordinatorImpl::MaybeInvoke(BrowserWindowInterface* bwi,
                                              mojom::InvocationSource source) {
  if (!bwi && GlicEnabling::IsLiveAndFloatyEnabledByFlags()) {
    return false;
  }
  BrowserWindowInterface* target_bwi =
      bwi ? bwi : GetActiveGlicEligibleBrowser(profile_);
  if (!target_bwi) {
    return false;
  }

  bool panel_closed = !IsPanelShowingForBrowser(*target_bwi);
  bool fre_override_compatible =
      !GlicEnabling::HasConsentedForProfile(profile_);

  if (fre_override_compatible && panel_closed &&
      (base::FeatureList::IsEnabled(features::kGlicMessageFirstFre) ||
       base::FeatureList::IsEnabled(features::kGlicActionFirstFRE))) {
    GlicInvokeOptions options(source);
    if (auto* active_tab = TabListInterface::From(target_bwi)->GetActiveTab()) {
      options.target = Target(*active_tab);
    }
    options.fre_override = mojom::FreOverride::kTrustFirstInline;
    Invoke(std::move(options));
    return true;
  }

  return false;
}

void GlicInstanceCoordinatorImpl::Show(BrowserWindowInterface* browser,
                                       mojom::InvocationSource source) {
  CHECK(GlicEnabling::ShouldShowGlicButton(profile_));

  // TODO(b/542727532): Follow up on whether MaybeInvoke is still needed and
  // remove if possible.
  if (MaybeInvoke(browser, source)) {
    return;
  }

  service()->enabling().MaybeRecordRecoveryOnInteraction();

  if (!browser) {
    if (!GlicEnabling::IsLiveAndFloatyEnabledByFlags()) {
#if !BUILDFLAG(IS_ANDROID)
      browser = chrome::OpenEmptyWindow(profile_);
#endif
      if (!browser) {
        LOG(ERROR)
            << "Could not find or create a browser window for Glic side panel.";
        return;
      }
    } else {
      EmbedderKey key = FloatingEmbedderKey();
      if (GlicInstanceImpl* instance = GetInstanceWithFloaty()) {
        instance->instance_metrics().OnToggle(source, key, /*is_showing=*/true);
        return;
      }

      InvokeAndLogToggle(source, glic::Floating(), key,
                         std::make_unique<glic::GlicWindowInvocationTracker>());
      return;
    }
  }

  auto* tab = TabListInterface::From(browser)->GetActiveTab();
  if (!tab) {
    LOG(ERROR) << "Active tab is null";
    return;
  }
  if (!GlicInstanceHelper::From(tab)) {
    LOG(ERROR) << "Tab doesn't have an instance helper in its UnownedUserData";
    return;
  }

  EmbedderKey key = SidePanelEmbedderKey(tab);
  if (GlicInstanceImpl* instance = GetInstanceImplForTab(tab);
      instance && instance->IsActiveEmbedder(key)) {
    instance->instance_metrics().OnToggle(source, key, /*is_showing=*/true);
    return;
  }

  InvokeAndLogToggle(source, tab->GetHandle(), key,
                     std::make_unique<glic::GlicWindowInvocationTracker>());
}

std::optional<GlicInstanceCoordinatorImpl::ToggleCloseTarget>
GlicInstanceCoordinatorImpl::FindCloseTargetForToggle(
    BrowserWindowInterface* browser) const {
  if (!browser) {
    if (!GlicEnabling::IsLiveAndFloatyEnabledByFlags()) {
      return std::nullopt;
    }
    GlicInstanceImpl* instance = GetInstanceWithFloaty();
    if (!instance) {
      return std::nullopt;
    }
    return ToggleCloseTarget{instance, FloatingEmbedderKey()};
  }

  if (!IsPanelShowingForBrowser(*browser)) {
    return std::nullopt;
  }
  auto* tab = TabListInterface::From(browser)->GetActiveTab();
  if (!tab) {
    return std::nullopt;
  }
  GlicInstanceImpl* instance = GetInstanceImplForTab(tab);
  if (!instance) {
    return std::nullopt;
  }
  EmbedderKey key = SidePanelEmbedderKey(tab);
  if (!instance->IsActiveEmbedder(key)) {
    return std::nullopt;
  }
  return ToggleCloseTarget{instance, std::move(key)};
}

bool GlicInstanceCoordinatorImpl::MaybeCloseForToggle(
    BrowserWindowInterface* browser,
    mojom::InvocationSource source) {
  std::optional<ToggleCloseTarget> target = FindCloseTargetForToggle(browser);
  if (!target) {
    return false;
  }
  target->instance->instance_metrics().OnToggle(source, target->key,
                                                /*is_showing=*/true);
  target->instance->Close(target->key);
  return true;
}

bool GlicInstanceCoordinatorImpl::WouldToggleClose(
    BrowserWindowInterface* browser) const {
  return FindCloseTargetForToggle(browser).has_value();
}

void GlicInstanceCoordinatorImpl::Toggle(BrowserWindowInterface* browser,
                                         bool prevent_close,
                                         mojom::InvocationSource source) {
  CHECK(GlicEnabling::ShouldShowGlicButton(profile_));

  if (MaybeInvoke(browser, source)) {
    return;
  }

  service()->enabling().MaybeRecordRecoveryOnInteraction();

  if (!prevent_close && MaybeCloseForToggle(browser, source)) {
    return;
  }

  Show(browser, source);
}

bool GlicInstanceCoordinatorImpl::MaybeStartWarming(
    GlicWarmingTrigger trigger) {
  return web_contents_warming_pool_->MaybeStartWarming(trigger);
}

void GlicInstanceCoordinatorImpl::Shutdown() {
  // Take ownership of the handlers to avoid iterator invalidation when
  // Cancel() removes them from invoke_handlers_.
  std::multimap<GlicInstance*, std::unique_ptr<GlicInvokeHandler>> handlers(
      std::move(invoke_handlers_));
  invoke_handlers_.clear();

  for (auto& [instance, handler] : handlers) {
    if (handler) {
      handler->Cancel(GlicInvokeError::kInstanceDestroyed);
    }
  }

  for (auto& [instance_id, instance] : instances_) {
    instance->Shutdown();
  }
  web_contents_warming_pool_->Shutdown();
  hotkey_manager_.reset();
}

void GlicInstanceCoordinatorImpl::Close(const CloseOptions& options) {
  // TODO(crbug.com/450286204): Determine whether there are cases where this
  // should be able to close a side panel UI instead.
  CloseFloaty(options);
}

void GlicInstanceCoordinatorImpl::RemoveAllInstances() {
  while (!instances_.empty()) {
    RemoveInstance(instances_.begin()->first);
  }
}

base::WeakPtr<GlicInstance> GlicInstanceCoordinatorImpl::Invoke(
    GlicInvokeOptions options) {
  return InvokeInternal(std::nullopt, std::move(options),
                        GlicInvokeWithAutoSubmitOptions());
}

base::WeakPtr<GlicInstance> GlicInstanceCoordinatorImpl::InvokeWithAutoSubmit(
    InvokeWithAutoSubmitPasskey auto_submit_passkey,
    GlicInvokeOptions options) {
  return InvokeInternal(auto_submit_passkey, std::move(options),
                        GlicInvokeWithAutoSubmitOptions());
}

base::WeakPtr<GlicInstance> GlicInstanceCoordinatorImpl::InvokeWithAutoSubmit(
    InvokeWithAutoSubmitPasskey auto_submit_passkey,
    GlicInvokeOptions options,
    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
  return InvokeInternal(auto_submit_passkey, std::move(options),
                        std::move(auto_submit_options));
}

base::WeakPtr<GlicInstanceImpl> GlicInstanceCoordinatorImpl::InvokeInternal(
    std::optional<InvokeWithAutoSubmitPasskey> auto_submit_passkey,
    GlicInvokeOptions options,
    GlicInvokeWithAutoSubmitOptions auto_submit_options,
    bool bypass_in_progress_check) {
  auto metrics =
      std::make_unique<GlicInvokeMetrics>(options.GetInvocationSource());

  if (!GlicEnabling::IsEnabledForProfile(profile_)) {
    metrics->RecordError(GlicInvokeError::kProfileNotEnabled);
    if (options.on_error) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(options.on_error),
                                    GlicInvokeError::kProfileNotEnabled));
    }
    return nullptr;
  }

  if (const auto* tab_handle =
          std::get_if<tabs::TabHandle>(&options.target.surface)) {
    if (tab_handle->raw_value() == tabs::TabHandle::NullValue) {
      metrics->RecordError(GlicInvokeError::kInvalidTab);
      if (options.on_error) {
        std::move(options.on_error).Run(GlicInvokeError::kInvalidTab);
      }
      return nullptr;
    }
  }

  // Validate against the requested surface, before it gets resolved and
  // rewritten below: a behavior that could never take effect is a caller
  // configuration error, whether or not an instance happens to be live.
  // TODO(b/559214349): Expose a way for callers to ask up front whether an
  // invocation would be rejected here or by the `kFail` check below, so that
  // UI entry points can be disabled or hidden instead of failing on click.
  if (!CanLiveModeBehaviorTakeEffect(options.target)) {
    metrics->RecordError(GlicInvokeError::kInvalidConfiguration);
    if (options.on_error) {
      std::move(options.on_error).Run(GlicInvokeError::kInvalidConfiguration);
    }
    return nullptr;
  }

  GlicInvokeHandler::ResolvedTarget resolved_target;
  tabs::TabInterface* tab = nullptr;

  auto resolve_surface = [&]() -> bool {
    resolved_target =
        GlicInvokeHandler::ResolveTargetSurface(profile_, options.target);
    if (const auto* tab_surface =
            std::get_if<GlicInvokeHandler::TabSurface>(&resolved_target)) {
      tab = tab_surface->tab;
      if (!tab || !GlicInstanceHelper::From(tab)) {
        GlicInvokeError error = GlicInvokeError::kTabClosed;
        if (const auto* tab_handle =
                std::get_if<tabs::TabHandle>(&options.target.surface)) {
          if (tab_handle->Get()) {
            error = GlicInvokeError::kInvalidTab;
          }
        }
        metrics->RecordError(error);
        if (options.on_error) {
          std::move(options.on_error).Run(error);
        }
        // TODO(crbug.com/483387751): Show default toast here once implemented.
        return false;
      }
      options.target.surface = tab->GetHandle();
    }
    return true;
  };

  // We generally want to resolve the target surface before the conversation.
  // The primary reason is that resolving the `DefaultConversation` might depend
  // on which surface the conversation is being invoked from (e.g. which tab it
  // is attached to, or whether it's floating).
  //
  // However, the `LastActiveOrNew` surface is a special case. It cannot be
  // resolved until we know the target `GlicInstance` and its last active
  // surface. Because of this Catch-22, we skip initial surface resolution for
  // `LastActiveOrNew` and defer it until after the instance is found. (Note
  // that a `LastActiveOrNew` surface will never result in finding a surface-
  // dependent `DefaultConversation` because the conversation must already have
  // an explicit instance to query its last active surface).
  if (!std::holds_alternative<LastActiveOrNew>(options.target.surface)) {
    if (!resolve_surface()) {
      return nullptr;
    }
  }

  GlicInstanceImpl* instance = nullptr;

  instance = std::visit(
      absl::Overload{
          [&](const ConversationId& conv_id) {
            if (conv_id.conversation_id.empty()) {
              metrics->RecordError(GlicInvokeError::kInvalidConversationId);
              if (options.on_error) {
                std::move(options.on_error)
                    .Run(GlicInvokeError::kInvalidConversationId);
              }
              // TODO(crbug.com/483387751): Show default toast here
              // once implemented.
              return static_cast<GlicInstanceImpl*>(nullptr);
            }
            return GetOrCreateInstanceImplForConversationId(
                conv_id.conversation_id, conv_id.turn_id);
          },
          [&](NewConversation) { return CreateGlicInstance(); },
          [&](const InstanceId& id) {
            GlicInstanceImpl* target_instance = GetInstanceImplFor(id);
            if (!target_instance) {
              metrics->RecordError(GlicInvokeError::kInstanceNotFound);
              if (options.on_error) {
                std::move(options.on_error)
                    .Run(GlicInvokeError::kInstanceNotFound);
              }
            }
            return target_instance;
          },
          [&](DefaultConversation) {
            if (std::holds_alternative<Floating>(resolved_target)) {
              return GetOrCreateInstanceImplForFloaty();
            }
            return GetOrCreateGlicInstanceImplForTab(tab);
          }},
      options.target.conversation);

  if (!instance) {
    return nullptr;
  }

  // `options.target.live_mode_behavior` decides what happens when this
  // invocation lands on an instance that is already in live (audio) mode,
  // superseding `options.preserve_active_surface`. A newly created instance is
  // never in live mode, so this only affects invocations that joined an
  // existing conversation. The interaction mode is sticky once reported by the
  // client, so a hidden instance doesn't count as being in live mode.
  const bool is_showing_live_mode =
      instance->IsShowing() && instance->IsLiveMode();
  if (is_showing_live_mode &&
      options.target.live_mode_behavior == LiveModeBehavior::kFail) {
    metrics->RecordError(GlicInvokeError::kLiveModeActive);
    if (options.on_error) {
      std::move(options.on_error).Run(GlicInvokeError::kLiveModeActive);
    }
    return nullptr;
  }

  const bool is_floating = instance->IsActiveEmbedder(FloatingEmbedderKey{});
  bool keep_in_floaty = false;
  if (is_showing_live_mode) {
    switch (options.target.live_mode_behavior) {
      case LiveModeBehavior::kProceedInLiveMode:
        // Live mode runs in the floaty, so carrying on in live mode means
        // leaving the conversation where it is.
        keep_in_floaty = is_floating;
        break;
      case LiveModeBehavior::kForceFloatingTextMode:
        // Unlike the above, this pulls the conversation into the floaty.
        keep_in_floaty = true;
        break;
      case LiveModeBehavior::kForceSidePanelTextMode:
      case LiveModeBehavior::kFail:
        break;
    }
  } else {
    keep_in_floaty = options.preserve_active_surface && is_floating;
  }

  // Leave the conversation in the floaty instead of pulling it into the
  // targeted tab's side panel.
  if (keep_in_floaty && tab) {
    // A `DefaultConversation` only resolves to a floating instance when the tab
    // is already bound to it. An explicitly targeted one may still need to
    // adopt the tab.
    if (GetInstanceImplForTab(tab) != instance) {
      // Bind first: rewriting the surface below discards the tab.
      // TODO(b/562983414): Infer a more specific pin trigger from the
      // invocation source.
      instance->BindTabWithoutShowing(tab, GlicPinTrigger::kInstanceCreation,
                                      options.pin_on_bind);
    }
    options.target.surface = Floating();
    if (!resolve_surface()) {
      return nullptr;
    }
  }

  // Now that the instance is fully resolved, we can safely resolve the
  // `LastActiveOrNew` surface and mutate `options.target.surface` to point to
  // the appropriate final target, before running the surface resolver.
  if (auto* last_active_or_new =
          std::get_if<LastActiveOrNew>(&options.target.surface)) {
    std::optional<Target::Surface> last_active =
        instance->GetLastActiveSurface();
    if (last_active) {
      options.target.surface = *last_active;
    } else {
      options.target.surface = NewTab{last_active_or_new->window,
                                      last_active_or_new->open_in_foreground};
    }

    if (!resolve_surface()) {
      return nullptr;
    }
  }

  if (bypass_in_progress_check) {
    auto handler = std::make_unique<GlicInvokeHandler>(
        *instance, resolved_target, std::move(options),
        std::move(auto_submit_options), auto_submit_passkey, std::move(metrics),
        base::DoNothing());
    GlicInvokeHandler* handler_ptr = handler.get();
    handler_ptr->set_completion_callback(
        base::BindOnce([](std::unique_ptr<GlicInvokeHandler> h, GlicInstance*,
                          GlicInvokeHandler*) {},
                       std::move(handler)));
    handler_ptr->Invoke();
    return instance->GetWeakPtr();
  }

  // Only invocations that send an invoke message to the web client conflict
  // with each other. Invocations that merely show the UI can safely run
  // simultaneously with any other invocation on the same instance.
  if (GlicInvokeHandler::RequiresClientInvoke(
          options, auto_submit_passkey.has_value())) {
    if (GlicInvokeHandler* in_progress = FindClientInvokeHandler(instance)) {
      if (options.supersede_if_in_progress) {
        // If requested by `options.supersede_if_in_progress` (e.g. for a
        // continuation prompt from the server during actuation), cancel the
        // previous handler so this invocation can proceed without being
        // rejected with kInvokeInProgress.
        std::unique_ptr<GlicInvokeHandler> old_handler =
            RemoveInvokeHandler(instance, in_progress);
        old_handler->Cancel(GlicInvokeError::kSuperseded);
      } else {
        metrics->RecordError(GlicInvokeError::kInvokeInProgress);
        if (options.on_error) {
          std::move(options.on_error).Run(GlicInvokeError::kInvokeInProgress);
        }
        // TODO(crbug.com/483387751): Show default toast here once implemented.
        return nullptr;
      }
    }
  }

  auto handler = std::make_unique<GlicInvokeHandler>(
      *instance, resolved_target, std::move(options),
      std::move(auto_submit_options), auto_submit_passkey, std::move(metrics),
      base::BindOnce(&GlicInstanceCoordinatorImpl::OnInvokeHandlerComplete,
                     base::Unretained(this)));
  GlicInvokeHandler* handler_ptr = handler.get();
  invoke_handlers_.emplace(instance, std::move(handler));
  handler_ptr->Invoke();

  return instance->GetWeakPtr();
}

GlicInvokeHandler* GlicInstanceCoordinatorImpl::FindClientInvokeHandler(
    GlicInstance* instance) const {
  auto [begin, end] = invoke_handlers_.equal_range(instance);
  for (auto it = begin; it != end; ++it) {
    if (it->second && it->second->requires_client_invoke()) {
      return it->second.get();
    }
  }
  return nullptr;
}

std::unique_ptr<GlicInvokeHandler>
GlicInstanceCoordinatorImpl::RemoveInvokeHandler(GlicInstance* instance,
                                                 GlicInvokeHandler* handler) {
  auto [begin, end] = invoke_handlers_.equal_range(instance);
  auto it = std::ranges::find(begin, end, handler, [](const auto& entry) {
    return entry.second.get();
  });
  if (it == end) {
    return nullptr;
  }
  return std::move(invoke_handlers_.extract(it).mapped());
}

void GlicInstanceCoordinatorImpl::OnInvokeHandlerComplete(
    GlicInstance* instance,
    GlicInvokeHandler* handler) {
  // This destroys `handler`, which is what the completion callback contract
  // requires.
  RemoveInvokeHandler(instance, handler);
}

void GlicInstanceCoordinatorImpl::CloseAndShutdownInstanceWithFrame(
    content::RenderFrameHost* render_frame_host) {
  for (auto& [id, instance] : instances_) {
    if (instance &&
        instance->host().IsWebContentPresentAndMatches(render_frame_host)) {
      instance->Shutdown();
    }
  }
}

void GlicInstanceCoordinatorImpl::CloseInstanceWithFrame(
    content::RenderFrameHost* render_frame_host) {
  for (auto& [id, instance] : instances_) {
    if (instance->host().IsWebContentPresentAndMatches(render_frame_host)) {
      instance->host().Close();
      return;
    }
  }
}

void GlicInstanceCoordinatorImpl::ArchiveInstanceWithFrame(
    content::RenderFrameHost* render_frame_host) {
  for (auto& [id, instance] : instances_) {
    if (instance->host().IsWebContentPresentAndMatches(render_frame_host)) {
      RemoveInstance(id);
      return;
    }
  }
}

void GlicInstanceCoordinatorImpl::CloseFloaty(const CloseOptions& options) {
  if (auto* floaty_instance = GetInstanceWithFloaty()) {
    floaty_instance->Close(FloatingEmbedderKey{}, options);
  }
}

bool GlicInstanceCoordinatorImpl::IsDetached() const {
  return GetInstanceWithFloaty() != nullptr;
}

bool GlicInstanceCoordinatorImpl::IsPanelShowingForBrowser(
    const BrowserWindowInterface& bwi) const {
  auto* tab_list =
      TabListInterface::From(const_cast<BrowserWindowInterface*>(&bwi));
  if (!tab_list) {
    return false;
  }
  if (const auto* instance = GetInstanceForTab(tab_list->GetActiveTab())) {
    return instance->IsShowing();
  }
  return false;
}

base::CallbackListSubscription
GlicInstanceCoordinatorImpl::AddGlobalShowHideCallback(
    base::RepeatingClosure callback) {
  return global_show_hide_callback_list_.Add(std::move(callback));
}

void GlicInstanceCoordinatorImpl::Reload(
    content::RenderFrameHost* render_frame_host) {
  for (auto& [id, instance] : instances_) {
    if (instance->host().IsWebContentPresentAndMatches(render_frame_host)) {
      instance->host().Reload();
      return;
    }
  }
}

base::WeakPtr<GlicInstanceCoordinatorImpl>
GlicInstanceCoordinatorImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::CallbackListSubscription GlicInstanceCoordinatorImpl::
    AddActiveInstanceChangedCallbackAndNotifyImmediately(
        ActiveInstanceChangedCallback callback) {
  // Fire immediately to give subscribers an initial value.
  callback.Run(active_instance_);
  auto subscription =
      active_instance_changed_callback_list_.Add(std::move(callback));
  return subscription;
}

GlicInstance* GlicInstanceCoordinatorImpl::GetActiveInstance() {
  return active_instance_;
}

GlicSharingManagerInternal&
GlicInstanceCoordinatorImpl::active_instance_sharing_manager() {
  CHECK(active_instance_sharing_manager_);
  return *active_instance_sharing_manager_;
}

GlicInstanceImpl* GlicInstanceCoordinatorImpl::GetInstanceImplForConversationId(
    const std::string& conversation_id) const {
  for (const auto& [id, instance] : instances_) {
    if (instance->conversation_id() == conversation_id) {
      return instance.get();
    }
  }
  return nullptr;
}

GlicInstanceImpl*
GlicInstanceCoordinatorImpl::GetOrCreateInstanceImplForConversationId(
    const std::string& conversation_id,
    const std::optional<std::string>& turn_id) {
  GlicInstanceImpl* instance =
      GetInstanceImplForConversationId(conversation_id);
  if (!instance) {
    instance = CreateGlicInstance();
    auto info = mojom::ConversationInfo::New();
    info->conversation_id = conversation_id;
    if (turn_id.has_value()) {
      info->turn_id = turn_id.value();
    }
    instance->RegisterConversation(std::move(info), base::DoNothing());
  } else if (turn_id.has_value()) {
    // Instance exists, update turn_id if provided.
    auto info = instance->GetConversationInfo();
    if (info && info->turn_id != turn_id.value()) {
      info->turn_id = turn_id.value();
      instance->RegisterConversation(std::move(info), base::DoNothing());
    }
  }
  return instance;
}

GlicInstanceImpl*
GlicInstanceCoordinatorImpl::GetOrCreateGlicInstanceImplForTab(
    tabs::TabInterface* tab) {
  if (GlicInstanceImpl* instance = GetInstanceImplForTab(tab)) {
    return instance;
  }

  if (last_active_instance_) {
    base::UmaHistogramCustomTimes(
        "Glic.Instance.TimeSinceLastInstanceActiveOnOpen",
        last_active_instance_->GetTimeSinceLastActive(), base::Seconds(1),
        base::Hours(24), 50);
  }

  // Create a new conversation and instance.
  return CreateGlicInstance();
}

GlicInstanceImpl* GlicInstanceCoordinatorImpl::GetInstanceImplFor(
    const InstanceId& id) const {
  auto it = instances_.find(id);
  if (it != instances_.end()) {
    return it->second.get();
  }
  return nullptr;
}

size_t GlicInstanceCoordinatorImpl::GetCurrentMaxAwakeInstancesLimit() const {
  CHECK(base::FeatureList::IsEnabled(kGlicMaxAwakeInstances));
  const size_t baseline_limit =
      static_cast<size_t>(std::max(1, kGlicMaxAwakeInstancesLimit.Get()));
  if (!base::FeatureList::IsEnabled(base::kStatefulMemoryPressure)) {
    return baseline_limit;
  }

  return CalculateAwakeInstancesLimit(baseline_limit, GetMemoryLimit());
}

void GlicInstanceCoordinatorImpl::TrimAwakeInstancesTo(
    size_t target_total_awake_count) {
  size_t total_awake_count = 0;
  std::vector<GlicInstanceImpl*> hibernatable_instances;
  for (const auto& [id, instance] : instances_) {
    if (!instance->IsHibernated()) {
      total_awake_count++;
      if (IsEligibleForHibernation(instance.get())) {
        hibernatable_instances.push_back(instance.get());
      }
    }
  }

  // If we are already within our total awake budget, or if there are no
  // eligible background instances we can safely hibernate, do nothing.
  if (total_awake_count <= target_total_awake_count ||
      hibernatable_instances.empty()) {
    return;
  }

  const size_t excess_count =
      std::min(total_awake_count - target_total_awake_count,
               hibernatable_instances.size());

  // Partition candidates by time since last active (descending = oldest first)
  // so that the `excess_count` oldest instances are placed at the front of the
  // vector.
  if (excess_count < hibernatable_instances.size()) {
    std::nth_element(hibernatable_instances.begin(),
                     hibernatable_instances.begin() + excess_count,
                     hibernatable_instances.end(),
                     [](const GlicInstanceImpl* a, const GlicInstanceImpl* b) {
                       return a->GetTimeSinceLastActive() >
                              b->GetTimeSinceLastActive();
                     });
  }

  for (size_t i = 0; i < excess_count; ++i) {
    hibernatable_instances[i]->Hibernate();
  }
}

void GlicInstanceCoordinatorImpl::ApplyMaxAwakeInstancesLimit() {
  if (!base::FeatureList::IsEnabled(kGlicMaxAwakeInstances)) {
    return;
  }

  // Subtract 1 from the limit to make room for the instance that is about to
  // awaken. The limit must be at least 1 to avoid an underflow.
  const size_t limit = std::max<size_t>(1, GetCurrentMaxAwakeInstancesLimit());
  TrimAwakeInstancesTo(limit - 1);
}

GlicInstanceImpl* GlicInstanceCoordinatorImpl::CreateGlicInstance(
    std::optional<InstanceId> instance_id) {
  auto instance = CreateInstanceImpl(instance_id);
  instance->instance_metrics().OnInstanceCreatedWithoutWarming();
  auto* instance_ptr = instance.get();
  instances_[instance->id()] = std::move(instance);

  if (auto* task_manager = instance_ptr->GetActorTaskManager()) {
    actuating_changed_subscriptions_[instance_ptr->id()] =
        task_manager->AddActuatingChangedCallback(base::BindRepeating(
            &GlicInstanceCoordinatorImpl::OnInstanceActuatingChanged,
            base::Unretained(this)));
  }

  metrics_.RecordCountOnCreation();

  return instance_ptr;
}

std::unique_ptr<GlicInstanceImpl>
GlicInstanceCoordinatorImpl::CreateInstanceImpl(std::optional<InstanceId> id) {
  InstanceId instance_id =
      id ? *id : InstanceId::Create(coordinator_uid_, next_instance_index_++);
  return std::make_unique<GlicInstanceImpl>(
      profile_, instance_id, weak_ptr_factory_.GetWeakPtr(),
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_)->metrics(),
      contextual_cueing_service_);
}


GlicInstanceImpl*
GlicInstanceCoordinatorImpl::GetOrCreateInstanceImplForFloaty() {
  auto* floaty_instance = GetInstanceWithFloaty();
  if (!floaty_instance && last_active_instance_) {
    base::UmaHistogramCustomTimes(
        "Glic.Instance.TimeSinceLastInstanceActiveOnOpen",
        last_active_instance_->GetTimeSinceLastActive(), base::Seconds(1),
        base::Hours(24), 50);
  }

  if (!floaty_instance && last_active_instance_ &&
      last_active_instance_->GetTimeSinceLastActive() < kFloatyMaxRecency) {
    floaty_instance = last_active_instance_;
  }

  // If there's not an open floaty, or a last active instance, create a new
  // instance.
  if (!floaty_instance) {
    floaty_instance = CreateGlicInstance();
  }
  return floaty_instance;
}

// Helper method for toggling the UI open. This should ONLY be used by the
// toggle flow (ToggleSidePanel, ToggleFloaty) as it bypasses the in-progress
// invocation check and sets fre_completion_wait_mode to kNever.
void GlicInstanceCoordinatorImpl::InvokeAndLogToggle(
    glic::mojom::InvocationSource source,
    Target::Surface surface,
    const EmbedderKey& key,
    std::unique_ptr<GlicWindowInvocationTracker> invocation_tracker) {
  if (!GlicEnabling::IsEnabledForProfile(profile_)) {
    // TODO(b/520041903): Remove this temporary workaround and route through
    // `InvokeInternal` once the `ClientLoadState` signal lands.
    // When the entrypoint is anchored for an onboarded user
    // (`ShouldShowGlicButton` is true while `IsEnabledForProfile` is false),
    // show the panel directly so the WebUI can render the `ProfileReadyState`
    // error screen (e.g. `kLocationMismatch` / `kIneligibleAccount`) without
    // starting a client invocation.
    if (!GlicEnabling::ShouldShowGlicButton(profile_)) {
      return;
    }
    GlicInstanceImpl* instance = nullptr;
    if (std::holds_alternative<Floating>(surface)) {
      instance = GetOrCreateInstanceImplForFloaty();
      ShowOptions show_options =
          ShowOptions::ForFloating(/*source_tab=*/tabs::TabHandle::Null());
      show_options.invocation_source = source;
      instance->Show(std::move(show_options));
    } else if (auto* tab_handle = std::get_if<tabs::TabHandle>(&surface)) {
      if (tabs::TabInterface* tab = tab_handle->Get()) {
        instance = GetOrCreateGlicInstanceImplForTab(tab);
        instance->Show(ShowOptions::ForSidePanel(
            *tab, GlicPinTrigger::kInstanceCreation, source));
      }
    }
    if (instance) {
      instance->instance_metrics().OnToggle(source, key, /*is_showing=*/false,
                                            std::move(invocation_tracker));
    }
    return;
  }

  GlicInvokeOptions invoke_options(source);
  invoke_options.target.surface = std::move(surface);
  invoke_options.fre_completion_wait_mode = FreCompletionWaitMode::kNever;
  if (!GlicEnabling::HasConsentedForProfile(profile_) &&
      (base::FeatureList::IsEnabled(features::kGlicMessageFirstFre) ||
       base::FeatureList::IsEnabled(features::kGlicActionFirstFRE))) {
    invoke_options.fre_override = mojom::FreOverride::kTrustFirstInline;
  }
  auto weak_instance = InvokeInternal(std::nullopt, std::move(invoke_options),
                                      GlicInvokeWithAutoSubmitOptions(),
                                      /*bypass_in_progress_check=*/true);
  if (weak_instance) {
    weak_instance->instance_metrics().OnToggle(
        source, key, /*is_showing=*/false, std::move(invocation_tracker));
  }
}

void GlicInstanceCoordinatorImpl::RemoveInstance(InstanceId id) {
  auto it = instances_.find(id);
  if (it == instances_.end()) {
    // This instance has already been removed, so there's no work to do.
    return;
  }
  GlicInstanceImpl* instance = it->second.get();
  OnInstanceActivationChanged(instance, false);
  actuating_changed_subscriptions_.erase(id);

  // Remove the instance first, and then delete. This way,
  // instances_ will not include the instance being deleted while
  // it's being deleted.
  instance->CloseInstanceAndShutdown();
  if (instance == last_active_instance_) {
    last_active_instance_ = nullptr;
  }
  auto instance_value = std::exchange(instances_[id], {});
  instances_.erase(id);
}

void GlicInstanceCoordinatorImpl::SwitchConversation(
    GlicInstanceImpl& source_instance,
    const ShowOptions& options,
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::SwitchConversationCallback callback) {
  ShowOptions mutable_options = options;
  mutable_options.focus_on_show = source_instance.HasFocus();
  mutable_options.reinitialize_if_already_active = true;
  mutable_options.invocation_source =
      mojom::InvocationSource::kConversationSwitch;

  GlicInstanceImpl* target_instance = nullptr;
  if (!info->conversation_id.empty()) {
    target_instance = GetInstanceImplForConversationId(info->conversation_id);
  }

  if (!target_instance) {
    // No instance exists for this conversation. If the current instance
    // already has a conversation, create a new instance. Otherwise, reuse
    // the current instance.
    target_instance = source_instance.conversation_id() ? CreateGlicInstance()
                                                        : &source_instance;
  }

  CHECK(target_instance);

  if (auto* side_panel_options = std::get_if<SidePanelShowOptions>(
          &mutable_options.embedder_options)) {
    // TODO(b/510405771): Remove animation suppression once bottom sheet hide is
    // cancelable.
    side_panel_options->suppress_opening_animation = true;
    side_panel_options->pin_trigger = GlicPinTrigger::kConversationChange;
    if (target_instance == &source_instance) {
      // If we are reusing the current instance in-place (as an optimization),
      // BindTab is not called again, so we must manually overwrite all
      // currently pinned tabs' pin triggers to kConversationChange to make the
      // pin trigger correct.
      for (auto* tab :
           target_instance->GetSharingManagerInternal().GetPinnedTabs()) {
        target_instance->GetSharingManagerInternal().SetPinTrigger(
            tab->GetHandle(), GlicPinTrigger::kConversationChange);
      }
    }
  }

  metrics_.RecordSwitchConversationTarget(
      !info->conversation_id.empty()
          ? std::optional<std::string>(info->conversation_id)
          : std::nullopt,
      target_instance->conversation_id(), active_instance_);

  target_instance->RegisterConversation(std::move(info), base::DoNothing());
  TransferTabGroupBinding(source_instance, *target_instance);
  target_instance->Show(mutable_options);
  target_instance->instance_metrics().OnSwitchToConversation(mutable_options);
  std::move(callback).Run(std::nullopt);
}

void GlicInstanceCoordinatorImpl::TransferTabGroupBinding(
    GlicInstanceImpl& source_instance,
    GlicInstanceImpl& target_instance) {
  std::optional<tab_groups::TabGroupId> group_id =
      source_instance.GetTabGroup();
  if (group_id.has_value() && &target_instance != &source_instance) {
    target_instance.BindTabGroup(*group_id);
  }
}

std::vector<glic::mojom::ConversationInfoPtr>
GlicInstanceCoordinatorImpl::GetRecentlyActiveConversations(size_t limit) {
  base::TimeDelta limit_delta = base::TimeDelta::Max();
  if (base::FeatureList::IsEnabled(kGlicMaxRecency)) {
    limit_delta = kGlicMaxRecencyValue.Get();
  }
  std::vector<GlicInstanceImpl*> sorted_instances =
      GetSortedRecentInstances(limit, limit_delta);

  std::vector<glic::mojom::ConversationInfoPtr> result;
  for (auto* instance : sorted_instances) {
    auto info = instance->GetConversationInfo();
    CHECK(info);
    result.push_back(std::move(info));
  }
  return result;
}

std::vector<ConversationInfo>
GlicInstanceCoordinatorImpl::GetRecentlyActiveInstances(
    size_t limit,
    base::TimeDelta max_time_since_active) {
  std::vector<GlicInstanceImpl*> sorted_instances =
      GetSortedRecentInstances(limit, max_time_since_active);

  std::vector<ConversationInfo> result;
  for (auto* instance : sorted_instances) {
    auto info = instance->GetConversationInfo();
    CHECK(info);
    result.push_back({instance->id(), info->conversation_title});
  }
  return result;
}

bool GlicInstanceCoordinatorImpl::IsTabPinnedToAnyInstance(
    const tabs::TabHandle& tab_handle) const {
  return std::ranges::any_of(instances_, [&](const auto& entry) {
    return entry.second->GetSharingManagerInternal().IsTabPinned(tab_handle);
  });
}

void GlicInstanceCoordinatorImpl::UnpinTabsFromAllInstances(
    base::span<const tabs::TabHandle> tab_handles,
    GlicUnpinTrigger trigger) {
  for (auto& entry : instances_) {
    entry.second->GetSharingManagerInternal().UnpinTabs(tab_handles, trigger);
  }
}

std::vector<GlicInstanceImpl*>
GlicInstanceCoordinatorImpl::GetSortedRecentInstances(
    size_t limit,
    base::TimeDelta max_time_since_active) const {
  // This will only cover recently active conversations that still have living
  // instances. If an instance is torn down because the user closed all bound
  // tabs, it will not be included in the list.
  std::vector<GlicInstanceImpl*> sorted_instances;
  for (auto& [id, instance] : instances_) {
    if (!instance->conversation_id()) {
      continue;
    }
    if (instance->GetTimeSinceLastActive() > max_time_since_active) {
      continue;
    }
    sorted_instances.push_back(instance.get());
  }

  std::sort(sorted_instances.begin(), sorted_instances.end(),
            [](GlicInstanceImpl* a, GlicInstanceImpl* b) {
              return a->GetLastActivationTimestamp() >
                     b->GetLastActivationTimestamp();
            });

  if (sorted_instances.size() > limit) {
    sorted_instances.resize(limit);
  }
  return sorted_instances;
}

void GlicInstanceCoordinatorImpl::UnbindTabFromAnyInstance(
    tabs::TabInterface* tab) {
  if (auto* instance = GetInstanceImplForTab(tab)) {
    instance->UnbindTab(tab);
  }
}

void GlicInstanceCoordinatorImpl::UnbindTabGroupFromAnyInstance(
    tab_groups::TabGroupId group_id,
    GlicInstanceImpl* excluding_instance) {
  for (const auto& [id, instance] : instances_) {
    if (instance.get() != excluding_instance &&
        instance->GetTabGroup() == group_id) {
      instance->UnbindTabGroup();
    }
  }
}

void GlicInstanceCoordinatorImpl::ContextAccessIndicatorChanged(
    GlicInstanceImpl& source_instance,
    bool enabled) {
  ComputeContentAccessIndicator();
}

std::unique_ptr<GlicWebContentsManager>
GlicInstanceCoordinatorImpl::CreateWebContentsManager() {
  metrics_.RecordCountAwakeOnContentsCreated();
  return web_contents_warming_pool_->TakeContainer();
}

void GlicInstanceCoordinatorImpl::OnInstanceActuatingChanged(bool actuating) {
  if (!actuating) {
    return;
  }
  metrics_.RecordCountActuatingOnTaskCreation();
}

void GlicInstanceCoordinatorImpl::SetWarmingEnabledForTesting(
    bool warming_enabled) {
  warming_enabled_ = warming_enabled;
}

GlicInstanceImpl* GlicInstanceCoordinatorImpl::GetInstanceWithFloaty() const {
  for (const auto& [unused, instance] : instances_) {
    if (instance->GetPanelState().kind == mojom::PanelStateKind::kDetached) {
      return instance.get();
    }
  }
  return nullptr;
}

void GlicInstanceCoordinatorImpl::OnWillCreateFloaty() {
  CloseFloaty();
}

void GlicInstanceCoordinatorImpl::OnTabEvent(const GlicTabEvent& event) {
  if (auto* grouped_event = std::get_if<TabGroupingChangedEvent>(&event)) {
    if (grouped_event->is_added) {
      if (auto group_id = grouped_event->tab->GetGroup()) {
        if (auto* instance = GetInstanceImplForTabGroup(group_id.value())) {
          instance->OnTabGroupingChanged(grouped_event->tab, /*is_added=*/true);
        }
      }
    } else {
      for (const auto& [id, instance] : instances_) {
        instance->OnTabGroupingChanged(grouped_event->tab, /*is_added=*/false);
      }
    }
    return;
  }

  auto* creation_event = std::get_if<TabCreationEvent>(&event);
  if (!creation_event) {
    return;
  }

  if (auto* restore_data = GetTabRestoreData(*creation_event)) {
    RestoreTab(creation_event->new_tab->GetContents(), restore_data->state());
    return;
  }

  MaybeDaisyChainNewTab(*creation_event);

  MaybeDaisyChainFromLinkClick(*creation_event);

  MaybeDaisyChainFromBookmark(*creation_event);
}

void GlicInstanceCoordinatorImpl::MaybeDaisyChainFromLinkClick(
    const TabCreationEvent& event) {
  if (event.creation_type != TabCreationType::kFromLink || !event.opener ||
      !event.new_tab) {
    return;
  }

  auto* instance = GetInstanceImplForTab(event.opener);
  if (!instance) {
    return;
  }

  instance->MaybeDaisyChainToTab(event.opener, event.new_tab,
                                 DaisyChainSource::kTabContents);
}

void GlicInstanceCoordinatorImpl::MaybeDaisyChainFromBookmark(
    const TabCreationEvent& event) {
  if (event.creation_type != TabCreationType::kFromBookmark || !event.old_tab ||
      !event.new_tab) {
    return;
  }

  auto* instance = GetInstanceImplForTab(event.old_tab);
  if (!instance) {
    return;
  }

  instance->MaybeDaisyChainToTab(event.old_tab, event.new_tab,
                                 DaisyChainSource::kBookmark);
}

void GlicInstanceCoordinatorImpl::MaybeDaisyChainNewTab(

    const TabCreationEvent& creation_event) {
  if (!base::FeatureList::IsEnabled(features::kGlicDaisyChainNewTabs)) {
    return;
  }

  if (creation_event.creation_type != TabCreationType::kUserInitiated ||
      !creation_event.old_tab || !creation_event.new_tab) {
    return;
  }

  PrefService* pref_service = profile_->GetPrefs();
  if (!pref_service ||
      !pref_service->GetBoolean(
          glic::prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled)) {
    return;
  }

  if (!GlicSidePanelCoordinator::IsGlicSidePanelActive(
          creation_event.old_tab)) {
    return;
  }

  auto* instance = CreateGlicInstance();
  SidePanelShowOptions side_panel_options{*creation_event.new_tab};
  side_panel_options.suppress_opening_animation = true;
  side_panel_options.pin_trigger = GlicPinTrigger::kNewTabDaisyChain;
  auto show_options = ShowOptions{side_panel_options};
  show_options.invocation_source = mojom::InvocationSource::kDaisyChainOnNewTab;
  instance->Show(show_options);

  instance->instance_metrics().OnDaisyChain(
      DaisyChainSource::kNewTab,
      /*success=*/true, creation_event.new_tab, creation_event.old_tab);
}

void GlicInstanceCoordinatorImpl::OnMemoryPressure(
    base::MemoryPressureLevel level) {
  const int memory_limit = GetMemoryLimit();

  metrics_.OnMemoryPressure(memory_limit);
  web_contents_warming_pool_->OnMemoryPressure(memory_limit);

  if (!base::FeatureList::IsEnabled(kGlicMaxAwakeInstances) ||
      !base::FeatureList::IsEnabled(base::kStatefulMemoryPressure)) {
    if (memory_limit <= base::kCriticalMemoryPressureThreshold) {
      TrimAwakeInstancesTo(0u);
    }
    return;
  }

  // Both features are enabled; dynamically trim awake instances to the limit
  // configured for the current memory limit.
  TrimAwakeInstancesTo(GetCurrentMaxAwakeInstancesLimit());
}

GlicInstanceImpl* GlicInstanceCoordinatorImpl::GetOrRestoreInstanceImpl(
    const GlicRestoredState::InstanceInfo& instance_info) {
  InstanceId instance_id(instance_info.instance_id);
  if (!instance_id.IsValid()) {
    return nullptr;
  }

  GlicInstanceImpl* instance = nullptr;
  if (!instance_info.conversation_id.empty()) {
    instance = GetInstanceImplForConversationId(instance_info.conversation_id);
    if (!instance) {
      // If lookup by conversation ID failed, but an instance with this ID
      // already exists, it implies an attempt to associate an existing instance
      // with a different conversation ID. Once an instance is associated with a
      // conversation ID, it cannot change. This indicates corrupt persisted
      // data or a logic bug. Return nullptr to avoid dangerously overwriting
      // the instance.
      if (GetInstanceImplFor(instance_id)) {
        LOG(ERROR) << "Instance restoration failed for conversation "
                   << instance_info.conversation_id
                   << ": The requested InstanceId " << instance_info.instance_id
                   << " already exists but is associated with a different "
                      "conversation.";
        return nullptr;
      }
    }
  } else {
    instance = GetInstanceImplFor(instance_id);
  }

  if (instance) {
    return instance;
  }

  // No instance could be found for the instance and conversation
  // id, so create a new instance with the instance and conversation ids.
  auto* target_instance = CreateGlicInstance(instance_id);

  auto info = mojom::ConversationInfo::New();
  info->conversation_id = instance_info.conversation_id;
  target_instance->RegisterConversation(std::move(info), base::DoNothing());
  return target_instance;
}

void GlicInstanceCoordinatorImpl::RestoreTab(
    content::WebContents* web_contents,
    const glic::GlicRestoredState& state) {
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
  if (!tab) {
    LOG(ERROR) << "Tab is null";
    return;
  }
  if (!GlicInstanceHelper::From(tab)) {
    LOG(ERROR) << "Tab doesn't have an instance helper in its UnownedUserData";
    return;
  }

  // `pin_on_bind` is set to `false` to prevent auto-pinning
  // during restoration, as explicit pinned state is handled separately.
  if (auto* bound_instance = GetOrRestoreInstanceImpl(state.bound_instance)) {
    if (state.side_panel_open) {
      auto side_panel_options = SidePanelShowOptions(*tab);
      side_panel_options.suppress_opening_animation = true;
      side_panel_options.pin_on_bind = false;
      side_panel_options.prefer_peek = true;
      auto show_options = ShowOptions{side_panel_options};
      show_options.invocation_source = mojom::InvocationSource::kTabRestore;
      bound_instance->Show(show_options);
    } else {
      bound_instance->BindTabWithoutShowing(tab, GlicPinTrigger::kUnknown,
                                            /*pin_on_bind=*/false);
    }
  }

  for (const auto& pinned_instance_info : state.pinned_instances) {
    if (auto* pinned_instance =
            GetOrRestoreInstanceImpl(pinned_instance_info)) {
      // `GlicPinTrigger::kRestore` is used to prevent auto-binding during this
      // pinning process.
      pinned_instance->GetSharingManagerInternal().PinTabs(
          {tab->GetHandle()}, GlicPinTrigger::kRestore);
    }
  }
}

void GlicInstanceCoordinatorImpl::MaybeStopListeningFloaty(
    GlicInstanceImpl* instance) {
  if (!instance) {
    return;
  }
  auto* floaty_instance = GetInstanceWithFloaty();
  if (!floaty_instance || instance == floaty_instance) {
    return;
  }

  // Another instance has become active, so stop the floaty instance
  // from listening to ensure a single active instance.
  if (floaty_instance->host().microphone_status() ==
      mojom::MicrophoneStatus::kListening) {
    floaty_instance->host().StopMicrophone(base::DoNothing());
  }
}

std::string GlicInstanceCoordinatorImpl::DescribeForTesting() {
  std::stringstream ss;
  for (auto& inst : instances_) {
    ss << inst.second->DescribeForTesting();  // IN-TEST
  }
  return ss.str();
}

}  // namespace glic
