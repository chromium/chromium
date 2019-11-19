// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/shaped_window_targeter.h"

#include <utility>

#include "ui/gfx/geometry/rect.h"

namespace keyboard {

ShapedWindowTargeter::ShapedWindowTargeter(
    std::vector<gfx::Rect> hit_test_rects)
    : hit_test_rects_(std::move(hit_test_rects)) {}

ShapedWindowTargeter::~ShapedWindowTargeter() = default;

std::unique_ptr<aura::WindowTargeter::HitTestRects>
ShapedWindowTargeter::GetExtraHitTestShapeRects(aura::Window* target) const {
  return std::make_unique<aura::WindowTargeter::HitTestRects>(hit_test_rects_);
}

}  // namespace keyboard
