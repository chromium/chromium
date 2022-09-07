// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_ANIMATION_RESIZER_H_
#define ASH_AMBIENT_UI_AMBIENT_ANIMATION_RESIZER_H_

#include "ash/ash_export.h"

namespace views {
class AnimatedImageView;
}  // namespace views

namespace ash {

// Each Skottie animation has fixed dimensions baked into its corresponding
// Lottie file that were picked by UX when the animation was designed. On the
// other hand, the UI view rendering the animation can have arbitrary
// dimensions. AmbientAnimationResizer modifies an AnimatedImageView such that
// its underlying Skottie animation fills its bounds according to the following
// UX requirements:
//
// Let's say the view is landscape with dimensions 1000x600 and the
// corresponding Lottie animation has dimensions 2000x1500. The animation
// must first be scaled down to match the UI's width. Then, the animation's
// height must be center-aligned and cropped:
// Width scale factor = 1000 / 2000 = 0.5
// New Animation width = 2000 * 0.5 = 1000
// New Animation height uncropped = 1500 * 0.5 = 750
// At this point, 750 > 600, so 150 pixels must be cropped from the height.
// Since the animation should be center-aligned vertically, 75 pixels will be
// cropped from the top and 75 pixels will be cropped from the bottom.
//
// UX has intentionally designed the ambient animations such that the above
// case is the most common. That is to say: the animation's dimensions are
// already larger than most views' expected dimensions, and the animation's
// width:height aspect ratio is intentionally smaller than most if not all of
// the expected UI aspect ratios (meaning the height gets cropped).
//
// Corner cases:
// 1) If the animation's new uncropped height is *less* than the the view's
//    height, keep the new height as is and vertically center-align the
//    animation within the view's bounds.
// 2) If the UI's width is greater than the animation's width, run the exact
//    same logic (scale the width, then crop the height). The only difference is
//    the animation will be scaled "up" instead of "down".
class ASH_EXPORT AmbientAnimationResizer {
 public:
  // Resizes the |animated_image_view| according to the UX requirements
  // described above. The |animated_image_view| must:
  // * Have initialized non-empty bounds.
  // * Have been initialized already with a lottie::Animation.
  //
  // |padding_for_jitter| specifies the additional padding in pixels that will
  // be applied to all 4 sides of the animation when it gets resized. For
  // landscape for example, the new animation width will end up being
  // |2 * padding_for_jitter| larger than the view's width. This allows the
  // animation to be translated in any direction by the padding amount while
  // still overlapping with the view's content bounds.
  static void Resize(views::AnimatedImageView& animated_image_view,
                     int padding_for_jitter = 0);
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_ANIMATION_RESIZER_H_
