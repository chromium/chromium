// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_ANIMATED_ROUNDED_IMAGE_VIEW_H_
#define ASH_LOGIN_UI_ANIMATED_ROUNDED_IMAGE_VIEW_H_

#include <cmath>
#include <vector>

#include "ash/ash_export.h"
#include "ash/login/ui/animation_frame.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace ash {

// A custom image view with rounded edges.
class ASH_EXPORT AnimatedRoundedImageView : public views::View {
  METADATA_HEADER(AnimatedRoundedImageView, views::View)

 public:
  enum class Playback {
    kFirstFrameOnly,  // Only the first frame in the animation will be shown.
    kLastFrameOnly,   // Only the last frame in the animation will be shown.
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

  AnimatedRoundedImageView(const AnimatedRoundedImageView&) = delete;
  AnimatedRoundedImageView& operator=(const AnimatedRoundedImageView&) = delete;

  ~AnimatedRoundedImageView() override;

  // Show an animation with specified playback mode.
  void SetAnimationDecoder(std::unique_ptr<AnimationDecoder> decoder,
                           Playback playback);

  // Show a static image.
  void SetImage(const gfx::ImageSkia& image);

  // Show a static image from image model.
  void SetImageModel(const ui::ImageModel& image_model);

  // Set playback type of the animation.
  void SetAnimationPlayback(Playback playback);

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnPaint(gfx::Canvas* canvas) override;

  // Invalidate frames. The next OnPaint will rebuild the frames.
  void InvalidateFrames();

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
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_ANIMATED_ROUNDED_IMAGE_VIEW_H_
