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
#include "cc/paint/paint_shader.h"
#include "cc/paint/paint_worklet_input.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class DiscardableImageStore;
class PaintOpBuffer;

// This class is used for generating discardable images data (see DrawImage
// for the type of data it stores). It allows the client to query a particular
// rect and get back a list of DrawImages in that rect.
class CC_PAINT_EXPORT DiscardableImageMap {
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

  DiscardableImageMap();
  ~DiscardableImageMap();

  bool empty() const { return image_id_to_rects_.empty(); }
  void GetDiscardableImagesInRect(const gfx::Rect& rect,
                                  std::vector<const DrawImage*>* images) const;
  const Rects& GetRectsForImage(PaintImage::Id image_id) const;
  gfx::ContentColorUsage content_color_usage() const {
    return content_color_usage_;
  }
  bool contains_hbd_images() const { return contains_hbd_images_; }
  const std::vector<AnimatedImageMetadata>& animated_images_metadata() const {
    return animated_images_metadata_;
  }

  void Reset();
  void Generate(const PaintOpBuffer& paint_op_buffer, const gfx::Rect& bounds);

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
  friend class ScopedMetadataGenerator;
  friend class DiscardableImageMapTest;

  std::unique_ptr<DiscardableImageStore> BeginGeneratingMetadata(
      const gfx::Size& bounds);
  void EndGeneratingMetadata(
      std::vector<std::pair<DrawImage, gfx::Rect>> images,
      base::flat_map<PaintImage::Id, gfx::Rect> image_id_to_rect);

  base::flat_map<PaintImage::Id, Rects> image_id_to_rects_;
  std::vector<AnimatedImageMetadata> animated_images_metadata_;
  std::vector<std::pair<DrawImage, gfx::Rect>> images_;
  // This r-tree is built lazily and in practice is only constructed
  // on the impl thread. The entries are DrawImage pointers in `images_`.
  mutable std::unique_ptr<RTree<const DrawImage*>> images_rtree_
      GUARDED_BY_CONTEXT(images_rtree_sequence_checker_);
  base::flat_map<PaintImage::Id, PaintImage::DecodingMode> decoding_mode_map_;
  gfx::ContentColorUsage content_color_usage_ = gfx::ContentColorUsage::kSRGB;
  bool contains_hbd_images_ = false;

  SEQUENCE_CHECKER(images_rtree_sequence_checker_);

  std::vector<PaintWorkletInputWithImageId> paint_worklet_inputs_;
};

}  // namespace cc

#endif  // CC_PAINT_DISCARDABLE_IMAGE_MAP_H_
