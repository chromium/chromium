// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_IMAGE_ANIMATION_COUNT_H_
#define CC_PAINT_IMAGE_ANIMATION_COUNT_H_

namespace cc {

// GIF and WebP support animation. The explanation below is in terms of GIF,
// but the same constants are used for WebP, too.
// GIFs have an optional 16-bit unsigned loop count that describes how an
// animated GIF should be cycled.  If the loop count is absent, the animation
// cycles once; if it is 0, the animation cycles infinitely; otherwise the
// animation plays n + 1 cycles (where n is the specified loop count).  If the
// GIF decoder defaults to kAnimationLoopOnce in the absence of any loop count
// and translates an explicit "0" loop count to kAnimationLoopInfinite, then we
// get a couple of nice side effects:
//   * By making kAnimationLoopOnce be 0, we allow the animation cycling code in
//     BitmapImage.cpp to avoid special-casing it, and simply treat all
//     non-negative loop counts identically.
//   * By making the other three constants negative, we avoid conflicts with any
//     real loop count values.
//   * The Paused value is intended to freeze the frame of an image animation.
//   It starts image animation, but not update the frame index at the animation
//   controller.
const int kAnimationLoopOnce = 0;
const int kAnimationLoopInfinite = -1;
const int kAnimationNone = -2;
const int kAnimationPaused = -3;

}  // namespace cc

#endif  // CC_PAINT_IMAGE_ANIMATION_COUNT_H_
