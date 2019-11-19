// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_STATUS_AREA_LAYOUT_MANAGER_H_
#define ASH_SYSTEM_STATUS_AREA_LAYOUT_MANAGER_H_

#include "ash/wm/wm_default_layout_manager.h"
#include "base/macros.h"

namespace ash {

class ShelfWidget;

// StatusAreaLayoutManager is a layout manager responsible for the status area.
// In any case when status area needs relayout it redirects this call to
// ShelfLayoutManager.
class StatusAreaLayoutManager : public WmDefaultLayoutManager {
 public:
  explicit StatusAreaLayoutManager(ShelfWidget* shelf_widget);
  ~StatusAreaLayoutManager() override;

  // WmDefaultLayoutManager:
  void OnWindowResized() override;
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

 private:
  // Updates layout of the status area. Effectively calls ShelfLayoutManager
  // to update layout of the shelf.
  void LayoutStatusArea();

  // True when inside LayoutStatusArea method.
  // Used to prevent calling itself again from SetChildBounds().
  bool in_layout_;

  ShelfWidget* shelf_widget_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaLayoutManager);
};

}  // namespace ash

#endif  // ASH_SYSTEM_STATUS_AREA_LAYOUT_MANAGER_H_
