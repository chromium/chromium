// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_IMAGE_BUILDER_H_
#define CC_PAINT_PAINT_IMAGE_BUILDER_H_

#include <utility>

#include "cc/paint/deferred_paint_record.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_generator.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_worklet_input.h"
#include "cc/paint/skia_paint_image_generator.h"
#include "cc/paint/texture_backing.h"
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
  PaintImageBuilder& operator=(PaintImageBuilder&& other);
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
    DCHECK(!sk_image->isTextureBacked());
    paint_image_.sk_image_ = std::move(sk_image);
    paint_image_.content_id_ = content_id;
    return std::move(*this);
  }
  PaintImageBuilder&& set_texture_backing(sk_sp<TextureBacking> texture_backing,
                                          PaintImage::ContentId content_id) {
    paint_image_.texture_backing_ = std::move(texture_backing);
    paint_image_.content_id_ = content_id;
    return std::move(*this);
  }
  PaintImageBuilder&& set_paint_record(PaintRecord paint_record,
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
  PaintImageBuilder&& set_gainmap_paint_image_generator(
      sk_sp<PaintImageGenerator> generator,
      const SkGainmapInfo& gainmap_info) {
    // Setting SkGainmapInfo with no gainmap image is an error.
    DCHECK(generator);
    paint_image_.gainmap_paint_image_generator_ = std::move(generator);
    paint_image_.gainmap_info_ = gainmap_info;
    return std::move(*this);
  }
  PaintImageBuilder&& set_hdr_metadata(
      std::optional<gfx::HDRMetadata> hdr_metadata) {
    paint_image_.hdr_metadata_ = hdr_metadata;
    return std::move(*this);
  }
  PaintImageBuilder&& set_reinterpret_as_srgb(bool reinterpret_as_srgb) {
    paint_image_.reinterpret_as_srgb_ = reinterpret_as_srgb;
    return std::move(*this);
  }
  PaintImageBuilder&& set_target_hdr_headroom(float target_hdr_headroom) {
    paint_image_.target_hdr_headroom_ = target_hdr_headroom;
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
  PaintImageBuilder&& set_may_be_lcp_candidate(bool may_be_lcp_candidate) {
    paint_image_.may_be_lcp_candidate_ = may_be_lcp_candidate;
    return std::move(*this);
  }
  PaintImageBuilder&& set_no_cache(bool no_cache) {
    paint_image_.no_cache_ = no_cache;
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

  PaintImageBuilder&& set_decoding_mode(
      PaintImage::DecodingMode decoding_mode) {
    paint_image_.decoding_mode_ = decoding_mode;
    return std::move(*this);
  }

  PaintImageBuilder&& set_deferred_paint_record(
      scoped_refptr<DeferredPaintRecord> input) {
    paint_image_.deferred_paint_record_ = std::move(input);
    return std::move(*this);
  }
  PaintImage TakePaintImage();

 private:
  friend class PaintOpReader;
  friend class PaintShader;
  friend class ImagePaintFilter;
  friend PaintImage CreateNonDiscardablePaintImage(const gfx::Size& size);

  PaintImageBuilder();
  PaintImageBuilder(PaintImage starting_image, bool clear_contents);

  // For GPU process callers using a texture backed SkImage.
  PaintImageBuilder&& set_texture_image(sk_sp<SkImage> sk_image,
                                        PaintImage::ContentId content_id) {
    paint_image_.sk_image_ = std::move(sk_image);
    paint_image_.content_id_ = content_id;
    return std::move(*this);
  }
  PaintImageBuilder&& set_gainmap_texture_image(
      sk_sp<SkImage> gainmap_sk_image,
      const SkGainmapInfo& gainmap_info) {
    paint_image_.gainmap_sk_image_ = std::move(gainmap_sk_image);
    paint_image_.gainmap_info_ = gainmap_info;
    return std::move(*this);
  }

  PaintImage paint_image_;
#if DCHECK_IS_ON()
  bool id_set_ = false;
#endif
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_IMAGE_BUILDER_H_
