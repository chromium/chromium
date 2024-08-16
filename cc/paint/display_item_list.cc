// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/display_item_list.h"

#include <limits>
#include <map>
#include <string>

#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/math_util.h"
#include "cc/debug/picture_debug_util.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "cc/paint/solid_color_analyzer.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {

namespace {

bool GetCanvasClipBounds(SkCanvas* canvas, gfx::Rect* clip_bounds) {
  SkRect canvas_clip_bounds;
  if (!canvas->getLocalClipBounds(&canvas_clip_bounds))
    return false;
  *clip_bounds = ToEnclosingRect(gfx::SkRectToRectF(canvas_clip_bounds));
  return true;
}

template <typename Function>
void IterateTextContent(const PaintOpBuffer& buffer,
                        const Function& yield,
                        const gfx::Rect& rect) {
  if (!buffer.has_draw_text_ops())
    return;
  for (const PaintOp& op : buffer) {
    if (op.GetType() == PaintOpType::kDrawTextBlob) {
      yield(static_cast<const DrawTextBlobOp&>(op), rect);
    } else if (op.GetType() == PaintOpType::kDrawRecord) {
      IterateTextContent(static_cast<const DrawRecordOp&>(op).record.buffer(),
                         yield, rect);
    }
  }
}

template <typename Function>
void IterateTextContentByOffsets(const PaintOpBuffer& buffer,
                                 const std::vector<size_t>& offsets,
                                 const std::vector<gfx::Rect>& rects,
                                 const Function& yield) {
  DCHECK(buffer.has_draw_text_ops());
  DCHECK_EQ(rects.size(), offsets.size());
  size_t index = 0;
  for (const PaintOp& op : PaintOpBuffer::OffsetIterator(buffer, offsets)) {
    if (op.GetType() == PaintOpType::kDrawTextBlob) {
      yield(static_cast<const DrawTextBlobOp&>(op), rects[index]);
    } else if (op.GetType() == PaintOpType::kDrawRecord) {
      IterateTextContent(static_cast<const DrawRecordOp&>(op).record.buffer(),
                         yield, rects[index]);
    }
    ++index;
  }
}

constexpr gfx::Rect kMaxBounds(std::numeric_limits<int>::max(),
                               std::numeric_limits<int>::max());

}  // namespace

DisplayItemList::DisplayItemList() {
  visual_rects_.reserve(1024);
  offsets_.reserve(1024);
  paired_begin_stack_.reserve(32);
}

DisplayItemList::~DisplayItemList() = default;

void DisplayItemList::Raster(
    SkCanvas* canvas,
    ImageProvider* image_provider,
    const ScrollOffsetMap* raster_inducing_scroll_offsets) const {
  PlaybackParams params(image_provider);
  params.raster_inducing_scroll_offsets = raster_inducing_scroll_offsets;
  Raster(canvas, params);
}

void DisplayItemList::Raster(SkCanvas* canvas,
                             const PlaybackParams& params) const {
#if DCHECK_IS_ON()
  DCHECK(IsFinalized());
#endif

  TRACE_EVENT_BEGIN1("cc", "DisplayItemList::Raster", "total_op_count",
                     TotalOpCount());
  std::vector<size_t> offsets = OffsetsOfOpsToRaster(canvas);
  if (offsets.empty()) {
    return;
  }
  paint_op_buffer_.Playback(canvas, params, /*local_ctm=*/true, &offsets);

  bool trace_enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("cc", &trace_enabled);
  if (trace_enabled) {
    size_t rastered_op_count = 0;
    for (PaintOpBuffer::PlaybackFoldingIterator it(paint_op_buffer_, &offsets);
         it; ++it) {
      rastered_op_count += 1 + it->AdditionalOpCount();
    }
    TRACE_EVENT_END1("cc", "DisplayItemList::Raster", "rastered_op_count",
                     rastered_op_count);
  }
}

std::vector<size_t> DisplayItemList::OffsetsOfOpsToRaster(
    SkCanvas* canvas) const {
  std::vector<size_t> offsets;
  gfx::Rect canvas_playback_rect;
  if (GetCanvasClipBounds(canvas, &canvas_playback_rect)) {
    rtree_.Search(canvas_playback_rect, &offsets);
  }
  return offsets;
}

