// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ui {
class LayerAnimationObserver;
}  // namespace ui

namespace views {
class Background;
class View;
}  // namespace views

namespace ash::holding_space_util {

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

// Creates a circular background of the specified `color` and `fixed_size`.
std::unique_ptr<views::Background> CreateCircleBackground(ui::ColorId color_id,
                                                          size_t fixed_size);

// Creates a circular background of the specified `color` and optional `insets`.
std::unique_ptr<views::Background> CreateCircleBackground(
    ui::ColorId color_id,
    const gfx::InsetsF& insets = gfx::InsetsF());

// Creates a highlight path generator that determines paths based on logic
// specified in the provided `callback`.
std::unique_ptr<views::HighlightPathGenerator> CreateHighlightPathGenerator(
    base::RepeatingCallback<gfx::RRectF()> callback);

}  // namespace ash::holding_space_util

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_
