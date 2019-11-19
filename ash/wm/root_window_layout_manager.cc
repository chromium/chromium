// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/root_window_layout_manager.h"

#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// RootWindowLayoutManager, public:

RootWindowLayoutManager::RootWindowLayoutManager(aura::Window* owner)
    : owner_(owner) {}

RootWindowLayoutManager::~RootWindowLayoutManager() = default;

////////////////////////////////////////////////////////////////////////////////
// RootWindowLayoutManager, aura::LayoutManager implementation:

void RootWindowLayoutManager::OnWindowResized() {
  gfx::Rect bounds(owner_->bounds().size());
  for (auto* container : containers_)
    container->SetBounds(bounds);
}

void RootWindowLayoutManager::OnWindowAddedToLayout(aura::Window* child) {}

void RootWindowLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {}

void RootWindowLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {}

void RootWindowLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {}

void RootWindowLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
}

void RootWindowLayoutManager::AddContainer(aura::Window* window) {
  DCHECK(!window->GetToplevelWindow());
  containers_.push_back(window);
}

}  // namespace ash