void DisplayItemList::CaptureContent(const gfx::Rect& rect,
                                     std::vector<NodeInfo>* content) const {
#if DCHECK_IS_ON()
  DCHECK(IsFinalized());
#endif

  if (!paint_op_buffer_.has_draw_text_ops())
    return;
  std::vector<size_t> offsets;
  std::vector<gfx::Rect> rects;
  rtree_.Search(rect, &offsets, &rects);
  IterateTextContentByOffsets(
      paint_op_buffer_, offsets, rects,
      [content](const DrawTextBlobOp& op, const gfx::Rect& rect) {
        // Only union the rect if the current is the same as the last one.
        if (!content->empty() && content->back().node_id == op.node_id)
          content->back().visual_rect.Union(rect);
        else
          content->emplace_back(op.node_id, rect);
      });
}

double DisplayItemList::AreaOfDrawText(const gfx::Rect& rect) const {
  if (!paint_op_buffer_.has_draw_text_ops())
    return 0;
  std::vector<size_t> offsets;
  std::vector<gfx::Rect> rects;
  rtree_.Search(rect, &offsets, &rects);
  DCHECK_EQ(offsets.size(), rects.size());

  double area = 0;
  size_t index = 0;
  for (const PaintOp& op :
       PaintOpBuffer::OffsetIterator(paint_op_buffer_, offsets)) {
    if (op.GetType() == PaintOpType::kDrawTextBlob ||
        // Don't walk into the record because the visual rect is already the
        // bounding box of the sub paint operations. This works for most paint
        // results for text generated by blink.
        (op.GetType() == PaintOpType::kDrawRecord &&
         static_cast<const DrawRecordOp&>(op).record.has_draw_text_ops())) {
      area += static_cast<double>(rects[index].width()) * rects[index].height();
    }
    ++index;
  }
  return area;
}

void DisplayItemList::EndPaintOfPairedEnd() {
#if DCHECK_IS_ON()
  DCHECK(IsPainting());
  DCHECK_LT(current_range_start_, paint_op_buffer_.size());
  current_range_start_ = kNotPainting;
#endif
  DCHECK(paired_begin_stack_.size());
  size_t last_begin_index = paired_begin_stack_.back().first_index;
  size_t last_begin_count = paired_begin_stack_.back().count;
  DCHECK_GT(last_begin_count, 0u);

  // Copy the visual rect at |last_begin_index| to all indices that constitute
  // the begin item. Note that because we possibly reallocate the
  // |visual_rects_| buffer below, we need an actual copy instead of a const
  // reference which can become dangling.
  auto visual_rect = visual_rects_[last_begin_index];
  for (size_t i = 1; i < last_begin_count; ++i)
    visual_rects_[i + last_begin_index] = visual_rect;
  paired_begin_stack_.pop_back();

  // Copy the visual rect of the matching begin item to the end item(s).
  visual_rects_.resize(paint_op_buffer_.size(), visual_rect);

  // The block that ended needs to be included in the bounds of the enclosing
  // block.
  GrowCurrentBeginItemVisualRect(visual_rect);
}

void DisplayItemList::Finalize() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "DisplayItemList::Finalize");
#if DCHECK_IS_ON()
  // If this fails a call to StartPaint() was not ended.
  DCHECK(!IsPainting());
  // If this fails we had more calls to EndPaintOfPairedBegin() than
  // to EndPaintOfPairedEnd().
  DCHECK(paired_begin_stack_.empty());
  DCHECK_EQ(visual_rects_.size(), offsets_.size());
  current_range_start_ = kFinalized;
#endif

  rtree_.Build(
      visual_rects_.size(),
      [this](size_t index) { return visual_rects_[index]; },
      [this](size_t index) { return offsets_[index]; });
  visual_rects_.clear();
  visual_rects_.shrink_to_fit();
  offsets_.clear();
  offsets_.shrink_to_fit();
  paired_begin_stack_.shrink_to_fit();
  paint_op_buffer_.ShrinkToFit();
}

PaintRecord DisplayItemList::FinalizeAndReleaseAsRecordForTesting() {
  Finalize();
  return paint_op_buffer_.ReleaseAsRecord();
}

void DisplayItemList::EmitTraceSnapshot() const {
  bool include_items;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.display_items"), &include_items);
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.display_items") ","
      TRACE_DISABLED_BY_DEFAULT("cc.debug.picture") ","
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.picture"),
      "cc::DisplayItemList", TRACE_ID_LOCAL(this),
      CreateTracedValue(include_items));
}

std::string DisplayItemList::ToString() const {
  base::trace_event::TracedValueJSON value;
  AddToValue(&value, true);
  return value.ToFormattedJSON();
}

