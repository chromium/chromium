// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DISCARDABLE_IMAGE_MAP_H_
#define CC_PAINT_DISCARDABLE_IMAGE_MAP_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "cc/base/rtree.h"
#include "cc/paint/draw_image.h"
#include "cc/paint/image_animation_count.h"
#include "cc/paint/image_id.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_worklet_input.h"
#include "cc/paint/scroll_offset_map.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class PaintOpBuffer;

// This class is used for generating discardable images data (see DrawImage
// for the type of data it stores). It allows the client to query a particular
// rect and get back a list of DrawImages in that rect.
class CC_PAINT_EXPORT DiscardableImageMap
    : public base::RefCounted<DiscardableImageMap> {
 public:
  using Rects = absl::InlinedVector<gfx::Rect, 1>;

  struct CC_PAINT_EXPORT AnimatedImageMetadata {
    AnimatedImageMetadata(
        PaintImage::Id paint_image_id,
        PaintImage::CompletionState completion_state,
        std::vector<FrameMetadata> frames,
        int repetition_count,
        PaintImage::AnimationSequenceId reset_animation_sequence_id);
    AnimatedImageMetadata(const AnimatedImageMetadata& other);
    ~AnimatedImageMetadata();

    PaintImage::Id paint_image_id;
    PaintImage::CompletionState completion_state;
    std::vector<FrameMetadata> frames;
    int repetition_count;
    PaintImage::AnimationSequenceId reset_animation_sequence_id;
  };

  static scoped_refptr<DiscardableImageMap> Generate(
      const PaintOpBuffer& paint_op_buffer,
      const gfx::Rect& bounds,
      const ScrollOffsetMap& raster_inducing_scroll_offsets);

  bool empty() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return image_id_to_rects_.empty();
  }
  std::vector<const DrawImage*> GetDiscardableImagesInRect(
      const gfx::Rect& rect) const;
  const Rects& GetRectsForImage(PaintImage::Id image_id) const;
  const std::vector<AnimatedImageMetadata>& animated_images_metadata() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return animated_images_metadata_;
  }

  // This should only be called once from the compositor thread at commit time.
  base::flat_map<PaintImage::Id, PaintImage::DecodingMode>
  TakeDecodingModeMap();

  using PaintWorkletInputWithImageId =
      std::pair<scoped_refptr<PaintWorkletInput>, PaintImage::Id>;
  const std::vector<PaintWorkletInputWithImageId>& paint_worklet_inputs()
      const {
    return paint_worklet_inputs_;
  }

 private:
  friend class DiscardableImageMapTest;
  friend class base::RefCounted<DiscardableImageMap>;
  class Generator;

  DiscardableImageMap();
  ~DiscardableImageMap();

  base::flat_map<PaintImage::Id, Rects> image_id_to_rects_;
  std::vector<AnimatedImageMetadata> animated_images_metadata_;
  std::vector<std::pair<DrawImage, gfx::Rect>> images_;
  // This r-tree is built lazily. The entries are DrawImage pointers in images_.
  mutable std::unique_ptr<RTree<const DrawImage*>> images_rtree_;
  base::flat_map<PaintImage::Id, PaintImage::DecodingMode> decoding_mode_map_;
  std::vector<PaintWorkletInputWithImageId> paint_worklet_inputs_;

  // The class should be used from single thread only.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace cc

#endif  // CC_PAINT_DISCARDABLE_IMAGE_MAP_H_
