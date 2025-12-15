// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/notimplemented.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/browser_ui/scoped_glic_button_indicator.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/fre/glic_fre_dialog_view.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom-data-view.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/glic/service/metrics/glic_instance_coordinator_metrics.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/glic/widget/glic_side_panel_ui.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_config.h"
#include "chrome/browser/glic/widget/glic_window_controller_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/display/screen.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget_observer.h"

namespace glic {

namespace {
constexpr base::TimeDelta kSidePanelMaxRecency = base::Minutes(20);
constexpr base::TimeDelta kFloatyMaxRecency = base::Hours(3);

BASE_FEATURE(kGlicMaxRecency, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGlicLiveModeOnlyGlow, base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<base::TimeDelta> kGlicMaxRecencyValue{
    &kGlicMaxRecency, "duration", base::Minutes(30)};

base::TimeDelta GetTimeSinceLastActive(GlicInstanceImpl* instance) {
  return base::TimeTicks::Now() - instance->GetLastActiveTime();
}
}  // namespace

// TODO(refactor): Remove after launching kGlicMultiInstance.
HostManager& GlicInstanceCoordinatorImpl::host_manager() {
  return *host_manager_;
}

GlicInstanceCoordinatorImpl::GlicInstanceCoordinatorImpl(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    GlicKeyedService* service,
    GlicEnabling* enabling,
    contextual_cueing::ContextualCueingService* contextual_cueing_service)
    : profile_(profile),
      service_(service),
      contextual_cueing_service_(contextual_cueing_service),
      memory_pressure_listener_registration_(
          FROM_HERE,
          base::MemoryPressureListenerTag::kGlicKeyedService,
          this),
      metrics_(this) {
  if (base::FeatureList::IsEnabled(features::kGlicDaisyChainNewTabs)) {
    tab_creation_observer_ = std::make_unique<GlicTabCreationObserver>(
        profile_,
        base::BindRepeating(&GlicInstanceCoordinatorImpl::OnTabCreated,
                            weak_ptr_factory_.GetWeakPtr()));
  }
  host_manager_ = std::make_unique<HostManager>(profile, GetWeakPtr());
}

GlicInstanceCoordinatorImpl::~GlicInstanceCoordinatorImpl() {
  // Delete all instances before destruction. Destroying web contents can result
  // in various calls to dependencies.
  active_instance_ = nullptr;
  last_active_instance_ = nullptr;
  auto instances = std::exchange(instances_, {});
  instances.clear();
  warmed_instance_.reset();
}

void GlicInstanceCoordinatorImpl::OnInstanceActivationChanged(
    GlicInstanceImpl* instance,
    bool is_active) {
  if (is_active && active_instance_ != instance) {
    active_instance_ = instance;
    last_active_instance_ = active_instance_;
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

void GlicInstanceCoordinatorImpl::NotifyActiveInstanceChanged() {
  active_instance_changed_callback_list_.Notify(active_instance_);
}

void GlicInstanceCoordinatorImpl::ComputeContentAccessIndicator() {
  if (active_instance_) {
    if (base::FeatureList::IsEnabled(kGlicLiveModeOnlyGlow)) {
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
  if (warmed_instance_) {
    instances.push_back(warmed_instance_.get());
  }
  for (auto& entry : instances_) {
    instances.push_back(entry.second.get());
  }
  return instances;
}

GlicInstance* GlicInstanceCoordinatorImpl::GetInstanceForTab(
    const tabs::TabInterface* tab) const {
  return GetInstanceImplForTab(tab);
}

void GlicInstanceCoordinatorImpl::Toggle(
    BrowserWindowInterface* browser,
    bool prevent_close,
    mojom::InvocationSource source,
    std::optional<std::string> prompt_suggestion) {
  if (!browser) {
    ToggleFloaty(prevent_close, source);
    return;
  }

  ToggleSidePanel(browser, prevent_close, source);
}

void GlicInstanceCoordinatorImpl::ShowAfterSignIn(
    base::WeakPtr<Browser> browser) {
  // TODO(crbug/4263869): Used by GlicPageHandler::SignInAndClosePanel(), which
  // should close glic and reopen it after signin is complete. This flow likely
  // still makes sense for the floating panel, but not for the side panel.
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::Shutdown() {
  host_manager().Shutdown();
}

void GlicInstanceCoordinatorImpl::Close() {
  // TODO(crbug.com/450286204): Determine whether there are cases where this
  // should be able to close a side panel UI instead.
  CloseFloaty();
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

void GlicInstanceCoordinatorImpl::CloseFloaty() {
  if (auto* floaty_instance = GetInstanceWithFloaty()) {
    floaty_instance->Close(FloatingEmbedderKey{});
  }
}

void GlicInstanceCoordinatorImpl::AddGlobalStateObserver(
    StateObserver* observer) {
  // TODO(b:448604727): The StateObserver needs to be split into two: one for if
  // the floating window is showing and one for the state of an individual
  // panel.
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::RemoveGlobalStateObserver(
    StateObserver* observer) {
  // TODO(b:448604727): The StateObserver needs to be split into two: one for if
  // the floating window is showing and one for the state of an individual
  // panel.
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::SetDraggableRegion(
    const SkRegion& draggable_region) {
  if (auto* floaty_instance = GetInstanceWithFloaty()) {
    floaty_instance->host().SetPanelDraggableRegion(draggable_region);
  }
}

bool GlicInstanceCoordinatorImpl::IsDetached() const {
  return GetInstanceWithFloaty() != nullptr;
}

bool GlicInstanceCoordinatorImpl::IsPanelShowingForBrowser(
    const BrowserWindowInterface& bwi) const {
  if (const auto* instance = GetInstanceForTab(
          const_cast<BrowserWindowInterface&>(bwi).GetActiveTabInterface())) {
    return instance->IsShowing();
  }
  return false;
}

base::CallbackListSubscription
GlicInstanceCoordinatorImpl::AddWindowActivationChangedCallback(
    WindowActivationChangedCallback callback) {
  // TODO: Notification of this callback list is not yet implemented.
  return window_activation_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicInstanceCoordinatorImpl::AddGlobalShowHideCallback(
    base::RepeatingClosure callback) {
  return global_show_hide_callback_list_.Add(std::move(callback));
}

void GlicInstanceCoordinatorImpl::Preload() {
  if (warming_enabled_) {
    CreateWarmedInstance();
    warmed_instance_->metrics()->OnWarmedInstanceCreated();
  } else {
    VLOG(1) << "Warming is disabled, skipping warming";
  }
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

GlicWidget* GlicInstanceCoordinatorImpl::GetGlicWidget() const {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return nullptr;
}

Browser* GlicInstanceCoordinatorImpl::attached_browser() {
  // Method should only be called on individual panels not the coordinator.
  // TODO: This can be called today, but it should not be.
  NOTIMPLEMENTED();
  return nullptr;
}

GlicWindowController::State GlicInstanceCoordinatorImpl::state() const {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return GlicWindowController::State::kClosed;
}

Profile* GlicInstanceCoordinatorImpl::profile() {
  return profile_;
}

gfx::Rect GlicInstanceCoordinatorImpl::GetInitialBounds(Browser* browser) {
  // TODO(crbug.com/441546104) - Remove from GlicWindowController.
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void GlicInstanceCoordinatorImpl::ShowDetachedForTesting() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::SetPreviousPositionForTesting(
    gfx::Point position) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
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

GlicInstanceImpl*
GlicInstanceCoordinatorImpl::GetOrCreateGlicInstanceImplForTab(
    tabs::TabInterface* tab) {
  if (GlicInstanceImpl* instance = GetInstanceImplForTab(tab)) {
    return instance;
  }

  if (base::FeatureList::IsEnabled(
          features::kGlicDefaultToLastActiveConversation) &&
      last_active_instance_ &&
      GetTimeSinceLastActive(last_active_instance_) < kSidePanelMaxRecency) {
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

GlicInstanceImpl* GlicInstanceCoordinatorImpl::CreateGlicInstance() {
  if (!warmed_instance_) {
    CreateWarmedInstance();
    // Records a just-in-time instance creation when warming is disabled or
    // failed.
    warmed_instance_->metrics()->OnInstanceCreatedWithoutWarming();
  }
  auto* instance_ptr = warmed_instance_.get();
  instances_[instance_ptr->id()] = std::move(warmed_instance_);
  // Records the promotion of an instance to an active one.
  instance_ptr->metrics()->OnInstancePromoted();
  if (warming_enabled_) {
    CreateWarmedInstance();
    // Records the creation of a new warmed instance to replace the promoted
    // one.
    warmed_instance_->metrics()->OnWarmedInstanceCreated();
  } else {
    VLOG(1) << "Warming is disabled, skipping warming";
  }
  return instance_ptr;
}

void GlicInstanceCoordinatorImpl::CreateWarmedInstance() {
  InstanceId instance_id = base::Uuid::GenerateRandomV4();
  warmed_instance_ = std::make_unique<GlicInstanceImpl>(
      profile_, instance_id, weak_ptr_factory_.GetWeakPtr(),
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_)->metrics(),
      contextual_cueing_service_);
}

GlicInstanceImpl*
GlicInstanceCoordinatorImpl::GetOrCreateInstanceImplForFloaty() {
  auto* floaty_instance = GetInstanceWithFloaty();
  if (!floaty_instance && last_active_instance_ &&
      GetTimeSinceLastActive(last_active_instance_) < kFloatyMaxRecency) {
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
    glic::mojom::InvocationSource source) {
  GetOrCreateInstanceImplForFloaty()->Toggle(
      ShowOptions::ForFloating(/*source_tab=*/tabs::TabHandle::Null()),
      prevent_close, source);
}

void GlicInstanceCoordinatorImpl::ToggleSidePanel(
    BrowserWindowInterface* browser,
    bool prevent_close,
    glic::mojom::InvocationSource source) {
  auto* tab = browser->GetActiveTabInterface();
  if (!tab) {
    return;
  }
  GlicInstanceImpl* instance = nullptr;
  if (source == glic::mojom::InvocationSource::kSharedImage) {
    // kSharedImage currently requires a new instance.
    instance = CreateGlicInstance();
  } else {
    instance = GetOrCreateGlicInstanceImplForTab(tab);
  }
  instance->Toggle(ShowOptions::ForSidePanel(*tab), prevent_close, source);
}

void GlicInstanceCoordinatorImpl::RemoveInstance(GlicInstanceImpl* instance) {
  OnInstanceActivationChanged(instance, false);

  // Remove the instance first, and then delete. This way, GetInstances() will
  // not return the instance being deleted while it's being deleted.
  InstanceId id = instance->id();
  instance->CloseInstanceAndShutdown();
  auto instance_value = std::exchange(instances_[id], {});
  instances_.erase(id);
  if (instance == last_active_instance_) {
    last_active_instance_ = nullptr;
  }
}

void GlicInstanceCoordinatorImpl::SwitchConversation(
    GlicInstanceImpl& source_instance,
    const ShowOptions& options,
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::SwitchConversationCallback callback) {
  ShowOptions mutable_options = options;
  mutable_options.focus_on_show = source_instance.HasFocus();
  mutable_options.reinitialize_if_already_active = true;

  GlicInstanceImpl* target_instance = nullptr;
  if (info) {
    for (const auto& [id, instance] : instances_) {
      if (instance->conversation_id().has_value() &&
          instance->conversation_id().value() == info->conversation_id) {
        target_instance = instance.get();
        break;
      }
    }
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
      info ? std::optional<std::string>(info->conversation_id) : std::nullopt,
      target_instance->conversation_id(), active_instance_);

  if (info) {
    target_instance->RegisterConversation(std::move(info), base::DoNothing());
  }

  target_instance->Show(mutable_options);
  target_instance->metrics()->OnSwitchToConversation(mutable_options);
  std::move(callback).Run(std::nullopt);
}

std::vector<glic::mojom::ConversationInfoPtr>
GlicInstanceCoordinatorImpl::GetRecentlyActiveConversations() {
  // This will only cover recently active conversations that still have living
  // instances. If an instance is torn down because the user closed all bound
  // tabs, it will not be included in the list.
  std::vector<GlicInstanceImpl*> sorted_instances;
  for (auto& [id, instance] : instances_) {
    if (!instance->conversation_id()) {
      continue;
    }
    if (base::FeatureList::IsEnabled(kGlicMaxRecency) &&
        GetTimeSinceLastActive(instance.get()) > kGlicMaxRecencyValue.Get()) {
      continue;
    }
    sorted_instances.push_back(instance.get());
  }

  std::sort(sorted_instances.begin(), sorted_instances.end(),
            [](GlicInstanceImpl* a, GlicInstanceImpl* b) {
              return a->GetLastActiveTime() > b->GetLastActiveTime();
            });

  std::vector<glic::mojom::ConversationInfoPtr> result;
  for (size_t i = 0; i < std::min(sorted_instances.size(), size_t{3}); ++i) {
    auto info = sorted_instances[i]->GetConversationInfo();
    CHECK(info);
    result.push_back(std::move(info));
  }
  return result;
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

void GlicInstanceCoordinatorImpl::SetWarmingEnabledForTesting(
    bool warming_enabled) {
  warming_enabled_ = warming_enabled;
  if (!warming_enabled_) {
    warmed_instance_.reset();
  }
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

void GlicInstanceCoordinatorImpl::OnTabCreated(tabs::TabInterface& old_tab,
                                               tabs::TabInterface& new_tab) {
  PrefService* pref_service = profile_->GetPrefs();
  if (!pref_service ||
      !pref_service->GetBoolean(
          glic::prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled)) {
    return;
  }
  auto* tab_features = old_tab.GetTabFeatures();
  if (!tab_features) {
    return;
  }

  auto* registry = tab_features->side_panel_registry();
  if (!registry) {
    return;
  }

  SidePanelEntry* glic_side_panel_entry =
      registry->GetEntryForKey(SidePanelEntryKey(SidePanelEntry::Id::kGlic));
  if (!glic_side_panel_entry) {
    return;
  }

  const auto& active_entry =
      registry->GetActiveEntryFor(glic_side_panel_entry->type());
  if (!active_entry.has_value() ||
      active_entry.value() != glic_side_panel_entry) {
    return;
  }

  auto* instance = CreateGlicInstance();
  SidePanelShowOptions side_panel_options{new_tab};
  side_panel_options.suppress_opening_animation = true;
  instance->Show(ShowOptions{side_panel_options});
}

void GlicInstanceCoordinatorImpl::OnMemoryPressure(
    base::MemoryPressureLevel level) {
  metrics_.OnMemoryPressure(level);

  if (level != base::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    return;
  }

  // Safeguard: Do not hibernate if there is only one instance left.
  if (instances_.size() <= 1) {
    return;
  }

  GlicInstanceImpl* least_recently_active_instance = nullptr;
  base::TimeTicks oldest_active_time = base::TimeTicks::Max();

  for (auto const& [id, instance] : instances_) {
    // Safeguard: Do not hibernate actuating or already hibernated instances.
    if (instance->IsActuating() || instance->IsHibernated()) {
      continue;
    }

    if (instance->GetLastActiveTime() < oldest_active_time) {
      oldest_active_time = instance->GetLastActiveTime();
      least_recently_active_instance = instance.get();
    }
  }

  if (least_recently_active_instance) {
    least_recently_active_instance->Hibernate();
  }
}

std::string GlicInstanceCoordinatorImpl::DescribeForTesting() {
  std::stringstream ss;
  for (auto& inst : instances_) {
    ss << inst.second->DescribeForTesting();  // IN-TEST
  }
  if (warmed_instance_) {
    ss << "(Warming instance) "
       << warmed_instance_->DescribeForTesting();  // IN-TEST
  }
  return ss.str();
}

}  // namespace glic
