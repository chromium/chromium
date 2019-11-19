// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_ANIMATIONS_H_
#define ASH_WM_WINDOW_ANIMATIONS_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/gfx/animation/tween.h"
#include "ui/wm/core/window_animations.h"

namespace aura {
class Window;
}

namespace ui {
class LayerTreeOwner;
}

// This is only for animations specific to Ash. For window animations shared
// with desktop Chrome, see ui/views/corewm/window_animations.h.
namespace ash {

// Amount of time for the cross fade animation.
constexpr base::TimeDelta kCrossFadeDuration =
    base::TimeDelta::FromMilliseconds(200);

// Implementation of cross fading. Window is the window being cross faded. It
// should be at the target bounds. |old_layer_owner| contains the previous layer
// from |window|.
ASH_EXPORT base::TimeDelta CrossFadeAnimation(
    aura::Window* window,
    std::unique_ptr<ui::LayerTreeOwner> old_layer_owner);

ASH_EXPORT bool AnimateOnChildWindowVisibilityChanged(aura::Window* window,
                                                      bool visible);

// Creates vector of animation sequences that lasts for |duration| and changes
// brightness and grayscale to |target_value|. Caller takes ownership of
// returned LayerAnimationSequence objects.
ASH_EXPORT std::vector<ui::LayerAnimationSequence*>
CreateBrightnessGrayscaleAnimationSequence(float target_value,
                                           base::TimeDelta duration);

// Returns the approximate bounds to which |window| will be animated when it
// is minimized. The bounds are approximate because the minimize animation
// involves rotation.
ASH_EXPORT gfx::Rect GetMinimizeAnimationTargetBoundsInScreen(
    aura::Window* window);

}  // namespace ash

#endif  // ASH_WM_WINDOW_ANIMATIONS_H_
