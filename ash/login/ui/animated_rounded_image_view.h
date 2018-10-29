// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_ANIMATED_ROUNDED_IMAGE_VIEW_H_
#define ASH_LOGIN_UI_ANIMATED_ROUNDED_IMAGE_VIEW_H_

#include <cmath>
#include <vector>

#include "ash/login/ui/animation_frame.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace ash {

// A custom image view with rounded edges.
class AnimatedRoundedImageView : public views::View {
 public:
  enum class Playback {
    kFirstFrameOnly,  // Only the first frame in the animation will be shown.
    kSingle,          // Play the animation only once.
    kRepeat,          // Play the animation repeatedly.
  };

  // Provides animation frames.
  class AnimationDecoder {
   public:
    virtual ~AnimationDecoder();
    virtual AnimationFrames Decode(float image_scale) = 0;
  };

  // Constructs a new rounded image view with rounded corners of radius
  // |corner_radius|.
  AnimatedRoundedImageView(const gfx::Size& size, int corner_radius);
  ~AnimatedRoundedImageView() override;

  // Show an animation with specified playback mode.
  void SetAnimationDecoder(std::unique_ptr<AnimationDecoder> decoder,
                           Playback playback);

  // Show a static image.
  void SetImage(const gfx::ImageSkia& image);

  // Set playback type of the animation.
  void SetAnimationPlayback(Playback playback);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  void StartOrStopAnimation();
  void UpdateAnimationFrame();
  void BuildAnimationFrames(float image_scale);

  // Currently displayed animation frame.
  int active_frame_ = 0;

  // Used to fetch animation frames for a given scale.
  std::unique_ptr<AnimationDecoder> decoder_;
  // The scale that |frames_| is using.
  float frames_scale_ = NAN;
  // The raw decoded animation frames.
  AnimationFrames frames_;
  // Animation playback type.
  Playback playback_ = Playback::kFirstFrameOnly;

  const gfx::Size image_size_;
  const int corner_radius_;

  base::OneShotTimer update_frame_timer_;

  DISALLOW_COPY_AND_ASSIGN(AnimatedRoundedImageView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_ANIMATED_ROUNDED_IMAGE_VIEW_H_
