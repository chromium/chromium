// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_ARC_ARC_VIRTUAL_KEYBOARD_CONTAINER_LAYOUT_MANAGER_H_
#define ASH_KEYBOARD_ARC_ARC_VIRTUAL_KEYBOARD_CONTAINER_LAYOUT_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class ArcVirtualKeyboardContainerLayoutManager : public aura::LayoutManager {
 public:
  explicit ArcVirtualKeyboardContainerLayoutManager(
      aura::Window* arc_ime_window_parent_container);

  ArcVirtualKeyboardContainerLayoutManager(
      const ArcVirtualKeyboardContainerLayoutManager&) = delete;
  ArcVirtualKeyboardContainerLayoutManager& operator=(
      const ArcVirtualKeyboardContainerLayoutManager&) = delete;

  // aura::LayoutManager:
  void OnWindowResized() override;
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

 private:
  raw_ptr<aura::Window> arc_ime_window_parent_container_;
};

}  // namespace ash

#endif  // ASH_KEYBOARD_ARC_ARC_VIRTUAL_KEYBOARD_CONTAINER_LAYOUT_MANAGER_H_
