// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/display_item_list.h"

#include <stddef.h>

#include <string>

#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/math_util.h"
#include "cc/debug/picture_debug_util.h"
#include "cc/paint/solid_color_analyzer.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

namespace {

bool GetCanvasClipBounds(SkCanvas* canvas, gfx::Rect* clip_bounds) {
  SkRect canvas_clip_bounds;
  if (!canvas->getLocalClipBounds(&canvas_clip_bounds))
    return false;
  *clip_bounds = ToEnclosingRect(gfx::SkRectToRectF(canvas_clip_bounds));
  return true;
}

void FillTextContent(const PaintOpBuffer* buffer,
                     std::vector<NodeId>* content) {
  for (auto* op : PaintOpBuffer::Iterator(buffer)) {
    if (op->GetType() == PaintOpType::DrawTextBlob) {
      content->push_back(static_cast<DrawTextBlobOp*>(op)->node_id);
    } else if (op->GetType() == PaintOpType::DrawRecord) {
      FillTextContent(static_cast<DrawRecordOp*>(op)->record.get(), content);
    }
  }
}

void FillTextContentByOffsets(const PaintOpBuffer* buffer,
                              const std::vector<size_t>& offsets,
                              std::vector<NodeId>* content) {
  if (!buffer)
    return;
  for (auto* op : PaintOpBuffer::OffsetIterator(buffer, &offsets)) {
    if (op->GetType() == PaintOpType::DrawTextBlob) {
      content->push_back(static_cast<DrawTextBlobOp*>(op)->node_id);
    } else if (op->GetType() == PaintOpType::DrawRecord) {
      FillTextContent(static_cast<DrawRecordOp*>(op)->record.get(), content);
    }
  }
}

}  // namespace

DisplayItemList::DisplayItemList(UsageHint usage_hint)
    : usage_hint_(usage_hint) {
  if (usage_hint_ == kTopLevelDisplayItemList) {
    visual_rects_.reserve(1024);
    offsets_.reserve(1024);
    begin_paired_indices_.reserve(32);
  }
}

DisplayItemList::~DisplayItemList() = default;

void DisplayItemList::Raster(SkCanvas* canvas,
                             ImageProvider* image_provider) const {
  DCHECK(usage_hint_ == kTopLevelDisplayItemList);
  gfx::Rect canvas_playback_rect;
  if (!GetCanvasClipBounds(canvas, &canvas_playback_rect))
    return;

  std::vector<size_t> offsets;
  rtree_.Search(canvas_playback_rect, &offsets);
  paint_op_buffer_.Playback(canvas, PlaybackParams(image_provider), &offsets);
}

void DisplayItemList::CaptureContent(const gfx::Rect& rect,
                                     std::vector<NodeId>* content) const {
  std::vector<size_t> offsets;
  rtree_.Search(rect, &offsets);
  FillTextContentByOffsets(&paint_op_buffer_, offsets, content);
}

void DisplayItemList::Finalize() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "DisplayItemList::Finalize");
#if DCHECK_IS_ON()
  // If this fails a call to StartPaint() was not ended.
  DCHECK(!IsPainting());
  // If this fails we had more calls to EndPaintOfPairedBegin() than
  // to EndPaintOfPairedEnd().
  DCHECK(begin_paired_indices_.empty());
  DCHECK_EQ(visual_rects_.size(), offsets_.size());
#endif

  if (usage_hint_ == kTopLevelDisplayItemList) {
    rtree_.Build(visual_rects_,
                 [](const std::vector<gfx::Rect>& rects, size_t index) {
                   return rects[index];
                 },
                 [this](const std::vector<gfx::Rect>& rects, size_t index) {
                   // Ignore the given rects, since the payload comes from
                   // offsets. However, the indices match, so we can just index
                   // into offsets.
                   return offsets_[index];
                 });
  }
  paint_op_buffer_.ShrinkToFit();
  visual_rects_.clear();
  visual_rects_.shrink_to_fit();
  offsets_.clear();
  offsets_.shrink_to_fit();
  begin_paired_indices_.shrink_to_fit();
}