std::unique_ptr<base::trace_event::TracedValue>
DisplayItemList::CreateTracedValue(bool include_items) const {
  auto state = std::make_unique<base::trace_event::TracedValue>();
  AddToValue(state.get(), include_items);
  return state;
}

void DisplayItemList::AddToValue(base::trace_event::TracedValue* state,
                                 bool include_items) const {
  state->BeginDictionary("params");

  // DisplayItemList doesn't know the current scroll offsets, so use zero.
  ScrollOffsetMap zero_scroll_offsets;
  for (auto [element_id, _] : raster_inducing_scrolls()) {
    zero_scroll_offsets[element_id] = gfx::PointF();
  }

  // For tracing code, just use the entire positive quadrant if the |rtree_|
  // has invalid bounds.
  gfx::Rect bounds = rtree_.bounds().value_or(kMaxBounds);
  if (include_items) {
    state->BeginArray("items");

    PlaybackParams params(nullptr, SkM44());
    params.raster_inducing_scroll_offsets = &zero_scroll_offsets;
    std::map<size_t, gfx::Rect> visual_rects = rtree_.GetAllBoundsForTracing();
    for (const PaintOp& op : paint_op_buffer_) {
      state->BeginDictionary();
      state->SetString("name", PaintOpTypeToString(op.GetType()));

      MathUtil::AddToTracedValue(
          "visual_rect",
          visual_rects[paint_op_buffer_.GetOpOffsetForTracing(op)], state);

      SkPictureRecorder recorder;
      SkCanvas* canvas = recorder.beginRecording(gfx::RectToSkRect(bounds));
      op.Raster(canvas, params);
      sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();

      if (picture->approximateOpCount()) {
        std::string b64_picture;
        PictureDebugUtil::SerializeAsBase64(picture.get(), &b64_picture);
        state->SetString("skp64", b64_picture);
      }

      state->EndDictionary();
    }

    state->EndArray();  // "items".
  }

  MathUtil::AddToTracedValue("layer_rect", bounds, state);
  state->EndDictionary();  // "params".

  {
    SkPictureRecorder recorder;
    SkCanvas* canvas = recorder.beginRecording(gfx::RectToSkRect(bounds));
    canvas->translate(-bounds.x(), -bounds.y());
    canvas->clipRect(gfx::RectToSkRect(bounds));
    Raster(canvas, /*image_provider=*/nullptr, &zero_scroll_offsets);
    sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();

    std::string b64_picture;
    PictureDebugUtil::SerializeAsBase64(picture.get(), &b64_picture);
    state->SetString("skp64", b64_picture);
  }
}

scoped_refptr<DiscardableImageMap> DisplayItemList::GenerateDiscardableImageMap(
    const ScrollOffsetMap& raster_inducing_scroll_offsets) const {
#if DCHECK_IS_ON()
  DCHECK(IsFinalized());
#endif

  // Bounds are only used to size an SkNoDrawCanvas.
  return DiscardableImageMap::Generate(paint_op_buffer_,
                                       bounds().value_or(kMaxBounds),
                                       raster_inducing_scroll_offsets);
}

bool DisplayItemList::GetColorIfSolidInRect(const gfx::Rect& rect,
                                            SkColor4f* color,
                                            int max_ops_to_analyze) const {
#if DCHECK_IS_ON()
  DCHECK(IsFinalized());
#endif

  std::vector<size_t>* offsets_to_use = nullptr;
  std::vector<size_t> offsets;
  if (rtree_.has_valid_bounds() && !rect.Contains(*bounds())) {
    rtree_.Search(rect, &offsets);
    offsets_to_use = &offsets;
  }

  std::optional<SkColor4f> solid_color =
      SolidColorAnalyzer::DetermineIfSolidColor(
          paint_op_buffer_, rect, max_ops_to_analyze, offsets_to_use);
  if (solid_color) {
    *color = *solid_color;
    return true;
  }
  return false;
}

