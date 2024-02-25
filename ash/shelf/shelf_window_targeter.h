// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_WINDOW_TARGETER_H_
#define ASH_SHELF_SHELF_WINDOW_TARGETER_H_

#include "ash/shelf/shelf_observer.h"
#include "ash/shell_observer.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/core/easy_resize_window_targeter.h"

namespace ash {

class Shelf;

// ShelfWindowTargeter makes it easier to resize windows with the mouse when the
// window-edge slightly overlaps with the shelf edge. The targeter also makes it
// easier to drag the shelf out with touch while it is hidden.
class ShelfWindowTargeter : public ::wm::EasyResizeWindowTargeter,
                            public aura::WindowObserver,
                            public ShellObserver,
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

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  // ShelfObserver:
  void OnShelfVisibilityStateChanged(ShelfVisibilityState new_state) override;

  // Updates `mouse_inset_size_for_shelf_visibility_` and
  // `touch_inset_for_shelf_visibility_`, and runs
  // `UpdateInsets()`.
  void UpdateInsetsForVisibilityState(ShelfVisibilityState state);

  // Updates targeter insets to insets specified by
  // `mouse_size_for_shelf_visibility_` and `touch_size_for_shelf_visibility_`
  // and the current shelf alighment.
  void UpdateInsets();

  raw_ptr<Shelf> shelf_;

  // The size of the insets above the shelf for mouse events for the current
  // shelf visibility.
  int mouse_inset_size_for_shelf_visibility_ = 0;

  // The size of the insets above the shelf for touch events for the current
  // shelf visibility.
  int touch_inset_size_for_shelf_visibility_ = 0;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_WINDOW_TARGETER_H_
