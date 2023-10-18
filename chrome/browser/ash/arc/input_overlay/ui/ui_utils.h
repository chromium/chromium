// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_UI_UTILS_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_UI_UTILS_H_

#include <string>

#include "ui/events/keycodes/dom/dom_code.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Rect;
}  // namespace gfx

namespace arc::input_overlay {

// Get text of `code` displayed on input mappings.
std::u16string GetDisplayText(const ui::DomCode code);

// Get the accessible name for displayed `text` showing on input mappings.
// Sometimes, `text` is a symbol.
std::u16string GetDisplayTextAccessibleName(const std::u16string& text);

// Returns bounds of `root_window` excluding the shelf if the shelf is visible.
gfx::Rect CalculateAvailableBounds(aura::Window* root_window);

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_UI_UTILS_H_
