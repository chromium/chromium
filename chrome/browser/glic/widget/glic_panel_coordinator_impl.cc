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
#include "chrome/common/chrome_features.h"
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
    : profile_(profile) {}

GlicPanelCoordinatorImpl::~GlicPanelCoordinatorImpl() = default;

void GlicPanelCoordinatorImpl::Toggle(BrowserWindowInterface* browser,
                                      bool prevent_close,
                                      mojom::InvocationSource source) {
  NOTIMPLEMENTED();
}

bool GlicPanelCoordinatorImpl::ActivateBrowser() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return false;
}

void GlicPanelCoordinatorImpl::ShowAfterSignIn(base::WeakPtr<Browser> browser) {
  NOTIMPLEMENTED();
}

void GlicPanelCoordinatorImpl::ToggleWhenNotAlwaysDetached(
    Browser* new_attached_browser,
    bool prevent_close,
    mojom::InvocationSource source) {
  NOTIMPLEMENTED();
}

void GlicPanelCoordinatorImpl::FocusIfOpen() {
  NOTIMPLEMENTED();
}

void GlicPanelCoordinatorImpl::Attach() {
  NOTIMPLEMENTED();
}

void GlicPanelCoordinatorImpl::Detach() {
  NOTIMPLEMENTED();
}

void GlicPanelCoordinatorImpl::Shutdown() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

void GlicPanelCoordinatorImpl::Resize(const gfx::Size& size,
                                      base::TimeDelta duration,
                                      base::OnceClosure callback) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

void GlicPanelCoordinatorImpl::EnableDragResize(bool enabled) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

void GlicPanelCoordinatorImpl::MaybeSetWidgetCanResize() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

gfx::Size GlicPanelCoordinatorImpl::GetSize() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return gfx::Size();
}

void GlicPanelCoordinatorImpl::SetDraggableAreas(
    const std::vector<gfx::Rect>& draggable_areas) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

void GlicPanelCoordinatorImpl::SetMinimumWidgetSize(const gfx::Size& size) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

void GlicPanelCoordinatorImpl::Close() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

void GlicPanelCoordinatorImpl::CloseWithReason(
    views::Widget::ClosedReason reason) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

void GlicPanelCoordinatorImpl::ShowTitleBarContextMenuAt(gfx::Point event_loc) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

bool GlicPanelCoordinatorImpl::ShouldStartDrag(
    const gfx::Point& initial_press_loc,
    const gfx::Point& mouse_location) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return false;
}

const mojom::PanelState& GlicPanelCoordinatorImpl::GetPanelState() const {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return panel_state_;
}

void GlicPanelCoordinatorImpl::AddStateObserver(StateObserver* observer) {
  // The StateObserver needs to be split into two: one for if the floating
  // window is showing and one for the state of an individual panel.
  NOTIMPLEMENTED();
}

void GlicPanelCoordinatorImpl::RemoveStateObserver(StateObserver* observer) {
  // The StateObserver needs to be split into two: one for if the floating
  // window is showing and one for the state of an individual panel.
  NOTIMPLEMENTED();
}

bool GlicPanelCoordinatorImpl::IsActive() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return false;
}

bool GlicPanelCoordinatorImpl::IsShowing() const {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return false;
}

bool GlicPanelCoordinatorImpl::IsAttached() const {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return false;
}

bool GlicPanelCoordinatorImpl::IsDetached() const {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return false;
}

base::CallbackListSubscription
GlicPanelCoordinatorImpl::AddWindowActivationChangedCallback(
    WindowActivationChangedCallback callback) {
  return window_activation_callback_list_.Add(std::move(callback));
}

void GlicPanelCoordinatorImpl::Preload() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

void GlicPanelCoordinatorImpl::Reload() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

bool GlicPanelCoordinatorImpl::IsWarmed() const {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return false;
}

base::WeakPtr<GlicWindowController> GlicPanelCoordinatorImpl::GetWeakPtr() {
  // TODO(crbug.com/441542357) - remove from public interface.
  // Method should only be called on individual panels not the coordinator.
  NOTREACHED();
}

GlicView* GlicPanelCoordinatorImpl::GetGlicView() const {
  // Method should only be called on individual panels not the coordinator.
  NOTREACHED();
}

base::WeakPtr<views::View> GlicPanelCoordinatorImpl::GetGlicViewAsView() {
  // Method should only be called on individual panels not the coordinator.
  NOTREACHED();
}

GlicWidget* GlicPanelCoordinatorImpl::GetGlicWidget() const {
  // Method should only be called on individual panels not the coordinator.
  NOTREACHED();
}

gfx::NativeWindow GlicPanelCoordinatorImpl::GetHostNativeWindow() {
  // Method should only be called on individual panels not the coordinator.
  NOTREACHED();
}

Browser* GlicPanelCoordinatorImpl::attached_browser() {
  // Method should only be called on individual panels not the coordinator.
  NOTREACHED();
}

GlicWindowController::State GlicPanelCoordinatorImpl::state() const {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return GlicWindowController::State::kClosed;
}

GlicWindowAnimator* GlicPanelCoordinatorImpl::window_animator() {
  // TODO(crbug.com/441545112) - Remove from GlicWindowController.
  // Method should only be called on individual panels not the coordinator.
  NOTREACHED();
}

Profile* GlicPanelCoordinatorImpl::profile() {
  return profile_;
}

gfx::Rect GlicPanelCoordinatorImpl::GetInitialBounds(Browser* browser) {
  // TODO(crbug.com/441546104) - Remove from GlicWindowController.
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void GlicPanelCoordinatorImpl::ShowDetachedForTesting() {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

void GlicPanelCoordinatorImpl::SetPreviousPositionForTesting(
    gfx::Point position) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
}

std::unique_ptr<GlicView>
GlicPanelCoordinatorImpl::CreateGlicViewForSidePanel() {
  // Method should only be called on individual panels not the coordinator.
  NOTREACHED();
}

base::CallbackListSubscription
GlicPanelCoordinatorImpl::RegisterFloatyStateChange(
    FloatyStateChangeCallback callback) {
  // Method should only be called on individual panels not the coordinator.
  NOTIMPLEMENTED();
  return floaty_state_change_callback_list_.Add(std::move(callback));
}

void GlicPanelCoordinatorImpl::AttachInstance(GlicInstance* instance) {
  NOTIMPLEMENTED();
}

void GlicPanelCoordinatorImpl::DetachInstance(GlicInstance* instance) {
  NOTIMPLEMENTED();
}
}  // namespace glic
