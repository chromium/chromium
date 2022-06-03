// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_

#include <memory>

#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets_f.h"

namespace ui {
class LayerAnimationObserver;
}  // namespace ui

namespace views {
class Background;
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
