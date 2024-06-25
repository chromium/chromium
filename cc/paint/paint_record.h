// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_RECORD_H_
#define CC_PAINT_PAINT_RECORD_H_

#include "cc/paint/paint_export.h"
#include "cc/paint/paint_op_buffer.h"
#include "third_party/skia/include/core/SkPicture.h"

class SkMatrix;
struct SkRect;

namespace cc {

class ImageProvider;

// Contains recorded paint result. It's basically a read-only (except for
// assignment/movement) version of PaintOpBuffer returned from PaintOpBuffer::
// ReleaseAsRecord(). Copy/assignment and movement are cheap, and movement is
// preferred to copy/assignment when possible. On copy/assignment, the new
// PaintRecord shares the same underlying data with the source.
class CC_PAINT_EXPORT PaintRecord {
 public:
  PaintRecord();
  ~PaintRecord();
  // After move, the source is invalid (with nullptr buffer_) and can't be
  // used anymore.
  PaintRecord(PaintRecord&&);
  PaintRecord& operator=(PaintRecord&&);
  PaintRecord(const PaintRecord&);
  PaintRecord& operator=(const PaintRecord&);

  bool EqualsForTesting(const PaintRecord& other) const {
    return buffer_->EqualsForTesting(*other.buffer_);
  }

  const PaintOpBuffer& buffer() const {
    CHECK(buffer_);
    return *buffer_;
  }

  size_t size() const { return buffer_->size(); }
  bool empty() const { return buffer_->empty(); }

  size_t bytes_used() const { return buffer_->bytes_used(); }
  size_t paint_ops_size() const { return buffer_->paint_ops_size(); }
  size_t total_op_count() const { return buffer_->total_op_count(); }
  int num_slow_paths_up_to_min_for_MSAA() const {
    return buffer_->num_slow_paths_up_to_min_for_MSAA();
  }
  bool has_non_aa_paint() const { return buffer_->has_non_aa_paint(); }
  bool has_draw_ops() const { return buffer_->has_draw_ops(); }
  bool has_draw_text_ops() const { return buffer_->has_draw_text_ops(); }
  bool has_save_layer_ops() const { return buffer_->has_save_layer_ops(); }
  bool has_save_layer_alpha_ops() const {
    return buffer_->has_save_layer_alpha_ops();
  }
  bool has_effects_preventing_lcd_text_for_save_layer_alpha() const {
    return buffer_->has_effects_preventing_lcd_text_for_save_layer_alpha();
  }
  bool has_discardable_images() const {
    return buffer_->has_discardable_images();
  }
  gfx::ContentColorUsage content_color_usage() const {
    return buffer_->content_color_usage();
  }
  const PaintOp& GetFirstOp() const { return buffer_->GetFirstOp(); }

  // Given the |bounds| of a PaintOpBuffer that would be transformed by |ctm|
  // when rendered, compute the bounds needed to raster the buffer at a fixed
  // scale into an auxiliary image instead of rasterizing at scale dynamically.
  // This is used to enforce scaling decisions made pre-serialization when
  // rasterizing after deserializing the buffer.
  static SkRect GetFixedScaleBounds(const SkMatrix& ctm,
                                    const SkRect& bounds,
                                    int max_texture_size);

  sk_sp<SkPicture> ToSkPicture(
      const SkRect& bounds,
      ImageProvider* image_provider = nullptr,
      const PlaybackCallbacks& callbacks = PlaybackCallbacks()) const;

  // Replays the paint record into the canvas.
  void Playback(SkCanvas* canvas) const { buffer_->Playback(canvas); }
  void Playback(SkCanvas* canvas,
                const PlaybackParams& params,
                bool local_ctm = true) const {
    buffer_->Playback(canvas, params, local_ctm);
  }

  // STL-like container support:
  using value_type = PaintOp;
  using const_iterator = PaintOpBuffer::Iterator;
  const_iterator begin() const;
  const_iterator end() const;

 private:
  friend class PaintOpBuffer;
  explicit PaintRecord(sk_sp<PaintOpBuffer> buffer);

  // This is never nullptr.
  sk_sp<PaintOpBuffer> buffer_;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_RECORD_H_
