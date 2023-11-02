// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/horizontal_image_sequence_animation_decoder.h"

#include "ui/gfx/image/image_skia_rep.h"

namespace ash {

HorizontalImageSequenceAnimationDecoder::
    HorizontalImageSequenceAnimationDecoder(const gfx::ImageSkia& image,
                                            base::TimeDelta duration,
                                            int num_frames)
    : image_(image), duration_(duration), num_frames_(num_frames) {}

HorizontalImageSequenceAnimationDecoder::
    ~HorizontalImageSequenceAnimationDecoder() = default;

AnimationFrames HorizontalImageSequenceAnimationDecoder::Decode(
    float image_scale) {
  SkBitmap bitmap = image_.GetRepresentation(image_scale).GetBitmap();

  float frame_width = static_cast<float>(bitmap.width()) / num_frames_;
  base::TimeDelta frame_duration = duration_ / num_frames_;

  AnimationFrames animation;
  animation.reserve(num_frames_);
  for (int i = 0; i < num_frames_; ++i) {
    // Get the subsection of the animation strip.
    SkBitmap frame_bitmap;
    bitmap.extractSubset(
        &frame_bitmap,
        SkIRect::MakeXYWH(std::round(i * frame_width), 0,
                          std::round(frame_width), bitmap.height()));

    // Add an animation frame.
    AnimationFrame frame;
    frame.duration = frame_duration;
    frame.image = gfx::ImageSkia::CreateFrom1xBitmap(frame_bitmap);
    animation.push_back(frame);
  }

  return animation;
}

}  // namespace ash
