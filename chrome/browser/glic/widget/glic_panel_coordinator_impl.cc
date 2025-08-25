// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_panel_coordinator_impl.h"

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
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/prefs/pref_service.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/screen.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget_observer.h"

namespace glic {
GlicPanelCoordinatorImpl::GlicPanelCoordinatorImpl(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    GlicKeyedService* service,
    GlicEnabling* enabling)
    : glic_window_controller_impl_(
          std::make_unique<GlicWindowControllerImpl>(profile,
                                                     identity_manager,
                                                     service,
                                                     enabling)) {}

GlicPanelCoordinatorImpl::~GlicPanelCoordinatorImpl() = default;

void GlicPanelCoordinatorImpl::Toggle(BrowserWindowInterface* browser,
                                      bool prevent_close,
                                      mojom::InvocationSource source) {
  glic_window_controller_impl_->Toggle(browser, prevent_close, source);
}

bool GlicPanelCoordinatorImpl::ActivateBrowser() {
  return glic_window_controller_impl_->ActivateBrowser();
}

void GlicPanelCoordinatorImpl::ShowAfterSignIn(base::WeakPtr<Browser> browser) {
  glic_window_controller_impl_->ShowAfterSignIn(browser);
}

void GlicPanelCoordinatorImpl::ToggleWhenNotAlwaysDetached(
    Browser* new_attached_browser,
    bool prevent_close,
    mojom::InvocationSource source) {
  glic_window_controller_impl_->ToggleWhenNotAlwaysDetached(
      new_attached_browser, prevent_close, source);
}

void GlicPanelCoordinatorImpl::FocusIfOpen() {
  glic_window_controller_impl_->FocusIfOpen();
}

void GlicPanelCoordinatorImpl::Attach() {
  glic_window_controller_impl_->Attach();
}

void GlicPanelCoordinatorImpl::Detach() {
  glic_window_controller_impl_->Detach();
}

void GlicPanelCoordinatorImpl::Shutdown() {
  glic_window_controller_impl_->Shutdown();
}

void GlicPanelCoordinatorImpl::Resize(const gfx::Size& size,
                                      base::TimeDelta duration,
                                      base::OnceClosure callback) {
  glic_window_controller_impl_->Resize(size, duration, std::move(callback));
}

void GlicPanelCoordinatorImpl::EnableDragResize(bool enabled) {
  glic_window_controller_impl_->EnableDragResize(enabled);
}

void GlicPanelCoordinatorImpl::MaybeSetWidgetCanResize() {
  glic_window_controller_impl_->MaybeSetWidgetCanResize();
}

gfx::Size GlicPanelCoordinatorImpl::GetSize() {
  return glic_window_controller_impl_->GetSize();
}

void GlicPanelCoordinatorImpl::SetDraggableAreas(
    const std::vector<gfx::Rect>& draggable_areas) {
  glic_window_controller_impl_->SetDraggableAreas(draggable_areas);
}

void GlicPanelCoordinatorImpl::SetMinimumWidgetSize(const gfx::Size& size) {
  glic_window_controller_impl_->SetMinimumWidgetSize(size);
}

void GlicPanelCoordinatorImpl::Close() {
  glic_window_controller_impl_->Close();
}

void GlicPanelCoordinatorImpl::CloseWithReason(
    views::Widget::ClosedReason reason) {
  glic_window_controller_impl_->CloseWithReason(reason);
}

void GlicPanelCoordinatorImpl::ShowTitleBarContextMenuAt(gfx::Point event_loc) {
  glic_window_controller_impl_->ShowTitleBarContextMenuAt(event_loc);
}

bool GlicPanelCoordinatorImpl::ShouldStartDrag(
    const gfx::Point& initial_press_loc,
    const gfx::Point& mouse_location) {
  return glic_window_controller_impl_->ShouldStartDrag(initial_press_loc,
                                                       mouse_location);
}

const mojom::PanelState& GlicPanelCoordinatorImpl::GetPanelState() const {
  return glic_window_controller_impl_->GetPanelState();
}

void GlicPanelCoordinatorImpl::AddStateObserver(StateObserver* observer) {
  glic_window_controller_impl_->AddStateObserver(observer);
}

void GlicPanelCoordinatorImpl::RemoveStateObserver(StateObserver* observer) {
  glic_window_controller_impl_->RemoveStateObserver(observer);
}

bool GlicPanelCoordinatorImpl::IsActive() {
  return glic_window_controller_impl_->IsActive();
}

bool GlicPanelCoordinatorImpl::IsShowing() const {
  return glic_window_controller_impl_->IsShowing();
}

bool GlicPanelCoordinatorImpl::IsAttached() const {
  return glic_window_controller_impl_->IsAttached();
}

bool GlicPanelCoordinatorImpl::IsDetached() const {
  return glic_window_controller_impl_->IsDetached();
}

base::CallbackListSubscription
GlicPanelCoordinatorImpl::AddWindowActivationChangedCallback(
    WindowActivationChangedCallback callback) {
  return glic_window_controller_impl_->AddWindowActivationChangedCallback(
      std::move(callback));
}

void GlicPanelCoordinatorImpl::Preload() {
  glic_window_controller_impl_->Preload();
}

void GlicPanelCoordinatorImpl::Reload() {
  glic_window_controller_impl_->Reload();
}

bool GlicPanelCoordinatorImpl::IsWarmed() const {
  return glic_window_controller_impl_->IsWarmed();
}

base::WeakPtr<GlicWindowController> GlicPanelCoordinatorImpl::GetWeakPtr() {
  return glic_window_controller_impl_->GetWeakPtr();
}

GlicView* GlicPanelCoordinatorImpl::GetGlicView() const {
  return glic_window_controller_impl_->GetGlicView();
}

base::WeakPtr<views::View> GlicPanelCoordinatorImpl::GetGlicViewAsView() {
  return glic_window_controller_impl_->GetGlicViewAsView();
}

GlicWidget* GlicPanelCoordinatorImpl::GetGlicWidget() const {
  return glic_window_controller_impl_->GetGlicWidget();
}

gfx::NativeWindow GlicPanelCoordinatorImpl::GetHostNativeWindow() {
  return glic_window_controller_impl_->GetHostNativeWindow();
}

Browser* GlicPanelCoordinatorImpl::attached_browser() {
  return glic_window_controller_impl_->attached_browser();
}

GlicWindowController::State GlicPanelCoordinatorImpl::state() const {
  return glic_window_controller_impl_->state();
}

GlicWindowAnimator* GlicPanelCoordinatorImpl::window_animator() {
  return glic_window_controller_impl_->window_animator();
}

Profile* GlicPanelCoordinatorImpl::profile() {
  return glic_window_controller_impl_->profile();
}

bool GlicPanelCoordinatorImpl::IsDragging() {
  return glic_window_controller_impl_->IsDragging();
}

gfx::Rect GlicPanelCoordinatorImpl::GetInitialBounds(Browser* browser) {
  return glic_window_controller_impl_->GetInitialBounds(browser);
}

void GlicPanelCoordinatorImpl::ShowDetachedForTesting() {
  glic_window_controller_impl_->ShowDetachedForTesting();  // IN-TEST
}

void GlicPanelCoordinatorImpl::SetPreviousPositionForTesting(
    gfx::Point position) {
  glic_window_controller_impl_->SetPreviousPositionForTesting(  // IN-TEST
      position);
}

std::unique_ptr<GlicView>
GlicPanelCoordinatorImpl::CreateGlicViewForSidePanel() {
  return glic_window_controller_impl_->CreateGlicViewForSidePanel();
}

base::CallbackListSubscription
GlicPanelCoordinatorImpl::RegisterFloatyStateChange(
    FloatyStateChangeCallback callback) {
  return glic_window_controller_impl_->RegisterFloatyStateChange(
      std::move(callback));
}
}  // namespace glic
