// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DISPLAY_ITEM_LIST_H_
#define CC_PAINT_DISPLAY_ITEM_LIST_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cc/base/rtree.h"
#include "cc/paint/directly_composited_image_info.h"
#include "cc/paint/discardable_image_map.h"
#include "cc/paint/image_id.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_op_buffer.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

class SkCanvas;

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

  void Raster(
      SkCanvas* canvas,
      ImageProvider* image_provider = nullptr,
      const ScrollOffsetMap* raster_inducing_scroll_offsets = nullptr) const;
  void Raster(SkCanvas* canvas, const PlaybackParams& params) const;
  std::vector<size_t> OffsetsOfOpsToRaster(SkCanvas* canvas) const;

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

  void PushDrawScrollingContentsOp(
      ElementId scroll_element_id,
      scoped_refptr<DisplayItemList> display_item_list,
      const gfx::Rect& visual_rect);

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

  // For testing only, to examine the painted result.
  PaintRecord FinalizeAndReleaseAsRecordForTesting();

  const PaintOpBuffer& paint_op_buffer() const { return paint_op_buffer_; }

  // Returns the indices in paint_op_buffer of paint ops whose visual rects
  // intersect `query`.
  void SearchOpsByRect(const gfx::Rect& query,
                       std::vector<size_t>* op_indices) const {
    return rtree_.Search(query, op_indices);
  }

  // If this list represents an image that should be directly composited (i.e.
  // rasterized at the intrinsic size of the image), return the intrinsic size
  // of the image and whether or not to use nearest neighbor filtering when
  // scaling the layer.
  std::optional<DirectlyCompositedImageInfo> GetDirectlyCompositedImageInfo()
      const;

  int num_slow_paths_up_to_min_for_MSAA() const {
    return paint_op_buffer_.num_slow_paths_up_to_min_for_MSAA();
  }
  bool has_non_aa_paint() const { return paint_op_buffer_.has_non_aa_paint(); }

  // This gives the total number of PaintOps.
  size_t TotalOpCount() const { return paint_op_buffer_.total_op_count(); }
  size_t BytesUsed() const {
    // TODO(jbroman): Does anything else owned by this class substantially
    // contribute to memory usage?
    // TODO(vmpstr): Probably DiscardableImageMap is worth counting here.
    return sizeof(*this) + paint_op_buffer_.bytes_used();
  }
  size_t OpBytesUsed() const { return paint_op_buffer_.paint_ops_size(); }

  scoped_refptr<DiscardableImageMap> GenerateDiscardableImageMap(
      const ScrollOffsetMap& raster_inducing_scroll_offsets) const;

  void EmitTraceSnapshot() const;

  gfx::Rect VisualRectForTesting(int index) const {
    return visual_rects_[static_cast<size_t>(index)];
  }

  // If a rectangle is solid color, returns that color. |max_ops_to_analyze|
  // indicates the maximum number of draw ops we consider when determining if a
  // rectangle is solid color.
  bool GetColorIfSolidInRect(const gfx::Rect& rect,
                             SkColor4f* color,
                             int max_ops_to_analyze = 1) const;

  std::string ToString() const;

  bool has_draw_ops() const { return paint_op_buffer_.has_draw_ops(); }
  bool has_draw_text_ops() const {
    return paint_op_buffer_.has_draw_text_ops();
  }
  bool has_save_layer_ops() const {
    return paint_op_buffer_.has_save_layer_ops();
  }
  bool has_save_layer_alpha_ops() const {
    return paint_op_buffer_.has_save_layer_alpha_ops();
  }
  bool has_effects_preventing_lcd_text_for_save_layer_alpha() const {
    return paint_op_buffer_
        .has_effects_preventing_lcd_text_for_save_layer_alpha();
  }
  bool has_discardable_images() const {
    return paint_op_buffer_.has_discardable_images();
  }
  gfx::ContentColorUsage content_color_usage() const {
    return paint_op_buffer_.content_color_usage();
  }

  // Ops with nested paint ops are considered as a single op.
  size_t num_paint_ops() const { return paint_op_buffer_.size(); }

  bool NeedsAdditionalInvalidationForLCDText(
      const DisplayItemList& old_list) const {
    return paint_op_buffer_.NeedsAdditionalInvalidationForLCDText(
        old_list.paint_op_buffer_);
  }

  std::optional<gfx::Rect> bounds() const { return rtree_.bounds(); }

  struct RasterInducingScrollInfo {
    // See PushDrawScrollingContentsOp() for how we handle visual rect of
    // nested DrawScrollingContentsOp.
    gfx::Rect visual_rect;
    bool has_discardable_images;
  };
  // Maps scroll element ids of DrawScrollingContentsOps to info.
  // This is only kept in the top-level DisplayItemList after recording.
  using RasterInducingScrollMap =
      base::flat_map<ElementId, RasterInducingScrollInfo>;
  const RasterInducingScrollMap& raster_inducing_scrolls() const {
    return raster_inducing_scrolls_;
  }

 private:
  friend class DisplayItemListTest;

  ~DisplayItemList();

  std::unique_ptr<base::trace_event::TracedValue> CreateTracedValue(
      bool include_items) const;
  void AddToValue(base::trace_event::TracedValue*, bool include_items) const;

  // If we're currently within a paired display item block, unions the
  // given visual rect with the begin display item's visual rect.
  void GrowCurrentBeginItemVisualRect(const gfx::Rect& visual_rect) {
    if (!paired_begin_stack_.empty())
      visual_rects_[paired_begin_stack_.back().first_index].Union(visual_rect);
  }

  RasterInducingScrollMap raster_inducing_scrolls_;

  // RTree stores offsets into the paint op buffer. It's available after
  // Finalize().
  RTree<size_t> rtree_;

  PaintOpBuffer paint_op_buffer_;

  // The visual rects associated with each of the paint ops in this
  // DisplayItemList. This is used during recording and is cleared in
  // Finalize().
  std::vector<gfx::Rect> visual_rects_;
  // Byte offsets associated with each of the ops. This is used during
  // recording and is cleared in Finalize().
  std::vector<size_t> offsets_;
  // A stack of paired begin sequences that haven't been closed. This is used
  // during recording and should be empty when Finalize() is called.
  struct PairedBeginInfo {
    // Index (into virual_rects_ and offsets_) of the first operation in the
    // paired begin sequence.
    size_t first_index;
    // Number of operations in the paired begin sequence.
    size_t count;
  };
  std::vector<PairedBeginInfo> paired_begin_stack_;

#if DCHECK_IS_ON()
  bool IsPainting() const {
    DCHECK(!IsFinalized());
    return current_range_start_ != kNotPainting;
  }
  // paint_op_buffer_ is not mutable once Finalize() is called.
  bool IsFinalized() const { return current_range_start_ == kFinalized; }
  static constexpr size_t kNotPainting = static_cast<size_t>(-1);
  static constexpr size_t kFinalized = static_cast<size_t>(-2);
  // While recording a range of ops, this is the position in the PaintOpBuffer
  // where the recording started.
  size_t current_range_start_ = kNotPainting;
#endif

  friend class base::RefCountedThreadSafe<DisplayItemList>;
  FRIEND_TEST_ALL_PREFIXES(DisplayItemListTest, BytesUsed);
};

}  // namespace cc

#endif  // CC_PAINT_DISPLAY_ITEM_LIST_H_
