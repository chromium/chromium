// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_VIRTUAL_KEYBOARD_CONTAINER_LAYOUT_MANAGER_H_
#define ASH_KEYBOARD_VIRTUAL_KEYBOARD_CONTAINER_LAYOUT_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "ui/aura/layout_manager.h"

namespace ash {

// LayoutManager for the virtual keyboard container window. It keeps the size of
// the virtual keyboard container window and the ime window parent container.
class VirtualKeyboardContainerLayoutManager : public aura::LayoutManager {
 public:
  explicit VirtualKeyboardContainerLayoutManager(
      aura::Window* ime_window_parent_container);

  VirtualKeyboardContainerLayoutManager(
      const VirtualKeyboardContainerLayoutManager&) = delete;
  VirtualKeyboardContainerLayoutManager& operator=(
      const VirtualKeyboardContainerLayoutManager&) = delete;

  // Overridden from aura::LayoutManager
  void OnWindowResized() override;
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

 private:
  raw_ptr<aura::Window> ime_window_parent_container_;
};

}  // namespace ash

#endif  // ASH_KEYBOARD_VIRTUAL_KEYBOARD_CONTAINER_LAYOUT_MANAGER_H_
