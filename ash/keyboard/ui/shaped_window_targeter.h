// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_SHAPED_WINDOW_TARGETER_H_
#define ASH_KEYBOARD_UI_SHAPED_WINDOW_TARGETER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/aura/window_targeter.h"

namespace keyboard {

// A WindowTargeter for windows with hit-test areas that can be defined by a
// list of rectangles.
class ShapedWindowTargeter : public aura::WindowTargeter {
 public:
  explicit ShapedWindowTargeter(std::vector<gfx::Rect> hit_test_rects);
  ~ShapedWindowTargeter() override;

 private:
  // aura::WindowTargeter:
  std::unique_ptr<aura::WindowTargeter::HitTestRects> GetExtraHitTestShapeRects(
      aura::Window* target) const override;

  std::vector<gfx::Rect> hit_test_rects_;

  DISALLOW_COPY_AND_ASSIGN(ShapedWindowTargeter);
};

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_SHAPED_WINDOW_TARGETER_H_
