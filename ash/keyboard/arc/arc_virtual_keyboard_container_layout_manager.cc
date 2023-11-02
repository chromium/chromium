// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/arc/arc_virtual_keyboard_container_layout_manager.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "base/check_op.h"

namespace ash {

ArcVirtualKeyboardContainerLayoutManager::
    ArcVirtualKeyboardContainerLayoutManager(aura::Window* parent)
    : arc_ime_window_parent_container_(parent) {
  DCHECK(arc_ime_window_parent_container_);
  DCHECK_EQ(arc_ime_window_parent_container_->GetId(),
            kShellWindowId_ArcImeWindowParentContainer);
}

void ArcVirtualKeyboardContainerLayoutManager::OnWindowResized() {
  aura::Window* vk_container = arc_ime_window_parent_container_->GetChildById(
      kShellWindowId_ArcVirtualKeyboardContainer);
  DCHECK(vk_container);
  vk_container->SetBounds(arc_ime_window_parent_container_->bounds());
}

void ArcVirtualKeyboardContainerLayoutManager::OnWindowAddedToLayout(
    aura::Window* child) {
  if (child->GetId() == kShellWindowId_ArcVirtualKeyboardContainer)
    SetChildBounds(child, arc_ime_window_parent_container_->bounds());
}

void ArcVirtualKeyboardContainerLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  if (child->bounds() != requested_bounds)
    SetChildBoundsDirect(child, requested_bounds);
}

}  // namespace ash
