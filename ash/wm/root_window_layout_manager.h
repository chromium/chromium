// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_ROOT_WINDOW_LAYOUT_MANAGER_H_
#define ASH_WM_ROOT_WINDOW_LAYOUT_MANAGER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/aura/layout_manager.h"

namespace ash {

// A layout manager for the root window.
// Resizes all of its immediate children and their descendants to fill the
// bounds of the associated window.
class RootWindowLayoutManager : public aura::LayoutManager {
 public:
  explicit RootWindowLayoutManager(aura::Window* owner);

  RootWindowLayoutManager(const RootWindowLayoutManager&) = delete;
  RootWindowLayoutManager& operator=(const RootWindowLayoutManager&) = delete;

  ~RootWindowLayoutManager() override;

  // Overridden from aura::LayoutManager:
  void OnWindowResized() override;
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override;
  void OnWindowRemovedFromLayout(aura::Window* child) override;
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override;
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

  void AddContainer(aura::Window* window);

 private:
  raw_ptr<aura::Window> owner_;
  std::vector<raw_ptr<aura::Window, VectorExperimental>> containers_;
};

}  // namespace ash

#endif  // ASH_WM_ROOT_WINDOW_LAYOUT_MANAGER_H_
