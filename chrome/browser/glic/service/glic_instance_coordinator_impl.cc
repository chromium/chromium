// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"

#include <algorithm>
#include <cstdint>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/glic/common/future_browser_features.h"
#include "chrome/browser/glic/common/glic_tab_observer.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/metrics/glic_instance_coordinator_metrics.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace glic {

namespace {
constexpr base::TimeDelta kFloatyMaxRecency = base::Hours(3);

BASE_FEATURE(kGlicMaxRecency, base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<base::TimeDelta> kGlicMaxRecencyValue{
    &kGlicMaxRecency, "duration", base::Minutes(30)};

GlicTabRestoreData* GetTabRestoreData(const TabCreationEvent& creation_event) {
  if (!base::FeatureList::IsEnabled(features::kGlicTabRestoration)) {
    return nullptr;
  }
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
}  // namespace

BASE_FEATURE(kGlicHibernateOnMemoryUsage, base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<int> kGlicHibernateMemoryThresholdMb{
    &kGlicHibernateOnMemoryUsage, "threshold_mb", 800};
constexpr base::FeatureParam<base::TimeDelta>
    kGlicHibernateMemoryPollingInterval{&kGlicHibernateOnMemoryUsage,
                                        "polling_interval", base::Minutes(10)};

BASE_FEATURE(kGlicMaxAwakeInstances, base::FEATURE_ENABLED_BY_DEFAULT);
constexpr base::FeatureParam<int> kGlicMaxAwakeInstancesLimit{
    &kGlicMaxAwakeInstances, "limit", 15};

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
          std::make_unique<GlicWebContentsWarmingPool>(profile)) {
  if (identity_manager) {
    identity_manager_observation_.Observe(identity_manager);
  }
  if (base::FeatureList::IsEnabled(features::kGlicDaisyChainNewTabs) ||
      base::FeatureList::IsEnabled(features::kGlicTabRestoration) ||
      base::FeatureList::IsEnabled(features::kGlicDaisyChainViaCoordinator)) {
    tab_observer_ = GlicTabObserver::Create(
        profile_, base::BindRepeating(&GlicInstanceCoordinatorImpl::OnTabEvent,
                                      weak_ptr_factory_.GetWeakPtr()));
  }
  if (base::FeatureList::IsEnabled(kGlicHibernateOnMemoryUsage)) {
    memory_monitor_timer_.Start(
        FROM_HERE, kGlicHibernateMemoryPollingInterval.Get(),
        base::BindRepeating(&GlicInstanceCoordinatorImpl::CheckMemoryUsage,
                            base::Unretained(this)));
  }
}

GlicInstanceCoordinatorImpl::~GlicInstanceCoordinatorImpl() {
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
  NotifyActiveInstanceChanged();
  ComputeContentAccessIndicator();
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

std::vector<GlicInstance*> GlicInstanceCoordinatorImpl::GetInstances() {
  std::vector<GlicInstance*> instances;
  for (auto& entry : instances_) {
    instances.push_back(entry.second.get());
  }
  return instances;
}

GlicInstance* GlicInstanceCoordinatorImpl::GetInstanceForTab(
    const tabs::TabInterface* tab) const {
  return GetInstanceImplForTab(tab);
}

void GlicInstanceCoordinatorImpl::CreateNewConversationForTabs(
    const std::vector<tabs::TabInterface*>& tabs) {
  if (tabs.empty()) {
    return;
  }

  GlicInstanceImpl* instance = CreateGlicInstance();
  ShowInstanceForTabs(instance, tabs, GlicPinTrigger::kContextMenu);
}

void GlicInstanceCoordinatorImpl::ShowInstanceForTabs(
    const std::vector<tabs::TabInterface*>& tabs,
    const InstanceId& instance_id) {
  auto* target_instance = GetInstanceImplFor(instance_id);

  if (!target_instance) {
    return;
  }

  ShowInstanceForTabs(target_instance, tabs, GlicPinTrigger::kContextMenu);
}

void GlicInstanceCoordinatorImpl::Toggle(
    BrowserWindowInterface* browser,
    bool prevent_close,
    mojom::InvocationSource source,
    std::optional<std::string> deprecated_prompt_suggestion) {
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
      ToggleFloaty(prevent_close, source, deprecated_prompt_suggestion);
      return;
    }
  }

  ToggleSidePanel(browser, prevent_close, source, deprecated_prompt_suggestion);
}

void GlicInstanceCoordinatorImpl::EnsurePreload() {
  web_contents_warming_pool_->EnsurePreload();
}

void GlicInstanceCoordinatorImpl::Shutdown() {
  for (auto& [instance_id, instance] : instances_) {
    instance->Shutdown();
  }
  web_contents_warming_pool_->Clear();
}

void GlicInstanceCoordinatorImpl::Close(const CloseOptions& options) {
  // TODO(crbug.com/450286204): Determine whether there are cases where this
  // should be able to close a side panel UI instead.
  CloseFloaty(options);
}

void GlicInstanceCoordinatorImpl::RemoveAllInstances() {
  while (!instances_.empty()) {
    RemoveInstance(instances_.begin()->second.get());
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

void GlicInstanceCoordinatorImpl::GetExperimentalTriggeringUpdates(
    mojo::PendingRemote<mojom::ExperimentalTriggeringUpdatesHandler> handler,
    base::OnceCallback<void(bool)> success_status_callback) {
  if (active_instance_) {
    active_instance_->host().GetExperimentalTriggeringUpdates(
        std::move(handler), std::move(success_status_callback));
  } else {
    std::move(success_status_callback).Run(false);
  }
}

base::WeakPtr<GlicInstance> GlicInstanceCoordinatorImpl::InvokeInternal(
    std::optional<InvokeWithAutoSubmitPasskey> auto_submit_passkey,
    GlicInvokeOptions options,
    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
  GlicInvokeHandler::ResolvedTarget resolved_target =
      GlicInvokeHandler::ResolveTargetSurface(profile_, options.target);
  tabs::TabInterface* tab = resolved_target.tab;
  options.target.surface = tab;

  if (!tab || !GlicInstanceHelper::From(tab)) {
    if (options.on_error) {
      std::move(options.on_error).Run(GlicInvokeError::kInvalidTab);
    }
    // TODO(crbug.com/483387751): Show default toast here once implemented.
    return nullptr;
  }

  GlicInstanceImpl* instance = nullptr;

  instance = std::visit(
      absl::Overload{[&](const ConversationId& conv_id) {
                       if (conv_id.conversation_id.empty()) {
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
                     [&](DefaultConversation) {
                       return GetOrCreateGlicInstanceImplForTab(tab);
                     }},
      options.target.conversation);

  if (!instance) {
    return nullptr;
  }

  if (invoke_handlers_.contains(instance)) {
    if (options.on_error) {
      std::move(options.on_error).Run(GlicInvokeError::kInvokeInProgress);
    }
    // TODO(crbug.com/483387751): Show default toast here once implemented.
    return nullptr;
  }

  invoke_handlers_[instance] = std::make_unique<GlicInvokeHandler>(
      *instance, resolved_target, std::move(options),
      std::move(auto_submit_options), auto_submit_passkey,
      base::BindOnce(&GlicInstanceCoordinatorImpl::OnInvokeHandlerComplete,
                     base::Unretained(this)));
  invoke_handlers_[instance]->Invoke();

  return instance->GetWeakPtr();
}

void GlicInstanceCoordinatorImpl::OnInvokeHandlerComplete(
    GlicInstance* instance,
    GlicInvokeHandler* handler) {
  invoke_handlers_.erase(instance);
}

void GlicInstanceCoordinatorImpl::CloseAndShutdownInstanceWithFrame(
    content::RenderFrameHost* render_frame_host) {
  for (auto* instance : GetInstances()) {
    if (instance &&
        instance->host().IsWebContentPresentAndMatches(render_frame_host)) {
      instance->host().Close();
      instance->host().Shutdown();
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
      RemoveInstance(instance.get());
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
  if (const auto* instance = GetInstanceForTab(
          TabListInterface::From(const_cast<BrowserWindowInterface*>(&bwi))
              ->GetActiveTab())) {
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

GlicInstanceImpl* GlicInstanceCoordinatorImpl::GetInstanceImplForConversationId(
    const std::string& conversation_id) {
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

  if (base::FeatureList::IsEnabled(
          features::kGlicDefaultToLastActiveConversation) &&
      last_active_instance_ &&
      last_active_instance_->GetTimeSinceLastActive() <
          features::kGlicDefaultToLastActiveConversationMaxRecency.Get() &&
      !last_active_instance_->IsActuating()) {
    last_active_instance_->instance_metrics().OnDaisyChain(
        DaisyChainSource::kLastActiveInstance,
        /*success=*/true, tab,
        /*source_tab=*/nullptr);
    return last_active_instance_;
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

void GlicInstanceCoordinatorImpl::ApplyMaxAwakeInstancesLimit() {
  if (base::FeatureList::IsEnabled(kGlicMaxAwakeInstances)) {
    size_t awake_count = 0;
    for (const auto& [id, instance] : instances_) {
      if (!instance->IsHibernated()) {
        awake_count++;
      }
    }

    const size_t limit = kGlicMaxAwakeInstancesLimit.Get();
    if (awake_count < limit) {
      return;
    }

    std::vector<GlicInstanceImpl*> hibernatable_instances;
    for (auto& [id, instance] : instances_) {
      if (!instance->IsHibernated() && !instance->IsActuating()) {
        hibernatable_instances.push_back(instance.get());
      }
    }

    // Sort candidates by time since last active (descending = oldest first).
    std::sort(hibernatable_instances.begin(), hibernatable_instances.end(),
              [](const GlicInstanceImpl* a, const GlicInstanceImpl* b) {
                return a->GetTimeSinceLastActive() >
                       b->GetTimeSinceLastActive();
              });

    // Hibernate until we reach `limit - 1`.
    int target_count = limit - 1;
    size_t excess_count = awake_count - target_count;

    for (size_t i = 0; i < excess_count && i < hibernatable_instances.size();
         ++i) {
      hibernatable_instances[i]->Hibernate();
    }
  }
}

GlicInstanceImpl* GlicInstanceCoordinatorImpl::CreateGlicInstance(
    std::optional<InstanceId> instance_id) {
  ApplyMaxAwakeInstancesLimit();

  auto instance = CreateInstanceImpl(instance_id);
  auto* instance_ptr = instance.get();
  instances_[instance->id()] = std::move(instance);
  // TODO(harringtond): Figure out what to do about this metric.
  instance_ptr->instance_metrics().OnInstanceCreatedWithoutWarming();
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

void GlicInstanceCoordinatorImpl::ShowInstanceForTabs(
    GlicInstanceImpl* instance,
    const std::vector<tabs::TabInterface*>& tabs,
    GlicPinTrigger pin_trigger) {
  for (tabs::TabInterface* tab : tabs) {
    SidePanelShowOptions side_panel_options(*tab);
    side_panel_options.pin_trigger = pin_trigger;
    ShowOptions show_opts(side_panel_options);
    show_opts.focus_on_show =
        IsActive(tab->GetBrowserWindowInterface()) && tab->IsActivated();
    // Explicitly pin the tabs for the context menu trigger.
    if (pin_trigger == GlicPinTrigger::kContextMenu) {
      instance->sharing_manager().PinTabs({tab->GetHandle()}, pin_trigger);
    }
    instance->Show(show_opts);
  }
}

GlicInstanceImpl*
GlicInstanceCoordinatorImpl::GetOrCreateInstanceImplForFloaty() {
  auto* floaty_instance = GetInstanceWithFloaty();
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

void GlicInstanceCoordinatorImpl::ToggleFloaty(
    bool prevent_close,
    glic::mojom::InvocationSource source,
    std::optional<std::string> prompt_suggestion) {
  CHECK(GlicEnabling::IsLiveAndFloatyEnabledByFlags());
  GetOrCreateInstanceImplForFloaty()->Toggle(
      ShowOptions::ForFloating(/*source_tab=*/tabs::TabHandle::Null()),
      prevent_close, source, prompt_suggestion);
}

void GlicInstanceCoordinatorImpl::ToggleSidePanel(
    BrowserWindowInterface* browser,
    bool prevent_close,
    mojom::InvocationSource source,
    std::optional<std::string> prompt_suggestion) {
  auto* tab = TabListInterface::From(browser)->GetActiveTab();
  if (!tab) {
    LOG(ERROR) << "Active tab is null";
    return;
  }
  if (!GlicInstanceHelper::From(tab)) {
    LOG(ERROR) << "Tab doesn't have an instance helper in its UnownedUserData";
    return;
  }

  GlicInstanceImpl* instance = nullptr;

  if (source == glic::mojom::InvocationSource::kSharedImage) {
    // kSharedImage currently requires a new instance.
    instance = CreateGlicInstance();
  } else {
    instance = GetOrCreateGlicInstanceImplForTab(tab);
  }
  // If the tab is already bound, then it already has a pin trigger and this pin
  // trigger will not be used. If it's not already bound, then we know it's a
  // newly created instance, so we provide the instance creation trigger.
  ShowOptions options = ShowOptions::ForSidePanel(
      *tab, GlicPinTrigger::kInstanceCreation, source);

  instance->Toggle(std::move(options), prevent_close, source,
                   prompt_suggestion);
}

void GlicInstanceCoordinatorImpl::RemoveInstance(GlicInstanceImpl* instance) {
  if (!instances_.contains(instance->id())) {
    // This instance has already been removed, so there's no work to do.
    return;
  }
  OnInstanceActivationChanged(instance, false);

  // Remove the instance first, and then delete. This way, GetInstances() will
  // not return the instance being deleted while it's being deleted.
  InstanceId id = instance->id();
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

  // TODO(b/510405771): Remove animation suppression once bottom sheet hide is
  // cancelable.
  if (auto* side_panel_options = std::get_if<SidePanelShowOptions>(
          &mutable_options.embedder_options)) {
    side_panel_options->suppress_opening_animation = true;
  }

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

  metrics_.RecordSwitchConversationTarget(
      !info->conversation_id.empty()
          ? std::optional<std::string>(info->conversation_id)
          : std::nullopt,
      target_instance->conversation_id(), active_instance_);

  target_instance->RegisterConversation(std::move(info), base::DoNothing());
  target_instance->Show(mutable_options);
  target_instance->instance_metrics().OnSwitchToConversation(mutable_options);
  std::move(callback).Run(std::nullopt);
}

std::vector<glic::mojom::ConversationInfoPtr>
GlicInstanceCoordinatorImpl::GetRecentlyActiveConversations(size_t limit) {
  std::vector<GlicInstanceImpl*> sorted_instances =
      GetSortedRecentInstances(limit);

  std::vector<glic::mojom::ConversationInfoPtr> result;
  for (auto* instance : sorted_instances) {
    auto info = instance->GetConversationInfo();
    CHECK(info);
    result.push_back(std::move(info));
  }
  return result;
}

std::vector<ConversationInfo>
GlicInstanceCoordinatorImpl::GetRecentlyActiveInstances(size_t limit) {
  std::vector<GlicInstanceImpl*> sorted_instances =
      GetSortedRecentInstances(limit);

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
    return entry.second->sharing_manager().IsTabPinned(tab_handle);
  });
}

void GlicInstanceCoordinatorImpl::UnpinTabsFromAllInstances(
    base::span<const tabs::TabHandle> tab_handles,
    GlicUnpinTrigger trigger) {
  for (auto& entry : instances_) {
    entry.second->sharing_manager().UnpinTabs(tab_handles, trigger);
  }
}

std::vector<GlicInstanceImpl*>
GlicInstanceCoordinatorImpl::GetSortedRecentInstances(size_t limit) const {
  // This will only cover recently active conversations that still have living
  // instances. If an instance is torn down because the user closed all bound
  // tabs, it will not be included in the list.
  std::vector<GlicInstanceImpl*> sorted_instances;
  for (auto& [id, instance] : instances_) {
    if (!instance->conversation_id()) {
      continue;
    }
    if (base::FeatureList::IsEnabled(kGlicMaxRecency) &&
        instance->GetTimeSinceLastActive() > kGlicMaxRecencyValue.Get()) {
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
    // `instance` may be deleted after this call.
    instance->UnbindEmbedder(EmbedderKey(tab));
  }
}

void GlicInstanceCoordinatorImpl::ContextAccessIndicatorChanged(
    GlicInstanceImpl& source_instance,
    bool enabled) {
  ComputeContentAccessIndicator();
}

std::unique_ptr<WebUIContentsContainer>
GlicInstanceCoordinatorImpl::CreateWebUIContentsContainer() {
  return web_contents_warming_pool_->TakeContainer();
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
}

void GlicInstanceCoordinatorImpl::MaybeDaisyChainFromLinkClick(
    const TabCreationEvent& event) {
  if (!base::FeatureList::IsEnabled(features::kGlicDaisyChainViaCoordinator)) {
    return;
  }

  if (event.creation_type != TabCreationType::kFromLink || !event.opener ||
      !event.new_tab) {
    return;
  }

  auto* instance = GetInstanceImplForTab(event.opener);
  if (!instance) {
    return;
  }

  instance->MaybeDaisyChainToTab(event.opener, event.new_tab);
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
  instance->Show(ShowOptions{side_panel_options});

  instance->instance_metrics().OnDaisyChain(
      DaisyChainSource::kNewTab,
      /*success=*/true, creation_event.new_tab, creation_event.old_tab);
}

void GlicInstanceCoordinatorImpl::OnMemoryPressure(
    base::MemoryPressureLevel level) {
  metrics_.OnMemoryPressure(level);

  if (level < base::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    return;
  }

  web_contents_warming_pool_->Clear();

  for (auto& [_, instance] : instances_) {
    if (instance->IsShowing() || instance->IsActuating() ||
        instance->IsHibernated()) {
      continue;
    }
    instance->Hibernate();
  }
}

void GlicInstanceCoordinatorImpl::CheckMemoryUsage() {
  struct ProcessInfo {
    uint64_t total_private_footprint_bytes = 0;
    std::vector<GlicInstanceImpl*> instances;
  };

  std::map<content::RenderProcessHost*, ProcessInfo> process_info_map;

  // Group instances by RenderProcessHost and sum up memory.
  for (auto const& [_, instance] : instances_) {
    content::RenderProcessHost* process =
        instance->host().GetWebClientRenderProcessHost();
    if (!process) {
      continue;
    }
    auto& info = process_info_map[process];
    info.instances.push_back(instance.get());
    // Only fetch memory once per process.
    if (info.total_private_footprint_bytes == 0) {
      info.total_private_footprint_bytes = process->GetPrivateMemoryFootprint();
    }
  }

  uint64_t threshold_bytes =
      static_cast<uint64_t>(kGlicHibernateMemoryThresholdMb.Get()) * 1024 *
      1024;

  for (const auto& [process, info] : process_info_map) {
    if (info.instances.empty()) {
      continue;
    }
    uint64_t average_memory_bytes =
        info.total_private_footprint_bytes / info.instances.size();

    if (average_memory_bytes < threshold_bytes) {
      continue;
    }
    for (GlicInstanceImpl* instance : info.instances) {
      if (instance->IsHibernated() || instance->IsActuating()) {
        continue;
      }
      metrics_.OnHighMemoryUsage(average_memory_bytes / 1024 / 1024);
      if (instance->IsShowing()) {
        // Only reload if the page has finished loading to avoid reload loops
        // during high load or slow startup.
        if (instance->host().IsPrimaryClientOpen()) {
          instance->host().Reload();
        }
      } else {
        instance->Hibernate();
      }
    }
  }
}

GlicInstanceImpl* GlicInstanceCoordinatorImpl::GetOrRestoreInstanceImpl(
    const GlicRestoredState::InstanceInfo& instance_info) {
  InstanceId instance_id(instance_info.instance_id);
  if (!instance_id.IsValid()) {
    return nullptr;
  }

  // Prioritize finding an existing instance by conversation ID, then by
  // instance ID.
  if (auto* instance =
          !instance_info.conversation_id.empty()
              ? GetInstanceImplForConversationId(instance_info.conversation_id)
              : GetInstanceImplFor(instance_id)) {
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
      bound_instance->Show(ShowOptions{side_panel_options});
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
      pinned_instance->sharing_manager().PinTabs({tab->GetHandle()},
                                                 GlicPinTrigger::kRestore);
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
