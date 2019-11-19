// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_IMAGE_BUILDER_H_
#define CC_PAINT_PAINT_IMAGE_BUILDER_H_

#include "cc/paint/paint_export.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_generator.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_worklet_input.h"
#include "cc/paint/skia_paint_image_generator.h"
#include "third_party/skia/include/core/SkImage.h"

namespace cc {

// Class used to construct a paint image.
class CC_PAINT_EXPORT PaintImageBuilder {
 public:
  static PaintImageBuilder WithDefault();

  // Starts with the given images. Everything, including the "contents" of the
  // image are copied.
  static PaintImageBuilder WithCopy(PaintImage image);

  // Starts with the given image's flags. Note that this does _not_ keep the
  // "contents" of the image. That is, it clears the cached SkImage, the set
  // SkImage, the set PaintRecord, and any other content type variables.
  static PaintImageBuilder WithProperties(PaintImage image);

  PaintImageBuilder(PaintImageBuilder&& other);
  ~PaintImageBuilder();

  PaintImageBuilder&& set_id(PaintImage::Id id) {
    paint_image_.id_ = id;
#if DCHECK_IS_ON()
    id_set_ = true;
#endif
    return std::move(*this);
  }

  PaintImageBuilder&& set_image(sk_sp<SkImage> sk_image,
                                PaintImage::ContentId content_id) {
    paint_image_.sk_image_ = std::move(sk_image);
    paint_image_.content_id_ = content_id;
    return std::move(*this);
  }
  PaintImageBuilder&& set_paint_record(sk_sp<PaintRecord> paint_record,
                                       const gfx::Rect& rect,
                                       PaintImage::ContentId content_id) {
    DCHECK_NE(content_id, PaintImage::kInvalidContentId);

    paint_image_.paint_record_ = std::move(paint_record);
    paint_image_.paint_record_rect_ = rect;
    paint_image_.content_id_ = content_id;
    return std::move(*this);
  }
  PaintImageBuilder&& set_paint_image_generator(
      sk_sp<PaintImageGenerator> generator) {
    paint_image_.paint_image_generator_ = std::move(generator);
    return std::move(*this);
  }

  PaintImageBuilder&& set_animation_type(PaintImage::AnimationType type) {
    paint_image_.animation_type_ = type;
    return std::move(*this);
  }
  PaintImageBuilder&& set_completion_state(PaintImage::CompletionState state) {
    paint_image_.completion_state_ = state;
    return std::move(*this);
  }
  PaintImageBuilder&& set_is_multipart(bool is_multipart) {
    paint_image_.is_multipart_ = is_multipart;
    return std::move(*this);
  }
  PaintImageBuilder&& set_is_high_bit_depth(bool is_high_bit_depth) {
    paint_image_.is_high_bit_depth_ = is_high_bit_depth;
    return std::move(*this);
  }
  PaintImageBuilder&& set_repetition_count(int count) {
    paint_image_.repetition_count_ = count;
    return std::move(*this);
  }
  PaintImageBuilder&& set_reset_animation_sequence_id(
      PaintImage::AnimationSequenceId id) {
    paint_image_.reset_animation_sequence_id_ = id;
    return std::move(*this);
  }

  // Makes the PaintImage represent a subset of the original image. The
  // subset must be non-empty and lie within the image bounds.
  PaintImageBuilder&& make_subset(const gfx::Rect& subset) {
    paint_image_ = paint_image_.MakeSubset(subset);
    return std::move(*this);
  }
  PaintImageBuilder&& set_decoding_mode(
      PaintImage::DecodingMode decoding_mode) {
    paint_image_.decoding_mode_ = decoding_mode;
    return std::move(*this);
  }
  PaintImageBuilder&& set_paint_worklet_input(
      scoped_refptr<PaintWorkletInput> input) {
    paint_image_.paint_worklet_input_ = std::move(input);
    return std::move(*this);
  }

  PaintImage TakePaintImage();

 private:
  PaintImageBuilder();
  PaintImageBuilder(PaintImage starting_image, bool clear_contents);

  PaintImage paint_image_;
#if DCHECK_IS_ON()
  bool id_set_ = false;
#endif
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_IMAGE_BUILDER_H_
