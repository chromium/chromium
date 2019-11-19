// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_preview_view.h"

#include "ash/wm/window_mirror_view_pip.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/wm/core/window_util.h"

namespace ash {

WindowPreviewView::WindowPreviewView(aura::Window* window,
                                     bool trilinear_filtering_on_init)
    : window_(window),
      trilinear_filtering_on_init_(trilinear_filtering_on_init) {
  DCHECK(window);
  aura::client::GetTransientWindowClient()->AddObserver(this);

  for (auto* window : GetTransientTreeIterator(window_))
    AddWindow(window);
}

WindowPreviewView::~WindowPreviewView() {
  for (auto* window : unparented_transient_children_)
    window->RemoveObserver(this);
  for (auto entry : mirror_views_)
    entry.first->RemoveObserver(this);
  aura::client::GetTransientWindowClient()->RemoveObserver(this);
}

void WindowPreviewView::RecreatePreviews() {
  for (auto entry : mirror_views_)
    entry.second->RecreateMirrorLayers();
}

gfx::Size WindowPreviewView::CalculatePreferredSize() const {
  // The preferred size of this view is the union of all the windows it is made
  // up of with, scaled to match the ratio of the main window to its mirror
  // view's preferred size.
  aura::Window* root = ::wm::GetTransientRoot(window_);
  DCHECK(root);
  const gfx::RectF union_rect = GetUnionRect();
  gfx::RectF window_bounds(root->GetBoundsInScreen());
  window_bounds.Inset(0, root->GetProperty(aura::client::kTopViewInset), 0, 0);
  gfx::SizeF window_size(1.f, 1.f);
  auto it = mirror_views_.find(root);
  if (it != mirror_views_.end())
    window_size = gfx::SizeF(it->second->CalculatePreferredSize());
  gfx::Vector2dF scale(window_bounds.width() / window_size.width(),
                       window_bounds.height() / window_size.height());
  return gfx::Size(gfx::ToRoundedInt(union_rect.width() * scale.x()),
                   gfx::ToRoundedInt(union_rect.height() * scale.y()));
}

void WindowPreviewView::Layout() {
  // Layout the windows in |mirror_view_| by keeping the same ratio of the
  // original windows to the union of all windows in |mirror_views_|.
  const gfx::RectF local_bounds = gfx::RectF(GetLocalBounds());
  const gfx::RectF union_rect = GetUnionRect();
  const gfx::Point union_origin = gfx::ToRoundedPoint(union_rect.origin());

  gfx::Vector2dF scale(local_bounds.width() / union_rect.width(),
                       local_bounds.height() / union_rect.height());
  for (auto entry : mirror_views_) {
    const gfx::Rect bounds = entry.first->GetBoundsInScreen();
    gfx::Rect mirror_bounds;
    mirror_bounds.set_x(
        gfx::ToRoundedInt((bounds.x() - union_origin.x()) * scale.x()));
    mirror_bounds.set_y(
        gfx::ToRoundedInt((bounds.y() - union_origin.y()) * scale.y()));
    mirror_bounds.set_width(gfx::ToRoundedInt(bounds.width() * scale.x()));
    mirror_bounds.set_height(gfx::ToRoundedInt(bounds.height() * scale.y()));
    entry.second->SetBoundsRect(mirror_bounds);
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

  if (window->type() == aura::client::WINDOW_TYPE_POPUP)
    return;

  if (!window->HasObserver(this))
    window->AddObserver(this);

  auto* mirror_view =
      window_util::IsArcPipWindow(window)
          ? new WindowMirrorViewPip(window, trilinear_filtering_on_init_)
          : new WindowMirrorView(window, trilinear_filtering_on_init_);
  mirror_views_[window] = mirror_view;
  AddChildView(mirror_view);
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

  RemoveChildView(it->second);
  it->first->RemoveObserver(this);
  mirror_views_.erase(it);
}

gfx::RectF WindowPreviewView::GetUnionRect() const {
  gfx::Rect bounds;
  for (auto entry : mirror_views_)
    bounds.Union(entry.first->GetBoundsInScreen());
  return gfx::RectF(bounds);
}

}  // namespace ash
