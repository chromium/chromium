// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notimplemented.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/browser_ui/scoped_glic_button_indicator.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/fre/glic_fre_dialog_view.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
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
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/screen.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget_observer.h"

namespace glic {

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
    : profile_(profile), contextual_cueing_service_(contextual_cueing_service) {
  host_manager_ = std::make_unique<HostManager>(profile, GetWeakPtr());
}

GlicInstanceCoordinatorImpl::~GlicInstanceCoordinatorImpl() {
  // Delete all instances before destruction. Destroying web contents can result
  // in various calls to dependencies.
  active_instance_ = nullptr;
  auto instances = std::exchange(instances_, {});
  instances.clear();
  warmed_instance_.reset();
}

void GlicInstanceCoordinatorImpl::OnInstanceActivationChanged(
    GlicInstance* instance,
    bool is_active) {
  if (is_active && active_instance_ != instance) {
    active_instance_ = instance;
  } else if (!is_active && active_instance_ == instance) {
    active_instance_ = nullptr;
  } else {
    return;
  }
  NotifyActiveInstanceChanged();
}

void GlicInstanceCoordinatorImpl::OnInstanceVisibilityChanged(
    GlicInstance* instance,
    bool is_showing) {
  // TODO(crbug.com/452963408): We think this will be useful, but if we find
  // that we're not using it, we should remove it.
}

void GlicInstanceCoordinatorImpl::NotifyActiveInstanceChanged() {
  active_instance_changed_callback_list_.Notify(active_instance_);
}

GlicInstanceImpl* GlicInstanceCoordinatorImpl::GetInstanceImplForTab(
    tabs::TabInterface* tab) {
  if (!tab) {
    return nullptr;
  }

  auto* helper = GlicInstanceHelper::From(tab);
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
    tabs::TabInterface* tab) {
  return GetInstanceImplForTab(tab);
}

void GlicInstanceCoordinatorImpl::FindInstanceFromGlicContentsAndBindToTab(
    content::WebContents* source_glic_web_contents,
    tabs::TabInterface* tab_to_bind) {
  // Find the instance for the given web contents
  for (auto const& [instance_id, instance] : instances_) {
    if (instance->host().webui_contents() == source_glic_web_contents) {
      // Show the instance in the new tab
      auto show_options = ShowOptions::ForSidePanel(*tab_to_bind);
      show_options.focus_on_show = tab_to_bind->IsActivated();
      instance->Show(show_options);
      return;
    }
  }
}

// TODO (crbug.com/451718132): Add test coverage for daisy chaining
// functionality
bool GlicInstanceCoordinatorImpl::FindInstanceFromIdAndBindToTab(
    const InstanceId& instance_id,
    tabs::TabInterface* tab_to_bind) {
  GlicInstanceImpl* instance = GetInstanceImplFor(instance_id);
  if (!instance || !tab_to_bind) {
    return false;
  }

  // Early return if an instance is already bound to the target tab.
  if (GetInstanceImplForTab(tab_to_bind)) {
    return false;
  }

  auto show_options = ShowOptions::ForSidePanel(*tab_to_bind);
  show_options.focus_on_show = tab_to_bind->IsActivated();
  instance->Show(show_options);
  return true;
}

void GlicInstanceCoordinatorImpl::Toggle(BrowserWindowInterface* browser,
                                         bool prevent_close,
                                         mojom::InvocationSource source) {
  if (!browser) {
    ToggleFloaty(prevent_close);
    return;
  }
  ToggleSidePanel(browser, prevent_close);
}

void GlicInstanceCoordinatorImpl::ShowAfterSignIn(
    base::WeakPtr<Browser> browser) {
  // TODO(crbug/4263869): Used by GlicPageHandler::SignInAndClosePanel(), which
  // should close glic and reopen it after signin is complete. This flow likely
  // still makes sense for the floating panel, but not for the side panel.
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::Shutdown() {
  // TODO(crbug.com/450286204): This is likely needed, or needed to be
  // refactored.
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::Close() {
  // TODO(crbug.com/450286204): This is likely needed, or needed to be
  // refactored.
  NOTIMPLEMENTED();
}

mojom::PanelState GlicInstanceCoordinatorImpl::GetGlobalPanelState() {
  // TODO: Currently called from GlicButtonController. Needs implemented or
  // refactored and removed.
  NOTIMPLEMENTED();
  return panel_state_;
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

bool GlicInstanceCoordinatorImpl::IsDetached() const {
  return GetInstanceWithFloaty() != nullptr;
}

base::CallbackListSubscription
GlicInstanceCoordinatorImpl::AddWindowActivationChangedCallback(
    WindowActivationChangedCallback callback) {
  // TODO: Notification of this callback list is not yet implemented.
  return window_activation_callback_list_.Add(std::move(callback));
}

void GlicInstanceCoordinatorImpl::Preload() {
  if (warming_enabled_) {
    CreateWarmedInstance();
  } else {
    VLOG(1) << "Warming is disabled, skipping warming";
  }
}

void GlicInstanceCoordinatorImpl::Reload(
    content::RenderFrameHost* render_frame_host) {
  for (auto iter = instances_.begin(); iter != instances_.end();) {
    // Advance iterator now, in case Reload deletes the instance.
    auto& instance = *iter++;
    instance.second->host().Reload(render_frame_host);
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

GlicInstanceImpl*
GlicInstanceCoordinatorImpl::GetOrCreateGlicInstanceImplForTab(
    tabs::TabInterface* tab) {
  if (GlicInstanceImpl* instance = GetInstanceImplForTab(tab)) {
    return instance;
  }

  // Create a new conversation and instance.
  auto* new_instance = CreateGlicInstance();
  if (tab) {
    new_instance->sharing_manager().PinTabs({tab->GetHandle()});
  }
  return new_instance;
}

GlicInstanceImpl* GlicInstanceCoordinatorImpl::GetInstanceImplFor(
    const InstanceId& id) {
  auto it = instances_.find(id);
  if (it != instances_.end()) {
    return it->second.get();
  }
  return nullptr;
}

GlicInstanceImpl* GlicInstanceCoordinatorImpl::CreateGlicInstance() {
  if (!warmed_instance_) {
    CreateWarmedInstance();
  }
  auto* instance_ptr = warmed_instance_.get();
  instances_[instance_ptr->id()] = std::move(warmed_instance_);
  if (warming_enabled_) {
    CreateWarmedInstance();
  } else {
    VLOG(1) << "Warming is disabled, skipping warming";
  }
  return instance_ptr;
}

void GlicInstanceCoordinatorImpl::CreateWarmedInstance() {
  // TODO: Sync this id with the web client.
  InstanceId instance_id = base::Uuid::GenerateRandomV4();
  warmed_instance_ = std::make_unique<GlicInstanceImpl>(
      profile_, instance_id, weak_ptr_factory_.GetWeakPtr(),
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_)->metrics(),
      contextual_cueing_service_);
}

void GlicInstanceCoordinatorImpl::ToggleFloaty(bool prevent_close) {
  auto* floaty_instance = GetInstanceWithFloaty();
  if (!floaty_instance) {
    floaty_instance = CreateGlicInstance();
  }
  floaty_instance->Toggle(ShowOptions::ForFloating(/*anchor_browser=*/nullptr),
                          prevent_close);
}

void GlicInstanceCoordinatorImpl::ToggleSidePanel(
    BrowserWindowInterface* browser,
    bool prevent_close) {
  auto* tab = browser->GetActiveTabInterface();
  if (!tab) {
    return;
  }
  auto* instance = GetOrCreateGlicInstanceImplForTab(tab);
  instance->Toggle(ShowOptions::ForSidePanel(*tab), prevent_close);
}

void GlicInstanceCoordinatorImpl::RemoveInstance(GlicInstance* instance) {
  OnInstanceActivationChanged(instance, false);
  // Remove the instance first, and then delete. This way, GetInstances() will
  // not return the instance being deleted while it's being deleted.
  InstanceId id = instance->id();
  auto instance_value = std::exchange(instances_[id], {});
  instances_.erase(id);
}

void GlicInstanceCoordinatorImpl::SwitchConversation(
    GlicInstanceImpl& source_instance,
    const ShowOptions& options,
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::SwitchConversationCallback callback) {
  GlicInstanceImpl* target_instance = nullptr;
  if (!info) {
    target_instance = CreateGlicInstance();
  } else {
    for (const auto& [id, instance] : instances_) {
      if (instance->conversation_id().has_value() &&
          instance->conversation_id().value() == info->conversation_id) {
        target_instance = instance.get();
        break;
      }
    }
    if (!target_instance) {
      // No instance exists for this conversation. If the current instance
      // already has a conversation, create a new instance. Otherwise, reuse
      // the current instance.
      target_instance = source_instance.conversation_id() ? CreateGlicInstance()
                                                          : &source_instance;
      target_instance->RegisterConversation(std::move(info), base::DoNothing());
    }
  }

  CHECK(target_instance);
  target_instance->Show(options);

  std::move(callback).Run(std::nullopt);
}

void GlicInstanceCoordinatorImpl::UnbindTabFromAnyInstance(
    tabs::TabInterface* tab) {
  if (auto* instance = GetInstanceImplForTab(tab)) {
    instance->UnbindEmbedder(EmbedderKey(tab));
  }
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

void GlicInstanceCoordinatorImpl::OnDetachRequested(GlicInstance* instance,
                                                    tabs::TabInterface* tab) {
  if (auto* floaty_instance = GetInstanceWithFloaty()) {
    floaty_instance->Close(FloatingEmbedderKey{});
  }
}
}  // namespace glic
