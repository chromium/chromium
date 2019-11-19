// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_layout_manager.h"

#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/system/status_area_widget.h"
#include "base/auto_reset.h"
#include "ui/aura/window.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// StatusAreaLayoutManager, public:

StatusAreaLayoutManager::StatusAreaLayoutManager(ShelfWidget* shelf_widget)
    : in_layout_(false), shelf_widget_(shelf_widget) {}

StatusAreaLayoutManager::~StatusAreaLayoutManager() = default;

////////////////////////////////////////////////////////////////////////////////
// StatusAreaLayoutManager, aura::LayoutManager implementation:

void StatusAreaLayoutManager::OnWindowResized() {
  LayoutStatusArea();
}

void StatusAreaLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  // Don't adjust children during StatusAreaWidget teardown. It's not clear if
  // this check is still necessary, but |this| is definitely destroyed after
  // StatusAreaWidget. http://crbug.com/700122.
  if (!shelf_widget_->status_area_widget())
    return;

  // Only need to have the shelf do a layout if the child changing is the status
  // area and the shelf isn't in the process of doing a layout.
  if (child != shelf_widget_->status_area_widget()->GetNativeWindow() ||
      in_layout_) {
    WmDefaultLayoutManager::SetChildBounds(child, requested_bounds);
    return;
  }

  // If the bounds match, no need to do anything. Check for target bounds to
  // ensure any active animation is retargeted.
  if (requested_bounds == child->GetTargetBounds())
    return;

  WmDefaultLayoutManager::SetChildBounds(child, requested_bounds);
  LayoutStatusArea();
}

////////////////////////////////////////////////////////////////////////////////
// StatusAreaLayoutManager, private:

void StatusAreaLayoutManager::LayoutStatusArea() {
  // Shelf layout manager may be already doing layout.
  if (shelf_widget_->shelf_layout_manager()->updating_bounds())
    return;

  base::AutoReset<bool> auto_reset_in_layout(&in_layout_, true);
  shelf_widget_->shelf_layout_manager()->LayoutShelf();
}

}  // namespace ash
