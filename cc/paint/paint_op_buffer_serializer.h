// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_OP_BUFFER_SERIALIZER_H_
#define CC_PAINT_PAINT_OP_BUFFER_SERIALIZER_H_

#include <concepts>
#include <memory>
#include <vector>

#include "base/memory/stack_allocated.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_op_buffer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/private/chromium/SkChromeRemoteGlyphCache.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {

class CC_PAINT_EXPORT PaintOpBufferSerializer {
  STACK_ALLOCATED();

 public:
  // As this code is performance sensitive, a raw function pointer is used.
  using SerializeCallback = size_t (*)(void*,
                                       const PaintOp&,
                                       const PaintOp::SerializeOptions&,
                                       const PaintFlags*,
                                       const SkM44&,
                                       const SkM44&);

  PaintOpBufferSerializer(SerializeCallback serialize_cb,
                          void* callback_data,
                          const PaintOp::SerializeOptions& options);
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
    gfx::Vector2dF post_scale = gfx::Vector2dF(1.f, 1.f);
    // If requires_clear is true, then this will raster will be cleared to
    // transparent.  If false, it assumes that the content will raster
    // opaquely up to content_size inset by 1 (with the last pixel being
    // potentially being partially transparent, if post scaled).
    bool requires_clear = true;
    // If clearing is needed, the color to clear to.
    SkColor4f background_color = SkColors::kTransparent;
  };
  // Serialize the buffer with a preamble. This function wraps the buffer in a
  // save/restore and includes any translations, scales, and clearing as
  // specified by the preamble.  This should generally be used for top level
  // rastering of an entire tile.
  void Serialize(const PaintOpBuffer& buffer,
                 const std::vector<size_t>* offsets,
                 const Preamble& preamble);
  // Serialize the buffer without a preamble. This function serializes the whole
  // buffer without any extra ops added.  No clearing is done.  This should
  // generally be used for internal PaintOpBuffers that want to be sent as-is.
  void Serialize(const PaintOpBuffer& buffer);
  // Serialize the buffer with a scale and a playback rect.  This should
  // generally be used for internal PaintOpBuffers in PaintShaders and
  // PaintFilters that need to guarantee the nested buffer is rasterized at the
  // specific scale to a separate image. This ensures that scale-dependent
  // analysis made during serialization is consistent with analysis done during
  // rasterization.
  void Serialize(const PaintOpBuffer& buffer,
                 const gfx::Rect& playback_rect,
                 const gfx::SizeF& post_scale);

  bool valid() const { return valid_; }

 private:
  PlaybackParams MakeParams(const SkCanvas* canvas) const;
  void SerializePreamble(SkCanvas* canvas,
                         const Preamble& preamble,
                         const PlaybackParams& params);
  // Serialize `buffer`, using the current transform as `original_ctm`.
  void SerializeBuffer(SkCanvas* canvas,
                       const PaintOpBuffer& buffer,
                       const std::vector<size_t>* offsets);
  // Serialize `buffer` using the provided `params`.
  void SerializeBufferWithParams(SkCanvas* canvas,
                                 const PlaybackParams& params,
                                 const PaintOpBuffer& buffer,
                                 const std::vector<size_t>* offsets);
  // Returns whether serialization of |op| succeeded and we need to serialize
  // the next PaintOp in the PaintOpBuffer.
  template<typename F>
  requires std::same_as<F, float>
  bool WillSerializeNextOp(const PaintOp& op,
                           SkCanvas* canvas,
                           const PlaybackParams& params,
                           F alpha);
  template<typename F>
  requires std::same_as<F, float>
  bool SerializeOpWithFlags(SkCanvas* canvas,
                            const PaintOpWithFlags& flags_op,
                            const PlaybackParams& params,
                            F alpha);

  ALWAYS_INLINE bool SerializeOp(SkCanvas* canvas,
                                 const PaintOp& op,
                                 const PaintFlags* flags_to_serialize,
                                 const PlaybackParams& params);
  void Save(SkCanvas* canvas, const PlaybackParams& params);
  void RestoreToCount(SkCanvas* canvas,
                      int count,
                      const PlaybackParams& params);
  void ClearForOpaqueRaster(SkCanvas* canvas,
                            const Preamble& preamble,
                            const PlaybackParams& params);
  void PlaybackOnAnalysisCanvas(SkCanvas* canvas,
                                const PaintOp& op,
                                const PaintFlags* flags_to_serialize,
                                const PlaybackParams& params);

  SerializeCallback serialize_cb_;
  void* callback_data_;
  PaintOp::SerializeOptions options_;

  size_t serialized_op_count_ = 0;
  bool valid_ = true;
};

// Serializes the ops in the memory available, fails on overflow.
class CC_PAINT_EXPORT SimpleBufferSerializer : public PaintOpBufferSerializer {
 public:
  SimpleBufferSerializer(void* memory,
                         size_t size,
                         const PaintOp::SerializeOptions& options);
  ~SimpleBufferSerializer() override;

  size_t written() const { return written_; }

 private:
  size_t SerializeToMemoryImpl(const PaintOp& op,
                               const PaintOp::SerializeOptions& options,
                               const PaintFlags* flags_to_serialize,
                               const SkM44& current_ctm,
                               const SkM44& original_ctm);

  static size_t SerializeToMemory(void* instance,
                                  const PaintOp& op,
                                  const PaintOp::SerializeOptions& options,
                                  const PaintFlags* flags_to_serialize,
                                  const SkM44& current_ctm,
                                  const SkM44& original_ctm) {
    return reinterpret_cast<SimpleBufferSerializer*>(instance)
        ->SerializeToMemoryImpl(op, options, flags_to_serialize, current_ctm,
                                original_ctm);
  }

  void* memory_;
  const size_t total_;
  size_t written_ = 0u;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_OP_BUFFER_SERIALIZER_H_
