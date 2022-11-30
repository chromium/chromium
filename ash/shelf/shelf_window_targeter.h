// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_WINDOW_TARGETER_H_
#define ASH_SHELF_SHELF_WINDOW_TARGETER_H_

#include "ash/shelf/shelf_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/core/easy_resize_window_targeter.h"

namespace ash {

class Shelf;

// ShelfWindowTargeter makes it easier to resize windows with the mouse when the
// window-edge slightly overlaps with the shelf edge. The targeter also makes it
// easier to drag the shelf out with touch while it is hidden.
class ShelfWindowTargeter : public ::wm::EasyResizeWindowTargeter,
                            public aura::WindowObserver,
                            public ShelfObserver {
 public:
  ShelfWindowTargeter(aura::Window* container, Shelf* shelf);

  ShelfWindowTargeter(const ShelfWindowTargeter&) = delete;
  ShelfWindowTargeter& operator=(const ShelfWindowTargeter&) = delete;

  ~ShelfWindowTargeter() override;

 private:
  // ::wm::EasyResizeWindowTargeter:
  bool ShouldUseExtendedBounds(const aura::Window* window) const override;
  bool GetHitTestRects(aura::Window* target,
                       gfx::Rect* hit_test_rect_mouse,
                       gfx::Rect* hit_test_rect_touch) const override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // ShelfObserver:
  void WillChangeVisibilityState(ShelfVisibilityState new_state) override;

  Shelf* shelf_;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_WINDOW_TARGETER_H_
