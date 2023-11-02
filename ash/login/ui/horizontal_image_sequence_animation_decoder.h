// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_HORIZONTAL_IMAGE_SEQUENCE_ANIMATION_DECODER_H_
#define ASH_LOGIN_UI_HORIZONTAL_IMAGE_SEQUENCE_ANIMATION_DECODER_H_

#include "ash/ash_export.h"
#include "ash/login/ui/animated_rounded_image_view.h"
#include "base/time/time.h"

namespace ash {

// Decodes an animation strip that is laid out 1xN (ie, the image grows in
// width, not height). There is no padding between frames in the animation
// strip.
//
// As an example, if the following ASCII art is 100 pixels wide, it has 4 frames
// each 25 pixels wide. The frames go from [0, 25), [25, 50), [50, 75), [75,
// 100). All frames have the same height of 25 pixels.
//
//    [1][2][3][4]
//
class ASH_EXPORT HorizontalImageSequenceAnimationDecoder
    : public AnimatedRoundedImageView::AnimationDecoder {
 public:
  HorizontalImageSequenceAnimationDecoder(const gfx::ImageSkia& image,
                                          base::TimeDelta duration,
                                          int num_frames);

  HorizontalImageSequenceAnimationDecoder(
      const HorizontalImageSequenceAnimationDecoder&) = delete;
  HorizontalImageSequenceAnimationDecoder& operator=(
      const HorizontalImageSequenceAnimationDecoder&) = delete;

  ~HorizontalImageSequenceAnimationDecoder() override;

  // AnimatedRoundedImageView::AnimationDecoder:
  AnimationFrames Decode(float image_scale) override;

 private:
  // The animation image source.
  gfx::ImageSkia image_;
  // The total duration of the animation.
  base::TimeDelta duration_;
  // The total number of frames in the animation.
  int num_frames_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_HORIZONTAL_IMAGE_SEQUENCE_ANIMATION_DECODER_H_
