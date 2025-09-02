// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_deref.h"
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
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_animator.h"
#include "chrome/browser/glic/widget/glic_window_config.h"
#include "chrome/browser/glic/widget/glic_window_controller_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/screen.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget_observer.h"

namespace glic {

Host& GlicInstanceCoordinatorImpl::host() const {
  NOTIMPLEMENTED();
  return host_manager_->primary_host();
}

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

void GlicInstanceCoordinatorImpl::OnBrowserRemoved(Browser* browser) {
  // If the removed browser has an attached Glic instance, the instance must be
  // destroyed as it cannot exist without its browser window.
  if (auto* instance = GetAttachedInstanceForBrowser(browser)) {
    RemoveInstance(instance);
  }

  // If the floating instance was associated with the removed browser, we must
  // disassociate it to prevent holding a dangling pointer. The floating
  // instance itself is not destroyed.
  if (floating_instance_ && floating_instance_->associated_bwi() == browser) {
    floating_instance_->DisassociateWindow();
  }
}

void GlicInstanceCoordinatorImpl::Toggle(BrowserWindowInterface* browser,
                                         bool prevent_close,
                                         mojom::InvocationSource source) {
  if (!browser) {
    ToggleFloaty();
    return;
  }
  if (auto* instance = GetAttachedInstanceForBrowser(browser)) {
    instance->Toggle();
    return;
  }
  auto new_instance =
      std::make_unique<GlicInstance>(browser, weak_ptr_factory_.GetWeakPtr());
  new_instance->SetEmbedderType(GlicInstance::EmbedderType::kSidePanel);
  new_instance->Show();
  attached_instances_.push_back(std::move(new_instance));
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

void GlicInstanceCoordinatorImpl::Attach() {
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::Detach() {
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::Shutdown() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::Resize(const gfx::Size& size,
                                         base::TimeDelta duration,
                                         base::OnceClosure callback) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::EnableDragResize(bool enabled) {
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

void GlicInstanceCoordinatorImpl::SetDraggableAreas(
    const std::vector<gfx::Rect>& draggable_areas) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

void GlicInstanceCoordinatorImpl::SetMinimumWidgetSize(const gfx::Size& size) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
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
  NOTREACHED();
}

GlicView* GlicInstanceCoordinatorImpl::GetGlicView() const {
  // Method should only be called on individual panels not the coordinator.
  NOTREACHED();
}

base::WeakPtr<views::View> GlicInstanceCoordinatorImpl::GetGlicViewAsView() {
  // Method should only be called on individual panels not the coordinator.
  NOTREACHED();
}

GlicWidget* GlicInstanceCoordinatorImpl::GetGlicWidget() const {
  // Method should only be called on individual panels not the coordinator.
  NOTREACHED();
}

gfx::NativeWindow GlicInstanceCoordinatorImpl::GetHostNativeWindow() {
  // Method should only be called on individual panels not the coordinator.
  NOTREACHED();
}

Browser* GlicInstanceCoordinatorImpl::attached_browser() {
  // Method should only be called on individual panels not the coordinator.
  NOTREACHED();
}

GlicWindowController::State GlicInstanceCoordinatorImpl::state() const {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return GlicWindowController::State::kClosed;
}

GlicWindowAnimator* GlicInstanceCoordinatorImpl::window_animator() {
  // TODO(crbug.com/441545112) - Remove from GlicWindowController.
  // Method should only be called on individual panels not the coordinator.
  NOTREACHED();
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

std::unique_ptr<GlicView>
GlicInstanceCoordinatorImpl::CreateGlicViewForSidePanel() {
  // Method should only be called on individual panels not the coordinator.
  NOTREACHED();
}

base::CallbackListSubscription
GlicInstanceCoordinatorImpl::RegisterFloatyStateChange(
    FloatyStateChangeCallback callback) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return floaty_state_change_callback_list_.Add(std::move(callback));
}

void GlicInstanceCoordinatorImpl::AttachInstance(GlicInstance* instance) {
  CHECK(HasAttachedInstance(instance) || IsFloatingInstance(instance));

  BrowserWindowInterface* bwi = instance->associated_bwi();
  if (!bwi) {
    return;
  }

  if (auto* current_instance = GetAttachedInstanceForBrowser(bwi)) {
    CHECK(current_instance == instance)
        << "Multiple Glic instances per browser window are not yet supported.";
  }

  // If we are attaching the floating instance, it needs to be moved to the
  // main list.
  if (floating_instance_ && floating_instance_.get() == instance) {
    attached_instances_.push_back(std::move(floating_instance_));
  }

  instance->SetEmbedderType(GlicInstance::EmbedderType::kSidePanel);
  instance->Show();
}

void GlicInstanceCoordinatorImpl::DetachInstance(GlicInstance* instance) {
  if (!HasAttachedInstance(instance)) {
    return;
  }
  if (instance == floating_instance_.get()) {
    // Panel is already detached do nothing.
    return;
  }
  instance->Close();

  if (floating_instance_) {
    ReattachFloatingInstance();
  }

  auto it =
      std::find_if(attached_instances_.begin(), attached_instances_.end(),
                   [instance](const auto& p) { return p.get() == instance; });
  if (it != attached_instances_.end()) {
    floating_instance_ = std::move(*it);
    attached_instances_.erase(it);
  }

  floating_instance_->SetEmbedderType(GlicInstance::EmbedderType::kFloating);
  floating_instance_->Show();
}

void GlicInstanceCoordinatorImpl::ShowAttached(BrowserWindowInterface* bwi) {
  NOTIMPLEMENTED();
}
void GlicInstanceCoordinatorImpl::ShowDetached() {
  NOTIMPLEMENTED();
}
void GlicInstanceCoordinatorImpl::ToggleFloaty() {
  NOTIMPLEMENTED();
}

GlicInstance* GlicInstanceCoordinatorImpl::GetAttachedInstanceForBrowser(
    BrowserWindowInterface* bwi) {
  for (auto& instance : attached_instances_) {
    if (instance->associated_bwi() == bwi) {
      return instance.get();
    }
  }
  return nullptr;
}

void GlicInstanceCoordinatorImpl::RemoveInstance(GlicInstance* instance) {
  std::erase_if(attached_instances_,
                [instance](const auto& i) { return i.get() == instance; });
}

bool GlicInstanceCoordinatorImpl::HasAttachedInstance(GlicInstance* instance) {
  for (auto& i : attached_instances_) {
    if (i.get() == instance) {
      return true;
    }
  }
  return false;
}

bool GlicInstanceCoordinatorImpl::IsFloatingInstance(GlicInstance* instance) {
  return floating_instance_ && floating_instance_.get() == instance;
}

void GlicInstanceCoordinatorImpl::ReattachFloatingInstance() {
  if (!floating_instance_) {
    return;
  }
  auto* bwi = floating_instance_->associated_bwi();
  if (bwi) {
    floating_instance_->SetEmbedderType(GlicInstance::EmbedderType::kSidePanel);
    floating_instance_->Show();
    attached_instances_.push_back(std::move(floating_instance_));
  }
  floating_instance_.reset();
}

}  // namespace glic
