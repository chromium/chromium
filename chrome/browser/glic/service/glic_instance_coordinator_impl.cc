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
#include "chrome/browser/glic/service/glic_conversation_helper.h"
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
Host& GlicInstanceCoordinatorImpl::host() {
  NOTIMPLEMENTED();
  return host_manager_->primary_host();
}

// TODO(refactor): Remove.
HostManager& GlicInstanceCoordinatorImpl::host_manager() {
  NOTIMPLEMENTED();
  return *host_manager_;
}

GlicInstanceCoordinatorImpl::GlicInstanceCoordinatorImpl(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    GlicKeyedService* service,
    GlicEnabling* enabling)
    : profile_(profile), host_manager_(std::make_unique<HostManager>(profile)) {
  browser_list_observation_.Observe(BrowserList::GetInstance());
}

GlicInstanceCoordinatorImpl::~GlicInstanceCoordinatorImpl() = default;

GlicInstance* GlicInstanceCoordinatorImpl::GetInstanceForTab(
    tabs::TabInterface* tab) {
  auto* helper = GlicConversationHelper::From(tab);
  CHECK(helper);

  auto conversation_id = helper->GetConversationId();
  if (conversation_id.has_value()) {
    if (auto* instance = GetInstanceFor(conversation_id.value())) {
      return instance;
    }
  }

  return nullptr;
}

void GlicInstanceCoordinatorImpl::OnBrowserAdded(Browser* browser) {}

void GlicInstanceCoordinatorImpl::OnBrowserRemoved(Browser* browser) {
  browser_to_conversation_map_.erase(browser);
}

void GlicInstanceCoordinatorImpl::OnInstanceOrphaned(GlicInstance* instance) {
  if (!IsFloatingInstance(instance)) {
    RemoveInstance(instance);
  }
}

Host* GlicInstanceCoordinatorImpl::GetHostForTab(tabs::TabInterface* tab) {
  if (GlicInstance* instance = GetInstanceForTab(tab)) {
    return &instance->host();
  }
  return nullptr;
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
  // TODO(crbug.com/441542357) - remove from public interface.
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return nullptr;
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
GlicInstanceCoordinatorImpl::CreateViewForSidePanel(tabs::TabInterface* tab) {
  if (!tab) {
    return nullptr;
  }
  auto* instance = GetOrCreateGlicInstanceForTab(tab);
  CHECK(instance);
  return instance->embedder().CreateGlicView();
}

void GlicInstanceCoordinatorImpl::SidePanelShown(Browser* browser) {
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

GlicInstance* GlicInstanceCoordinatorImpl::GetOrCreateGlicInstanceForTab(
    tabs::TabInterface* tab) {
  if (GlicInstance* instance = GetInstanceForTab(tab)) {
    return instance;
  }

  auto* helper = GlicConversationHelper::From(tab);
  CHECK(helper);

  // If the tab is not part of a conversation, we will check if the browser
  // window is.
  auto* bwi = tab->GetBrowserWindowInterface();
  auto it = browser_to_conversation_map_.find(bwi);
  if (it != browser_to_conversation_map_.end()) {
    helper->SetConversationId(it->second);
    return GetInstanceFor(it->second);
  }

  // Create a new conversation and instance.
  auto* new_instance = CreateGlicInstance(bwi);
  helper->SetConversationId(new_instance->conversation_id());
  return new_instance;
}

GlicInstance* GlicInstanceCoordinatorImpl::GetInstanceFor(
    const ConversationId& id) {
  auto it = instances_.find(id);
  if (it != instances_.end()) {
    return it->second.get();
  }
  return nullptr;
}

GlicInstance* GlicInstanceCoordinatorImpl::CreateGlicInstance(
    BrowserWindowInterface* bwi) {
  // TODO: Sync this id with the web client.
  ConversationId new_conversation_id = base::Uuid::GenerateRandomV4();
  auto new_instance = std::make_unique<GlicInstance>(
      profile_, CreateHost(), new_conversation_id,
      weak_ptr_factory_.GetWeakPtr());
  if (bwi) {
    browser_to_conversation_map_[bwi] = new_conversation_id;
  }
  auto* instance_ptr = new_instance.get();
  instances_[new_conversation_id] = std::move(new_instance);
  return instance_ptr;
}

void GlicInstanceCoordinatorImpl::ToggleFloaty() {
  if (!floating_instance_) {
    // Create a new floating instance if one doesn't exist.
    floating_instance_ = CreateGlicInstance(/*bwi=*/nullptr);
    floating_instance_->SetEmbedderType(GlicInstance::EmbedderType::kFloating);
  }

  CHECK(floating_instance_->GetEmbedderType() ==
        GlicInstance::EmbedderType::kFloating);
  if (floating_instance_->IsShowing()) {
    floating_instance_->Close();
  } else {
    floating_instance_->Show(/*tab=*/nullptr);
  }
}

void GlicInstanceCoordinatorImpl::ToggleSidePanel(
    BrowserWindowInterface* browser) {
  auto* tab = browser->GetActiveTabInterface();
  if (!tab) {
    return;
  }
  auto* instance = GetOrCreateGlicInstanceForTab(tab);
  CHECK(instance->GetEmbedderType() == GlicInstance::EmbedderType::kSidePanel);
  if (instance->IsShowing()) {
    instance->Close();
  } else {
    instance->Show(tab);
  }
}

void GlicInstanceCoordinatorImpl::RemoveInstance(GlicInstance* instance) {
  instances_.erase(instance->conversation_id());
}

bool GlicInstanceCoordinatorImpl::HasAttachedInstance(GlicInstance* instance) {
  NOTIMPLEMENTED();
  return false;
}

bool GlicInstanceCoordinatorImpl::IsFloatingInstance(GlicInstance* instance) {
  return floating_instance_ && floating_instance_ == instance;
}

void GlicInstanceCoordinatorImpl::ReattachFloatingInstance() {
  NOTIMPLEMENTED();
}

std::unique_ptr<Host> GlicInstanceCoordinatorImpl::CreateHost() {
  auto host = std::make_unique<Host>(
      profile_, base::BindOnce(&GlicInstanceCoordinatorImpl::OnDestroyingHost,
                               base::Unretained(this)));
  host_manager_->AddHost(host.get());
  return host;
}

void GlicInstanceCoordinatorImpl::OnDestroyingHost(Host* host) {
  host_manager_->RemoveHost(host);
}

}  // namespace glic
