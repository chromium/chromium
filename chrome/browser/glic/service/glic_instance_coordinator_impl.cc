// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
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
#include "chrome/browser/glic/host/glic_ui_embedder.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/glic/widget/glic_side_panel_ui.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_animator.h"
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

// TODO(refactor): Remove.
HostManager& GlicInstanceCoordinatorImpl::host_manager() {
  NOTIMPLEMENTED();
  return *host_manager_;
}

std::vector<Host*> GlicInstanceCoordinatorImpl::GetHosts() {
  std::vector<Host*> hosts;
  for (const auto& [id, instance] : instances_) {
    hosts.push_back(&instance->host());
  }
  return hosts;
}

GlicInstanceCoordinatorImpl::GlicInstanceCoordinatorImpl(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    GlicKeyedService* service,
    GlicEnabling* enabling)
    : profile_(profile) {
  host_manager_ = std::make_unique<HostManager>(profile, GetWeakPtr());
}

GlicInstanceCoordinatorImpl::~GlicInstanceCoordinatorImpl() = default;

GlicInstanceImpl* GlicInstanceCoordinatorImpl::GetInstanceImplForTab(
    tabs::TabInterface* tab) {
  if (!tab) {
    return nullptr;
  }

  auto* helper = GlicInstanceHelper::From(tab);
  CHECK(helper);

  auto instance_id = helper->GetInstanceId();
  if (instance_id.has_value()) {
    if (auto* instance = GetInstanceImplFor(instance_id.value())) {
      return instance;
    }
  }

  return nullptr;
}

void GlicInstanceCoordinatorImpl::OnInstanceOrphaned(GlicInstance* instance) {
  if (floating_instance_key_.has_value() &&
      floating_instance_key_.value() == instance->id()) {
    return;
  }
  RemoveInstance(instance);
}

Host* GlicInstanceCoordinatorImpl::GetHostForTab(tabs::TabInterface* tab) {
  if (GlicInstance* instance = GetInstanceImplForTab(tab)) {
    return &instance->host();
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
    tabs::TabInterface* tab) {
  return GetInstanceImplForTab(tab);
}

void GlicInstanceCoordinatorImpl::Toggle(BrowserWindowInterface* browser,
                                         bool prevent_close,
                                         mojom::InvocationSource source) {
  if (!browser) {
    ToggleFloaty();
    return;
  }
  ToggleSidePanel(browser);
}

bool GlicInstanceCoordinatorImpl::ActivateBrowser() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return false;
}

void GlicInstanceCoordinatorImpl::ShowAfterSignIn(
    base::WeakPtr<Browser> browser) {
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::ToggleWhenNotAlwaysDetached(
    Browser* new_attached_browser,
    bool prevent_close,
    mojom::InvocationSource source) {
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::FocusIfOpen() {
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::Shutdown() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::MaybeSetWidgetCanResize() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

gfx::Size GlicInstanceCoordinatorImpl::GetSize() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return gfx::Size();
}

void GlicInstanceCoordinatorImpl::Close() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::CloseWithReason(
    views::Widget::ClosedReason reason) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::ShowTitleBarContextMenuAt(
    gfx::Point event_loc) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

bool GlicInstanceCoordinatorImpl::ShouldStartDrag(
    const gfx::Point& initial_press_loc,
    const gfx::Point& mouse_location) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return false;
}

const mojom::PanelState& GlicInstanceCoordinatorImpl::GetPanelState() const {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return panel_state_;
}

void GlicInstanceCoordinatorImpl::AddStateObserver(StateObserver* observer) {
  // The StateObserver needs to be split into two: one for if the floating
  // window is showing and one for the state of an individual panel.
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::RemoveStateObserver(StateObserver* observer) {
  // The StateObserver needs to be split into two: one for if the floating
  // window is showing and one for the state of an individual panel.
  NOTIMPLEMENTED();
}

bool GlicInstanceCoordinatorImpl::IsActive() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return false;
}

bool GlicInstanceCoordinatorImpl::IsShowing() const {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return false;
}

bool GlicInstanceCoordinatorImpl::IsAttached() const {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return false;
}

bool GlicInstanceCoordinatorImpl::IsDetached() const {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return false;
}

base::CallbackListSubscription
GlicInstanceCoordinatorImpl::AddWindowActivationChangedCallback(
    WindowActivationChangedCallback callback) {
  return window_activation_callback_list_.Add(std::move(callback));
}

void GlicInstanceCoordinatorImpl::Preload() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::Reload() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

bool GlicInstanceCoordinatorImpl::IsWarmed() const {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return false;
}

base::WeakPtr<GlicWindowController> GlicInstanceCoordinatorImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

GlicView* GlicInstanceCoordinatorImpl::GetGlicView() const {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return nullptr;
}

base::WeakPtr<views::View> GlicInstanceCoordinatorImpl::GetGlicViewAsView() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return nullptr;
}

