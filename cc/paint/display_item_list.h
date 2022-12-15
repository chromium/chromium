// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DISPLAY_ITEM_LIST_H_
#define CC_PAINT_DISPLAY_ITEM_LIST_H_

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "cc/base/rtree.h"
#include "cc/paint/discardable_image_map.h"
#include "cc/paint/image_id.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_op_buffer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

class SkCanvas;

namespace gpu::raster {
class RasterImplementation;
class RasterImplementationGLES;
}  // namespace gpu::raster

namespace base::trace_event {
class TracedValue;
}  // namespace base::trace_event

namespace cc {

// DisplayItemList is a container of paint operations. One can populate the list
// using StartPaint, followed by push{,_with_data,_with_array} functions
// specialized with ops coming from paint_op_buffer.h. Internally, the
// DisplayItemList contains a PaintOpBuffer and defers op saving to it.
// Additionally, it store some meta information about the paint operations.
// Specifically, it creates an rtree to assist in rasterization: when
// rasterizing a rect, it queries the rtree to extract only the byte offsets of
// the ops required and replays those into a canvas.
class CC_PAINT_EXPORT DisplayItemList
    : public base::RefCountedThreadSafe<DisplayItemList> {
 public:
  DisplayItemList();
  DisplayItemList(const DisplayItemList&) = delete;
  DisplayItemList& operator=(const DisplayItemList&) = delete;

  void Raster(SkCanvas* canvas, ImageProvider* image_provider = nullptr) const;

  // Captures |DrawTextBlobOp|s intersecting |rect| and returns the associated
  // |NodeId|s in |content|.
  void CaptureContent(const gfx::Rect& rect,
                      std::vector<NodeInfo>* content) const;

  // Returns the approximate total area covered by |DrawTextBlobOp|s
  // intersecting |rect|, used for statistics purpose.
  double AreaOfDrawText(const gfx::Rect& rect) const;

  void StartPaint() {
#if DCHECK_IS_ON()
    DCHECK(!IsPainting());
    current_range_start_ = paint_op_buffer_.size();
#endif
  }

  // Push functions construct a new op on the paint op buffer, while maintaining
  // bookkeeping information. Must be called after invoking StartPaint().
  // Returns the id (which is an opaque value) of the operation that can be used
  // in UpdateSaveLayerBounds().
  template <typename T, typename... Args>
  size_t push(Args&&... args) {
#if DCHECK_IS_ON()
    DCHECK(IsPainting());
#endif
    size_t offset = paint_op_buffer_.next_op_offset();
    offsets_.push_back(offset);
    const T& op = paint_op_buffer_.push<T>(std::forward<Args>(args)...);
    DCHECK(op.IsValid());
    return offset;
  }

  // Called by blink::PaintChunksToCcLayer when an effect ends, to update the
  // bounds of a SaveLayer[Alpha]Op which was emitted when the effect started.
  // This is needed because blink doesn't know the bounds when an effect starts.
  // Don't add other mutation methods like this if there is better alternative.
  void UpdateSaveLayerBounds(size_t id, const SkRect& bounds) {
    paint_op_buffer_.UpdateSaveLayerBounds(id, bounds);
  }

  void EndPaintOfUnpaired(const gfx::Rect& visual_rect) {
#if DCHECK_IS_ON()
    DCHECK(IsPainting());
    current_range_start_ = kNotPainting;
#endif
    visual_rects_.resize(paint_op_buffer_.size(), visual_rect);
    GrowCurrentBeginItemVisualRect(visual_rect);
  }

  void EndPaintOfPairedBegin() {
#if DCHECK_IS_ON()
    DCHECK(IsPainting());
    DCHECK_LT(current_range_start_, paint_op_buffer_.size());
    current_range_start_ = kNotPainting;
#endif
    DCHECK_LT(visual_rects_.size(), paint_op_buffer_.size());
    size_t count = paint_op_buffer_.size() - visual_rects_.size();
    paired_begin_stack_.push_back({visual_rects_.size(), count});
    visual_rects_.resize(paint_op_buffer_.size());
  }

  void EndPaintOfPairedEnd();

  // Called after all items are appended, to process the items.
  void Finalize();

  // Calls Finalize(), and returns a PaintRecord from this DisplayItemList,
  // leaving |this| in an empty state.
  sk_sp<PaintRecord> FinalizeAndReleaseAsRecord();

  struct DirectlyCompositedImageResult {
    // See PictureLayerImpl::direct_composited_image_default_raster_scale_.
    gfx::Vector2dF default_raster_scale;
    bool nearest_neighbor;
  };

  // If this list represents an image that should be directly composited (i.e.
  // rasterized at the intrinsic size of the image), return the intrinsic size
  // of the image and whether or not to use nearest neighbor filtering when
  // scaling the layer.
  absl::optional<DirectlyCompositedImageResult>
  GetDirectlyCompositedImageResult() const;

  int num_slow_paths_up_to_min_for_MSAA() const {
    return paint_op_buffer_.num_slow_paths_up_to_min_for_MSAA();
  }
  bool HasNonAAPaint() const { return paint_op_buffer_.HasNonAAPaint(); }

  // This gives the total number of PaintOps.
  size_t TotalOpCount() const { return paint_op_buffer_.total_op_count(); }
  size_t BytesUsed() const {
    // TODO(jbroman): Does anything else owned by this class substantially
    // contribute to memory usage?
    // TODO(vmpstr): Probably DiscardableImageMap is worth counting here.
    return sizeof(*this) + paint_op_buffer_.bytes_used();
  }
  size_t OpBytesUsed() const { return paint_op_buffer_.paint_ops_size(); }

  const DiscardableImageMap& discardable_image_map() const {
    return image_map_;
  }
  base::flat_map<PaintImage::Id, PaintImage::DecodingMode>
  TakeDecodingModeMap() {
    return image_map_.TakeDecodingModeMap();
  }

  void EmitTraceSnapshot() const;
  void GenerateDiscardableImagesMetadata();

  gfx::Rect VisualRectForTesting(int index) {
    return visual_rects_[static_cast<size_t>(index)];
  }

  // If a rectangle is solid color, returns that color. |max_ops_to_analyze|
  // indicates the maximum number of draw ops we consider when determining if a
  // rectangle is solid color.
  bool GetColorIfSolidInRect(const gfx::Rect& rect,
                             SkColor4f* color,
                             int max_ops_to_analyze = 1);

  std::string ToString() const;

  bool has_draw_ops() const { return paint_op_buffer_.has_draw_ops(); }
  bool has_draw_text_ops() const {
    return paint_op_buffer_.has_draw_text_ops();
  }

  // Ops with nested paint ops are considered as a single op.
  size_t num_paint_ops() const { return paint_op_buffer_.size(); }

  bool NeedsAdditionalInvalidationForLCDText(
      const DisplayItemList& old_list) const {
    return paint_op_buffer_.NeedsAdditionalInvalidationForLCDText(
        old_list.paint_op_buffer_);
  }

 private:
  friend class DisplayItemListTest;
  friend gpu::raster::RasterImplementation;
  friend gpu::raster::RasterImplementationGLES;

  ~DisplayItemList();

  void Reset();

  std::unique_ptr<base::trace_event::TracedValue> CreateTracedValue(
      bool include_items) const;
  void AddToValue(base::trace_event::TracedValue*, bool include_items) const;

  // If we're currently within a paired display item block, unions the
  // given visual rect with the begin display item's visual rect.
  void GrowCurrentBeginItemVisualRect(const gfx::Rect& visual_rect) {
    if (!paired_begin_stack_.empty())
      visual_rects_[paired_begin_stack_.back().first_index].Union(visual_rect);
  }

  // Shared between Finalize() and FinalizeAndReleaseAsRecord(). Does not modify
  // `paint_op_buffer_`.
  void FinalizeImpl();

  // RTree stores indices into the paint op buffer.
  // TODO(vmpstr): Update the rtree to store offsets instead.
  RTree<size_t> rtree_;
  DiscardableImageMap image_map_;
  PaintOpBuffer paint_op_buffer_;

  // The visual rects associated with each of the display items in the
  // display item list. These rects are intentionally kept separate because they
  // are used to decide which ops to walk for raster.
  std::vector<gfx::Rect> visual_rects_;
  // Byte offsets associated with each of the ops.
  std::vector<size_t> offsets_;
  // A stack of paired begin sequences that haven't been closed.
  struct PairedBeginInfo {
    // Index (into virual_rects_ and offsets_) of the first operation in the
    // paired begin sequence.
    size_t first_index;
    // Number of operations in the paired begin sequence.
    size_t count;
  };
  std::vector<PairedBeginInfo> paired_begin_stack_;

#if DCHECK_IS_ON()
  // While recording a range of ops, this is the position in the PaintOpBuffer
  // where the recording started.
  bool IsPainting() const { return current_range_start_ != kNotPainting; }
  const size_t kNotPainting = static_cast<size_t>(-1);
  size_t current_range_start_ = kNotPainting;
#endif

  friend class base::RefCountedThreadSafe<DisplayItemList>;
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, BytesUsed);
};

}  // namespace cc

#endif  // CC_PAINT_DISPLAY_ITEM_LIST_H_
