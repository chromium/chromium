// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/pixel/ash_pixel_diff_util.h"

#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

void PopulateUiComponentScreenBounds(std::vector<gfx::Rect>* rects) {}

gfx::Rect GetUiComponentScreenBounds(views::View* view) {
  return view->GetBoundsInScreen();
}

gfx::Rect GetUiComponentScreenBounds(views::Widget* widget) {
  return widget->GetWindowBoundsInScreen();
}

gfx::Rect GetUiComponentScreenBounds(aura::Window* window) {
  return window->GetBoundsInScreen();
}

}  // namespace ash