GlicWidget* GlicInstanceCoordinatorImpl::GetGlicWidget() const {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return nullptr;
}

gfx::NativeWindow GlicInstanceCoordinatorImpl::GetHostNativeWindow() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return gfx::NativeWindow{};
}

Browser* GlicInstanceCoordinatorImpl::attached_browser() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return nullptr;
}

GlicWindowController::State GlicInstanceCoordinatorImpl::state() const {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return GlicWindowController::State::kClosed;
}

GlicWindowAnimator* GlicInstanceCoordinatorImpl::window_animator() {
  // TODO(crbug.com/441545112) - Remove from GlicWindowController.
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return nullptr;
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

std::unique_ptr<views::View>
GlicInstanceCoordinatorImpl::CreateViewForSidePanel(tabs::TabInterface& tab) {
  GlicInstanceImpl* instance = GetOrCreateGlicInstanceImplForTab(&tab);
  CHECK(instance);
  return instance->CreateViewForSidePanel(&tab);
}

void GlicInstanceCoordinatorImpl::SidePanelShown(
    BrowserWindowInterface* browser) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

base::CallbackListSubscription
GlicInstanceCoordinatorImpl::RegisterFloatyStateChange(
    FloatyStateChangeCallback callback) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return floaty_state_change_callback_list_.Add(std::move(callback));
}

void GlicInstanceCoordinatorImpl::AttachInstance(GlicInstance* instance) {
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::DetachInstance(GlicInstance* instance) {
  NOTIMPLEMENTED();
}

GlicInstanceImpl*
GlicInstanceCoordinatorImpl::GetOrCreateGlicInstanceImplForTab(
    tabs::TabInterface* tab) {
  if (GlicInstanceImpl* instance = GetInstanceImplForTab(tab)) {
    return instance;
  }

  auto* helper = GlicInstanceHelper::From(tab);
  CHECK(helper);

  // Create a new conversation and instance.
  auto* new_instance = CreateGlicInstance();
  helper->SetInstanceId(new_instance->id());
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
  // TODO: Sync this id with the web client.
  InstanceId instance_id = base::Uuid::GenerateRandomV4();
  auto new_instance = std::make_unique<GlicInstanceImpl>(
      profile_, instance_id, weak_ptr_factory_.GetWeakPtr());
  auto* instance_ptr = new_instance.get();
  instances_[instance_id] = std::move(new_instance);
  return instance_ptr;
}

void GlicInstanceCoordinatorImpl::ToggleFloaty() {
  if (!floating_instance_key_.has_value()) {
    floating_instance_key_ = CreateGlicInstance()->id();
  }
  auto instance_iter = instances_.find(*floating_instance_key_);
  CHECK(instance_iter != instances_.end());
  GlicInstanceImpl* instance = instance_iter->second.get();
  instance->Toggle(GlicInstanceImpl::EmbedderType::kFloating, nullptr);
}

void GlicInstanceCoordinatorImpl::ToggleSidePanel(
    BrowserWindowInterface* browser) {
  auto* tab = browser->GetActiveTabInterface();
  if (!tab) {
    return;
  }
  auto* instance = GetOrCreateGlicInstanceImplForTab(tab);
  instance->Toggle(GlicInstanceImpl::EmbedderType::kSidePanel, tab);
}

void GlicInstanceCoordinatorImpl::RemoveInstance(GlicInstance* instance) {
  instances_.erase(instance->id());
}

bool GlicInstanceCoordinatorImpl::HasAttachedInstance(GlicInstance* instance) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace glic
