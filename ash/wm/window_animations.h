// Copyright 2012 The Chromium Authors
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
class Layer;
class LayerTreeOwner;
}

// This is only for animations specific to Ash. For window animations shared
// with desktop Chrome, see ui/wm/core/window_animations.h.
namespace ash {

// Direction for ash-specific window animations used in workspaces and
// lock/unlock animations.
enum LayerScaleAnimationDirection {
  LAYER_SCALE_ANIMATION_ABOVE,
  LAYER_SCALE_ANIMATION_BELOW,
};

// Applies scale related to the specified LayerScaleAnimationDirection.
ASH_EXPORT void SetTransformForScaleAnimation(
    ui::Layer* layer,
    LayerScaleAnimationDirection type);

// Implementation of cross fading. Window is the window being cross faded. It
// should be at the target bounds. `old_layer_owner` contains the previous layer
// from `window`.
ASH_EXPORT void CrossFadeAnimation(
    aura::Window* window,
    std::unique_ptr<ui::LayerTreeOwner> old_layer_owner);

// Implementation of cross fading for floating/unfloating a window. If
// `to_float` is true, animates to floated state, else animates unfloat.
ASH_EXPORT void CrossFadeAnimationForFloatUnfloat(
    aura::Window* window,
    std::unique_ptr<ui::LayerTreeOwner> old_layer_owner,
    bool to_float);

// Implementation of cross fading which only animates the new layer. The old
// layer will be owned by an observer which will update the transform as the new
// layer's transform and bounds change. This is used by the
// WorkspaceWindowResizer which needs to animate a window which has its bounds
// updated throughout the course of the animation.
ASH_EXPORT void CrossFadeAnimationAnimateNewLayerOnly(
    aura::Window* window,
    const gfx::Rect& target_bounds,
    base::TimeDelta duration,
    gfx::Tween::Type tween_type,
    const std::string& histogram_name);

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

// Triggers the window bounce animation.
ASH_EXPORT void BounceWindow(aura::Window* window);

}  // namespace ash

#endif  // ASH_WM_WINDOW_ANIMATIONS_H_
