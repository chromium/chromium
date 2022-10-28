// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ANIMATIONS_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ANIMATIONS_H_

#include "base/functional/callback.h"

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

// Immediately shows `layer` if `animate` is false. Does a linear fade in of
// `layer` if `animate` is true.
void PerformFadeInLayer(ui::Layer* layer, bool animate);

// Immediately hides `layer` if `animate` is false. Does a linear fade out of
// `layer` if `animate` is true.
void PerformFadeOutLayer(ui::Layer* layer,
                         bool animate,
                         base::OnceClosure on_animation_ended_callback);

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ANIMATIONS_H_
