// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_ANIMATION_FRAME_H_
#define ASH_LOGIN_UI_ANIMATION_FRAME_H_

#include <vector>

#include "base/time/time.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// A frame of an animation, which contains an image and the length that image
// should be display.
struct AnimationFrame {
  gfx::ImageSkia image;
  base::TimeDelta duration;
};

using AnimationFrames = std::vector<AnimationFrame>;

}  // namespace ash

#endif  // ASH_LOGIN_UI_ANIMATION_FRAME_H_