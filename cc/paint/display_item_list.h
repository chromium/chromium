// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DISPLAY_ITEM_LIST_H_
#define CC_PAINT_DISPLAY_ITEM_LIST_H_

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/rtree.h"
#include "cc/paint/discardable_image_map.h"
#include "cc/paint/image_id.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_op_buffer.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"

class SkCanvas;

namespace gpu {
namespace raster {
class RasterImplementation;
class RasterImplementationGLES;
}  // namespace raster
}  // namespace gpu

namespace base {
namespace trace_event {
class TracedValue;
}
}

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
  // TODO(vmpstr): It would be cool if we didn't need this, and instead used
  // PaintOpBuffer directly when we needed to release this as a paint op buffer.
  enum UsageHint { kTopLevelDisplayItemList, kToBeReleasedAsPaintOpBuffer };

  explicit DisplayItemList(UsageHint = kTopLevelDisplayItemList);
  DisplayItemList(const DisplayItemList&) = delete;
  DisplayItemList& operator=(const DisplayItemList&) = delete;

  void Raster(SkCanvas* canvas, ImageProvider* image_provider = nullptr) const;

  // Captures the DrawTextBlobOp within |rect| and returns the associated
  // NodeId in |content|.
  void CaptureContent(const gfx::Rect& rect,
                      std::vector<NodeId>* content) const;

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
    if (usage_hint_ == kTopLevelDisplayItemList)
      offsets_.push_back(offset);
    paint_op_buffer_.push<T>(std::forward<Args>(args)...);
    return offset;
  }

  UsageHint GetUsageHint() const { return usage_hint_; }

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
    if (usage_hint_ == kToBeReleasedAsPaintOpBuffer)
      return;

    visual_rects_.resize(paint_op_buffer_.size(), visual_rect);
    GrowCurrentBeginItemVisualRect(visual_rect);
  }

  void EndPaintOfPairedBegin() {
#if DCHECK_IS_ON()
    DCHECK(IsPainting());
    DCHECK_LT(current_range_start_, paint_op_buffer_.size());
    current_range_start_ = kNotPainting;
#endif
    if (usage_hint_ == kToBeReleasedAsPaintOpBuffer)
      return;

    DCHECK_LT(visual_rects_.size(), paint_op_buffer_.size());
    size_t count = paint_op_buffer_.size() - visual_rects_.size();
    visual_rects_.resize(paint_op_buffer_.size());
    begin_paired_indices_.push_back(
        std::make_pair(visual_rects_.size() - 1, count));
  }

  void EndPaintOfPairedEnd() {
#if DCHECK_IS_ON()
    DCHECK(IsPainting());
    DCHECK_LT(current_range_start_, paint_op_buffer_.size());
    current_range_start_ = kNotPainting;
#endif
    if (usage_hint_ == kToBeReleasedAsPaintOpBuffer)
      return;

    DCHECK(begin_paired_indices_.size());
    size_t last_begin_index = begin_paired_indices_.back().first;
    size_t last_begin_count = begin_paired_indices_.back().second;
    DCHECK_GT(last_begin_count, 0u);
    DCHECK_GE(last_begin_index, last_begin_count - 1);

    // Copy the visual rect at |last_begin_index| to all indices that constitute
    // the begin item. Note that because we possibly reallocate the
    // |visual_rects_| buffer below, we need an actual copy instead of a const
    // reference which can become dangling.
    auto visual_rect = visual_rects_[last_begin_index];
    for (size_t i = last_begin_index - last_begin_count + 1;
         i < last_begin_index; ++i) {
      visual_rects_[i] = visual_rect;
    }
    begin_paired_indices_.pop_back();

    // Copy the visual rect of the matching begin item to the end item(s).
    visual_rects_.resize(paint_op_buffer_.size(), visual_rect);

    // The block that ended needs to be included in the bounds of the enclosing
    // block.
    GrowCurrentBeginItemVisualRect(visual_rect);
  }

  // Called after all items are appended, to process the items.
  void Finalize();

  int NumSlowPaths() const { return paint_op_buffer_.numSlowPaths(); }
  bool HasNonAAPaint() const { return paint_op_buffer_.HasNonAAPaint(); }
  bool HasText() const { return paint_op_buffer_.HasText(); }

  // This gives the total number of PaintOps.
  size_t TotalOpCount() const { return paint_op_buffer_.total_op_count(); }
  size_t BytesUsed() const;

  const DiscardableImageMap& discardable_image_map() const {
    return image_map_;
  }
  base::flat_map<PaintImage::Id, PaintImage::DecodingMode>
  TakeDecodingModeMap() {
    return image_map_.TakeDecodingModeMap();
  }

  void EmitTraceSnapshot() const;
  void GenerateDiscardableImagesMetadata();

  gfx::Rect VisualRectForTesting(int index) { return visual_rects_[index]; }

  // Generate a PaintRecord from this DisplayItemList, leaving |this| in
  // an empty state.
  sk_sp<PaintRecord> ReleaseAsRecord();

  // If a rectangle is solid color, returns that color. |max_ops_to_analyze|
  // indicates the maximum number of draw ops we consider when determining if a
  // rectangle is solid color.
  bool GetColorIfSolidInRect(const gfx::Rect& rect,
                             SkColor* color,
                             int max_ops_to_analyze = 1);

 private:
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, TraceEmptyVisualRect);
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, AsValueWithNoOps);
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, AsValueWithOps);
  friend gpu::raster::RasterImplementation;
  friend gpu::raster::RasterImplementationGLES;

  ~DisplayItemList();

  void Reset();

  std::unique_ptr<base::trace_event::TracedValue> CreateTracedValue(
      bool include_items) const;

  // If we're currently within a paired display item block, unions the
  // given visual rect with the begin display item's visual rect.
  void GrowCurrentBeginItemVisualRect(const gfx::Rect& visual_rect) {
    DCHECK_EQ(usage_hint_, kTopLevelDisplayItemList);
    if (!begin_paired_indices_.empty())
      visual_rects_[begin_paired_indices_.back().first].Union(visual_rect);
  }

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
  // A stack of pairs of indices and counts. The indices are into the
  // |visual_rects_| for each paired begin range that hasn't been closed. The
  // counts refer to the number of visual rects in that begin sequence that end
  // with the index.
  std::vector<std::pair<size_t, size_t>> begin_paired_indices_;

#if DCHECK_IS_ON()
  // While recording a range of ops, this is the position in the PaintOpBuffer
  // where the recording started.
  bool IsPainting() const { return current_range_start_ != kNotPainting; }
  const size_t kNotPainting = static_cast<size_t>(-1);
  size_t current_range_start_ = kNotPainting;
#endif

  UsageHint usage_hint_;

  friend class base::RefCountedThreadSafe<DisplayItemList>;
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, BytesUsed);
};

}  // namespace cc

#endif  // CC_PAINT_DISPLAY_ITEM_LIST_H_
