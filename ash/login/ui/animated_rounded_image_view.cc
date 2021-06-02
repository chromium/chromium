// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/animated_rounded_image_view.h"

#include <limits>

#include "base/bind.h"
#include "base/numerics/ranges.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"

namespace ash {
namespace {

// Decodes a single animation frame.
class SingleFrameImageDecoder
    : public AnimatedRoundedImageView::AnimationDecoder {
 public:
  SingleFrameImageDecoder(const gfx::ImageSkia& image) : image_(image) {}
  ~SingleFrameImageDecoder() override = default;

  // AnimatedRoundedImageView::AnimationDecoder:
  AnimationFrames Decode(float image_scale) override {
    AnimationFrame frame;
    frame.image = image_;
    return {frame};
  }

 private:
  gfx::ImageSkia image_;

  DISALLOW_COPY_AND_ASSIGN(SingleFrameImageDecoder);
};

}  // namespace

AnimatedRoundedImageView::AnimationDecoder::~AnimationDecoder() = default;

AnimatedRoundedImageView::AnimatedRoundedImageView(const gfx::Size& size,
                                                   int corner_radius)
    : image_size_(size), corner_radius_(corner_radius) {}

AnimatedRoundedImageView::~AnimatedRoundedImageView() = default;

void AnimatedRoundedImageView::SetAnimationDecoder(
    std::unique_ptr<AnimationDecoder> decoder,
    Playback playback) {
  decoder_ = std::move(decoder);
  playback_ = playback;
  // Force a new decode and repaint.
  frames_scale_ = NAN;
  SchedulePaint();
}

void AnimatedRoundedImageView::SetImage(const gfx::ImageSkia& image) {
  SetAnimationDecoder(std::make_unique<SingleFrameImageDecoder>(image),
                      Playback::kFirstFrameOnly);
}

void AnimatedRoundedImageView::SetAnimationPlayback(Playback playback) {
  playback_ = playback;
  StartOrStopAnimation();
}

gfx::Size AnimatedRoundedImageView::CalculatePreferredSize() const {
  return gfx::Size(image_size_.width() + GetInsets().width(),
                   image_size_.height() + GetInsets().height());
}

void AnimatedRoundedImageView::OnPaint(gfx::Canvas* canvas) {
  // Rebuild animation frames if the scaling has changed.
  if (decoder_ &&
      !base::IsApproximatelyEqual(canvas->image_scale(), frames_scale_,
                                  std::numeric_limits<float>::epsilon())) {
    BuildAnimationFrames(canvas->image_scale());
    StartOrStopAnimation();
  }

  // Nothing to render.
  if (frames_.empty())
    return;

  View::OnPaint(canvas);
  gfx::Rect image_bounds(GetContentsBounds());
  image_bounds.ClampToCenteredSize(GetPreferredSize());
  const SkScalar kRadius[8] = {
      SkIntToScalar(corner_radius_), SkIntToScalar(corner_radius_),
      SkIntToScalar(corner_radius_), SkIntToScalar(corner_radius_),
      SkIntToScalar(corner_radius_), SkIntToScalar(corner_radius_),
      SkIntToScalar(corner_radius_), SkIntToScalar(corner_radius_)};
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(image_bounds), kRadius);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  canvas->DrawImageInPath(frames_[active_frame_].image, image_bounds.x(),
                          image_bounds.y(), path, flags);
}

void AnimatedRoundedImageView::StartOrStopAnimation() {
  // If animation is disabled or if there are less than 2 frames, show a static
  // image.
  if (playback_ == Playback::kFirstFrameOnly || frames_.size() < 2) {
    active_frame_ = 0;
    update_frame_timer_.Stop();
    SchedulePaint();
    return;
  }

  // Start animation.
  active_frame_ = -1;
  UpdateAnimationFrame();
}

void AnimatedRoundedImageView::UpdateAnimationFrame() {
  DCHECK(!frames_.empty());

  // Note: |active_frame_| may be invalid.
  active_frame_ = (active_frame_ + 1) % frames_.size();
  SchedulePaint();

  if (static_cast<size_t>(active_frame_ + 1) < frames_.size() ||
      playback_ == Playback::kRepeat) {
    // Schedule next frame update.
    update_frame_timer_.Start(
        FROM_HERE, frames_[active_frame_].duration,
        base::BindOnce(&AnimatedRoundedImageView::UpdateAnimationFrame,
                       base::Unretained(this)));
  }
}

void AnimatedRoundedImageView::BuildAnimationFrames(float image_scale) {
  frames_scale_ = image_scale;
  AnimationFrames frames = decoder_->Decode(frames_scale_);
  frames_.clear();
  frames_.reserve(frames.size());
  for (AnimationFrame frame : frames) {
    frame.image = gfx::ImageSkiaOperations::CreateResizedImage(
        frame.image, skia::ImageOperations::RESIZE_BEST, image_size_);
    DCHECK(frame.image.bitmap()->isImmutable());
    frames_.emplace_back(frame);
  }
}

}  // namespace ash
