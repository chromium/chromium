// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BUBBLE_BUBBLE_UTILS_H_
#define ASH_BUBBLE_BUBBLE_UTILS_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace views {
class Label;
}  // namespace views

namespace ash::bubble_utils {

// Returns false if `event` should not close a bubble. Returns true if `event`
// should close a bubble, or if more processing is required. Callers may also
// need to check for a click on the view that spawned the bubble (otherwise the
// bubble will close and immediately reopen).
ASH_EXPORT bool ShouldCloseBubbleForEvent(const ui::LocatedEvent& event);

// Enumeration of supported label styles.
enum class LabelStyle {
  kBadge,
  kBody,
  kChipBody,
  kChipTitle,
  kHeader,
  kSubheader,
  kSubtitle,
};

// Applies the specified `style` to the given `label`.
ASH_EXPORT void ApplyStyle(views::Label* label, LabelStyle style);

// Creates a label with optional `text` matching the specified `style`. The
// label will paint correctly even if it is not added to the view hierarchy.
std::unique_ptr<views::Label> CreateLabel(
    LabelStyle style,
    const std::u16string& text = std::u16string());

}  // namespace ash::bubble_utils

#endif  // ASH_BUBBLE_BUBBLE_UTILS_H_
