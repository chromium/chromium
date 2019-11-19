// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_OP_BUFFER_SERIALIZER_H_
#define CC_PAINT_PAINT_OP_BUFFER_SERIALIZER_H_

#include "cc/paint/paint_op_buffer.h"

#include "third_party/skia/src/core/SkRemoteGlyphCache.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {
class ClientPaintCache;
class TransferCacheSerializeHelper;

class CC_PAINT_EXPORT PaintOpBufferSerializer {
 public:
  using SerializeCallback =
      base::RepeatingCallback<size_t(const PaintOp*,
                                     const PaintOp::SerializeOptions&)>;

  PaintOpBufferSerializer(SerializeCallback serialize_cb,
                          ImageProvider* image_provider,
                          TransferCacheSerializeHelper* transfer_cache,
                          ClientPaintCache* paint_cache,
                          SkStrikeServer* strike_server,
                          sk_sp<SkColorSpace> color_space,
                          bool can_use_lcd_text,
                          bool context_supports_distance_field_text,
                          int max_texture_size,
                          size_t max_texture_bytes);
  virtual ~PaintOpBufferSerializer();

  struct Preamble {
    // The full size of the content, to know whether edge texel clearing
    // is required or not.  The full_raster_rect and playback_rect must
    // be contained in this size.
    gfx::Size content_size;
    // Rect in content space (1 unit = 1 pixel) of this tile.
    gfx::Rect full_raster_rect;
    // A subrect in content space (full_raster_rect must contain this) of
    // the partial content to play back.
    gfx::Rect playback_rect;
    // The translation and scale to do after
    gfx::Vector2dF post_translation;
    gfx::SizeF post_scale = gfx::SizeF(1.f, 1.f);
    // If requires_clear is true, then this will raster will be cleared to
    // transparent.  If false, it assumes that the content will raster
    // opaquely up to content_size inset by 1 (with the last pixel being
    // potentially being partially transparent, if post scaled).
    bool requires_clear = true;
    // If clearing is needed, the color to clear to.
    SkColor background_color = SK_ColorTRANSPARENT;
  };
  // Serialize the buffer with a preamble. This function wraps the buffer in a
  // save/restore and includes any translations, scales, and clearing as
  // specified by the preamble.  This should generally be used for top level
  // rastering of an entire tile.
  void Serialize(const PaintOpBuffer* buffer,
                 const std::vector<size_t>* offsets,
                 const Preamble& preamble);
  // Serialize the buffer without a preamble. This function serializes the whole
  // buffer without any extra ops added.  No clearing is done.  This should
  // generally be used for internal PaintOpBuffers that want to be sent as-is.
  void Serialize(const PaintOpBuffer* buffer);
  // Serialize the buffer with a scale and a playback rect.  This should
  // generally be used for internal PaintOpBuffers in PaintShaders that have
  // a scale and a tiling, but don't want the clearing or other complicated
  // logic of the top level Serialize.
  // post_matrix_for_analysis adds a scale that is not added to the serialized
  // buffer, but used in analysis. This is required for cases that don't modify
  // the record during serialization, but need to send resources based on the
  // raster scale (mainly PaintRecord backed PaintFilters).
  void Serialize(const PaintOpBuffer* buffer,
                 const gfx::Rect& playback_rect,
                 const gfx::SizeF& post_scale,
                 const SkMatrix& post_matrix_for_analysis);

  bool valid() const { return valid_; }

 private:
  void SerializePreamble(const Preamble& preamble,
                         const PaintOp::SerializeOptions& options,
                         const PlaybackParams& params);
  void SerializeBuffer(const PaintOpBuffer* buffer,
                       const std::vector<size_t>* offsets);
  bool SerializeOpWithFlags(const PaintOpWithFlags* flags_op,
                            PaintOp::SerializeOptions* options,
                            const PlaybackParams& params,
                            uint8_t alpha);
  bool SerializeOp(const PaintOp* op,
                   const PaintOp::SerializeOptions& options,
                   const PlaybackParams& params);
  void Save(const PaintOp::SerializeOptions& options,
            const PlaybackParams& params);
  void RestoreToCount(int count,
                      const PaintOp::SerializeOptions& options,
                      const PlaybackParams& params);
  PaintOp::SerializeOptions MakeSerializeOptions();
  void ClearForOpaqueRaster(const Preamble& preamble,
                            const PaintOp::SerializeOptions& options,
                            const PlaybackParams& params);
  void PlaybackOnAnalysisCanvas(const PaintOp* op,
                                const PaintOp::SerializeOptions& options,
                                const PlaybackParams& params);

  SerializeCallback serialize_cb_;
  ImageProvider* image_provider_;
  TransferCacheSerializeHelper* transfer_cache_;
  ClientPaintCache* paint_cache_;
  SkStrikeServer* strike_server_;
  sk_sp<SkColorSpace> color_space_;
  bool can_use_lcd_text_;
  bool context_supports_distance_field_text_;
  int max_texture_size_;
  size_t max_texture_bytes_;

  SkTextBlobCacheDiffCanvas text_blob_canvas_;
  bool valid_ = true;
};

// Serializes the ops in the memory available, fails on overflow.
class CC_PAINT_EXPORT SimpleBufferSerializer : public PaintOpBufferSerializer {
 public:
  SimpleBufferSerializer(void* memory,
                         size_t size,
                         ImageProvider* image_provider,
                         TransferCacheSerializeHelper* transfer_cache,
                         ClientPaintCache* paint_cache,
                         SkStrikeServer* strike_server,
                         sk_sp<SkColorSpace> color_space,
                         bool can_use_lcd_text,
                         bool context_supports_distance_field_text,
                         int max_texture_size,
                         size_t max_texture_bytes);
  ~SimpleBufferSerializer() override;

  size_t written() const { return written_; }

 private:
  size_t SerializeToMemory(const PaintOp* op,
                           const PaintOp::SerializeOptions& options);

  void* memory_;
  const size_t total_;
  size_t written_ = 0u;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_OP_BUFFER_SERIALIZER_H_
