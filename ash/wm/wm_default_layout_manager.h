// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WM_DEFAULT_LAYOUT_MANAGER_H_
#define ASH_WM_WM_DEFAULT_LAYOUT_MANAGER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/aura/layout_manager.h"

namespace ash {

// The default window layout manager used by ash.
class ASH_EXPORT WmDefaultLayoutManager : public aura::LayoutManager {
 public:
  WmDefaultLayoutManager();
  ~WmDefaultLayoutManager() override;

 protected:
  // Overridden from aura::LayoutManager:
  void OnWindowResized() override;
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override;
  void OnWindowRemovedFromLayout(aura::Window* child) override;
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override;
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WmDefaultLayoutManager);
};

}  // namespace ash

#endif  // ASH_WM_WM_DEFAULT_LAYOUT_MANAGER_H_
