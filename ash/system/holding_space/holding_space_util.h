// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets_f.h"

namespace ui {
class LayerAnimationObserver;
}  // namespace ui

namespace views {
class Background;
class Label;
class View;
}  // namespace views

namespace ash {
namespace holding_space_util {

// Animates in the specified `view` with the specified `duration` and optional
// `delay`, associating `observer` with the created animation sequences.
void AnimateIn(views::View* view,
               base::TimeDelta duration,
               base::TimeDelta delay,
               ui::LayerAnimationObserver* observer);

// Animates out the specified `view` with the specified `duration, associating
// `observer` with the created animation sequences.
void AnimateOut(views::View* view,
                base::TimeDelta duration,
                ui::LayerAnimationObserver* observer);

// Enumeration of supported label styles.
enum class LabelStyle {
  kBadge,
  kBody,
  kChip,
  kHeader,
};

// Applies the specified `style` to the given `label`.
void ApplyStyle(views::Label* label, LabelStyle style);

// Creates a label with optional `text` matching the specified `style`.
std::unique_ptr<views::Label> CreateLabel(
    LabelStyle style,
    const base::string16& text = base::string16());

// Creates a circular background of the specified `color` and `fixed_size`.
std::unique_ptr<views::Background> CreateCircleBackground(SkColor color,
                                                          size_t fixed_size);

// Creates a circular background of the specified `color` and optional `insets`.
std::unique_ptr<views::Background> CreateCircleBackground(
    SkColor color,
    const gfx::InsetsF& insets = gfx::InsetsF());

}  // namespace holding_space_util
}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_