size_t DisplayItemList::BytesUsed() const {
  // TODO(jbroman): Does anything else owned by this class substantially
  // contribute to memory usage?
  // TODO(vmpstr): Probably DiscardableImageMap is worth counting here.
  return sizeof(*this) + paint_op_buffer_.bytes_used();
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

std::unique_ptr<base::trace_event::TracedValue>
DisplayItemList::CreateTracedValue(bool include_items) const {
  auto state = std::make_unique<base::trace_event::TracedValue>();
  state->BeginDictionary("params");

  gfx::Rect bounds;
  if (rtree_.has_valid_bounds()) {
    bounds = rtree_.GetBoundsOrDie();
  } else {
    // For tracing code, just use the entire positive quadrant if the |rtree_|
    // has invalid bounds.
    bounds = gfx::Rect(INT_MAX, INT_MAX);
  }

  if (include_items) {
    state->BeginArray("items");

    PlaybackParams params(nullptr, SkMatrix::I());
    std::map<size_t, gfx::Rect> visual_rects = rtree_.GetAllBoundsForTracing();
    for (const PaintOp* op : PaintOpBuffer::Iterator(&paint_op_buffer_)) {
      state->BeginDictionary();
      state->SetString("name", PaintOpTypeToString(op->GetType()));

      MathUtil::AddToTracedValue(
          "visual_rect",
          visual_rects[paint_op_buffer_.GetOpOffsetForTracing(op)],
          state.get());

      SkPictureRecorder recorder;
      SkCanvas* canvas = recorder.beginRecording(gfx::RectToSkRect(bounds));
      op->Raster(canvas, params);
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

  MathUtil::AddToTracedValue("layer_rect", bounds, state.get());
  state->EndDictionary();  // "params".

  {
    SkPictureRecorder recorder;
    SkCanvas* canvas = recorder.beginRecording(gfx::RectToSkRect(bounds));
    canvas->translate(-bounds.x(), -bounds.y());
    canvas->clipRect(gfx::RectToSkRect(bounds));
    Raster(canvas);
    sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();

    std::string b64_picture;
    PictureDebugUtil::SerializeAsBase64(picture.get(), &b64_picture);
    state->SetString("skp64", b64_picture);
  }
  return state;
}

void DisplayItemList::GenerateDiscardableImagesMetadata() {
  DCHECK(usage_hint_ == kTopLevelDisplayItemList);

  gfx::Rect bounds;
  if (rtree_.has_valid_bounds()) {
    bounds = rtree_.GetBoundsOrDie();
  } else {
    // Bounds are only used to size an SkNoDrawCanvas, pass INT_MAX.
    bounds = gfx::Rect(INT_MAX, INT_MAX);
  }

  image_map_.Generate(&paint_op_buffer_, bounds);
}

void DisplayItemList::Reset() {
#if DCHECK_IS_ON()
  DCHECK(!IsPainting());
  DCHECK(begin_paired_indices_.empty());
#endif

  rtree_.Reset();
  image_map_.Reset();
  paint_op_buffer_.Reset();
  visual_rects_.clear();
  visual_rects_.shrink_to_fit();
  offsets_.clear();
  offsets_.shrink_to_fit();
  begin_paired_indices_.clear();
  begin_paired_indices_.shrink_to_fit();
}

sk_sp<PaintRecord> DisplayItemList::ReleaseAsRecord() {
  sk_sp<PaintRecord> record =
      sk_make_sp<PaintOpBuffer>(std::move(paint_op_buffer_));

  Reset();
  return record;
}

bool DisplayItemList::GetColorIfSolidInRect(const gfx::Rect& rect,
                                            SkColor* color,
                                            int max_ops_to_analyze) {
  DCHECK(usage_hint_ == kTopLevelDisplayItemList);
  std::vector<size_t>* offsets_to_use = nullptr;
  std::vector<size_t> offsets;
  if (rtree_.has_valid_bounds() && !rect.Contains(rtree_.GetBoundsOrDie())) {
    rtree_.Search(rect, &offsets);
    offsets_to_use = &offsets;
  }

  base::Optional<SkColor> solid_color =
      SolidColorAnalyzer::DetermineIfSolidColor(
          &paint_op_buffer_, rect, max_ops_to_analyze, offsets_to_use);
  if (solid_color) {
    *color = *solid_color;
    return true;
  }
  return false;
}

}  // namespace cc
