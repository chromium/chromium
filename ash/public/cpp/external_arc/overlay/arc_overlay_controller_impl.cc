// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/overlay/arc_overlay_controller_impl.h"

#include "ash/wm/window_state.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class OverlayNativeViewHost final : public views::NativeViewHost {
  METADATA_HEADER(OverlayNativeViewHost, views::NativeViewHost)

 public:
  OverlayNativeViewHost() {
    set_suppress_default_focus_handling();
    GetViewAccessibility().SetRole(ax::mojom::Role::kApplication);
    GetViewAccessibility().SetName(
        std::string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  }

  OverlayNativeViewHost(const OverlayNativeViewHost&) = delete;
  OverlayNativeViewHost& operator=(const OverlayNativeViewHost&) = delete;
  ~OverlayNativeViewHost() override = default;

  // views::NativeViewHost:
  void OnFocus() override {
    auto* widget = views::Widget::GetWidgetForNativeView(native_view());
    if (widget) {
      GetWidget()->GetFocusManager()->set_shortcut_handling_suspended(true);
      widget->GetNativeWindow()->Focus();
    }
  }
};

BEGIN_METADATA(OverlayNativeViewHost)
END_METADATA

}  // namespace

ArcOverlayControllerImpl::ArcOverlayControllerImpl(aura::Window* host_window)
    : host_window_(host_window) {
  DCHECK(host_window_);

  VLOG(1) << "Host is " << host_window_->GetName();

  host_window_observer_.Observe(host_window_.get());

  overlay_container_ = new OverlayNativeViewHost();
  overlay_container_observer_.Observe(overlay_container_.get());

  auto* const widget = views::Widget::GetWidgetForNativeWindow(
      host_window_->GetToplevelWindow());
  DCHECK(widget);
  DCHECK(widget->GetContentsView());
  widget->GetContentsView()->AddChildView(overlay_container_.get());
}

ArcOverlayControllerImpl::~ArcOverlayControllerImpl() {
  EnsureOverlayWindowClosed();
  OnOverlayWindowClosed();
}

void ArcOverlayControllerImpl::AttachOverlay(aura::Window* overlay_window) {
  if (!overlay_container_ || !host_window_)
    return;

  DCHECK(overlay_window);
  DCHECK(!overlay_container_->native_view())
      << "An overlay is already attached";

  VLOG(1) << "Attaching overlay " << overlay_window->GetName() << " to host "
          << host_window_->GetName();

  overlay_window_ = overlay_window;
  overlay_window_observer_.Observe(overlay_window);

  ash::WindowState* host_window_state =
      ash::WindowState::Get(host_window_->GetToplevelWindow());
  saved_host_can_consume_system_keys_ =
      host_window_state->CanConsumeSystemKeys();
  host_window_state->SetCanConsumeSystemKeys(false);

  overlay_container_->Attach(overlay_window_);
  overlay_container_->GetNativeViewContainer()->SetEventTargeter(
      std::make_unique<aura::WindowTargeter>());

  overlay_container_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  overlay_container_->RequestFocus();

  // Make sure that the overlay comes on top of other windows.
  host_window_->StackChildAtTop(overlay_container_->GetNativeViewContainer());

  UpdateHostBounds();
}

void ArcOverlayControllerImpl::OnWindowDestroying(aura::Window* window) {
  if (host_window_observer_.IsObservingSource(window)) {
    host_window_ = nullptr;
    host_window_observer_.Reset();
    EnsureOverlayWindowClosed();
  }

  if (overlay_window_observer_.IsObservingSource(window))
    OnOverlayWindowClosed();
}

void ArcOverlayControllerImpl::OnViewIsDeleting(views::View* observed_view) {
  if (overlay_container_observer_.IsObservingSource(observed_view)) {
    OnOverlayWindowClosed();
    overlay_container_observer_.Reset();
    overlay_container_ = nullptr;
  }
}

void ArcOverlayControllerImpl::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (host_window_observer_.IsObservingSource(window) &&
      old_bounds.size() != new_bounds.size()) {
    UpdateHostBounds();
  }
}

void ArcOverlayControllerImpl::UpdateHostBounds() {
  if (!overlay_container_observer_.IsObserving()) {
    LOG(ERROR) << "No container to resize";
    return;
  }

  gfx::Point origin;
  gfx::Size size = host_window_->bounds().size();
  ConvertPointFromWindow(host_window_, &origin);
  overlay_container_->SetBounds(origin.x(), origin.y(), size.width(),
                                size.height());
}

void ArcOverlayControllerImpl::ConvertPointFromWindow(aura::Window* window,
                                                      gfx::Point* point) {
  views::Widget* const widget = overlay_container_->GetWidget();
  aura::Window::ConvertPointToTarget(window, widget->GetNativeWindow(), point);
  views::View::ConvertPointFromWidget(widget->GetContentsView(), point);
}

void ArcOverlayControllerImpl::EnsureOverlayWindowClosed() {
  // Ensure the overlay window is closed.
  if (overlay_window_observer_.IsObserving()) {
    VLOG(1) << "Forcing-closing overlay " << overlay_window_->GetName();
    auto* const widget =
        views::Widget::GetWidgetForNativeWindow(overlay_window_);
    widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

void ArcOverlayControllerImpl::OnOverlayWindowClosed() {
  ResetFocusBehavior();
  RestoreHostCanConsumeSystemKeys();
  overlay_window_ = nullptr;
  overlay_window_observer_.Reset();
}

void ArcOverlayControllerImpl::ResetFocusBehavior() {
  if (overlay_container_ && overlay_container_->GetWidget()) {
    overlay_container_->SetFocusBehavior(views::View::FocusBehavior::NEVER);
    overlay_container_->GetWidget()
        ->GetFocusManager()
        ->set_shortcut_handling_suspended(false);
  }
}

void ArcOverlayControllerImpl::RestoreHostCanConsumeSystemKeys() {
  if (host_window_observer_.IsObserving()) {
    ash::WindowState* host_window_state =
        ash::WindowState::Get(host_window_->GetToplevelWindow());
    host_window_state->SetCanConsumeSystemKeys(
        saved_host_can_consume_system_keys_);
  }
}

}  // namespace ash
