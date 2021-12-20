// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ANIMATIONS_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ANIMATIONS_H_

#include "base/callback_forward.h"
#include "base/callback_helpers.h"

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

// Animates the desks templates grid when it is shown, fading out current
// overview items and widgets, and fading in the grid.
void PerformFadeInLayer(ui::Layer* layer);

// Animates linear fade out of overview items.
void PerformFadeOutLayer(
    ui::Layer* layer,
    base::OnceClosure on_animation_ended_callback = base::DoNothing());

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ANIMATIONS_H_
