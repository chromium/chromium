// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BUBBLE_BUBBLE_UTILS_H_
#define ASH_BUBBLE_BUBBLE_UTILS_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "ash/style/ash_color_id.h"
#include "ui/color/color_id.h"
#include "ui/gfx/font.h"

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace views {
class Label;
}  // namespace views

namespace ash {

enum class TypographyToken;

namespace bubble_utils {

// Returns false if `event` should not close a bubble. Returns true if `event`
// should close a bubble, or if more processing is required. Callers may also
// need to check for a click on the view that spawned the bubble (otherwise the
// bubble will close and immediately reopen).
ASH_EXPORT bool ShouldCloseBubbleForEvent(const ui::LocatedEvent& event);

// Applies the specified `style` and `text_color` to the given `label`.
ASH_EXPORT void ApplyStyle(
    views::Label* label,
    TypographyToken style,
    ui::ColorId text_color_id = kColorAshTextColorPrimary);

// Creates a label with optional `text` and `text_color` matching the specified
// `style`. The label will paint correctly even if it is not added to the view
// hierarchy.
ASH_EXPORT std::unique_ptr<views::Label> CreateLabel(
    TypographyToken style,
    const std::u16string& text = std::u16string(),
    ui::ColorId text_color_id = kColorAshTextColorPrimary);

}  // namespace bubble_utils
}  // namespace ash

#endif  // ASH_BUBBLE_BUBBLE_UTILS_H_
