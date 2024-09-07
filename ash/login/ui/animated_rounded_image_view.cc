// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/animated_rounded_image_view.h"

#include <limits>

#include "base/functional/bind.h"
#include "base/numerics/ranges.h"
#include "base/scoped_observation.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {
namespace {

// Decodes a single animation frame from image.
class SingleFrameImageDecoder
    : public AnimatedRoundedImageView::AnimationDecoder {
 public:
  explicit SingleFrameImageDecoder(const gfx::ImageSkia& image)
      : image_(image) {}

  SingleFrameImageDecoder(const SingleFrameImageDecoder&) = delete;
  SingleFrameImageDecoder& operator=(const SingleFrameImageDecoder&) = delete;

  ~SingleFrameImageDecoder() override = default;

  // AnimatedRoundedImageView::AnimationDecoder:
  AnimationFrames Decode(float image_scale) override {
    AnimationFrame frame;
    frame.image = image_;
    return {frame};
  }

 private:
  gfx::ImageSkia image_;
};

// Decodes a single animation frame from image model.
class SingleFrameImageModelDecoder
    : public AnimatedRoundedImageView::AnimationDecoder,
      public views::ViewObserver {
 public:
  explicit SingleFrameImageModelDecoder(const ui::ImageModel& image_model,
                                        AnimatedRoundedImageView* view)
      : image_model_(image_model), view_(view) {
    CHECK(view_);
    view_observer_.Observe(view);
  }

  SingleFrameImageModelDecoder(const SingleFrameImageModelDecoder&) = delete;
  SingleFrameImageModelDecoder& operator=(const SingleFrameImageModelDecoder&) =
      delete;

  ~SingleFrameImageModelDecoder() override { view_ = nullptr; }

  AnimationFrames Decode(float image_scale) override {
    const ui::ColorProvider* color_provider = view_->GetColorProvider();
    CHECK(color_provider);
    AnimationFrame frame;
    frame.image = image_model_.Rasterize(color_provider);
    return {frame};
  }

 private:
  void OnViewThemeChanged(views::View* observed_view) override {
    CHECK_EQ(observed_view, view_);
    view_->InvalidateFrames();
    view_->SchedulePaint();
  }

  ui::ImageModel image_model_;
  raw_ptr<AnimatedRoundedImageView> view_ = nullptr;
  base::ScopedObservation<views::View, ViewObserver> view_observer_{this};
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
  InvalidateFrames();
  SchedulePaint();
}

void AnimatedRoundedImageView::SetImage(const gfx::ImageSkia& image) {
  SetAnimationDecoder(std::make_unique<SingleFrameImageDecoder>(image),
                      Playback::kFirstFrameOnly);
}

void AnimatedRoundedImageView::SetImageModel(
    const ui::ImageModel& image_model) {
  auto decoder =
      std::make_unique<SingleFrameImageModelDecoder>(image_model, this);
  SetAnimationDecoder(std::move(decoder), Playback::kFirstFrameOnly);
}

void AnimatedRoundedImageView::SetAnimationPlayback(Playback playback) {
  playback_ = playback;
  StartOrStopAnimation();
}

gfx::Size AnimatedRoundedImageView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
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
  if (frames_.empty()) {
    return;
  }

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

void AnimatedRoundedImageView::InvalidateFrames() {
  frames_scale_ = NAN;
  frames_.clear();
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

  if (playback_ == Playback::kLastFrameOnly) {
    CHECK(!frames_.empty());
    active_frame_ = frames_.size() - 1;
    update_frame_timer_.Stop();
    SchedulePaint();
    return;
  }

  // Start animation.
  active_frame_ = -1;
  UpdateAnimationFrame();
}

void AnimatedRoundedImageView::UpdateAnimationFrame() {
  if (frames_.empty()) {
    // The frames_ was invalidated, awaiting the next OnPaint to regenerate the
    // frames_.
    return;
  }

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

BEGIN_METADATA(AnimatedRoundedImageView)
END_METADATA

}  // namespace ash
