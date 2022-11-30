// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_ANIMATION_ATTRIBUTION_TRANSFORMER_H_
#define ASH_AMBIENT_UI_AMBIENT_ANIMATION_ATTRIBUTION_TRANSFORMER_H_

#include "ash/ash_export.h"

namespace views {
class AnimatedImageView;
}  // namespace views

namespace ash {

// "Attribution" refers to the text credits that may optionally accompany each
// photo that's assigned to a dynamic asset in an animation. The Lottie files
// for ambient mode have a placeholder for each dynamic asset where its
// attribution text should go.
//
// The attribution text box's coordinates must be baked into the Lottie file.
// However, UX requires that it is positioned such that the bottom-right of the
// text box has 24 pixels of padding from the bottom-right of the view.
// Additionally, the text box's width should extend from the left side of the
// view all the way to (width - 24) to account for long attributions.
// Visually, it looks like this:
//
// View:
// +-----------------------------------------------+
// |                                               |
// |                                               |
// |                                               |
// |                                               |
// |                                               |
// |                                               |
// |                                               |
// |                                               |
// |                                               |
// |-------------------------------------------+   |
// |                           Attribution Text|   |
// |-------------------------------------------+   |
// |                                               |
// +-----------------------------------------------+
//
// The animation already right-aligns the text within the box, but since the
// view's boundaries can vary from device to device, it is impossible to
// specify text box coordinates in the lottie file that work for all devices.
//
// To accomplish this, AmbientAnimationAttributionTransformer uses Skottie's
// text/transform property observer API to intercept and modify the text box's
// coordinates.
class ASH_EXPORT AmbientAnimationAttributionTransformer {
 public:
  // Modifies the lottie animation embedded within the |animated_image_view|
  // such that its attribution text box matches the UX requirements described
  // above.
  static void TransformTextBox(views::AnimatedImageView& animated_image_view);
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_ANIMATION_ATTRIBUTION_TRANSFORMER_H_
