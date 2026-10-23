// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_ANIMATIONS_H_
#define ASH_WM_WINDOW_ANIMATIONS_H_

#include <memory>
#include <optional>
#include <string_view>

#include "ash/ash_export.h"
#include "ash/constants/ash_features.h"
#include "base/time/time.h"
#include "ui/gfx/animation/tween.h"
#include "ui/wm/core/window_animations.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}

namespace ui {
class Layer;
class LayerTreeOwner;
}

// This is only for animations specific to Ash. For window animations shared
// with desktop Chrome, see ui/wm/core/window_animations.h.
namespace ash {

struct BoundsChangeAnimationParams {
  // The default duration for an animation between two sets of bounds.
  static constexpr base::TimeDelta kDefaultDuration = base::Milliseconds(120);

  // The default duration for an animation to float or unfloat a window.
  static constexpr base::TimeDelta kFloatUnfloatDuration =
      base::Milliseconds(400);

  enum class Type {
    kBounds,
    kCrossFade,
    // Cross-fade that only animates the new layer while an observer updates the
    // old layer's transform as the window's bounds change during a drag.
    kCrossFadeAnimateNewLayerOnly,
  };

  Type type = features::IsCrossFadeBoundsChangeAnimationEnabled()
                  ? Type::kCrossFade
                  : Type::kBounds;
  std::optional<base::TimeDelta> duration = kDefaultDuration;
  gfx::Tween::Type tween_type = gfx::Tween::LINEAR;
  std::string_view histogram_name;
};

inline constexpr BoundsChangeAnimationParams kBoundsAnimationParams{
    .type = BoundsChangeAnimationParams::Type::kBounds,
    .duration = BoundsChangeAnimationParams::kDefaultDuration,
    .tween_type = gfx::Tween::LINEAR,
};
inline constexpr BoundsChangeAnimationParams kCrossFadeAnimationParams{
    .type = BoundsChangeAnimationParams::Type::kCrossFade,
    .duration = std::nullopt,
    .tween_type = gfx::Tween::EASE_OUT,
};
inline constexpr BoundsChangeAnimationParams kFloatCrossFadeAnimationParams{
    .type = BoundsChangeAnimationParams::Type::kCrossFade,
    .duration = BoundsChangeAnimationParams::kFloatUnfloatDuration,
    .tween_type = gfx::Tween::ACCEL_30_DECEL_20_85,
};
inline constexpr BoundsChangeAnimationParams kUnfloatCrossFadeAnimationParams{
    .type = BoundsChangeAnimationParams::Type::kCrossFade,
    .duration = BoundsChangeAnimationParams::kFloatUnfloatDuration,
    .tween_type = gfx::Tween::FAST_OUT_SLOW_IN_3,
};

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

// Cross-fades `window` from `old_layer_owner` (the previous layer tree
// recreated from `window`) to `window`'s current target bounds. Both
// `old_layer_owner->root()->bounds()` and `window->bounds()` must be non-empty.
// `animation_params` controls the cross-fade behavior:
// - `type`: `kCrossFade` animates both layers; `kCrossFadeAnimateNewLayerOnly`
//   animates only the new layer while an observer updates the old layer's
//   transform as `window`'s bounds and transform change (`kBounds` is invalid).
// - `duration`: uses an area-based duration if `std::nullopt`.
// - `tween_type`: tween type for the transition.
// - `histogram_name`: smoothness histogram to record (defaults to
//   `Ash.Window.AnimationSmoothness.CrossFade` when empty).
ASH_EXPORT void CrossFadeAnimation(
    aura::Window* window,
    std::unique_ptr<ui::LayerTreeOwner> old_layer_owner,
    const BoundsChangeAnimationParams& animation_params =
        kCrossFadeAnimationParams);

// Returns true if `window` is currently animating with a cross-fade animation.
ASH_EXPORT bool HasCrossFadeAnimation(aura::Window* window);

// Animates `window` from `old_origin` (in parent coordinates) to `window`'s
// current origin using a layer transform animation. `window` must not be
// currently cross-fading (`!HasCrossFadeAnimation(window)`). Uses `duration`
// (defaulting to `kDefaultDuration`), `tween_type`, and `histogram_name` (if
// non-empty) from `animation_params`.
ASH_EXPORT void PositionAnimation(
    aura::Window* window,
    const gfx::Point& old_origin,
    const BoundsChangeAnimationParams& animation_params = {});

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