namespace {

std::optional<DirectlyCompositedImageInfo>
DirectlyCompositedImageInfoForPaintOpBuffer(const PaintOpBuffer& op_buffer) {
  // A PaintOpBuffer for an image may have 1 (a kDrawimagerect or a kDrawrecord
  // that recursively contains a PaintOpBuffer for an image) or 4 paint
  // operations:
  //  (1) kSave
  //  (2) kTranslate which applies an offset of the image in the layer
  //   or kConcat with a transformation rotating the image by +/-90 degrees for
  //      image orientation
  //  (3) kDrawimagerect or kDrawrecord (see the 1 operation case above)
  //  (4) kRestore
  // The following algorithm also supports kTranslate and kConcat in the same
  // PaintOpBuffer (i.e. 5 operations).
  constexpr size_t kMaxDrawImageOps = 5;
  if (op_buffer.size() > kMaxDrawImageOps)
    return std::nullopt;

  bool transpose_image_size = false;
  std::optional<DirectlyCompositedImageInfo> result;
  for (const PaintOp& op : op_buffer) {
    switch (op.GetType()) {
      case PaintOpType::kSave:
      case PaintOpType::kRestore:
      case PaintOpType::kTranslate:
        break;
      case PaintOpType::kConcat: {
        // We only expect a single transformation. If we see another one, then
        // this image won't be eligible for directly compositing.
        if (transpose_image_size)
          return std::nullopt;
        // The transformation must be before the kDrawimagerect operation.
        if (result)
          return std::nullopt;

        const ConcatOp& concat_op = static_cast<const ConcatOp&>(op);
        if (!MathUtil::SkM44Preserves2DAxisAlignment(concat_op.matrix))
          return std::nullopt;

        // If the image has been rotated +/-90 degrees we'll need to transpose
        // the width and height dimensions to account for the same transform
        // applying when the layer bounds were calculated. Since we already
        // know that the transformation preserves axis alignment, we only
        // need to confirm that this is not a scaling operation.
        transpose_image_size = (concat_op.matrix.rc(0, 0) == 0);
        break;
      }
      case PaintOpType::kDrawImageRect: {
        if (result)
          return std::nullopt;
        const auto& draw_image_rect_op =
            static_cast<const DrawImageRectOp&>(op);
        const SkRect& src = draw_image_rect_op.src;
        const SkRect& dst = draw_image_rect_op.dst;
        if (src.isEmpty() || dst.isEmpty())
          return std::nullopt;
        result.emplace();
        result->default_raster_scale = gfx::Vector2dF(
            src.width() / dst.width(), src.height() / dst.height());
        // Ensure the layer will use nearest neighbor when drawn by the display
        // compositor, if required.
        result->nearest_neighbor =
            draw_image_rect_op.flags.getFilterQuality() ==
            PaintFlags::FilterQuality::kNone;
        break;
      }
      case PaintOpType::kDrawRecord:
        if (result)
          return std::nullopt;
        result = DirectlyCompositedImageInfoForPaintOpBuffer(
            static_cast<const DrawRecordOp&>(op).record.buffer());
        if (!result)
          return std::nullopt;
        break;
      default:
        // Disqualify the layer as a directly composited image if any other
        // paint op is detected.
        return std::nullopt;
    }
  }

  if (result && transpose_image_size)
    result->default_raster_scale.Transpose();
  return result;
}

}  // anonymous namespace

std::optional<DirectlyCompositedImageInfo>
DisplayItemList::GetDirectlyCompositedImageInfo() const {
#if DCHECK_IS_ON()
  DCHECK(IsFinalized());
#endif
  return DirectlyCompositedImageInfoForPaintOpBuffer(paint_op_buffer_);
}

void DisplayItemList::PushDrawScrollingContentsOp(
    ElementId scroll_element_id,
    scoped_refptr<DisplayItemList> display_item_list,
    const gfx::Rect& visual_rect) {
  StartPaint();
  push<DrawScrollingContentsOp>(scroll_element_id, display_item_list);
  for (auto& [nested_scroll_element_id, info] :
       std::move(display_item_list->raster_inducing_scrolls_)) {
    // For a nested scroller, we use the parent scroller's visual rect (which
    // will eventually use the top-level scroller's visual rect in the layer).
    // This will cause over-invalidation when the nested scroller scrolls, but
    // avoids the complexity and cost of mapping the visual rect of nested
    // scroller to the layer space, especially when the parent scroller scrolls.
    // TODO(crbug.com/359279553): Evaluate if the optimization is worth it.
    raster_inducing_scrolls_[nested_scroll_element_id] =
        RasterInducingScrollInfo{visual_rect, info.has_discardable_images};
  }
  raster_inducing_scrolls_[scroll_element_id] = RasterInducingScrollInfo{
      visual_rect, display_item_list->has_discardable_images()};
  EndPaintOfUnpaired(visual_rect);
}

}  // namespace cc
