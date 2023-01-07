// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BUBBLE_BUBBLE_UTILS_H_
#define ASH_BUBBLE_BUBBLE_UTILS_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/style/ash_color_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/font.h"

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

enum class FontName {
  kGoogleSans,
  kRoboto,
};

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

struct ASH_EXPORT LabelStyleOverrides {
  LabelStyleOverrides();
  LabelStyleOverrides(
      absl::optional<gfx::Font::Weight> font_weight,
      absl::optional<AshColorProvider::ContentLayerType> text_color);
  ~LabelStyleOverrides();

  absl::optional<gfx::Font::Weight> font_weight;
  absl::optional<AshColorProvider::ContentLayerType> text_color;
};

// Applies the specified `style` to the given `label`.
ASH_EXPORT void ApplyStyle(
    views::Label* label,
    LabelStyle style,
    const LabelStyleOverrides& overrides = LabelStyleOverrides{});

// Creates a label with optional `text` matching the specified `style`. The
// label will paint correctly even if it is not added to the view hierarchy.
// NOTE: `LabelStyleOverrides` can be provided for any necessary overrides.
std::unique_ptr<views::Label> CreateLabel(
    LabelStyle style,
    const std::u16string& text = std::u16string(),
    const LabelStyleOverrides& overrides = LabelStyleOverrides{});
}  // namespace ash::bubble_utils

#endif  // ASH_BUBBLE_BUBBLE_UTILS_H_
