// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_PIXEL_ASH_PIXEL_DIFF_UTIL_H_
#define ASH_TEST_PIXEL_ASH_PIXEL_DIFF_UTIL_H_

#include <string>
#include <vector>

#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

std::string GetScreenshotPrefixForCurrentTestInfo();

// Returns the screen bounds of a UI component (a view, a widget or a window).
gfx::Rect GetUiComponentScreenBounds(views::View* view);
gfx::Rect GetUiComponentScreenBounds(views::Widget* widget);
gfx::Rect GetUiComponentScreenBounds(aura::Window* window);

// An empty function. Used by the overloaded variadic function.
void PopulateUiComponentScreenBounds(std::vector<gfx::Rect>* rects);

// Populates `rects` with the screen bounds of UI components. Each UI component
// can be a view, a widget or a window.
template <typename U, typename... T>
void PopulateUiComponentScreenBounds(std::vector<gfx::Rect>* rects,
                                     U ui_component,
                                     T... ui_components) {
  rects->push_back(GetUiComponentScreenBounds(ui_component));
  PopulateUiComponentScreenBounds(rects, ui_components...);
}

}  // namespace ash

#endif  // ASH_TEST_PIXEL_ASH_PIXEL_DIFF_UTIL_H_
