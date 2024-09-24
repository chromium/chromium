// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_image_builder.h"

namespace cc {

// static
PaintImageBuilder PaintImageBuilder::WithDefault() {
  return PaintImageBuilder();
}

// static
PaintImageBuilder PaintImageBuilder::WithCopy(PaintImage paint_image) {
  return PaintImageBuilder(std::move(paint_image), false);
}

// static
PaintImageBuilder PaintImageBuilder::WithProperties(PaintImage paint_image) {
  return PaintImageBuilder(std::move(paint_image), true);
}

PaintImageBuilder::PaintImageBuilder() = default;
PaintImageBuilder::PaintImageBuilder(PaintImage image, bool clear_contents)
    : paint_image_(std::move(image)) {
#if DCHECK_IS_ON()
  id_set_ = true;
#endif
  if (clear_contents) {
    paint_image_.sk_image_ = nullptr;
    paint_image_.paint_record_ = std::nullopt;
    paint_image_.paint_record_rect_ = gfx::Rect();
    paint_image_.paint_image_generator_ = nullptr;
    paint_image_.cached_sk_image_ = nullptr;
    paint_image_.texture_backing_ = nullptr;
  }
}
PaintImageBuilder::PaintImageBuilder(PaintImageBuilder&& other) = default;
PaintImageBuilder& PaintImageBuilder::operator=(PaintImageBuilder&& other) =
    default;
PaintImageBuilder::~PaintImageBuilder() = default;

PaintImage PaintImageBuilder::TakePaintImage() {
#if DCHECK_IS_ON()
  DCHECK(id_set_);
  if (paint_image_.sk_image_) {
    DCHECK(!paint_image_.paint_record_);
    DCHECK(!paint_image_.paint_image_generator_);
    DCHECK(!paint_image_.sk_image_->isLazyGenerated());
    DCHECK(!paint_image_.deferred_paint_record_);
    DCHECK(!paint_image_.gainmap_paint_image_generator_);
    if (paint_image_.gainmap_sk_image_) {
      DCHECK(!paint_image_.gainmap_sk_image_->isLazyGenerated());
    }
    // TODO(khushalsagar): Assert that we don't have an animated image type
    // here. The only case where this is possible is DragImage. There are 2 use
    // cases going through that path, re-orienting the image and for use by the
    // DragController. The first should never be triggered for an animated
    // image (orientation changes can only be specified by JPEGs, none of the
    // animation image types use it). For the latter the image is required to be
    // decoded and used in blink, and should only need the default frame.
  } else if (paint_image_.paint_record_) {
    DCHECK(!paint_image_.sk_image_);
    DCHECK(!paint_image_.paint_image_generator_);
    DCHECK(!paint_image_.deferred_paint_record_);
    DCHECK(!paint_image_.gainmap_paint_image_generator_);
    DCHECK(!paint_image_.gainmap_sk_image_);
    // TODO(khushalsagar): Assert that we don't have an animated image type
    // here.
  } else if (paint_image_.paint_image_generator_) {
    DCHECK(!paint_image_.sk_image_);
    DCHECK(!paint_image_.paint_record_);
    DCHECK(!paint_image_.deferred_paint_record_);
    DCHECK(!paint_image_.gainmap_sk_image_);
  } else if (paint_image_.deferred_paint_record_) {
    DCHECK(!paint_image_.sk_image_);
    DCHECK(!paint_image_.paint_record_);
    DCHECK(!paint_image_.paint_image_generator_);
    DCHECK(!paint_image_.gainmap_sk_image_);
    DCHECK(!paint_image_.gainmap_paint_image_generator_);
  }

  if (paint_image_.HasGainmap()) {
    DCHECK(paint_image_.paint_image_generator_ ||
           paint_image_.gainmap_sk_image_);
  }

  if (paint_image_.ShouldAnimate()) {
    DCHECK(paint_image_.paint_image_generator_)
        << "Animated images must provide a generator";
    for (const auto& frame : paint_image_.GetFrameMetadata())
      DCHECK_GT(frame.duration, base::TimeDelta());
  }
#endif
  if (paint_image_.reinterpret_as_srgb_) {
    if (paint_image_.sk_image_) {
      paint_image_.sk_image_ = paint_image_.sk_image_->reinterpretColorSpace(
          SkColorSpace::MakeSRGB());
    }
    if (paint_image_.cached_sk_image_) {
      paint_image_.cached_sk_image_ =
          paint_image_.cached_sk_image_->reinterpretColorSpace(
              SkColorSpace::MakeSRGB());
    }
  }

  // We may already have a cached_sk_image_ if this builder was created with a
  // copy.
  if (!paint_image_.cached_sk_image_)
    paint_image_.CreateSkImage();
  return std::move(paint_image_);
}

}  // namespace cc
