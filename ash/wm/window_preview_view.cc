// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_preview_view.h"

#include "ash/wm/window_mirror_view_pip.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

gfx::Rect GetClientAreaBoundsInScreen(aura::Window* window) {
  const int inset = window->GetProperty(aura::client::kTopViewInset);
  if (inset > 0) {
    gfx::Rect bounds = window->GetBoundsInScreen();
    bounds.Inset(gfx::Insets::TLBR(inset, 0, 0, 0));
    return bounds;
  }
  // The source window may not have a widget in unit tests.
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget || !widget->client_view())
    return gfx::Rect();
  views::View* client_view = widget->client_view();
  gfx::Rect bounds = client_view->GetLocalBounds();
  views::View::ConvertRectToScreen(client_view, &bounds);
  return bounds;
}

}  // namespace

WindowPreviewView::WindowPreviewView(aura::Window* window) : window_(window) {
  DCHECK(window);
  aura::client::GetTransientWindowClient()->AddObserver(this);

  for (auto* transient_window : GetTransientTreeIterator(window_))
    AddWindow(transient_window);
}

WindowPreviewView::~WindowPreviewView() {
  for (aura::Window* window : unparented_transient_children_) {
    window->RemoveObserver(this);
  }
  for (auto entry : mirror_views_)
    entry.first->RemoveObserver(this);
  aura::client::GetTransientWindowClient()->RemoveObserver(this);
}

void WindowPreviewView::RecreatePreviews() {
  for (auto entry : mirror_views_)
    entry.second->RecreateMirrorLayers();
}

gfx::Size WindowPreviewView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // The preferred size of this view is the union of all the windows it is made
  // up of with, scaled to match the ratio of the main window to its mirror
  // view's preferred size.
  aura::Window* root = ::wm::GetTransientRoot(window_);
  DCHECK(root);
  const gfx::RectF union_rect = GetUnionRect();
  gfx::RectF window_bounds(GetClientAreaBoundsInScreen(root));
  gfx::SizeF window_size(1.f, 1.f);
  auto it = mirror_views_.find(root);
  if (it != mirror_views_.end()) {
    window_size = gfx::SizeF(it->second->CalculatePreferredSize({}));
    if (window_size.IsEmpty())
      return gfx::Size();  // Avoids divide by zero below.
  }
  gfx::Vector2dF scale(window_bounds.width() / window_size.width(),
                       window_bounds.height() / window_size.height());
  return gfx::ToRoundedSize(
      gfx::ScaleSize(union_rect.size(), scale.x(), scale.y()));
}

void WindowPreviewView::Layout(PassKey) {
  const gfx::RectF union_rect = GetUnionRect();
  if (union_rect.IsEmpty())
    return;  // Avoids divide by zero below.

  // Layout the windows in |mirror_view_| by keeping the same ratio of the
  // original windows to the union of all windows in |mirror_views_|.
  const gfx::RectF local_bounds = gfx::RectF(GetLocalBounds());
  const gfx::Point union_origin = gfx::ToRoundedPoint(union_rect.origin());

  gfx::Vector2dF scale(local_bounds.width() / union_rect.width(),
                       local_bounds.height() / union_rect.height());
  for (auto entry : mirror_views_) {
    const gfx::Rect bounds = GetClientAreaBoundsInScreen(entry.first) -
                             union_origin.OffsetFromOrigin();
    entry.second->SetBoundsRect(
        gfx::ScaleToRoundedRect(bounds, scale.x(), scale.y()));
  }
}

void WindowPreviewView::OnTransientChildWindowAdded(
    aura::Window* parent,
    aura::Window* transient_child) {
  aura::Window* root = ::wm::GetTransientRoot(window_);
  if (!::wm::HasTransientAncestor(parent, root) && parent != root)
    return;

  if (!transient_child->parent()) {
    transient_child->AddObserver(this);
    unparented_transient_children_.emplace(transient_child);
    return;
  }

  AddWindow(transient_child);
}

void WindowPreviewView::OnTransientChildWindowRemoved(
    aura::Window* parent,
    aura::Window* transient_child) {
  aura::Window* root = ::wm::GetTransientRoot(window_);
  if (!::wm::HasTransientAncestor(parent, root) && parent != root)
    return;

  RemoveWindow(transient_child);
}

void WindowPreviewView::OnWindowDestroying(aura::Window* window) {
  RemoveWindow(window);
}

void WindowPreviewView::OnWindowParentChanged(aura::Window* window,
                                              aura::Window* parent) {
  if (!unparented_transient_children_.contains(window))
    return;

  DCHECK(parent);
  unparented_transient_children_.erase(window);
  window->RemoveObserver(this);
  AddWindow(window);
}

void WindowPreviewView::AddWindow(aura::Window* window) {
  DCHECK(!mirror_views_.contains(window));
  DCHECK(!unparented_transient_children_.contains(window));
  DCHECK(!window->HasObserver(this));

  if (window->GetType() == aura::client::WINDOW_TYPE_POPUP)
    return;

  if (!window->HasObserver(this))
    window->AddObserver(this);

  auto mirror_view = window_util::IsArcPipWindow(window)
                         ? std::make_unique<WindowMirrorViewPip>(window)
                         : std::make_unique<WindowMirrorView>(window);
  mirror_views_[window] = mirror_view.get();
  AddChildView(std::move(mirror_view));
}

void WindowPreviewView::RemoveWindow(aura::Window* window) {
  auto iter = unparented_transient_children_.find(window);
  if (iter != unparented_transient_children_.end()) {
    unparented_transient_children_.erase(iter);
    window->RemoveObserver(this);
    DCHECK(!mirror_views_.count(window));
    return;
  }

  auto it = mirror_views_.find(window);
  if (it == mirror_views_.end())
    return;

  auto* view = it->second.get();
  RemoveChildViewT(view);
  it->first->RemoveObserver(this);

  mirror_views_.erase(it);
}

gfx::RectF WindowPreviewView::GetUnionRect() const {
  gfx::Rect bounds;
  for (auto entry : mirror_views_)
    bounds.Union(GetClientAreaBoundsInScreen(entry.first));
  return gfx::RectF(bounds);
}

BEGIN_METADATA(WindowPreviewView)
END_METADATA

}  // namespace ash
