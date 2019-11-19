// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/custom_tab/arc_custom_tab_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/threading/sequenced_task_runner_handle.h"
#include "components/exo/surface.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// Enumerates surfaces under the window.
void EnumerateSurfaces(aura::Window* window, std::vector<exo::Surface*>* out) {
  auto* surface = exo::Surface::AsSurface(window);
  if (surface)
    out->push_back(surface);
  for (aura::Window* child : window->children())
    EnumerateSurfaces(child, out);
}

}  // namespace

ArcCustomTab::ArcCustomTab() = default;
ArcCustomTab::~ArcCustomTab() = default;

// static
std::unique_ptr<ArcCustomTab> ArcCustomTab::Create(aura::Window* arc_app_window,
                                                   int32_t surface_id,
                                                   int32_t top_margin) {
  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(arc_app_window);
  if (!widget) {
    LOG(ERROR) << "No widget for the ARC app window.";
    return nullptr;
  }
  auto* parent = widget->widget_delegate()->GetContentsView();
  auto view = std::make_unique<ArcCustomTabView>(arc_app_window, surface_id,
                                                 top_margin);
  parent->AddChildView(view.get());
  parent->SetLayoutManager(std::make_unique<views::FillLayout>());
  parent->Layout();

  return std::move(view);
}

ArcCustomTabView::ArcCustomTabView(aura::Window* arc_app_window,
                                   int32_t surface_id,
                                   int32_t top_margin)
    : host_(new views::NativeViewHost()),
      arc_app_window_(arc_app_window),
      surface_id_(surface_id),
      top_margin_(top_margin) {
  AddChildView(host_);
  arc_app_window_->AddObserver(this);
  set_owned_by_client();
}

ArcCustomTabView::~ArcCustomTabView() {
  for (auto* window : observed_surfaces_)
    window->RemoveObserver(this);
  arc_app_window_->RemoveObserver(this);

  if (surface_window_)
    surface_window_->RemoveObserver(this);
  if (native_view_container_)
    native_view_container_->RemoveObserver(this);
}

void ArcCustomTabView::Attach(gfx::NativeView view) {
  DCHECK(view);
  DCHECK(!host_->native_view());
  host_->Attach(view);
  native_view_container_ = host_->GetNativeViewContainer();
  native_view_container_->SetEventTargeter(
      std::make_unique<aura::WindowTargeter>());
  native_view_container_->parent()->StackChildAtTop(native_view_container_);
  native_view_container_->AddObserver(this);
}

gfx::NativeView ArcCustomTabView::GetHostView() {
  return host_->native_view();
}

void ArcCustomTabView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (previous_bounds.size() != size()) {
    InvalidateLayout();
    host_->InvalidateLayout();
  }
}

void ArcCustomTabView::Layout() {
  exo::Surface* surface = FindSurface();
  if (!surface)
    return;
  DCHECK(observed_surfaces_.empty());
  aura::Window* surface_window = surface->window();
  if (surface_window_ != surface_window) {
    if (surface_window_)
      surface_window_->RemoveObserver(this);
    surface_window->AddObserver(this);
    surface_window_ = surface_window;
  }
  gfx::Point topleft(0, top_margin_),
      bottomright(surface_window->bounds().width(),
                  surface_window->bounds().height());
  ConvertPointFromWindow(surface_window, &topleft);
  ConvertPointFromWindow(surface_window, &bottomright);
  gfx::Rect bounds(topleft, gfx::Size(bottomright.x() - topleft.x(),
                                      bottomright.y() - topleft.y()));
  host_->SetBoundsRect(bounds);
}

void ArcCustomTabView::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  if (params.receiver == arc_app_window_) {
    auto* surface = exo::Surface::AsSurface(params.target);
    if (surface && params.new_parent != nullptr) {
      // Call Layout() aggressively without checking the surface ID to start
      // observing surface ID updates in case it's not set yet.
      Layout();
    }
  }
}

void ArcCustomTabView::OnWindowBoundsChanged(aura::Window* window,
                                             const gfx::Rect& old_bounds,
                                             const gfx::Rect& new_bounds,
                                             ui::PropertyChangeReason reason) {
  if (window == surface_window_ && old_bounds.size() != new_bounds.size()) {
    InvalidateLayout();
    host_->InvalidateLayout();
  }
}

void ArcCustomTabView::OnWindowPropertyChanged(aura::Window* window,
                                               const void* key,
                                               intptr_t old) {
  if (observed_surfaces_.contains(window)) {
    if (key == exo::kClientSurfaceIdKey) {
      // Client surface ID was updated. Try to find the surface again.
      Layout();
    }
  }
}

void ArcCustomTabView::OnWindowStackingChanged(aura::Window* window) {
  if (window != host_->GetNativeViewContainer() || reorder_scheduled_)
    return;
  reorder_scheduled_ = true;
  // Reordering should happen asynchronously -- some entity (like
  // views::WindowReorderer) changes the window orders, and then ensures layer
  // orders later. Changing order here synchronously leads to inconsistent
  // window/layer ordering and causes weird graphical effects.
  // TODO(hashimoto): fix the views ordering and remove this handling.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ArcCustomTabView::EnsureWindowOrders,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ArcCustomTabView::OnWindowDestroying(aura::Window* window) {
  if (observed_surfaces_.contains(window)) {
    window->RemoveObserver(this);
    observed_surfaces_.erase(window);
  }
  if (window == surface_window_) {
    window->RemoveObserver(this);
    surface_window_ = nullptr;
  }
  if (window == native_view_container_) {
    window->RemoveObserver(this);
    native_view_container_ = nullptr;
  }
}

void ArcCustomTabView::EnsureWindowOrders() {
  reorder_scheduled_ = false;
  if (native_view_container_)
    native_view_container_->parent()->StackChildAtTop(native_view_container_);
}

void ArcCustomTabView::ConvertPointFromWindow(aura::Window* window,
                                              gfx::Point* point) {
  aura::Window::ConvertPointToTarget(window, GetWidget()->GetNativeWindow(),
                                     point);
  views::View::ConvertPointFromWidget(parent(), point);
}

exo::Surface* ArcCustomTabView::FindSurface() {
  std::vector<exo::Surface*> surfaces;
  EnumerateSurfaces(arc_app_window_, &surfaces);

  // Try to find the surface.
  for (auto* surface : surfaces) {
    if (surface->GetClientSurfaceId() == surface_id_) {
      // Stop observing surfaces for ID updates.
      for (auto* window : observed_surfaces_)
        window->RemoveObserver(this);
      observed_surfaces_.clear();
      return surface;
    }
  }
  // Surface not found. Start observing surfaces for ID updates.
  for (auto* surface : surfaces) {
    if (surface->GetClientSurfaceId() == 0) {
      if (observed_surfaces_.insert(surface->window()).second)
        surface->window()->AddObserver(this);
    }
  }
  return nullptr;
}

}  // namespace ash
